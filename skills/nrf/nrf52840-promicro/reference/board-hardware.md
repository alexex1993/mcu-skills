# ProMicro nRF52840 (V1940) hardware reference

Part I is the board as wired; Part II is the development guide.

**Provenance, because it matters here.** This board has **no published
schematic**. Part I's pin map is derived from a working Arduino variant for
this exact board (V1940) cross-checked against two independent sources that
agree with it: the nice!nano v2 pinout published by Nice Keyboards, and the
upstream Zephyr board port `boards/others/promicro_nrf52840` (contributed by
MASSDRIVER EI, 2024). Chip-level facts come from the nRF52840 Product
Specification v1.11. Anything about the charger, the regulator or a
battery-sense divider is **not verified for V1940** and is marked as such.

---

# Part I — the board

## 1. Overview

| | |
|---|---|
| Board | ProMicro nRF52840, silkscreen **V1940**; also sold as SuperMini nRF52840, nRF52840 Pro Micro, "nice!nano clone" |
| Compatible with | nice!nano v2 (pin-for-pin), nRFMicro; Zephyr board target `promicro_nrf52840`, ZMK board `nice_nano_v2` |
| MCU | nRF52840 QIAA (aQFN73) — ARM Cortex-M4F + FPU @ **64 MHz fixed**, 1 MB flash, 256 KB RAM |
| Radio | BLE 5.x, 802.15.4 (Thread/Zigbee), ANT, 2.4 GHz proprietary; on-module chip antenna |
| Form factor | Pro Micro: ~33 × 18 mm, 2× 12-pin 2.54 mm headers, castellated edges |
| USB | USB-C, native USB 2.0 Full Speed device on-chip — **no CH340/CP2102** |
| Bootloader | Adafruit nRF52 UF2 bootloader, factory-flashed, with **SoftDevice S140 6.1.1** |
| Logic | 3.3 V, **not 5 V tolerant** on any pin |
| Debug | SWDIO / SWCLK pads on the underside, no header |

## 2. Pin map

Arduino pin numbers are the Pro Micro **silkscreen** numbers, defined by
`g_ADigitalPinMap` in the variant. "Port pin" is the nRF52840 name — the one
you need for the Nordic datasheet, Zephyr devicetree, `nrfjprog`, and the
`32*port + pin` encoding used inside the map.

### 2.1 Header pins

| Silk | Arduino | Port pin | SAADC | Default role | Notes |
|---|---|---|---|---|---|
| 0 | 0 | P0.08 | — | `Serial1` RX | |
| 1 | 1 | P0.06 | — | `Serial1` TX | |
| 2 | 2 | P0.17 | — | `Wire` SDA | |
| 3 | 3 | P0.20 | — | `Wire` SCL | |
| 4 | 4 | P0.22 | — | | |
| 5 | 5 | P0.24 | — | | |
| 6 | 6 | P1.00 | — | | port 1: no ADC |
| 7 | 7 | P0.11 | — | | |
| 8 | 8 | P1.04 | — | | port 1: no ADC |
| 9 | 9 | P1.06 | — | | port 1: no ADC |
| 10 | 10 | P0.09 | — | `SPI` SS | **NFC1** — needs `CONFIG_NFCT_PINS_AS_GPIOS` |
| 14 | 14 | P1.11 | — | `SPI` MISO | port 1: no ADC |
| 15 | 15 | P1.13 | — | `SPI` SCK | port 1: no ADC |
| 16 | 16 | P0.10 | — | `SPI` MOSI | **NFC2** — needs `CONFIG_NFCT_PINS_AS_GPIOS` |
| A0 | 18 | P1.15 | **none** | digital only | **trap** — labelled analog, cannot be sampled |
| A1 | 19 | P0.02 | AIN0 | analog in | |
| A2 | 20 | P0.29 | AIN5 | analog in | |
| A3 | 21 | P0.31 | AIN7 | analog in | also `AREF` in the variant |

Plus the power pins: **RAW** (USB 5 V / battery input, ahead of the
regulator), **VCC** (3.3 V regulator output), **GND** ×3, **RST** (P0.18,
active low — short to GND twice quickly for the bootloader).

