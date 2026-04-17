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
    p.add_argument("--name", default="HTag",
                   help="Device name or prefix to match")
    p.add_argument("--name-prefix", action="store_true",
                   help="Match any device whose name starts with --name")
    p.add_argument("--expect-after", default="HTag",
                   help="Name (or prefix) to expect after reboot")
    p.add_argument("--expect-same-name", action="store_true")
    p.add_argument("--target-id", type=lambda v: int(v, 0))
    p.add_argument("--no-wait-after", action="store_true")
    p.add_argument("--crc-adjust", type=int, default=0)
    p.add_argument("--abort-after-bytes", type=int)
    p.add_argument("--chunk-size", type=int, default=128)
    p.add_argument("--write-with-response", action="store_true")
    p.add_argument("--poll-every-chunks", type=int, default=8)
    p.add_argument("--inter-chunk-delay-ms", type=float, default=1.0)
    p.add_argument("--scan-timeout", type=float, default=10.0)
    p.add_argument("--progress-every-bytes", type=int, default=4096)
    p.add_argument("--resume-retries", type=int, default=5)
    p.add_argument("--tail-safe-bytes", type=int, default=4096)
    p.add_argument("--tail-chunk-size", type=int, default=16)
    p.add_argument("--tail-write-with-response", action="store_true")
    p.add_argument("--tail-poll-every-chunks", type=int, default=1)
    p.add_argument("--tail-inter-chunk-delay-ms", type=float, default=5.0)
    p.add_argument("--page-size", type=int, default=4096)
    p.add_argument("--page-cross-delay-ms", type=float, default=20.0)
    p.add_argument("--gatt-timeout", type=float, default=10.0)
    p.add_argument("--begin-timeout", type=float, default=45.0)
    p.add_argument("--commit-timeout", type=float, default=15.0)
    return p.parse_args()


async def find_device(name: str, timeout: float, prefix: bool = False):
    devices = await BleakScanner.discover(timeout=timeout, return_adv=True)
    for _addr, (dev, adv) in devices.items():
        local_name = adv.local_name or ""
        dev_name = dev.name or ""
        uuids = [u.lower() for u in (adv.service_uuids or [])]
        if prefix:
            if dev_name.startswith(name) or local_name.startswith(name):
                return dev
        elif dev_name == name or local_name == name:
            return dev
    return None


async def read_status(client: BleakClient):
    raw = bytes(await asyncio.wait_for(client.read_gatt_char(OTA_STATUS_UUID), timeout=10.0))
    if len(raw) < 10:
        raise RuntimeError("short status response")
    return {
        "state": raw[0],
        "bytes_received": int.from_bytes(raw[1:5], "little"),
        "last_status": raw[5],
        "target_id": int.from_bytes(raw[6:10], "little"),
    }


async def read_status_with_timeout(client: BleakClient, timeout: float):
    raw = bytes(await asyncio.wait_for(client.read_gatt_char(OTA_STATUS_UUID), timeout=timeout))
    if len(raw) < 10:
        raise RuntimeError("short status response")
    return {
        "state": raw[0],
        "bytes_received": int.from_bytes(raw[1:5], "little"),
        "last_status": raw[5],
        "target_id": int.from_bytes(raw[6:10], "little"),
    }


async def write_gatt_with_timeout(client: BleakClient, uuid: str, payload: bytes, response: bool, timeout: float):
    await asyncio.wait_for(client.write_gatt_char(uuid, payload, response=response), timeout=timeout)


