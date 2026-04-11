# nRF52840 ProPico / SuperMini Board Notes

This folder collects the board evidence used for bring-up of the AliExpress
`nRF52840` ProPico / SuperMini / nice!nano-compatible board.

## What This Board Appears To Be

- `nRF52840` clone board in the Pro Micro / SuperMini / nice!nano family
- UF2-capable over USB on some units
- SWD pads broken out on the back

## Key Pin Assumptions

- `LED0 = P0.15`
  - Source: Zephyr `promicro_nrf52840` board docs and DTS
- `P0.13` is not the user LED
  - It is called out as the external VCC / power-switch control on clone-family
    docs

## Local Evidence Saved Here

- `sources/icbbuy_nrf52840.html`
- `sources/zephyr_promicro_nrf52840.html`
- `sources/joric_nrfmicro_alternatives.html`
- `local_refs/board.yml`
- `local_refs/promicro_nrf52840_nrf52840.dts`
- `supermini-nrf52840-kicad/`
- `ProMicro_NRF52840 V1.2-PCB FootPrint.PcbDoc`
- `qs-nrf52840_v1.2-pcb_footprint.zip`
- two pasted screenshots of the board schematic

## Practical Bring-Up Decision

For first hardware validation in this repo, treat this board as:

- CPU: `nRF52840`
- first candidate LED pin: `P0.15`
- LED polarity: active high

That is enough for a first board-specific blink test without pretending we have
full production-grade board support yet.

## Source Highlights

- Zephyr board doc:
  - states this family is "Nice!Nano / Pro Micro / SuperMini nRF52840"
  - states `LED0 = P0.15`
- Zephyr DTS:
  - `gpios = <&gpio0 15 GPIO_ACTIVE_HIGH>;`
- ICBbuy wiki:
  - confirms clone-family board identity and UF2-style bring-up expectations

## Known Gaps

- This does not yet prove every pin in the pasted schematic.
- It does not yet prove whether every unit ships with the same bootloader.
- It does not yet establish a dedicated repo board target beyond blink bring-up.
