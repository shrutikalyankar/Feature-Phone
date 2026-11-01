# Feature Phone Firmware (Team EM4)

Firmware for the low-power feature phone — ECEN 5833 Low Power Embedded Design Techniques, CU Boulder, Fall 2026.
Team EM4: Shruti Kalyankar, Pranav Shastry, Hafsa Shaukat.

## Target hardware
| Block | Part |
|---|---|
| MCU / BLE | EFR32BG24B110F1536IM48-B (QFN48) |
| Cellular | SIM7600NA-H |
| Power | BQ25570 (solar), BQ24074 (USB charger), 2x TPS63802, TPS61169 |
| Display | BT815 |
| Keypad / LEDs | TCA8418, LP5030 |
| Audio | NAU8810 |

## Layout
- `src/app/` application logic (state machine, UI, call/SMS flow)
- `src/drivers/` peripheral drivers (display, keypad, codec, modem, LED)
- `src/power/` power management: battery sense, charge status, sleep control
- `include/` public headers
- `docs/` design notes and pin map (`docs/pinmap.md`)
- `hardware/` schematic / PCB references (exports only, no Altium sources)
- `tools/` helper scripts

## Toolchain
Silicon Labs Simplicity Studio 5 with the Simplicity SDK (EFR32xG24). Generated project files are git-ignored; commit the `.slcp` project description and sources only.
