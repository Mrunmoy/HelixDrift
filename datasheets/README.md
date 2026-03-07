# Datasheets

This directory tracks the primary hardware references used by HelixDrift.

## Required Devices

- MCU: `nRF52840`
- IMU: `LSM6DSO`
- Magnetometer: `BMM350`
- Barometer: `LPS22DF`

## Files

- `INDEX.md`: master list of parts, links, and expected local filenames.
- `fetch_datasheets.sh`: helper script to download PDFs into this directory.
- `sources.env`: URL definitions used by `fetch_datasheets.sh`.

## Usage

```bash
cd datasheets
./fetch_datasheets.sh
```

If a vendor link changes, update `sources.env` and re-run.
