# ESP32-S3 Mocap Node Example

This is the ESP-IDF project used by `./magic.sh` on branch `esp32s3-xiao`.

## Build

From repo root:

```bash
./build.py --esp32s3-only
```

## Flash

```bash
source third_party/esp-idf/export.sh
idf.py -C examples/esp32s3-mocap-node -B build/esp32s3 -p /dev/ttyACM0 flash monitor
```

Adjust serial port as needed.