Physical arrangement is the standard Pro Micro / nice!nano v2 one — USB at the
top; left column top-to-bottom `1, 0, GND, GND, 2, 3, 4, 5, 6, 7, 8, 9`, right
column top-to-bottom `RAW, GND, RST, VCC, A3, A2, A1, A0, 15, 14, 16, 10`.
Confirm against your own silkscreen before soldering; only the label → port
mapping above is verified.

### 2.2 Pins that exist in code but not on the header

| Arduino | Port pin | Board function |
|---|---|---|
| 11 | **P0.15** | **on-board blue LED, active HIGH** — `LED_BUILTIN` |
| 12 | P0.26 | LED fallback candidate on other clone revisions |
| 13 | P0.30 | LED fallback candidate on other clone revisions |
| 17 | P0.28 | AIN4, unrouted on this footprint |

The Zephyr port and the nice!nano v2 documentation both put the single LED on
**P0.15** (Zephyr labels it "Red LED" in devicetree; on this board it is blue —
the label is upstream's, not a hardware difference). If yours does not light,
build the template's `--full` variant with `-DBLINK_ALL_LED_CANDIDATES=1`: it
blinks P0.15, P0.26 and P0.30 simultaneously and the working one shows itself.

### 2.3 Deliberately absent from the pin map

| Port pin | Why |
|---|---|
| P0.00 / P0.01 | XL1 / XL2 — the 32.768 kHz crystal pads (see §5) |
| P0.18 | RESET (`nRESET`, configured through UICR `PSELRESET`) |
| P0.19, P0.21, P0.23, P0.25, P0.27, P1.01–P1.03, P1.05, P1.07–P1.10, P1.12, P1.14 | not routed out of the module / not on this footprint |
| P0.05, P0.03, P0.04 | AIN1/AIN3/AIN2 — **not on the header** on this footprint, though they are the chip's other analog channels |

That last row is why analog work on this board is limited to three usable
channels: the chip has eight, the footprint exposes three.

## 3. Power

| Rail | Where | Notes |
|---|---|---|
| USB 5 V | USB-C VBUS → **RAW** | |
| RAW | pin, also battery + input on most revisions | ahead of the regulator |
| VCC | pin, 3.3 V | regulator output; also feeds the module |

Most V1940 boards carry a Li-ion charger and B+/B− pads on the underside, and
some carry a battery-voltage divider. **The V1940 schematic is not published,
the charge current is not documented, and the divider's pin and ratio are not
verified.** nice!nano v2 uses a divider into AIN2 (P0.04) with an enable pin —
P0.04 is *not on this footprint's header*, which is consistent with an
on-board-only divider, but this has not been confirmed on V1940. Measure
before you trust any of it, and never publish a battery-percentage curve based
on the assumption.

Per-pin current: 0.5 mA standard drive, 15 mA in high-drive mode (`H0H1`);
total GPIO current is bounded well below the sum of those.

## 4. Flash and RAM map

With the factory bootloader and SoftDevice S140 6.1.1:

| Region | Start | End | Size | Contents |
|---|---|---|---|---|
| MBR | `0x00000` | `0x01000` | 4 KB | Nordic Master Boot Record |
| SoftDevice | `0x01000` | `0x26000` | 148 KB | S140 6.1.1 |
| **Application** | `0x26000` | `0xED000` | **815,104 B** | your firmware |
| InternalFS | `0xED000` | `0xF4000` | 28 KB | Adafruit LittleFS (`LFS_FLASH_ADDR`, 7 × 4 KB pages) — reserved whether used or not |
| Bootloader | `0xF4000` | `0xFF000` | 44 KB | Adafruit UF2 bootloader |
| BL settings | `0xFF000` | `0x100000` | 4 KB | DFU state, `bootloader.settings_addr` |

RAM: the SoftDevice owns everything below **`0x20006000`**; the application
gets `0x20006000`–`0x20040000` = **237,568 B**. Both figures come from
`nrf52840_s140_v6.ld` in Adafruit core 1.7.0, which is what actually links your
firmware. Every stock Adafruit nRF52840 board JSON in platform-nordicnrf52
10.11.0 nevertheless claims `maximum_ram_size: 248832`, so PlatformIO's
progress bar runs ~5 % (11,264 B) optimistic on any board definition copied
from one. The vendored JSON here says 237,568.

UICR (`0x10001000`) holds the NFC-pins-as-GPIO bit, `PSELRESET`, and the
approtect setting. It is only cleared by a **full chip erase**, so those
settings survive reflashing — and a chip erase also removes the bootloader and
SoftDevice, which is why "just erase everything" is a recovery of last resort.

## 5. Clocks

| Clock | Source | Notes |
|---|---|---|
| HFCLK | internal 64 MHz RC, or the module's 32 MHz crystal | fixed 64 MHz core; USB and the radio force the crystal on |
| LFCLK | **32.768 kHz crystal (XL1/XL2 = P0.00/P0.01) — often not populated on clones**, or internal 32 kHz RC | selected in `variant.h`: `USE_LFXO` / `USE_LFRC` |

There is no PLL to configure and no `HSE_VALUE`-style constant to get wrong;
the LF clock choice is the only clock decision on this board, and it is the one
that hangs boards. `USE_LFRC` uses the internal RC (~250 ppm, recalibrated
periodically, a few µA more in sleep) and works everywhere. `USE_LFXO` requires
the crystal to be physically present: without it the SoftDevice blocks waiting
for `EVENTS_LFCLKSTARTED`, USB stays enumerated and nothing else ever runs.

## 6. USB and the bootloader

The nRF52840 has a native USB 2.0 FS device peripheral — there is no
USB-to-serial chip, which has two consequences worth internalising:

- The USB device disappears when your firmware resets or crashes, and comes
  back as a different device (bootloader vs application). "The port vanished"
  is normal behaviour, not a fault.
- `Serial` is a CDC endpoint implemented in firmware by TinyUSB. It exists
  only while your firmware is running and only after enumeration completes.

The bootloader enumerates as a USB mass-storage drive named `NICENANO` or
`PROMICRO` (revision-dependent) plus a CDC port for DFU. Application USB IDs
in the vendored board JSON: `239A:0029/8029/002A/802A` (Adafruit) and
`1915:520F` (Nordic) — a superset, because clone revisions differ.

---

# Part II — development guide

## 7. Toolchain

Verified combination:

| | |
|---|---|
| PlatformIO platform | `nordicnrf52` **10.11.0** |
| Framework package | `framework-arduinoadafruitnrf52` **1.10700.0** = Adafruit nRF52 Arduino core **1.7.0** |
| Compiler | `toolchain-gccarmnoneeabi` 1.70201.0 (GCC 7.2.1) |
| DFU tool | `tool-adafruit-nrfutil` 1.503.0 |
| CMSIS | `framework-cmsis` 2.50700.210515 |

The board is **not** in the platform's board list (44 nRF5 boards, none of them
a nice!nano or Pro Micro clone) and its variant is **not** in the core's
variants directory (14 variants, none matching). Both are vendored in the
project — see `template/boards/`. `"variants_dir": "boards/variants"` in the
board JSON is resolved against the project root and is what makes the core find
`variant.h`.

Other cores and frameworks that work on this hardware, for orientation:
Zephyr (§10), ZMK (`nice_nano_v2` board), CircuitPython (a UF2 you drag on),
and the Nordic nRF Connect SDK. The Adafruit Arduino core is the one this skill
covers.

## 8. Peripheral cookbook

Full compilable versions of everything here are in `recipes.md`; this section
is the *why*.

**GPIO.** `pinMode`/`digitalWrite` take Arduino pin numbers and go through
`g_ADigitalPinMap`. For the LED prefer `ledOn(pin)` / `ledOff(pin)` from the
core — they apply `LED_STATE_ON` from `variant.h`, so code stays correct on a
revision with the opposite polarity. High-drive mode (15 mA) is not exposed by
the Arduino API; set `NRF_P0->PIN_CNF[n]` DRIVE bits directly if you need it.

**USB CDC (`Serial`).** Requires `#include <Adafruit_TinyUSB.h>` — that library
defines the object. `Serial.begin(115200)` is a formality; the baud rate is
ignored. Do not gate startup on the port being open (SKILL.md rule 5). Costs
~35 KB flash and ~5.7 KB RAM over a build with no USB at all.

**UART (`Serial1`).** A real UARTE on D0/D1 (P0.08 RX, P0.06 TX), completely
independent of `Serial`. Any baud rate; `Serial1.setPins()` can move it, since
nRF52 peripherals are routed to pins by register, not fixed by silicon.

