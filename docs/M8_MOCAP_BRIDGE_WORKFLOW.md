# M8 Mocap Bridge Workflow

Current first end-to-end multi-node RF workflow:

- local ProPico: synthetic mocap `node=1`
- remote ProPico on `hpserver1`: synthetic mocap `node=2`
- local `nRF52840` dongle: central receiver with native USB CDC host output
- current preferred RF channel: `40`

## Repo-Native Build Helpers

Build central dongle:

```bash
tools/nrf/build_mocap_bridge.sh central nrf52840dongle/nrf52840/bare 1
```

Build local ProPico node:

```bash
tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 1
```

Build a native-USB-observable ProPico node for UF2 bring-up:

```bash
HELIX_MOCAP_EXTRA_CONF_FILE=node_usb_debug.conf \
tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 3
```

Build remote ProPico node:

```bash
ssh litu@hpserver1 "cd /home/litu/sandbox/embedded/HelixDrift && \
  ./tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 2"
```

## Expected Host Path

After flashing the central dongle firmware, the dongle should enumerate as:

- vendor: `HelixDrift`
- product: `Helix Mocap Central`

Typical local port:

- `/dev/ttyACM3`

When a UF2-flashed native-USB node runtime USB path is healthy, it should
enumerate separately from the dongle as:

- vendor: `HelixDrift`
- product: `Helix Mocap Node`

If a UF2-flashed node leaves bootloader mode but does not re-enumerate on USB,
use the board LED as a fallback runtime probe:

- fast blink: app alive, no confirmed TX success yet
- medium blink: TX succeeding, no sync anchors yet
- slow blink: sync anchors received from the central

## Two-Node Smoke

Run the full two-node hardware smoke:

```bash
tools/nrf/mocap_bridge_two_node_smoke.sh \
  litu@hpserver1 \
  /home/litu/sandbox/embedded/HelixDrift \
  123456 \
  NRF52840_XXAA \
  69656876 \
  /dev/ttyACM3 \
  10 \
  2 \
  1
```

Pass condition:

- both `node=1` and `node=2` appear on the dongle USB stream
- each node sustains roughly `50 Hz`
- the central summary reports `tracked=2`
- current best known two-node result on channel `40`:
  - `RATE node=1 hz=49.67 gap_per_1k=0.00`
  - `RATE node=2 hz=49.67 gap_per_1k=0.00`
  - `RATE combined_hz=99.33`
  - `SYNC_DELTA_US min=6000 median=10000 p90=12000 p99=13000 max=14000`

## Higher-Rate Node Builds

To test `100 Hz` per node, rebuild the node roles with a `10 ms` send period:

```bash
tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 1 \
  -DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=10

ssh litu@hpserver1 "cd /home/litu/sandbox/embedded/HelixDrift && \
  ./tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 2 \
  -DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=10"
```

For a full scripted characterize pass, use:

```bash
tools/nrf/mocap_bridge_characterize.sh \
  litu@hpserver1 \
  /home/litu/sandbox/embedded/HelixDrift \
  123456 \
  NRF52840_XXAA \
  69656876 \
  /dev/ttyACM3 \
  60 \
  2 \
  1 \
  10 \
  artifacts/rf/mocap_bridge_two_node_100hz_60s
```

For a multi-rate sweep on the same hardware lane, use:

```bash
tools/nrf/mocap_bridge_rate_sweep.sh \
  litu@hpserver1 \
  /home/litu/sandbox/embedded/HelixDrift \
  123456 \
  NRF52840_XXAA \
  69656876 \
  /dev/ttyACM3 \
  6 \
  5 \
  "20 10" \
  artifacts/rf/rate_sweep_smoke
```

Current sweep artifact:

- [mocap_bridge_rate_sweep.csv](/home/mrumoy/sandbox/embedded/HelixDrift/artifacts/rf/rate_sweep_smoke/mocap_bridge_rate_sweep.csv)

Current best known two-node `100 Hz` result on channel `40`:

- short proof window:
  - `RATE node=1 hz=98.50 gap_per_1k=0.00`
  - `RATE node=2 hz=98.50 gap_per_1k=0.00`
  - `RATE combined_hz=197.00`
  - `SYNC_DELTA_US min=0 median=6000 p90=9000 p99=10000 max=10000`
- first `20s` artifact:
  - `RATE node=1 hz=98.45 gap_per_1k=0.51`
  - `RATE node=2 hz=98.50 gap_per_1k=0.00`
  - `RATE combined_hz=196.95`
  - `SYNC_DELTA_US min=0 median=6000 p90=9000 p99=10000 max=12000`
  - outputs:
    - [mocap_bridge_two_node_100hz.csv](/home/mrumoy/sandbox/embedded/HelixDrift/artifacts/rf/mocap_bridge_two_node_100hz.csv)
    - [mocap_bridge_two_node_100hz.summary](/home/mrumoy/sandbox/embedded/HelixDrift/artifacts/rf/mocap_bridge_two_node_100hz.summary)
- stronger `60s` artifact:
  - `RATE node=1 hz=98.48 gap_per_1k=0.00`
  - `RATE node=2 hz=98.48 gap_per_1k=0.00`
  - `RATE combined_hz=196.97`
  - `SYNC_DELTA_US min=0 median=6000 p90=9000 p99=10000 max=10000`
  - outputs:
    - [mocap_bridge_two_node_100hz_60s.csv](/home/mrumoy/sandbox/embedded/HelixDrift/artifacts/rf/mocap_bridge_two_node_100hz_60s.csv)
    - [mocap_bridge_two_node_100hz_60s.summary](/home/mrumoy/sandbox/embedded/HelixDrift/artifacts/rf/mocap_bridge_two_node_100hz_60s.summary)

## Host Logger

For manual host inspection:

```bash
python3 tools/analysis/log_mocap_bridge.py /dev/ttyACM3 --summary-seconds 1.0
```
