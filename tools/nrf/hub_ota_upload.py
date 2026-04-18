#!/usr/bin/env python3
"""Upload a signed firmware image to a Tag through the Hub's BLE relay.

The Hub receives UartOtaProtocol binary frames over USB CDC and forwards
them to the target Tag's BLE GATT OTA service.

Usage:
    python3 hub_ota_upload.py <image.signed.bin> --port /dev/ttyACM1 --target HTag-0D16
"""
import argparse
import struct
import sys
import time
import zlib

import serial


# UartOtaProtocol constants
SOF = bytes([0x48, 0x44])  # 'HD'
HEADER_SIZE = 5
FOOTER_SIZE = 1

# Frame types
INFO_REQ = 0x10
INFO_RSP = 0x11
CTRL_WRITE = 0x20
CTRL_RSP = 0x21
DATA_WRITE = 0x30
DATA_RSP = 0x31
STATUS_REQ = 0x40
STATUS_RSP = 0x41
ESB_TRIG_REQ = 0x50
ESB_TRIG_RSP = 0x51

# OTA commands
CMD_BEGIN = 0x01
CMD_ABORT = 0x02
CMD_COMMIT = 0x03


def checksum(frame_type, payload):
    s = frame_type
    s += len(payload) & 0xFF
    s += (len(payload) >> 8) & 0xFF
    for b in payload:
        s += b
    return s & 0xFF


def encode_frame(frame_type, payload=b""):
    length = len(payload)
    header = SOF + bytes([frame_type, length & 0xFF, (length >> 8) & 0xFF])
    cs = checksum(frame_type, payload)
    return header + payload + bytes([cs])


def decode_frame(data):
    if len(data) < HEADER_SIZE + FOOTER_SIZE:
        return None, None
    if data[0:2] != SOF:
        return None, None
    ftype = data[2]
    plen = data[3] | (data[4] << 8)
    if len(data) < HEADER_SIZE + plen + FOOTER_SIZE:
        return None, None
    payload = data[HEADER_SIZE:HEADER_SIZE + plen]
    expected = checksum(ftype, payload)
    if data[HEADER_SIZE + plen] != expected:
        return None, None
    return ftype, payload


def read_response(ser, expected_type, timeout=30.0):
    """Read bytes until a valid UartOtaProtocol response frame is found."""
    buf = bytearray()
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            # Try to find a frame in the buffer
            while len(buf) >= HEADER_SIZE + FOOTER_SIZE:
                # Find SOF
                idx = buf.find(SOF)
                if idx < 0:
                    buf.clear()
                    break
                if idx > 0:
                    buf = buf[idx:]
                if len(buf) < HEADER_SIZE:
                    break
                plen = buf[3] | (buf[4] << 8)
                frame_len = HEADER_SIZE + plen + FOOTER_SIZE
                if len(buf) < frame_len:
                    break
                ftype, payload = decode_frame(bytes(buf[:frame_len]))
                buf = buf[frame_len:]
                if ftype == expected_type:
                    return payload
                # Skip non-matching frames (FRAME/SUMMARY text lines might be mixed in)
    return None


def parse_args():
    p = argparse.ArgumentParser(description="Upload firmware to a Tag through the Hub's BLE relay.")
    p.add_argument("image_bin", help="Path to the signed .bin firmware image")
    p.add_argument("--port", default="/dev/ttyACM1", help="Hub USB CDC serial port")
    p.add_argument("--target", required=True, help="Target Tag BLE name (e.g. HTag-0D16)")
    p.add_argument("--target-id", type=lambda v: int(v, 0), default=0x52840070,
                   help="OTA target identity hex (must match Kconfig HELIX_OTA_TARGET_ID)")
    p.add_argument("--trigger-node", type=lambda v: int(v, 0), default=0,
                   help="ESB node_id (1-255) to trigger remote OTA reboot before upload. "
                        "0 = no trigger (Tag must already be in BLE OTA window). "
                        "0xFF = broadcast to all nodes.")
    p.add_argument("--chunk-size", type=int, default=128, help="Data chunk size in bytes")
    p.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    return p.parse_args()


