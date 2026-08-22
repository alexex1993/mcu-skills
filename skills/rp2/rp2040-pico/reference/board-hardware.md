# Raspberry Pi Pico hardware reference

Part I is the board as wired (from the Pico datasheet Rev 3 and the RP2040
datasheet RP2040-DS-1.4.3+); Part II is the development guide. Sources:
`RP2040.PDF` = Raspberry Pi Pico Datasheet (board-level, Rev 2.2,
2023-06-14); `RP-008371-DS-1-rp2040-datasheet.pdf` = RP2040 chip datasheet.

## 1. Overview

| | |
|---|---|
| Board | Raspberry Pi Pico, SC0915 (also SC0916), Rev 1–3 share this pinout |
| MCU | RP2040, QFN-56 — 2× ARM Cortex-M0+ @ **133 MHz** max (arduino-pico runs 133 MHz; the Pico SDK default is 125 MHz) |
| Memory | 264 KB SRAM in 6 banks (4×64 KB striped + 2×4 KB); **no internal flash** — 2 MB Winbond W25Q16JV on QSPI, XIP + 16 KB cache |
| Logic levels | 3.3 V fixed (IOVDD tied to the 3V3 rail); default drive 4 mA, 12 mA max per pin |
| LED | GPIO25, **active-HIGH**, not on the header (TP5 test point only) |
| Button | **BOOTSEL** — only sampled at power-up; not a runtime user button |
| USB | micro-B, USB 1.1 Full Speed, device or host; PHY on-chip, 27 Ω series resistors on board |
| ADC | 12-bit, 500 ksps, 4 channels on GPIO26-29 + channel 4 = die temperature sensor |
| PWM | 16 channels (8 slices × A/B); every GPIO 0-29 can be a PWM output |
| Debug | 3-pin SWD header on the bottom edge (SWCLK, GND, SWDIO), ~60 kΩ internal pull-ups |
| Size | 51 × 21 mm, 40-pin 2.54 mm DIP + castellations, usable as an SMT module |

## 2. Pin map

Physical numbering (datasheet Figure 4): USB at the top, pin 1 top-left,
pins 1-20 down the left edge, pins 21-40 back up the right edge (pin 40 is
top-right, next to the USB connector). GPIO names are the silicon GPIO
numbers — the Arduino core uses the same numbers.

### 2.1 Left edge, pins 1-20 (top to bottom)

| Pin | Signal | Alt functions worth knowing |
|---|---|---|
| 1 | GPIO0 | UART0 TX (`Serial1` default), I2C0 SDA, SPI0 RX, PWM0 A |
| 2 | GPIO1 | UART0 RX (`Serial1` default), I2C0 SCL, SPI0 CSn, PWM0 B |
| 3 | GND | |
| 4 | GPIO2 | SPI0 SCK, I2C1 SDA, PWM1 A |
| 5 | GPIO3 | SPI0 TX(MOSI), I2C1 SCL, PWM1 B |
| 6 | GPIO4 | **I2C0 SDA (`Wire` default)**, UART1 TX, PWM2 A |
| 7 | GPIO5 | **I2C0 SCL (`Wire` default)**, UART1 RX, PWM2 B |
| 8 | GND | |
| 9 | GPIO6 | I2C1 SDA, SPI0 SCK, PWM3 A |
| 10 | GPIO7 | I2C1 SCL, SPI0 TX, PWM3 B |
| 11 | GPIO8 | **UART1 TX (`Serial2` default)**, I2C0 SDA, PWM4 A |
| 12 | GPIO9 | **UART1 RX (`Serial2` default)**, I2C0 SCL, PWM4 B |
| 13 | GND | |
| 14 | GPIO10 | SPI1 SCK, I2C1 SDA, PWM5 A |
| 15 | GPIO11 | SPI1 TX, I2C1 SCL, PWM5 B |
| 16 | GPIO12 | **SPI1 MISO (default)**, UART0 TX, PWM6 A |
| 17 | GPIO13 | **SPI1 CS (default)**, UART0 RX, PWM6 B |
| 18 | GND | |
| 19 | GPIO14 | SPI1 SCK, I2C1 SDA, PWM7 A |
| 20 | GPIO15 | SPI1 TX, I2C1 SCL, PWM7 B; borrowed ~800 µs by the USB enumeration fix |