**I2C (`Wire`).** D2/D3 = P0.17 SDA, P0.20 SCL. No pull-ups on the board — the
module does not provide them and the internal ones (~13 kΩ) are too weak for
anything but a short bus at 100 kHz. Fit 4.7 kΩ externally. `Wire.setPins()`
before `begin()` moves the bus to any two GPIOs.

**SPI.** D14 MISO / D15 SCK / D16 MOSI, SS on D10. Note that **MOSI and SS are
the NFC pins** — SPI on the defaults only works once `UICR->NFCPINS` has been
cleared, which the core does at boot when built with
`-DCONFIG_NFCT_PINS_AS_GPIOS` (plural — see §8.1 below; already correct in the
vendored board JSON). Up to 8 MHz with SPIM.

### 8.1 UICR settings the core writes at boot

`system_nrf52840.c` (Nordic MDK, compiled into every build) checks two macros
in `SystemInit`, before `main`:

| Macro | Effect | Note |
|---|---|---|
| `CONFIG_NFCT_PINS_AS_GPIOS` | clears `UICR->NFCPINS` PROTECT, freeing P0.09/P0.10 for GPIO | **plural.** The singular `CONFIG_NFCT_PINS_AS_GPIO` that most clone board JSONs carry matches nothing and is silently a no-op — verified: adding the S grows the build by 104 B and is what puts the NVMC write into `SystemInit` |
| `CONFIG_GPIO_AS_PINRESET` | writes `UICR->PSELRESET[0..1] = 18`, making P0.18 the reset pin | not needed here: the factory Adafruit bootloader already configures P0.18, which is what makes double-tap RESET work |

Both are one-shot: the code only writes when UICR does not already hold the
value, and each write is followed immediately by `NVIC_SystemReset()`. So the
first boot on a fresh chip resets once and then behaves normally — on the
serial console this looks like a spurious reboot. UICR survives reflashing;
only a full chip erase clears it.

**ADC (SAADC).** Only P0.02–P0.05 and P0.28–P0.31 have channels; on this
footprint that is **A1 (P0.02/AIN0), A2 (P0.29/AIN5), A3 (P0.31/AIN7)** only.
Core defaults, from `wiring_analog_nRF52.c`: 10-bit result, internal 0.6 V
reference, gain 1/6 → **0–3.6 V full scale, independent of VDD**. So:

```
millivolts = raw * 3600 / (1 << resolution)
```

`analogReadResolution(12)` widens the result (14 is the hardware maximum);
`analogReference(AR_INTERNAL_3_0)` gives a 3.0 V scale (gain 1/5);
`analogReference(AR_VDD4)` makes it ratiometric to VDD instead;
`analogOversampling(n)` enables SAADC burst mode for averaging in hardware.

**PWM.** The nRF52840 has four PWM peripherals; the Adafruit core exposes them
through `HardwarePWM` and plain `analogWrite`. Any GPIO can be a PWM output —
there is no fixed pin-to-channel mapping to work around, unlike most MCUs.

**BLE.** `#include <bluefruit.h>`. See SKILL.md "When the task is BLE".

**Filesystem.** `InternalFS` (Adafruit LittleFS) on the 28 KB at `0xED000`.
Shared with BLE bonding data; formatting it drops all bonds.

## 9. Flashing, and getting out of trouble

**Normal upload.**

```sh
pio run
# double-tap RESET (two presses within ~0.5 s, or bridge RST–GND twice)
# LED fades slowly; NICENANO/PROMICRO drive appears
pio run -t upload -t monitor
```

What PlatformIO does: builds `firmware.hex`; runs `adafruit-nrfutil dfu genpkg
--dev-type 0x0052 --sd-req <sd_fwid>` to produce `firmware.zip`; touches the
serial port at 1200 baud (`use_1200bps_touch`); then `adafruit-nrfutil dfu
serial -p <port> -b 115200 --singlebank -pkg firmware.zip`.

