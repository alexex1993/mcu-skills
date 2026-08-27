---
name: nrf52840-promicro
description: Firmware development for the ProMicro nRF52840 (V1940) — the nice!nano v2 compatible clone also sold as SuperMini nRF52840, nRF52840 Pro Micro or nRFMicro (nRF52840 QIAA, Cortex-M4F @ 64 MHz, 1 MB flash / 256 KB RAM, BLE 5 + 802.15.4, native USB, Adafruit UF2 bootloader with SoftDevice S140). Covers the PlatformIO + Adafruit nRF52 Arduino setup for a board that ships in neither, the vendored board definition and Pro Micro pin map, USB CDC via TinyUSB, Bluefruit BLE, the 32.768 kHz crystal that clones omit, SAADC pins, NFC pins as GPIO, and Zephyr's promicro_nrf52840 board target. Use when working on this board or any nice!nano v2 / ZMK keyboard controller: project setup, platformio.ini, pin mapping, LED not blinking, double-tap RESET and UF2/DFU flashing, SoftDevice version mismatches, a board that enumerates but never runs, or debugging why something on the board does not work.
---

# ProMicro nRF52840 (V1940)

A nice!nano v2 clone: nRF52840 module on a Pro Micro footprint, USB-C, factory
Adafruit UF2 bootloader with SoftDevice S140 6.1.1. Sold as *ProMicro
nRF52840*, *SuperMini nRF52840*, *nRF52840 Pro Micro*; the ZMK/QMK wireless
keyboard crowd is where most of the documentation lives.

Its failure modes are quiet and all look like "the board is dead": the board
is in neither PlatformIO's board list nor the Arduino core's variant list, so
half of what a tutorial tells you to type does not apply; a missing 32.768 kHz
crystal hangs the SoftDevice with USB still enumerated; and `Serial` does not
exist until you include a library that the code never otherwise mentions.