### 2.2 Right edge, pins 21-40 (bottom to top)

| Pin | Signal | Notes |
|---|---|---|
| 21 | GPIO16 | **SPI0 MISO (default)**, UART0 TX, PWM0 A |
| 22 | GPIO17 | **SPI0 CS (default)**, UART0 RX, PWM0 B |
| 23 | GND | |
| 24 | GPIO18 | **SPI0 SCK (default)**, I2C1 SDA, PWM1 A |
| 25 | GPIO19 | **SPI0 MOSI (default)**, I2C1 SCL, PWM1 B |
| 26 | GPIO20 | PWM2 A, CLOCK GPIN0 |
| 27 | GPIO21 | PWM2 B, CLOCK GPOUT0 |
| 28 | GND | |
| 29 | GPIO22 | PWM3 A, CLOCK GPIN1 |
| 30 | **RUN** | RP2040 reset, active-low, ~50 kΩ internal pull-up — the "reset button" |
| 31 | GPIO26 | **A0**, ADC0, PWM5 A, I2C1 SDA — not 5 V tolerant |
| 32 | GPIO27 | **A1**, ADC1, PWM5 B, I2C1 SCL — not 5 V tolerant |
| 33 | **AGND** | analog ground for GPIO26-29, own ground plane; tie to GND if ADC unused |
| 34 | GPIO28 | **A2**, ADC2, PWM6 A — not 5 V tolerant |
| 35 | **ADC_VREF** | ADC supply/reference = 3V3 through 200 Ω + 2.2 µF RC filter; external shunt ref goes here |
| 36 | **3V3** | SMPS output, powers RP2040 + external circuitry, keep external load < 300 mA |
| 37 | **3V3_EN** | SMPS enable, 100 kΩ pull-up to VSYS; short low to de-power the board |
| 38 | GND | |
| 39 | **VSYS** | 1.8-5.5 V system input, feeds the buck-boost SMPS |
| 40 | **VBUS** | 5 V from the USB connector through D1; feed 5 V here for USB host mode |

Full GPIO function matrix (every GPIO → its 9 alt functions): RP2040
datasheet §1.4.3 Table 2. The pattern: SPI0/UART0/I2C0 reappear every 8
pins, SPI1/UART1/I2C1 every 8 pins offset by 4; PWM slice = GPIO/2, channel
A = even pin, B = odd pin.

### 2.3 GPIOs that exist in code but not on the header

| GPIO | Board function | Consequence |
|---|---|---|
| 23 | SMPS power-save (RT6150 PS pin), output | drive HIGH to force PWM mode → less ripple, worse light-load efficiency. Leave alone unless chasing ADC noise |
| 24 | VBUS sense, input | HIGH while USB VBUS present; free input otherwise (nothing on header) |
| 25 | user LED | `LED_BUILTIN`, active-HIGH |
| 29 | **A3 = VSYS/3** through 100k/200k divider | `analogRead(A3)` × 3 = VSYS volts; not usable externally |

Test points (SMT underside): TP1 GND, TP2 USB DM, TP3 USB DP, TP4 GPIO23
(do not use), TP5 GPIO25/LED (output-only, swings 0 → LED Vf), TP6 BOOTSEL
(short low at power-up = BOOTSEL mode without the button).

## 3. Power tree

```
USB VBUS (5 V) ──D1 Schottky──┬── VSYS (1.8-5.5 V) ── RT6150 buck-boost ── 3V3
external supply ──diode/FET──┘        │                        ├── RP2040 (core LDO → 1.1 V)
                                GPIO24 senses VBUS            ├── W25Q16JV flash
                                R5/R6 divide VSYS/3 → A3      ├── ADC_VREF (200 Ω + 2.2 µF)
                                                              └── 3V3 header pin (< 300 mA out)
```

- USB-only setups may short VBUS (pin 40) to VSYS (pin 39) to remove the
  D1 diode drop.
- Battery: single Li-ion (3.0-4.2 V) or 3×AA into VSYS through a diode or
  P-FET for ORing (datasheet §4.5); bare VSYS range 1.8-5.5 V.
- USB host mode: the port needs 5 V on VBUS — powering only VSYS leaves the
  host port dead.
- The 3V3 pin is an output. Driving it from an external rail bypasses the
  SMPS and back-powers everything, including out-of-spec if > 3.3 V.
