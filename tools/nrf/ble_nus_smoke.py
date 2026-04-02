#!/usr/bin/env python3
import argparse
import asyncio
import sys
import threading
import time

import serial
from bleak import BleakClient, BleakScanner


NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Connect to a Nordic UART Service peripheral and verify a payload reaches the board UART."
    )
    parser.add_argument("--name", default="Nordic_UART_Service", help="Expected BLE device name.")
    parser.add_argument("--serial-port", default="/dev/ttyACM0", help="Board serial port to observe.")
    parser.add_argument("--baud", type=int, default=115200, help="Board serial baud rate.")
    parser.add_argument("--message", default="HELIX_BLE_SMOKE", help="Payload text to send.")
    parser.add_argument("--scan-timeout", type=float, default=10.0, help="BLE scan timeout in seconds.")
    parser.add_argument("--observe-timeout", type=float, default=8.0, help="UART observation window in seconds.")
    return parser.parse_args()


def read_serial_window(port: str, baud: int, timeout_s: float, sink: list[bytes], stop_flag: dict[str, bool]) -> None:
    ser = serial.Serial(port, baud, timeout=0.2)
    ser.setDTR(True)
    start = time.time()
    try:
        while time.time() - start < timeout_s and not stop_flag["stop"]:
            chunk = ser.read(1024)
            if chunk:
                sink.append(chunk)
    finally:
        ser.close()


async def find_device(name: str, scan_timeout: float):
    return await BleakScanner.find_device_by_filter(
        lambda d, adv: d.name == name
        or NUS_SERVICE_UUID in [u.lower() for u in (adv.service_uuids or [])],
        timeout=scan_timeout,
    )


async def send_message(name: str, scan_timeout: float, payload: bytes) -> str:
    device = await find_device(name, scan_timeout)
    if not device:
        raise RuntimeError(f"BLE target not found: {name}")

    async with BleakClient(device) as client:
        await client.write_gatt_char(NUS_RX_CHAR_UUID, payload, response=False)

    return f"{device.address} {device.name}"


def main() -> int:
    args = parse_args()
    payload_text = args.message
    payload = (payload_text + "\n").encode("utf-8")

    serial_buf: list[bytes] = []
    stop_flag = {"stop": False}
    reader = threading.Thread(
        target=read_serial_window,
        args=(args.serial_port, args.baud, args.observe_timeout, serial_buf, stop_flag),
        daemon=True,
    )
    reader.start()

    try:
        target = asyncio.run(send_message(args.name, args.scan_timeout, payload))
    finally:
        time.sleep(1.0)
        stop_flag["stop"] = True
        reader.join()

    output = b"".join(serial_buf).decode("utf-8", "replace")

    print(f"BLE target: {target}")
    print("SERIAL OUTPUT START")
    print(output)
    print("SERIAL OUTPUT END")

    if payload_text not in output:
        print(f"expected payload not observed on {args.serial_port}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
