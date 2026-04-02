#!/usr/bin/env python3
import argparse
import os
import sys
import termios
import time


SOF0 = 0x48
SOF1 = 0x44
FRAME_INFO_REQ = 0x10
FRAME_INFO_RSP = 0x11
FRAME_CTRL_WRITE = 0x20
FRAME_CTRL_RSP = 0x21
FRAME_DATA_WRITE = 0x30
FRAME_DATA_RSP = 0x31
FRAME_STATUS_REQ = 0x40
FRAME_STATUS_RSP = 0x41

CMD_BEGIN = 0x01
CMD_ABORT = 0x02
CMD_COMMIT = 0x03


def crc32_ieee(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if (crc & 1) else (crc >> 1)
    return crc ^ 0xFFFFFFFF


def checksum(frame_type: int, payload: bytes) -> int:
    total = frame_type + (len(payload) & 0xFF) + ((len(payload) >> 8) & 0xFF) + sum(payload)
    return total & 0xFF


def encode_frame(frame_type: int, payload: bytes) -> bytes:
    return bytes([SOF0, SOF1, frame_type, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload + bytes([checksum(frame_type, payload)])


class FrameReader:
    def __init__(self):
        self.buf = bytearray()

    def feed(self, chunk: bytes):
        self.buf.extend(chunk)
        frames = []
        while True:
            start = self.buf.find(bytes([SOF0, SOF1]))
            if start < 0:
                self.buf.clear()
                break
            if start > 0:
                del self.buf[:start]
            if len(self.buf) < 5:
                break
            payload_len = self.buf[3] | (self.buf[4] << 8)
            frame_len = 6 + payload_len
            if len(self.buf) < frame_len:
                break
            frame = bytes(self.buf[:frame_len])
            del self.buf[:frame_len]
            calc = checksum(frame[2], frame[5:-1])
            if calc != frame[-1]:
                continue
            frames.append((frame[2], frame[5:-1]))
        return frames


class SerialPort:
    def __init__(self, path: str, baud: int = 115200):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        if baud != 115200:
            raise ValueError("only 115200 baud is supported")
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.reader = FrameReader()

    def close(self):
        os.close(self.fd)

    def write(self, data: bytes):
        offset = 0
        while offset < len(data):
            try:
                written = os.write(self.fd, data[offset:])
            except BlockingIOError:
                time.sleep(0.01)
                continue
            offset += written

    def read_frames(self, timeout: float):
        end = time.time() + timeout
        frames = []
        while time.time() < end:
            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                chunk = b""
            if chunk:
                frames.extend(self.reader.feed(chunk))
                if frames:
                    return frames
            time.sleep(0.01)
        return frames

    def transact(self, frame_type: int, payload: bytes, expect_type: int, timeout: float):
        self.write(encode_frame(frame_type, payload))
        end = time.time() + timeout
        while time.time() < end:
            for rx_type, rx_payload in self.read_frames(0.1):
                if rx_type == expect_type:
                    return rx_payload
        raise TimeoutError(f"timed out waiting for frame type 0x{expect_type:02x}")


def decode_info(payload: bytes):
    if len(payload) < 16:
        raise ValueError("short info response")
    version_len = payload[15]
    if len(payload) < 16 + version_len:
        raise ValueError("truncated version")
    return {
        "protocol": payload[0],
        "state": payload[1],
        "last_status": payload[2],
        "bytes_received": int.from_bytes(payload[3:7], "little"),
        "slot_size": int.from_bytes(payload[7:11], "little"),
        "interval_ms": int.from_bytes(payload[11:15], "little"),
        "version": payload[16:16 + version_len].decode("utf-8", errors="replace"),
    }


def decode_status(payload: bytes):
    if len(payload) < 6:
        raise ValueError("short status response")
    return {
        "state": payload[0],
        "last_status": payload[1],
        "bytes_received": int.from_bytes(payload[2:6], "little"),
    }


def main():
    parser = argparse.ArgumentParser(description="Upload a signed image over HelixDrift UART OTA.")
    parser.add_argument("port")
    parser.add_argument("image_bin")
    parser.add_argument("--chunk-size", type=int, default=128)
    parser.add_argument("--expect-before")
    parser.add_argument("--expect-after")
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    with open(args.image_bin, "rb") as fh:
        image = fh.read()

    port = SerialPort(args.port)
    try:
        info = decode_info(port.transact(FRAME_INFO_REQ, b"", FRAME_INFO_RSP, args.timeout))
        print(f"before: version={info['version']} state={info['state']} slot=0x{info['slot_size']:x}")
        if args.expect_before and info["version"] != args.expect_before:
            raise SystemExit(f"unexpected starting version: {info['version']}")

        crc = crc32_ieee(image)
        begin_payload = bytes([CMD_BEGIN]) + len(image).to_bytes(4, "little") + crc.to_bytes(4, "little")
        ctrl = port.transact(FRAME_CTRL_WRITE, begin_payload, FRAME_CTRL_RSP, args.timeout)
        if not ctrl or ctrl[0] != 0:
            raise SystemExit(f"begin failed: {ctrl[0] if ctrl else 'missing'}")

        offset = 0
        while offset < len(image):
            chunk = image[offset:offset + args.chunk_size]
            payload = offset.to_bytes(4, "little") + chunk
            rsp = port.transact(FRAME_DATA_WRITE, payload, FRAME_DATA_RSP, args.timeout)
            if len(rsp) < 5 or rsp[0] != 0:
                raise SystemExit(f"data write failed at {offset}: {rsp[0] if rsp else 'missing'}")
            rx = int.from_bytes(rsp[1:5], "little")
            if rx != offset + len(chunk):
                raise SystemExit(f"device bytes_received mismatch: got {rx}, expected {offset + len(chunk)}")
            offset += len(chunk)

        commit = port.transact(FRAME_CTRL_WRITE, bytes([CMD_COMMIT]), FRAME_CTRL_RSP, args.timeout)
        if not commit or commit[0] != 0:
            raise SystemExit(f"commit failed: {commit[0] if commit else 'missing'}")
        print("commit: OK, waiting for reboot")

        # The target resets itself after commit, so reopen after a short settle period.
        port.close()
        time.sleep(1.0)
        port = SerialPort(args.port)

        deadline = time.time() + 15.0
        while time.time() < deadline:
            try:
                info = decode_info(port.transact(FRAME_INFO_REQ, b"", FRAME_INFO_RSP, 1.0))
            except TimeoutError:
                continue
            print(f"after: version={info['version']} state={info['state']} slot=0x{info['slot_size']:x}")
            if args.expect_after:
                if info["version"] == args.expect_after:
                    return 0
            else:
                return 0

        raise SystemExit("timed out waiting for upgraded image after reset")
    finally:
        port.close()


if __name__ == "__main__":
    raise SystemExit(main())
