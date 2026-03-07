# On-Target Test Log Template

## Session Metadata

- Date:
- Tester:
- Firmware commit:
- Board revision:
- Sensor board revisions:
- Power source:

## Results Table

| ID | Test | Result (`PASS`/`FAIL`) | Evidence | Notes |
|---|---|---|---|---|
| OT-001 | Flash + boot |  |  |  |
| OT-002 | LSM6DSO init on TWIM0 |  |  |  |
| OT-003 | BMM350 init on TWIM1 |  |  |  |
| OT-004 | LPS22DF init on TWIM1 |  |  |  |
| OT-005 | Quaternion stream cadence |  |  |  |
| OT-006 | Health telemetry cadence |  |  |  |
| OT-007 | Calibration command 1 |  |  |  |
| OT-008 | Calibration command 2 |  |  |  |
| OT-009 | Calibration reset command 3 |  |  |  |
| OT-010 | Timestamp sync stability |  |  |  |
| OT-011 | Static jitter sanity |  |  |  |
| OT-012 | Dynamic motion continuity |  |  |  |
| OT-013 | Runtime/power smoke |  |  |  |

## Defects / Follow-ups

- Issue:
  - Repro steps:
  - Expected:
  - Actual:
  - Logs/attachments:
