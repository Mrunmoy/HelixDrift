#!/usr/bin/env python3
import argparse
import asyncio
import sys
import zlib

from bleak import BleakClient, BleakScanner


OTA_SERVICE_UUID = "3ef6a001-2d3b-4f2a-89e4-7b59d1c0a001"
OTA_CTRL_UUID = "3ef6a002-2d3b-4f2a-89e4-7b59d1c0a001"
OTA_DATA_UUID = "3ef6a003-2d3b-4f2a-89e4-7b59d1c0a001"
OTA_STATUS_UUID = "3ef6a004-2d3b-4f2a-89e4-7b59d1c0a001"

CMD_BEGIN = 0x01
CMD_ABORT = 0x02
CMD_COMMIT = 0x03


def parse_args():
    p = argparse.ArgumentParser(description="Upload a signed image over Helix BLE OTA.")
    p.add_argument("image_bin")
    p.add_argument("--name", default="HelixOTA-v1")
    p.add_argument("--expect-after", default="HelixOTA-v2")
    p.add_argument("--expect-same-name", action="store_true")
    p.add_argument("--no-wait-after", action="store_true")
    p.add_argument("--crc-adjust", type=int, default=0)
    p.add_argument("--abort-after-bytes", type=int)
    p.add_argument("--chunk-size", type=int, default=16)
    p.add_argument("--poll-every-chunks", type=int, default=1)
    p.add_argument("--inter-chunk-delay-ms", type=float, default=10.0)
    p.add_argument("--scan-timeout", type=float, default=10.0)
    return p.parse_args()


async def find_device(name: str, timeout: float):
    return await BleakScanner.find_device_by_filter(
        lambda d, adv: d.name == name or OTA_SERVICE_UUID in [u.lower() for u in (adv.service_uuids or [])],
        timeout=timeout,
    )


async def read_status(client: BleakClient):
    raw = bytes(await client.read_gatt_char(OTA_STATUS_UUID))
    if len(raw) < 6:
        raise RuntimeError("short status response")
    return {
        "state": raw[0],
        "bytes_received": int.from_bytes(raw[1:5], "little"),
        "last_status": raw[5],
    }


async def main_async():
    args = parse_args()
    image = open(args.image_bin, "rb").read()
    crc = (zlib.crc32(image) + args.crc_adjust) & 0xFFFFFFFF

    device = await find_device(args.name, args.scan_timeout)
    if not device:
        raise RuntimeError(f"target not found: {args.name}")

    print(f"before: {device.address} {device.name}")
    async with BleakClient(device) as client:
        status = await read_status(client)
        print(f"status-before: state={status['state']} bytes={status['bytes_received']} last={status['last_status']}")

        begin = bytes([CMD_BEGIN]) + len(image).to_bytes(4, "little") + crc.to_bytes(4, "little")
        await client.write_gatt_char(OTA_CTRL_UUID, begin, response=True)
        status = await read_status(client)
        if status["last_status"] != 0:
            raise RuntimeError(f"begin failed: {status}")

        offset = 0
        chunks_since_poll = 0
        while offset < len(image):
            chunk = image[offset:offset + args.chunk_size]
            payload = offset.to_bytes(4, "little") + chunk
            await client.write_gatt_char(OTA_DATA_UUID, payload, response=False)
            expected = offset + len(chunk)
            chunks_since_poll += 1
            if args.inter_chunk_delay_ms > 0:
                await asyncio.sleep(args.inter_chunk_delay_ms / 1000.0)
            should_poll = args.poll_every_chunks > 0 and chunks_since_poll >= args.poll_every_chunks
            if should_poll or expected == len(image) or (args.abort_after_bytes is not None and expected >= args.abort_after_bytes):
                status = await read_status(client)
                if status["last_status"] != 0 or status["bytes_received"] != expected:
                    raise RuntimeError(f"data write failed at {offset}: {status}, expected bytes={expected}")
                chunks_since_poll = 0
            offset = expected
            if args.abort_after_bytes is not None and offset >= args.abort_after_bytes:
                await client.write_gatt_char(OTA_CTRL_UUID, bytes([CMD_ABORT]), response=True)
                status = await read_status(client)
                if status["last_status"] != 0:
                    raise RuntimeError(f"abort failed: {status}")
                print(f"abort: OK after {offset} bytes")
                break

        if args.abort_after_bytes is None:
            await client.write_gatt_char(OTA_CTRL_UUID, bytes([CMD_COMMIT]), response=True)
            status = await read_status(client)
            if status["last_status"] != 0:
                raise RuntimeError(f"commit failed: {status}")
            print("commit: OK, waiting for reboot")
        else:
            if args.no_wait_after:
                return 0

    if args.no_wait_after:
        return 0

    expected_name = args.name if args.expect_same_name else args.expect_after
    for _ in range(20):
        await asyncio.sleep(1.0)
        device = await find_device(expected_name, 2.0)
        if device:
            print(f"after: {device.address} {device.name}")
            return 0
    raise RuntimeError(f"timed out waiting for {expected_name}")


def main():
    try:
        return asyncio.run(main_async())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