async def main_async():
    args = parse_args()
    image = open(args.image_bin, "rb").read()
    crc = (zlib.crc32(image) + args.crc_adjust) & 0xFFFFFFFF

    offset = 0
    next_progress = args.progress_every_bytes if args.progress_every_bytes > 0 else None
    announced_before = False
    retries_left = args.resume_retries
    while True:
        device = await find_device(args.name, args.scan_timeout, prefix=args.name_prefix)
        if not device:
            raise RuntimeError(f"target not found: {args.name}")

        if not announced_before:
            print(f"before: {device.address} {device.name}")
            announced_before = True

        try:
            async with BleakClient(device) as client:
                status = await read_status_with_timeout(client, args.gatt_timeout)
                print(f"status-before: state={status['state']} bytes={status['bytes_received']} last={status['last_status']} target=0x{status['target_id']:08x}")
                if args.target_id is not None and status["target_id"] != args.target_id:
                    raise RuntimeError(f"wrong target id: device=0x{status['target_id']:08x} expected=0x{args.target_id:08x}")

                target_id = args.target_id if args.target_id is not None else status["target_id"]
                # A zero-byte RECEIVING state is stale: no useful data has landed, and a fresh
                # BEGIN would otherwise be rejected as invalid while the target still thinks
                # an old session is in progress.
                if status["state"] == 1 and status["bytes_received"] == 0:
                    print("stale-receiving: aborting zero-byte session")
                    await write_gatt_with_timeout(client, OTA_CTRL_UUID, bytes([CMD_ABORT]), response=True, timeout=args.gatt_timeout)
                    status = await read_status_with_timeout(client, args.gatt_timeout)

                if status["state"] != 1 or status["bytes_received"] == 0:
                    offset = 0
                    begin = bytes([CMD_BEGIN]) + len(image).to_bytes(4, "little") + crc.to_bytes(4, "little") + target_id.to_bytes(4, "little")
                    print(f"begin: size={len(image)} crc=0x{crc:08x}")
                    await write_gatt_with_timeout(client, OTA_CTRL_UUID, begin, response=True, timeout=args.begin_timeout)
                    status = await read_status_with_timeout(client, args.begin_timeout)
                    print(f"status-after-begin: state={status['state']} bytes={status['bytes_received']} last={status['last_status']}")
                    if status["last_status"] != 0:
                        raise RuntimeError(f"begin failed: {status}")
                else:
                    offset = status["bytes_received"]
                    print(f"resume-at: {offset}")

                chunks_since_poll = 0
                while offset < len(image):
                    remaining = len(image) - offset
                    use_tail_mode = args.tail_safe_bytes > 0 and remaining <= args.tail_safe_bytes
                    chunk_size = args.tail_chunk_size if use_tail_mode else args.chunk_size
                    write_with_response = args.tail_write_with_response if use_tail_mode else args.write_with_response
                    poll_every_chunks = args.tail_poll_every_chunks if use_tail_mode else args.poll_every_chunks
                    inter_chunk_delay_ms = args.tail_inter_chunk_delay_ms if use_tail_mode else args.inter_chunk_delay_ms
                    chunk = image[offset:offset + chunk_size]
                    payload = offset.to_bytes(4, "little") + chunk
                    try:
                        await write_gatt_with_timeout(
                            client,
                            OTA_DATA_UUID,
                            payload,
                            response=write_with_response,
                            timeout=args.gatt_timeout,
                        )
                    except Exception as exc:
                        detail = f"write failed at offset={offset} size={len(chunk)} remaining={remaining}"
                        try:
                            status = await read_status_with_timeout(client, min(args.gatt_timeout, 3.0))
                            detail += (
                                f" status=state={status['state']} bytes={status['bytes_received']}"
                                f" last={status['last_status']} target=0x{status['target_id']:08x}"
                            )
                        except Exception as status_exc:
                            detail += f" status-read-failed={status_exc}"
                        raise RuntimeError(f"{detail}: {exc}") from exc
                    expected = offset + len(chunk)
                    chunks_since_poll += 1
                    crossed_page = args.page_size > 0 and (offset // args.page_size) != ((expected - 1) // args.page_size)
                    if crossed_page and args.page_cross_delay_ms > 0:
                        await asyncio.sleep(args.page_cross_delay_ms / 1000.0)
                    if inter_chunk_delay_ms > 0:
                        await asyncio.sleep(inter_chunk_delay_ms / 1000.0)
                    should_poll = poll_every_chunks > 0 and chunks_since_poll >= poll_every_chunks
                    if should_poll or expected == len(image) or (args.abort_after_bytes is not None and expected >= args.abort_after_bytes):
                        status = await read_status_with_timeout(client, args.gatt_timeout)
                        if status["last_status"] != 0 or status["bytes_received"] != expected:
                            raise RuntimeError(f"data write failed at {offset}: {status}, expected bytes={expected}")
                        if next_progress is not None:
                            while expected >= next_progress:
                                print(f"progress: {expected}/{len(image)}")
                                next_progress += args.progress_every_bytes
                        chunks_since_poll = 0
                    offset = expected
                    if args.abort_after_bytes is not None and offset >= args.abort_after_bytes:
                        await write_gatt_with_timeout(client, OTA_CTRL_UUID, bytes([CMD_ABORT]), response=True, timeout=args.gatt_timeout)
                        status = await read_status_with_timeout(client, args.gatt_timeout)
                        if status["last_status"] != 0:
                            raise RuntimeError(f"abort failed: {status}")
                        print(f"abort: OK after {offset} bytes")
                        break

                if args.abort_after_bytes is None:
                    print("commit: sending")
                    await write_gatt_with_timeout(client, OTA_CTRL_UUID, bytes([CMD_COMMIT]), response=True, timeout=args.commit_timeout)
                    status = await read_status_with_timeout(client, args.commit_timeout)
                    if status["last_status"] != 0:
                        raise RuntimeError(f"commit failed: {status}")
                    print("commit: OK, waiting for reboot")
                else:
                    if args.no_wait_after:
                        return 0
            break
        except Exception as exc:
            if retries_left <= 0:
                raise
            retries_left -= 1
            print(f"resume: {exc} (retries left {retries_left})")
            await asyncio.sleep(2.0)

    if args.no_wait_after:
        return 0

    expected_name = args.name if args.expect_same_name else args.expect_after
    for _ in range(20):
        await asyncio.sleep(1.0)
        device = await find_device(expected_name, 2.0, prefix=True)
        if device:
            print(f"after: {device.address} {device.name}")
            return 0
    raise RuntimeError(f"timed out waiting for {expected_name}")


def main():
    try:
        return asyncio.run(main_async())
    except Exception as exc:
        print(f"error: {exc!r}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