def main():
    args = parse_args()
    image = open(args.image_bin, "rb").read()
    crc = zlib.crc32(image) & 0xFFFFFFFF

    print(f"Image: {len(image)} bytes, CRC: 0x{crc:08x}")
    print(f"Target: {args.target} (ID: 0x{args.target_id:08x})")
    print(f"Port: {args.port}")

    ser = serial.Serial(args.port, args.baud, timeout=0.1,
                         dsrdtr=True, rtscts=False, write_timeout=5)
    ser.reset_input_buffer()

    # Step 0: Optionally trigger remote OTA reboot via ESB
    if args.trigger_node != 0:
        print(f"\n--- ESB trigger: node={args.trigger_node} (reboot to BLE OTA) ---")
        ser.write(encode_frame(ESB_TRIG_REQ, bytes([args.trigger_node & 0xFF, 3])))
        rsp = read_response(ser, ESB_TRIG_RSP, timeout=5.0)
        if rsp is None or rsp[0] != 0x00:
            print(f"WARNING: ESB trigger response: {rsp}")
        else:
            print("Trigger queued, waiting 4s for Tag reboot + BLE advertise...")
            time.sleep(4)

    # Step 1: Send InfoReq with target name
    print(f"\n--- Connecting to {args.target} through Hub ---")
    info_payload = args.target.encode("ascii")
    ser.write(encode_frame(INFO_REQ, info_payload))
    rsp = read_response(ser, INFO_RSP, timeout=15.0)
    if rsp is None or rsp[0] != 0x00:
        print(f"ERROR: InfoReq failed: {rsp}")
        return 1

    # Wait for Hub to scan, connect, and discover GATT
    print("Hub scanning and connecting...")
    time.sleep(3)

    # Step 2: Read status to verify connection
    ser.write(encode_frame(STATUS_REQ))
    rsp = read_response(ser, STATUS_RSP, timeout=10.0)
    if rsp is None or len(rsp) < 2:
        print(f"ERROR: StatusReq failed (Hub may not have connected to Tag)")
        return 1
    print(f"Tag status: {rsp.hex()}")

    # Step 3: Send BEGIN
    begin_payload = bytes([CMD_BEGIN]) + \
        struct.pack("<I", len(image)) + \
        struct.pack("<I", crc) + \
        struct.pack("<I", args.target_id)
    print(f"\n--- BEGIN: size={len(image)} crc=0x{crc:08x} ---")
    ser.write(encode_frame(CTRL_WRITE, begin_payload))
    rsp = read_response(ser, CTRL_RSP, timeout=60.0)
    if rsp is None or rsp[0] != 0x00:
        print(f"ERROR: BEGIN failed: {rsp}")
        return 1
    print("BEGIN ACK received, waiting for flash erase...")

    # Tag defers flash erase out of GATT callback — poll STATUS until
    # state transitions from IDLE (0) to RECEIVING (1). Erase takes ~10s.
    erase_deadline = time.time() + 20.0
    while time.time() < erase_deadline:
        time.sleep(0.5)
        ser.write(encode_frame(STATUS_REQ))
        rsp = read_response(ser, STATUS_RSP, timeout=3.0)
        if rsp is not None and len(rsp) >= 1 and rsp[0] == 1:  # OtaState::RECEIVING
            break
    else:
        print("ERROR: Tag did not transition to RECEIVING after 20s")
        return 1
    print("BEGIN OK (erase complete, Tag ready to receive)")

    # Step 4: Send data chunks
    offset = 0
    last_progress = 0
    print(f"\n--- Transferring {len(image)} bytes ---")
    while offset < len(image):
        end = min(offset + args.chunk_size, len(image))
        chunk = image[offset:end]
        data_payload = struct.pack("<I", offset) + chunk
        ser.write(encode_frame(DATA_WRITE, data_payload))
        offset = end

        if offset - last_progress >= 4096:
            pct = offset * 100 // len(image)
            print(f"  progress: {offset}/{len(image)} ({pct}%)")
            last_progress = offset

        # Small delay to avoid overwhelming the Hub's BLE write-no-rsp queue
        time.sleep(0.002)

    print(f"  progress: {offset}/{len(image)} (100%)")

    # Step 5: Send COMMIT
    print(f"\n--- COMMIT ---")
    ser.write(encode_frame(CTRL_WRITE, bytes([CMD_COMMIT])))
    rsp = read_response(ser, CTRL_RSP, timeout=15.0)
    if rsp is not None and rsp[0] == 0x00:
        print("COMMIT OK — Tag will reboot through MCUboot")
    elif rsp is None:
        # Timeout is expected: Tag reboots before relay can send response
        print("COMMIT response timeout (expected — Tag rebooting)")
    else:
        print(f"ERROR: COMMIT failed: {rsp}")
        return 1

    # Step 6: Wait for Hub to restart ESB
    print("\nWaiting for Hub ESB to restart...")
    time.sleep(5)
    ser.reset_input_buffer()
    deadline = time.time() + 30
    while time.time() < deadline:
        line = ser.readline()
        if line and b"HELIX_MOCAP_BRIDGE_READY" in line:
            print("Hub ESB restarted — OTA complete!")
            return 0
        if line and b"FRAME" in line:
            print("Hub ESB restarted (FRAME data flowing) — OTA complete!")
            return 0
    print("WARNING: Hub didn't confirm ESB restart, but OTA may have succeeded")
    return 0


if __name__ == "__main__":
    sys.exit(main())