- `reference/board-hardware.md` — the complete board reference: full pin map
  (silkscreen → Arduino number → P0.xx/P1.xx → peripheral), power, flash and
  RAM maps, bootloader layout, clocks, **plus** a development guide (Part II:
  §7 toolchain, §8 peripheral cookbook, §9 flashing and recovery, §10 the
  Zephyr board target, §11 symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini`, the board JSON,
  `variant.h`/`variant.cpp` excerpts, USB CDC + `Serial1`, SAADC with the
  gain math, `Wire`/`SPI`, BLE UART, InternalFS, deep sleep, UF2 conversion.
- `template/` — a **project that builds clean**, in three variants, plus a
  scaffold script. See `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | nRF52840 QIAA (aQFN73 module) — ARM Cortex-M4F @ **64 MHz, fixed**; no PLL to configure |
| Memory | 1 MB internal flash, 256 KB RAM. With S140 6.1.1 + UF2 bootloader the app gets **815,104 B flash** (`0x26000`–`0xED000`) and **237,568 B RAM** (`0x20006000`–`0x20040000`) |
| LED | **P0.15** = Arduino pin `11` (`LED_BUILTIN`), **active HIGH**, not on the header. `P0.26`/`P0.30` are the fallback candidates on other clone revisions |
| Button | **RESET only** — no user button. Double-tap it to reach the bootloader |
| USB | USB-C, native FS device (no USB-serial chip). `Serial` = **USB CDC via TinyUSB**; bootloader enumerates as a `NICENANO`/`PROMICRO` mass-storage drive |
| Radio | BLE 5 / 802.15.4 / ANT via **SoftDevice S140 6.1.1** already on the board; Bluefruit is the Arduino API |
| LF clock | **32.768 kHz crystal is often NOT populated** on clones. `USE_LFRC` in `variant.h` (internal RC) is the safe default; `USE_LFXO` hangs a board without the crystal |
| ADC | SAADC, 14-bit max, **10-bit by default**, internal 0.6 V ref + 1/6 gain = **0–3.6 V full scale**. Channels only on P0.02–P0.05, P0.28–P0.31 — **A0 (P1.15) has no ADC** |
| Bus defaults | `Wire` = D2/D3 (P0.17/P0.20) · `SPI` = D14/D15/D16 + SS D10 (P1.11/P1.13/P0.10, P0.09) · `Serial1` = D0/D1 (P0.08/P0.06) |
| Logic | 3.3 V, **not 5 V tolerant**; 15 mA per pin (high-drive), 0.5 mA standard drive |
| Power | USB 5 V → on-board 3.3 V regulator; battery pads on the underside on most revisions. **No published schematic for V1940** — treat battery-sense and charger claims as unverified |
| Debug | SWDIO/SWCLK pads on the back, no header. J-Link, CMSIS-DAP or a WCH-LinkE in ARM mode |
| Toolchain | PlatformIO + `nordicnrf52` 10.11.0 + `framework-arduinoadafruitnrf52` 1.10700.0 (Adafruit core 1.7.0). Zephyr also supports it as `promicro_nrf52840/nrf52840/uf2` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **This board does not exist in PlatformIO — vendor it.** `platform-
   nordicnrf52` 10.11.0 ships 44 nRF5 boards, none of them a nice!nano or
   Pro Micro clone, and the Adafruit core ships 14 variants, none with this
   pin map. Every project needs `boards/promicro_nrf52840.json` and
   `boards/variants/promicro_nrf52840/` copied in. Substituting
   `board = adafruit_feather_nrf52840` *builds and flashes* — and then every
   pin number is wrong and the LED is on a pin this board does not use.
   Scaffold from `template/` instead of hand-writing them.
2. **`"variants_dir": "boards/variants"` must be in the board JSON.** It is
   resolved relative to the project root and is the only thing pointing the
   core at your vendored `variant.h`. Without it the build fails deep inside
   the core with `cores/nRF5/Uart.h:27:10: fatal error: variant.h: No such
   file or directory` — which reads like a broken core install, not a missing
   JSON key.
3. **Keep `USE_LFRC` in `variant.h` unless you have confirmed the crystal.**
   Many clones leave the 32.768 kHz LF crystal unpopulated. With `USE_LFXO`
   the SoftDevice waits forever for a clock that never starts: USB still
   enumerates, the LED never blinks, `Serial` never opens — indistinguishable
   from a bad flash. `USE_LFRC` costs ~250 ppm of timing accuracy and a little
   sleep current, and always works.
4. **`Serial` only exists if you `#include <Adafruit_TinyUSB.h>`.** The object
   is defined by the TinyUSB library, not by `Arduino.h`. Omit the include and
   the link fails with `undefined reference to 'Serial'` and
   `undefined reference to 'Adafruit_USBD_CDC::begin(unsigned long)'` — an
   error that points at your own `setup()` and looks like a toolchain problem.
5. **Never `while (!Serial);`.** USB CDC is only "open" once a host opens the
   port. On a board running from a battery or a charger-only USB port that
   loop never exits and the firmware appears bricked. Print unconditionally and
   accept that early bytes are lost.
6. **The board does not enter the bootloader by itself from a cold start.**
   Double-tap RESET (two presses within ~0.5 s, or bridge RST to GND twice);
   the LED starts a slow fade and a `NICENANO`/`PROMICRO` drive appears. A
   *running* sketch with working USB CDC also reboots into DFU on PlatformIO's
   1200-baud touch, so repeat uploads often need no button — but if the
   previous firmware crashed, hung on rule 3, or never brought USB up, the
   touch does nothing and `pio run -t upload` fails to find a port.
7. **The SoftDevice FWID in the board JSON must match what is on the board.**
   PlatformIO packages the DFU zip with `--sd-req 0x00B6` (S140 **6.1.1**) from
   `build.softdevice.sd_fwid`, and the bootloader rejects a package whose
   `sd-req` it does not recognise. If your board came with S140 7.x, set
   `sd_version` to `7.3.0` and `sd_fwid` to `0x0123` — **and note that Adafruit
   core 1.7.0 ships only `nrf52840_s140_v6.ld`**, so you also need a newer core
   via `platform_packages` before `nrf52840_s140_v7.ld` exists to link against.
8. **`A0` is a trap: P1.15 has no ADC channel.** The nRF52840's SAADC reaches
   only P0.02–P0.05 and P0.28–P0.31; nothing on port 1 is analog. `analogRead(A0)`
   compiles, returns a meaningless number, and looks like a wiring fault. The
   real analog pins on the header are **A1 (P0.02/AIN0), A2 (P0.29/AIN5),
   A3 (P0.31/AIN7)**.
9. **`analogRead` returns 10-bit values by default on a 14-bit ADC,** and full
   scale is a fixed **3.6 V** (internal 0.6 V reference × 1/6 gain), not VDD.
   Two consequences: a 3.3 V signal reads ~92 % of scale, never 1023; and the
   reading does *not* track the supply, so dividing by 1023 and multiplying by
   3.3 is wrong by 9 %. `analogReadResolution(12)` and scale by 3600/4096 mV.
10. **The NFC-pins-as-GPIO flag is `CONFIG_NFCT_PINS_AS_GPIOS` — with an S.**
    P0.09/P0.10 are D10 (`SS`) and D16 (`MOSI`), and they are NFC antenna pins
    until UICR says otherwise. Most clone board JSONs in circulation (including
    the one this skill was extracted from) define the singular
    `-DCONFIG_NFCT_PINS_AS_GPIO`, which **matches nothing in the core** — the
    only spelling `system_nrf52840.c` tests is the plural. Verified: adding the
    S grows the build by 104 B and puts the NVMC/UICR write into `SystemInit`;
    without it that code is not compiled at all, and whether SPI works on the
    default pins is left to whatever the bootloader happened to leave in UICR.
    When the flag is correct the core clears `UICR->NFCPINS` on first boot and
    **immediately calls `NVIC_SystemReset()`** — so the first run after flashing
    resets once, which looks like a crash loop if you are watching the serial
    port. UICR survives reflashing and is only cleared by a full chip erase.
11. **The linker stops at `0xED000`, not at the bootloader.** The 28 KB from
    `0xED000` to `0xF4000` is the Adafruit `InternalFS`/LittleFS region
    (`LFS_FLASH_ADDR 0xED000`, 7 × 4 KB pages) — it is reserved whether or not
    you use it. Firmware larger than 815,104 B fails to link even though the
    chip has 1 MB.
12. **PlatformIO's RAM percentage is 11 KB optimistic on stock board JSONs.**
    Every Adafruit nRF52840 board definition shipped with platform-nordicnrf52
    10.11.0 — and every clone board JSON copied from one — sets
    `upload.maximum_ram_size` to 248,832 B, while the linker script the same
    core uses (`nrf52840_s140_v6.ld`) gives the application
    `0x20006000`–`0x20040000` = **237,568 B**. The progress bar is computed
    against the larger number, so a build reported as comfortably inside RAM
    can still overflow at link time. The template's JSON is corrected to
    237,568.
13. **The SoftDevice owns hardware once BLE starts.** `Bluefruit.begin()`
    takes TIMER0, RTC0, several PPI channels, SWI2/SWI4 and all RAM below
    `0x20006000`. Touching those directly after that point produces hard faults
    or a silent radio; use the Bluefruit/`sd_*` APIs instead. Also note the
    Adafruit core runs **FreeRTOS underneath** — `loop()` is a task, `delay()`
    yields, and a tight busy-wait starves the USB and BLE stacks.
14. **Only three pins are missing from the header, and one is the LED.** P0.15
    (LED), P0.26, P0.30 and P0.28 are not broken out on this footprint; a
    wiring plan that uses them compiles and does nothing visible. Conversely
    P0.00/P0.01 (XL1/XL2) and P0.18 (RESET) are absent from the pin map on
    purpose — do not add them.
15. **3.3 V logic, not 5 V tolerant.** Anything 5 V (a classic HD44780, most
    NeoPixel strips at 5 V, an Uno's I2C bus) needs level shifting. This kills
    boards silently over hours, not instantly.

## When the task is BLE

Everything goes through Bluefruit (`#include <bluefruit.h>`), which drives the
S140 SoftDevice already flashed on the board — you are not compiling a radio
stack, you are calling one. `Bluefruit.begin()` is where a wrong LF clock
(rule 3) actually hangs, so if BLE code stops at `begin()` on a board whose
blink sketch worked, suspect the crystal before the BLE code.

The cost is real: the `--ble` template variant is 128,676 B flash / 15,588 B
RAM against 20,196 B / 3,092 B for a bare blink — Bluefruit and its LittleFS
bonding store are ~73 KB of that. `Bluefruit.begin(peripheral, central)` takes
connection counts, and each connection costs RAM out of the same budget.
`setTxPower()` accepts only −40, −20, −16, −12, −8, −4, 0, +2, +3, +4, +5, +6,
+7, +8 dBm; other values are silently clamped. Bonding data lives in InternalFS
at `0xED000` and survives a reflash — a peer that "won't reconnect after you
changed the code" is usually a stale bond, cleared with
`Bluefruit.Periph.clearBonds()`.

For ZMK/QMK keyboard firmware, do not use this Arduino setup at all: those
build on Zephyr and already have a `nice_nano_v2` board target that matches
this hardware. `reference/board-hardware.md` §10 covers the Zephyr route,
including the pins where upstream's `promicro_nrf52840` devicetree disagrees
with the Pro Micro silkscreen.

## When the task is low power

The nRF52840's advertised microamps depend on things this board makes easy to
get wrong. `USE_LFRC` (rule 3) recalibrates the RC oscillator periodically and
costs a few µA over a crystal — if you *have* the crystal and need sleep
current, that is the one reason to switch to `USE_LFXO`. The USB CDC stack
keeps the HFCLK and USB peripheral alive whenever a host is attached, so any
current measurement taken over USB measures the USB stack, not your firmware;
measure from battery pads with the CDC variant not even compiled in (the
`--minimal` template variant exists partly for this). `systemOff()` from the
core (or `sd_power_system_off()` under the SoftDevice) reaches the ~0.4 µA
System OFF state, from which only a configured pin wake or reset returns —
and it never returns to the next line, so treat it as a jump to reset.

The on-board 3.3 V regulator's quiescent current is the floor for the whole
board and is **not documented for this clone**; measured deep-sleep figures for
nice!nano v2 are in the tens of µA. Do not quote a number for V1940 you have
not measured.

## Starting a new project

Do not hand-assemble one — the vendored board definition is the hard part.

```sh
~/.claude/skills/nrf52840-promicro/template/variants/new-project.sh <target-dir> [--full|--minimal|--ble]
cd <target-dir> && pio run
# double-tap RESET, then:
pio run -t upload -t monitor
```

- `--minimal` — blink P0.15, no USB stack. **20,196 B** flash / **3,092 B**
  RAM. Flash this first on an unfamiliar board: if the LED does not blink, the
  problem is the bootloader dance or your board's LED pin, not your code.
- `--full` (default) — heartbeat + USB-CDC status line + 12-bit ADC on A1 +
  die temperature. **55,320 B** / **8,772 B**.
- `--ble` — heartbeat + BLE Nordic UART Service bridged to USB CDC.
  **128,676 B** / **15,588 B**.

All three build as-is with platform-nordicnrf52 10.11.0 + Adafruit core 1.7.0
(verified). Nothing is generated and no paths are embedded, so copying the tree
by hand works identically. `template/README.md` maps files to subsystems.

When the user already has a project that "can't find the board", prefer copying
`template/boards/` into it and setting `board = promicro_nrf52840` over
rewriting their code — that is usually the whole fix.

## Flashing

```sh
pio run                    # build
# double-tap RESET — LED fades slowly, NICENANO/PROMICRO drive appears
pio run -t upload -t monitor
```

PlatformIO builds `firmware.hex`, packages it into `firmware.zip` (a Nordic DFU
package with `--sd-req 0x00B6`), touches the port at 1200 baud, and runs
`adafruit-nrfutil dfu serial --singlebank`. The board reboots into the new
firmware on its own; the CDC port re-enumerates as `/dev/cu.usbmodem*` or a COM
port a second later, and the monitor's baud rate is ignored.

The drag-and-drop route: convert the hex with
[`uf2conv.py`](https://github.com/microsoft/uf2) (family `0xADA52840`) and drop
the `.uf2` on the bootloader drive; it reboots when the copy finishes.

**Recovery.** The double-tap bootloader survives any application crash, so a
board that has stopped responding is almost always recoverable that way — try
a slower or faster double-tap, and check the cable carries data. If the
bootloader itself is gone (a bad SWD write, a wrong-SoftDevice flash), the only
way back is SWD on the rear pads with J-Link, pyOCD, or OpenOCD + a CMSIS-DAP
probe: `nrf5 mass_erase`, then flash
`nice_nano_bootloader-0.9.2_s140_6.1.1.hex`. Full sequence in
`reference/board-hardware.md` §9.

## Reporting

State honestly what was verified and what was not. In this skill: all three
template variants and the compile-marked recipes build clean against
platform-nordicnrf52 10.11.0 + framework-arduinoadafruitnrf52 1.10700.0; the
board definition, `g_ADigitalPinMap` and the P0.15 LED come from a working
project for this exact V1940 board. The linker/flash/RAM addresses, the
`InternalFS` region, the SAADC defaults and the SoftDevice FWIDs were read out
of the installed core and platform sources, not guessed. **No current, timing
or analog measurement here was taken with instruments,** and **V1940 has no
published schematic** — the battery charger, battery-sense divider, regulator
quiescent current and any second LED on your revision are unverified. Say so
rather than quoting a nice!nano v2 figure as if it were this board's.