- 3V3_EN shorted low = board completely dead-looking. Check it before
  declaring a Pico bricked.
- Typical draw (datasheet §3.1): ~9 mA BOOTSEL idle, ~1.3 mA SLEEP, ~0.8 mA
  DORMANT, ~90 mA with the Popcorn VGA demo.

## 4. Clock sources

| Source | Value | Notes |
|---|---|---|
| XOSC crystal | **12 MHz** | the USB bootloader requires it |
| PLL_SYS | 125 MHz (SDK default) / 133 MHz (arduino-pico F_CPU) | on-chip PLL from XOSC |
| PLL_USB | 48 MHz | USB and ADC clocking |
| ROSC | ~6.5 MHz, imprecise | runs at boot until PLLs lock; COUNT registers unusable (erratum E7) |

F_CPU is set by the board definition (133 MHz). Overclocking to 240 MHz is
community-common but **outside spec** — arduino-pico bumps the core
regulator above 133 MHz automatically; treat anything over 133 MHz as
overclocking and say so when reporting.

## 5. Memory map

| Region | Address | Size | Notes |
|---|---|---|---|
| Boot ROM | `0x00000000` | 16 KB | mask ROM, USB bootloader lives here, cannot be overwritten |
| XIP flash | `0x10000000` | 2 MB | code executes in place; no-cache/-no-alloc aliases at `0x1100…`/`0x1200…`/`0x1300…` |
| — EEPROM emulation | `0x101ff000` | 4 KB | last flash sector, arduino-pico EEPROM + filesystem default |
| SRAM0-3 striped | `0x20000000` | 256 KB | interleave optimized for dual-core |
| SRAM4, SRAM5 | `0x20040000`, `0x20041000` | 4 KB each | non-striped; SRAM5 = core 1 stack by default |
| SRAM non-striped alias | `0x21000000` | 256 KB | SRAM0-3 without interleave |
| XIP cache as SRAM | `0x15000000` | 16 KB | usable as RAM with caching disabled (erratum E9 applies) |
| APB peripherals | `0x40000000`+ | — | UART0 `0x40034000`, SPI0 `0x4003c000`, I2C0 `0x40044000`, ADC `0x4004c000`, PWM `0x40050000`, TIMER `0x40054000` … |
| AHB peripherals | `0x50000000`+ | — | DMA `0x50000000`, USB `0x50100000` |
| SIO (GPIO, FIFOs) | `0xd0000000` | — | single-cycle GPIO window |

Flash layout from `0x10000000`: 256 B boot2 (W25Q080 second-stage
bootloader), then the program; PlatformIO reports a maximum sketch size of
2,093,056 B with the last 4 KB reserved.

## 6. Errata that matter on this board

Full list: RP2040 datasheet Appendix B. All silicon in retail Picos is B2,
which already fixes E2 and E5 in hardware; older B0/B1 boards exist in the
wild.

| Erratum | Summary | What you see |
|---|---|---|
| E5 (B0/B1 only) | USB device never exits RESET on a busy bus | board powered, LED works, no COM port; plug in directly, away from chatty hub neighbours |
| E6 | digital inputs on GPIO26-29 not disabled by default for ADC | SDK/core disables them early; custom bare-metal must too |
| E11 | ADC DNL error peaks at codes 512, 1536, 2560, 3584; ENOB 8.7 bits | histogram spikes, missing codes — not your wiring |
| E13 | DMA abort clears early, spurious IRQ | poll CTRL.BUSY after abort, not ABORT |
| E7/E10 | ROSC/XOSC COUNT and ROSC BADWRITE unusable | do not use for timing |
| E9 | ROM cannot boot straight into cache-as-SRAM | re-disable XIP cache in main SRAM first |
| E1 | watchdog tick counts twice | WDT timeouts halve vs. programmed value |

# Part II — development guide

## 7. Toolchain and project configuration

Verified combination: **PlatformIO 6.x, platform-raspberrypi 1.19.0,
arduino-pico 5.6.0** (`framework-arduinopico 1.50600.0`), gcc-arm-none-eabi.

```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino
board_build.core = earlephilhower   ; see below — this line is load-bearing
monitor_speed = 115200
```

**The core choice is the biggest silent divergence in this platform.**
`framework = arduino` with no `board_build.core` builds against the Arduino
**Mbed** core (`framework-arduino-mbed`), not the earlephilhower
arduino-pico core that the Pico ecosystem (books, tutorials, Raspberry Pi's
own docs) assumes. Differences you will hit:

