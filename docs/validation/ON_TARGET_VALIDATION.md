# On-Target Validation Checklist (XIAO nRF52840)

Use this after cross-build passes and hardware is wired per `README.md`.

## Test Environment

- Board: Seeed XIAO nRF52840
- Sensors: LSM6DSO, BMM350, LPS22DF (individual dev boards)
- Power: USB and intended LiPo setup
- Firmware: `nrf52_mocap_node` from current `main`
- Build commit: `<fill>`
- Test date: `<fill>`

## Pre-Flash

- [ ] `./build.py -t` passes locally.
- [ ] `nrf52_mocap_node.hex` generated.
- [ ] SWD probe or UF2 path available.

## Flash + Boot

- [ ] Flash succeeds (`nrfjprog --program ... --verify --reset`).
- [ ] Device boots without reset loop.
- [ ] No hard fault on startup.

## Sensor Bus Bring-Up

- [ ] TWIM0 responds for LSM6DSO.
- [ ] TWIM1 responds for BMM350.
- [ ] TWIM1 responds for LPS22DF.
- [ ] `imu.init()`, `mag.init()`, `baro.init()` all succeed.

## Runtime Behavior

- [ ] Quaternion stream active at expected cadence (~50 Hz in performance mode).
- [ ] Health telemetry frame active (~1 Hz when hook is implemented).
- [ ] No sustained packet drops under normal motion.
- [ ] Timestamp sync anchor updates do not break stream continuity.

## Calibration + Control

- [ ] Command `1` captures stationary calibration.
- [ ] Command `2` captures T-pose calibration (after stationary).
- [ ] Command `3` resets calibration.
- [ ] Calibration state appears correctly in health telemetry.

## Motion Quality Sanity

- [ ] Static orientation is stable (low jitter at rest).
- [ ] Smooth motion tracks continuously (no major jumps).
- [ ] Fast motions recover quickly (no prolonged drift spikes).

## Power + Thermal

- [ ] USB current in expected range for selected profile.
- [ ] Battery runtime smoke test passes target duration window.
- [ ] Device/sensors remain within safe touch temperature.

## Exit Criteria

- [ ] All critical checks pass.
- [ ] Failures have issue links + reproduction notes.
- [ ] Results copied into `docs/validation/TEST_LOG_TEMPLATE.md`.