**UF2 drag-and-drop.** Convert with `uf2conv.py -c -f 0xADA52840
firmware.hex -o firmware.uf2` and copy it to the bootloader drive. Useful when
the DFU serial route is fighting you, and the only route from a machine without
PlatformIO.

**SoftDevice version mismatch.** The bootloader refuses a package whose
`--sd-req` FWID it does not know. Known S140 FWIDs: **6.1.1 = `0x00B6`**,
**7.3.0 = `0x0123`** (S132 6.1.1 = `0x00B7`, for contrast). Change both
`sd_version` and `sd_fwid` in the board JSON together. Adafruit core 1.7.0
ships `nrf52840_s140_v6.ld` but **no** `nrf52840_s140_v7.ld` — with an S140 7.x
board you also need a newer core:

```ini
platform_packages =
    framework-arduinoadafruitnrf52 @ https://github.com/adafruit/Adafruit_nRF52_Arduino.git#<tag>
```

**Recovery ladder**, cheapest first:

1. Double-tap RESET again — timing is fussier than it sounds; try slower.
2. Different USB cable (charge-only cables are the most common "dead board").
3. Different port; avoid hubs.
4. From the bootloader drive, drag a known-good UF2 (e.g. CircuitPython for
   nRF52840 nice!nano) to prove the bootloader itself is alive.
5. SWD on the rear pads, if the bootloader is gone. J-Link, pyOCD, or OpenOCD
   with a CMSIS-DAP probe (a WCH-LinkE in ARM mode works):

   ```sh
   openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg
   telnet localhost 4444
   ```
   ```
   reset halt
   nrf5 mass_erase
   flash write_image erase nice_nano_bootloader-0.9.2_s140_6.1.1.hex
   ```

   A mass erase clears UICR too, so the NFC-pins-as-GPIO and `PSELRESET`
   settings go with it; they are re-applied by the core on the next boot.

Step 5 is the only step that can permanently change the board, and it is also
the only one that recovers a board whose bootloader was overwritten. Everything
above it is safe to retry indefinitely.

## 10. The Zephyr route

Upstream Zephyr carries this board as `boards/others/promicro_nrf52840`
(contributed 2024), with two targets:

| Target | Partitioning |
|---|---|
| `promicro_nrf52840/nrf52840` | bare, `FLASH_LOAD_OFFSET 0x1000` (after the Nordic MBR) |
| `promicro_nrf52840/nrf52840/uf2` | UF2 output on, partitions from `nrf52840_partition_uf2_sdv6.dtsi` — **this is the one for a factory board** |

```sh
west build -b promicro_nrf52840/nrf52840/uf2 zephyr/samples/basic/blinky
# double-tap RESET, mount the drive, then
west flash
```

What the upstream devicetree gives you: `led0` on **P0.15 active-high**
(agreeing with the Arduino variant), `nfct-pins-as-gpios` and `gpio-as-nreset`
in UICR, USB CDC ACM as the console, ADC/PWM/SPI/I2C/watchdog/IEEE 802.15.4
enabled, and `CONFIG_ARM_MPU` + `CONFIG_HW_STACK_PROTECTION` on by default.

**Where upstream disagrees with the Pro Micro silkscreen** — check this before
wiring anything against the Zephyr defaults, because the pinctrl file assigns
peripherals to pins on the basis of the chip, not the header:

| Zephyr node | Pins in `promicro_nrf52840-pinctrl.dtsi` | On the header? |
|---|---|---|
| `uart0` | TX P0.09, RX P0.10 | yes — but those are **D10 (SS) and D16 (MOSI)**, not the D0/D1 the silkscreen labels RX/TX |
| `i2c0` | SDA P1.00, SCL P0.11 | yes — **D6 and D7**, not the D2/D3 the silkscreen labels SDA/SCL |
| `i2c1` | SDA P1.04, SCL P1.06 | yes — D8 and D9 |
| `spi2` | SCK P1.01, MOSI P1.02, MISO P1.07 | **no — none of these three are broken out** on this footprint |

So a Zephyr `spi2` sample builds, runs, and drives nothing you can reach. Add a
board overlay remapping `spi2` to P1.13 / P0.10 / P1.11 (silkscreen 15 / 16 /
14) to match the Arduino pinout, and expect the same for `uart0` and `i2c0` if
you are following the silkscreen labels.