| | earlephilhower (`board_build.core = earlephilhower`) | Mbed (default when omitted) |
|---|---|---|
| `Serial` | USB CDC (TinyUSB); `Serial1`/`Serial2` hardware UARTs | USB CDC; `Serial1` only, no Serial2 |
| PWM | any GPIO, 8-bit @ 1 kHz default, `analogWriteFreq/Range/Resolution` | any GPIO, 500 Hz default, **no frequency API** |
| EEPROM | flash-emulated, `begin/put/commit` | **no EEPROM library** |
| I2C | `Wire` (4/5) and `Wire1` (default 26/27, remappable via setSDA/setSCL) | `Wire` (4/5) only |
| Blink cost | ~58 KB flash / ~8.7 KB RAM | ~4 KB flash / ~40 KB RAM |
| bundled extras | FreeRTOS (SMP), LittleFS, FatFS, I2S, Encoder… | Portenta/Nicla-oriented libraries |

Both cores put `LED_BUILTIN` on GPIO25 and A0-A3 on GPIO26-29, and both
reboot into BOOTSEL on a 1200-baud touch. The Mbed core additionally
supports the double-tap reset → bootloader (via RUN, pin 30). This skill's
template and recipes use the earlephilhower core.

The Pico SDK itself is available as `framework = picosdk` when you want
plain C/C++ without Arduino.

## 8. Peripheral cookbook

### 8.1 USB CDC serial

`Serial` is the USB CDC port — enumeration starts when the host opens the
port, and prints made before that are dropped. Do not gate startup on
`while (!Serial)`; print state periodically instead. `Serial.begin()`
takes a baud argument for compatibility, ignores it. `Serial1` (UART0 on
GPIO0/1) and `Serial2` (UART1 on GPIO8/9) are real UARTs with hardware
FIFOs.

### 8.2 ADC

- `analogRead(A0..A2)` — 10-bit **by default**; `analogReadResolution(12)`
  for native 12 bits. `analogReadTemp()` returns die temperature in °C.
- `analogRead(A3)`/`analogRead(29)` reads VSYS/3 — multiply by 3.
- Reference = ADC_VREF = 3V3 through an RC filter: ratiometric to the SMPS,
  ~30 mV built-in offset (150 µA × 200 Ω), ±1-2% absolute error, ENOB 8.7
  bits (see SKILL.md analog section before trusting absolute numbers).
- GPIO26-29 are not 5 V tolerant: absolute maximum VDDIO + 0.3 V.

### 8.3 PWM

`analogWrite(pin, 0..255)` on any GPIO, 1 kHz default. `analogWriteFreq()`
accepts 100 Hz-10 MHz (clamped outside); `analogWriteResolution(2..16)`.
PWM slice = `pin / 2`: GPIO pairs 0&1, 2&3 … 28&29 share one counter, so
they share frequency (and only A/B duty is independent). Servo library
available (`lib_deps = arduino-libraries/Servo`).

### 8.4 I2C / SPI

`Wire` = i2c0 on 4/5 by default; `Wire1` = i2c1 defaulting to 26/27 — which
are A0/A1 — remap with `Wire1.setSDA(18); Wire1.setSCL(19);` style calls
before `begin()` (any valid pair from the §2 table: I2C1 fits 2/3, 6/7,
10/11, 14/15, 18/19, 22/23, 26/27). `SPI` = spi0 on 17/18/19/16; the
earlephilhower core also exposes a `SPI1` object on 13(SS)/14(SCK)/15(MOSI)/
12(MISO), the Mbed core only one SPI.

### 8.5 EEPROM

Flash emulation in the last 4 KB sector: `EEPROM.begin(n)` maps a shadow,
`put()` stages (update semantics — compares first), **`commit()` burns**;
`get()` never wears. A commit with changes = one 4 KB sector erase; the
W25Q16JV is rated 100 k P/E cycles. Nothing persists without `commit()` —
silently.

### 8.6 Interrupts, dual core, concurrency

- `attachInterrupt()` works on **every** GPIO, edge or level, with optional
  `FALLING/RISING/CHANGE` — no PCINT-style restrictions.
