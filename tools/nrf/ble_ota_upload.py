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
    p.add_argument("--chunk-size", type=int, default=16)
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
    crc = zlib.crc32(image) & 0xFFFFFFFF

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
        while offset < len(image):
            chunk = image[offset:offset + args.chunk_size]
            payload = offset.to_bytes(4, "little") + chunk
            await client.write_gatt_char(OTA_DATA_UUID, payload, response=False)
            await asyncio.sleep(0.01)
            status = await read_status(client)
            expected = offset + len(chunk)
            if status["last_status"] != 0 or status["bytes_received"] != expected:
                raise RuntimeError(f"data write failed at {offset}: {status}, expected bytes={expected}")
            offset = expected

        await client.write_gatt_char(OTA_CTRL_UUID, bytes([CMD_COMMIT]), response=True)
        status = await read_status(client)
        if status["last_status"] != 0:
            raise RuntimeError(f"commit failed: {status}")
        print("commit: OK, waiting for reboot")

    for _ in range(20):
        await asyncio.sleep(1.0)
        device = await find_device(args.expect_after, 2.0)
        if device:
            print(f"after: {device.address} {device.name}")
            return 0
    raise RuntimeError(f"timed out waiting for {args.expect_after}")


def main():
    try:
        return asyncio.run(main_async())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