## 11. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| `Error: Unknown board ID 'promicro_nrf52840'` | the board JSON is not vendored in the project | copy `template/boards/` into the project root |
| `cores/nRF5/Uart.h:27:10: fatal error: variant.h: No such file or directory` | `variants_dir` missing from the board JSON, or the variant directory is not where it points | add `"variants_dir": "boards/variants"` to `build` |
| `undefined reference to 'Serial'`, `undefined reference to 'Adafruit_USBD_CDC::begin(unsigned long)'` | `Adafruit_TinyUSB.h` not included | `#include <Adafruit_TinyUSB.h>` in the file that uses `Serial` |
| Board enumerates over USB, LED never blinks, no serial output | `USE_LFXO` in `variant.h` on a board with no 32.768 kHz crystal — the SoftDevice waits for `EVENTS_LFCLKSTARTED` forever | switch to `USE_LFRC` |
| BLE sketch hangs at `Bluefruit.begin()`, blink sketch was fine | same LF clock problem — `begin()` is where the SoftDevice starts | `USE_LFRC` |
| Firmware appears hung until you open a serial monitor, then runs | `while (!Serial)` in `setup()` | delete it; print unconditionally |
| `pio run -t upload` cannot find a port / DFU times out | board is not in the bootloader; the 1200-baud touch only works if the *running* firmware has USB CDC up | double-tap RESET first |
| DFU starts and the bootloader rejects the package | `sd_fwid` in the board JSON ≠ the SoftDevice on the board | S140 6.1.1 → `0x00B6`, 7.3.0 → `0x0123`; a 7.x board also needs a newer core for `nrf52840_s140_v7.ld` |
| LED does not blink but upload succeeded | LED on a different pin on this revision | build with `-DBLINK_ALL_LED_CANDIDATES=1`; P0.15 / P0.26 / P0.30 |
| SPI does nothing on the default pins | P0.09/P0.10 are still NFC antenna pins — usually because the board JSON says `CONFIG_NFCT_PINS_AS_GPIO` (singular), which matches nothing | use `CONFIG_NFCT_PINS_AS_GPIOS`, **with an S**, in `extra_flags`; it is already correct in the vendored JSON |
| First boot after flashing resets once, serial port drops | correct: the core cleared `UICR->NFCPINS` and called `NVIC_SystemReset()` to apply it. Happens once per chip, not once per flash | nothing to fix |
| `analogRead(A0)` returns nonsense | P1.15 has no SAADC channel | use A1/A2/A3 (P0.02/P0.29/P0.31) |
| Analog readings ~9 % low, or a 3.3 V input never reaches full scale | full scale is 3.6 V (internal 0.6 V ref × 1/6 gain), not VDD, and resolution defaults to 10-bit | `analogReadResolution(12)`; scale by 3600/4096 mV |
| Firmware over ~815 KB fails to link with 1 MB of flash on the chip | SoftDevice takes `0x00000`–`0x26000`, InternalFS + bootloader take `0xED000`–`0x100000` | nothing to fix — that is the budget with this bootloader |
| RAM overflow at link time although PlatformIO reported plenty free | `upload.maximum_ram_size` in the board JSON disagrees with the linker script | the real limit is 237,568 B (`0x20006000`–`0x20040000`) |
| BLE peer will not reconnect after a firmware change | stale bond in InternalFS, which survives reflashing | `Bluefruit.Periph.clearBonds()`, or format InternalFS |
| I2C works on a breadboard, fails with longer wires | no pull-ups on the board; internal ones are ~13 kΩ | external 4.7 kΩ to 3.3 V |
| Board dies slowly after connecting a 5 V sensor | no pin is 5 V tolerant | level shifter |
| Timing drifts, or sleep current is higher than expected | `USE_LFRC` is the internal RC, ~250 ppm | populate the crystal and switch to `USE_LFXO` — only if the pads have one |
| Zephyr SPI sample builds and runs but nothing appears on the wire | upstream `spi2` pinctrl uses P1.01/P1.02/P1.07, none of which are on the header | board overlay remapping to P1.13 / P0.10 / P1.11 (§10) |