- Define `setup1()`/`loop1()` and the core launches your code on **core 1**
  (nothing runs there otherwise; core 0 runs Arduino). Communication via
  `rp2040.fifo.available()/read()/write()` (each direction 8-deep) or
  shared memory + your own sync. FreeRTOS (SMP) ships with the core:
  `#include <FreeRTOS.h>` and both cores scheduler-run.
- Flash writes (EEPROM.commit, LittleFS) execute-from-the-same-QSPI-flash:
  the writing core stalls until the sector operation completes; keep them
  out of timing-critical windows.

### 8.7 PIO

2 blocks × 4 state machines, 32 instructions each, on any GPIO. Available
from Arduino via `#include <hardware/pio.h>` + `.pio` files (assembled by
the `pioasm` tool — PlatformIO handles `.pio` sources automatically in the
earlephilhower core). Use for WS2812, VGA, SDIO, precise WS2812-grade
timing — anything bit-banging can't hold. Reference: RP2040 datasheet §3.6.

## 9. Flashing and recovery

1. **Normal (no button):** `pio run -t upload`. PlatformIO opens the CDC
   port at 1200 baud; the running sketch calls `reset_usb_boot()` and
   re-enters BOOTSEL; picotool writes flash and reboots the board.
2. **Firmware crashed / USB dead:** hold **BOOTSEL**, plug (or replug) the
   USB cable, release. A mass-storage drive `RPI-RP2` appears (~128 MB
   reported). Either re-run `pio run -t upload` (picotool finds the device
   in BOOTSEL), or drag `.pio/build/pico/firmware.uf2` onto the drive; the
   board reboots into the new firmware when the copy finishes.
3. **Full erase:** `pio run -t erase` copies `flash_nuke.uf2` — erases all
   2 MB including EEPROM and any filesystem. The board re-enumerates as
   `RPI-RP2` when done.
4. **SWD:** the 3-pin header (SWCLK, GND, SWDIO) with a second Pico
   flashed as Picoprobe, or any CMSIS-DAP probe. `upload_protocol =
   picoprobe` (or `cmsis-dap`), `debug_tool = picoprobe`. SWD can also
   rescue a board whose flash is thoroughly wedged — the boot ROM is mask
   ROM and survives everything: **no firmware can permanently brick a
   Pico**.

The monitor: `pio device monitor` attaches to the CDC port
(`/dev/cu.usbmodem*`, `COMx`); baud ignored.

## 10. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| Upload: `Cannot find BOOTSEL disk` / picotool finds nothing | previous firmware crashed or disabled USB | hold BOOTSEL + replug USB, re-run upload |
| `EEPROM.h: No such file or directory` | building against the Mbed core (`board_build.core` missing) | add `board_build.core = earlephilhower` |
| nothing on the USB-serial adapter wired to GPIO0/1 | `Serial` is USB CDC, not UART0 | use `Serial1.begin(115200)` |
| prints appear only after opening the monitor, earlier ones lost | CDC drops bytes until the host opens the port | print periodically from `loop()`, don't banner once in `setup()` |
| analogRead never exceeds 1023 | default resolution is 10-bit | `analogReadResolution(12)` |
| two PWM pins fight / duty looks wrong on the odd pin | both pins share a PWM slice (`pin/2`) | pick pins on different slices, or set duty on both |
| I2C1 works until ADC reads start, then garbage | Wire1 default pins 26/27 are A0/A1 | `Wire1.setSDA()/setSCL()` to e.g. 18/19 |
| board runs with nothing plugged into USB | voltage on GPIO26-29 back-powers 3V3 through the ADC diode | check external wiring on A0-A2 / AGND |
| board dead, no LED, not in BOOTSEL | 3V3_EN shorted low, or VSYS under 1.8 V | release 3V3_EN (pin 37), check supply on VSYS |
| USB port never enumerates on a hub | erratum E5 on B0/B1: busy transaction translator | plug direct or away from busy devices |
| A3 always reads ~1.65 V (code 512-ish) | A3 is VSYS/3, not a free pin | ×3 for VSYS; use A0-A2 for signals |
| `while(!Serial)` sketch "hangs" at boot | waits for the host to open the port | remove the wait, or open the monitor |
| WDT reset happens at half the programmed timeout | erratum E1 | program 2× the desired timeout |
| board not detected as RPI-RP2 despite BOOTSEL | USB cable is power-only | use a data cable |
