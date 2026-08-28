# WeAct RP2350A Core Board template project

A complete PlatformIO + Arduino project for the WeAct Studio RP2350A Core
Board, **both revisions**. Builds clean as-is; nothing is generated and no
paths are embedded, so copying this tree by hand works exactly as well as
running the scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal] [--v10|--v20] [--16mb]
cd <target-dir>
pio run -t upload -t monitor
```

## Which revision, which flash

Four envs, one line to pick between them:

| env | board | flash | max sketch |
|---|---|---|---|
| `weact_rp2350a_v20` (default) | RP2350A_V20 | 4 MB | 4,190,208 B |
| `weact_rp2350a_v20_16mb` | RP2350A_V20 | 16 MB | 16,773,120 B |
| `weact_rp2350a_v10` | RP2350A_V10 | 4 MB | 4,190,208 B |
| `weact_rp2350a_v10_16mb` | RP2350A_V10 | 16 MB | 16,773,120 B |

`-DBOARD_REV=10|20` is what `include/board.h` switches on, and it **refuses
to compile without it** — a wrong-revision build cannot happen quietly. Tell
the revisions apart by the top row of pins: V2.0 has `VREF` where V1.0 has
`29`, V2.0 has one user LED where V1.0 has two plus a third button marked
`23@KEY`.

Flash size is not detectable at build time. The `--full` sketch reads the
flash chip's JEDEC ID at boot and prints **both** the real size and the size
the build assumed — if they disagree, switch envs. Claiming 16 MB on a 4 MB
board gives you a linker that thinks it has room it does not have and an
EEPROM/filesystem region addressed past the end of the chip.

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | LED heartbeat, USB-CDC report, real flash size from the JEDEC ID, ADC on A0 at native 12 bits, die temperature, EEPROM boot counter; **V2.0**: VSYS sense + VBUS sense + SMPS mode pin, **V1.0**: second LED + KEY button | **37,676 B** (V2.0) / 37,608 B (V1.0) — 0.9 % of 4,190,208 B | **9,824 B** — 1.9 % of 512 KB |
| `--minimal` | Blink on GP25. Nothing else. | **32,916 B** — 0.8 % | **8,504 B** — 1.6 % |

Figures are what `pio run` reports for platform-raspberrypi 1.20.0 with
arduino-pico 6.0.0 (earlephilhower). The floor is the TinyUSB CDC stack: even
a bare blink costs ~33 KB flash and ~8.5 KB RAM. **Flash `--minimal` first on
an unfamiliar board**: if the LED does not blink after `pio run -t upload`,
the problem is the cable, the BOOT dance or the env choice, not the code.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — the four envs, board/core selection, upload behaviour | both |
| `include/board.h` | **the pin map for both revisions**, and the `BOARD_REV` guard | both |
| `src/main.cpp` | heartbeat + USB-CDC report + JEDEC flash size + ADC + EEPROM + per-revision extras | full |
| `variants/minimal/main.cpp` | the minimal blink | minimal |

To strip a `--full` scaffold back to something of your own, keep
`include/board.h` — that is the reusable half — and replace `src/main.cpp`.

## Uploading

`pio run -t upload -t monitor` flashes over the USB-C cable with no button
press: PlatformIO touches the USB CDC port at 1200 baud, a running
arduino-pico sketch reboots itself into BOOTSEL and picotool flashes it, then
reboots the board. If the running firmware has crashed or its USB is dead,
hold **BOOT** while (re)plugging the cable, release, then re-run — a USB drive
named `RP2350` appears (not `RPI-RP2`; that is the RP2040 name). The monitor
attaches to the CDC port (`/dev/cu.usbmodem*` on macOS, a COM port on
Windows); the baud rate is ignored.

## What is verified on hardware

All four envs build clean, and the `--minimal` blink is what the author ran on
a **V2.0** board: it uploads over USB-C and blinks GP25. Everything about
**V1.0 is transcribed from `V10_SCH.pdf` and WeAct's pinout card, not run** —
the LED2/KEY pins, the LDO input range and the free ADC3 on GP29 are
schematic-derived. The JEDEC flash-size read, the EEPROM counter, the VSYS
sense math and the ADC numbers are compile-verified only; in particular the
V2.0 VSYS network puts a FET between the divider and GP29, so treat `×3` as a
starting point to calibrate, not a specification.

If something misbehaves, say so — that is a gap in this skill, not in your
board.

## Third-party code

None. Everything here is original to this template; the arduino-pico core and
the TinyUSB and pico-sdk code it links against ship with PlatformIO's
`raspberrypi` platform under their own licenses.
