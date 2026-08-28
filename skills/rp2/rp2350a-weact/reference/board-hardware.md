# WeAct Studio RP2350A Core Board — hardware reference

> Sources, all in the vendor repo `WeActStudio.RP2350ACoreBoard`:
> `RP2350A_V10/HDK/V10_SCH.pdf`, `RP2350A_V20/HDK/V20_SCH.pdf`, the two pinout
> cards (`V10_PinOUT.png`, `介绍 - 1.png`), the two dimension drawings
> (`尺寸图.pdf`), and `Doc/pico-2-datasheet.pdf`. RP2350 silicon facts come from
> the RP2350 datasheet and its errata.
>
> **Verified on hardware: RP2350A_V20 only** — blink on GP25 and USB-C upload.
> Everything specific to V1.0 is transcribed from `V10_SCH.pdf`; every voltage,
> current and ADC figure below is datasheet- or schematic-derived, not measured.

---

## 1. Overview

A 51.00 × 21.00 mm, 40-pin, 0.7"-row-pitch (17.78 mm) RP2350A core board — the
same footprint as a Raspberry Pi Pico 2, with USB-C instead of micro-B and
RESET and BOOT buttons instead of Pico's single BOOTSEL. Two revisions ship
under the same product name, and they are **not the same board**.

### Telling the revisions apart

Look at the top row of pins (the row that carries the power pins), or at the
buttons:

| | RP2350A_V10 | RP2350A_V20 |
|---|---|---|
| silk on back | `RP2350A_V10` | `RP2350A_V20` |
| second power pin | `5V` | `VSYS` |
| pin in the Pico ADC_VREF slot | `29` (GP29 = ADC3) | `VREF` (ADC_VREF) |
| user LEDs | **two**: `25@LED`, `24@LED` | **one**: `25@LED` |
| buttons | RESET, BOOT, **`23@KEY`** | RESET, BOOT |
| round test pads | none | `PG`, `29` |
| exposed GPIO | 27 (4 ADC) | 26 (3 ADC) |
| Pico 2 drop-in | **no** — 2 pins differ | yes, pin-for-pin |

The vendor's own summary: *V1x brings out every pin; V2x can completely
replace the official Pico 2.* Both carry the same RP2350A in QFN-60, the same
12 MHz crystal, and the same choice of 4 MB or 16 MB QSPI flash.

### Silicon

| | |
|---|---|
| Part | RP2350A, QFN-60, 30 GPIO (GP0–GP29) |
| Cores | 2 × Cortex-M33 @ up to 150 MHz **or** 2 × Hazard3 RISC-V, selected at boot |
| SRAM | 520 KB (8 × 64 KB striped banks + 2 × 4 KB), plus 8 KB OTP |
| Flash | none on-chip — external QSPI, XIP from `0x10000000` |
| Analog | 4-channel 12-bit ADC + on-die temperature sensor (channel 4) |
| Digital | 2 × UART, 2 × I2C, 2 × SPI, **24 PWM channels (12 slices)**, 3 × PIO (12 state machines), USB 1.1 device/host, HSTX |
| Security | TrustZone-M, signed boot, OTP-backed keys — unused by the Arduino core |

---

## 2. Pin map

The 40-pin header, numbered the Pico way: pin 1 is GP0 at the corner farthest
from the USB connector on the bottom row; pins 1–20 run along that row, 21–40
back up the top row, so pin 40 (`VIN`) is the top corner next to USB. The
silkscreen prints GPIO numbers, not pin numbers.

**Bottom row — identical on both revisions:**

| Pin | Silk | Function | Alt functions |
|---|---|---|---|
| 1 | `0` | GP0 | UART0_TX, I2C0_SDA, SPI0_RX |
| 2 | `1` | GP1 | UART0_RX, I2C0_SCL, SPI0_CSn |
| 3 | `G` | GND | |
| 4 | `2` | GP2 | I2C1_SDA, SPI0_SCK |
| 5 | `3` | GP3 | I2C1_SCL, SPI0_TX |
| 6 | `4` | GP4 | UART1_TX, I2C0_SDA, SPI0_RX — **`Wire` SDA** |
| 7 | `5` | GP5 | UART1_RX, I2C0_SCL, SPI0_CSn — **`Wire` SCL** |
| 8 | `G` | GND | |
| 9 | `6` | GP6 | I2C1_SDA, SPI0_SCK |
| 10 | `7` | GP7 | I2C1_SCL, SPI0_TX |
| 11 | `8` | GP8 | UART1_TX, I2C0_SDA, SPI1_RX — **`Serial2` TX** |
| 12 | `9` | GP9 | UART1_RX, I2C0_SCL, SPI1_CSn — **`Serial2` RX** |
| 13 | `G` | GND | |
| 14 | `10` | GP10 | I2C1_SDA, SPI1_SCK |
| 15 | `11` | GP11 | I2C1_SCL, SPI1_TX |
| 16 | `12` | GP12 | UART0_TX, I2C0_SDA, SPI1_RX, HSTX |
| 17 | `13` | GP13 | UART0_RX, I2C0_SCL, SPI1_CSn, HSTX |
| 18 | `G` | GND | |
| 19 | `14` | GP14 | I2C1_SDA, SPI1_SCK, HSTX |
| 20 | `15` | GP15 | I2C1_SCL, SPI1_TX, HSTX |

**Top row — the last two entries and pin 35 are where the revisions split:**

| Pin | Silk | Function | Alt functions |
|---|---|---|---|
| 21 | `16` | GP16 | UART0_TX, I2C0_SDA, SPI0_RX, HSTX — **`SPI` MISO** |
| 22 | `17` | GP17 | UART0_RX, I2C0_SCL, SPI0_CSn, HSTX — **`SPI` CS** |
| 23 | `G` | GND | |
| 24 | `18` | GP18 | I2C1_SDA, SPI0_SCK, HSTX — **`SPI` SCK** |
| 25 | `19` | GP19 | I2C1_SCL, SPI0_TX, HSTX — **`SPI` MOSI** |
| 26 | `20` | GP20 | I2C0_SDA |
| 27 | `21` | GP21 | I2C0_SCL |
| 28 | `G` | GND | |
| 29 | `22` | GP22 | |
| 30 | `RUN` | RUN — pull to GND to reset the chip | |
| 31 | `26` | GP26 / **ADC0** | I2C1_SDA — **`Wire1` SDA default** |
| 32 | `27` | GP27 / **ADC1** | I2C1_SCL — **`Wire1` SCL default** |
| 33 | `G` | AGND — analog ground, use it for ADC returns | |
| 34 | `28` | GP28 / **ADC2** | |
| 35 | V1.0 `29` → GP29 / **ADC3**<br>V2.0 `VREF` → ADC_VREF | |
| 36 | `3V3` | 3.3 V **output** | |
| 37 | `EN` | 3V3_EN — pull low to kill the 3.3 V rail | |
| 38 | `G` | GND | |
| 39 | V1.0 `5V` → LDO input, **3.6–6.5 V**<br>V2.0 `VSYS` → buck-boost input, **1.8–5.5 V** | |
| 40 | `VIN` | **USB VBUS**, ahead of the Schottky — on both revisions | |

> Pin 40's silkscreen says `VIN` on both boards and it is *not* the input pin:
> it is USB VBUS. The input is pin 39, and pin 39 means different things and
> takes different voltages on the two revisions. This is the single most
> expensive difference between them.

### GPIO that are not on the header

| GPIO | V1.0 | V2.0 |
|---|---|---|
| GP23 | `23@KEY` push-button, **active-LOW**, external 5.1 kΩ pull-up (R2) + 0 Ω series (R4) | SMPS `MODE`: LOW (100 kΩ pulldown, the default) = PFM, HIGH = forced PWM |
| GP24 | `24@LED`, blue, **active-HIGH** through 5.1 kΩ (R6) | VBUS sense: VBUS through 100 kΩ/100 kΩ (R28/R29) — HIGH when USB is plugged in |
| GP25 | `25@LED`, green, **active-HIGH** through 5.1 kΩ (R7) | same green LED (the only one) |
| GP29 | on the header as ADC3 | VSYS sense, round test pad `29` only |

All three GPIOs are exposed as bare test points (T2 = GP23, T3 = GP24,
T4 = GP25) on both revisions, and GP29 is T6 on V2.0.

Both user LEDs use a 5.1 kΩ series resistor, i.e. roughly 0.25 mA — they are
*dim*. A board whose LED "does not light" in daylight is usually working.

---

## 3. Connectors, buttons and test points

| Item | Where | Notes |
|---|---|---|
| USB-C | end of the board | USB 1.1 device/host, D+/D− through 27 Ω (R11/R12), CC1/CC2 through 5.1 kΩ (R8/R9) — sink-only, no PD |
| 40-pin header | both long edges | 2.54 mm pitch, rows 17.78 mm apart |
| SWD pads | 4 round pads at the end opposite USB | silk `3V3` / `DIO` / `CLK` / `GND` = 3V3, SWDIO, SWCLK, GND. **Not** the Pico's 3-pin arrangement |
| `RESET` | V1.0: left, next to BOOT · V2.0: right, next to the SWD pads | grounds RUN; no software visibility |
| `BOOT` | left, next to USB | grounds QSPI_SS through R15 1 kΩ. The bootrom samples it at power-up, but a running sketch can read it too — arduino-pico's `BOOTSEL` object floats flash CS and samples the pad (recipe 15). On V2.0 it is the only user button |
| `23@KEY` | right side, **V1.0 only** | GP23 to GND, 5.1 kΩ pull-up |
| `PWR` LED | next to USB | red, straight off the 3.3 V rail through 5.1 kΩ (R3) — lit means the rail is up |
| `PG` pad | **V2.0 only** | buck-boost power-good output (T5) |
| `29` pad | **V2.0 only** | GP29 / VSYS sense (T6) |
| `ADC_VREF` | V1.0: test point T1 · V2.0: T1 **and** header pin 35 | |

---

## 4. Power tree

### V1.0 — linear

```
USB-C VBUS ──┬── header pin 40 "VIN"
             │
            D1 Schottky (U3)
             │
   +5V net ──┴── header pin 39 "5V"  ← the input pin: 3.6 V … 6.5 V
             │   C1 10 µF, C2 0.1 µF
          U2 LDO (VIN / VSS / CE / VOUT), CE pulled up by R5 5.1 kΩ
             │
          +3V3 ── header pin 36, PWR LED, everything on board
                  rated 3.3 V @ 800 mA by the vendor's own card
```

Consequences of the LDO: **minimum input ≈ 3.6 V**, so a single Li-ion cell
browns out as it discharges, and every volt above 3.3 V is burned as heat
(6.5 V in at 300 mA = ~1 W in a SOT-23-5). No VSYS pin exists, and there is no
VSYS or VBUS sense wired to any GPIO.

### V2.0 — buck-boost, Pico 2 style

```
USB-C VBUS ──┬── header pin 40 "VIN"
             │      └─ R28/R29 100 kΩ divider → GP24 (VBUS sense)
            D1 Schottky
             │
   VSYS ─────┴── header pin 39 "VSYS"  ← the input pin: 1.8 V … 5.5 V
             │   C1 10 µF
             │   └─ R23/R24 100 kΩ divider → Q1 (FET, gate at +3V3)
             │        → GP29 with R25 100 kΩ to GND   (≈ VSYS/3)
             │   └─ R5 100 kΩ → 3V3_EN (header pin 37)
             │
          U2 buck-boost (EN / MODE / AGND / FB / PG / VOUT, L2 0.47 µH,
             FB divider R21 511 kΩ / R22 91 kΩ)
             │   MODE ← GP23 (R20 100 kΩ pulldown)
             │   PG   → test pad
             │
          +3V3 ── header pin 36, C4 22 µF, PWR LED, everything on board
```

Consequences of the buck-boost: a 1S Li-ion works all the way down, 3 × AA
works, and USB works, with no reconfiguration — but **6.5 V on pin 39 is over
the absolute maximum**, unlike on V1.0.

### True on both

- `3V3` (pin 36) is an **output**. Back-feeding it drives the regulator's
  output node and the RP2350 core supply directly.
- `EN` / 3V3_EN (pin 37) pulled low disables the regulator: the board looks
  completely dead, PWR LED off, with USB plugged in.
- `RUN` (pin 30) to GND is the reset, and is what the RESET button does.
- The RP2350 runs its **own** internal switching regulator for the 1.1 V core
  rail (VREG_VIN/VREG_LX/VREG_FB, L1 3.3 µH, C19/C20 4.7 µF). That is on-chip
  business — do not confuse it with the board regulator.
- GP26–GP29 are **not 5 V tolerant** and carry a reverse diode to 3V3: a live
  signal on an analog pin while the board is unpowered back-powers the board
  through the pin.

---

## 5. Clocks

| Source | Value | Notes |
|---|---|---|
| Y1 crystal | **12 MHz**, C22/C23 12 pF, R17 1 kΩ in series with XOUT | the only external clock; identical to Pico 2 |
| System PLL | 12 MHz → **150 MHz** | arduino-pico's `f_cpu` for `board = rpipico2`; the SDK default is also 150 MHz |
| USB PLL | 12 MHz → 48 MHz | |
| RTC / AON timer | derived, no 32.768 kHz crystal is fitted | a low-power always-on timebase has to come from software or an external part |

Anything above 150 MHz is overclocking. It usually works — the RP2350 has
headroom — but it is outside the published spec and must be labelled as such.

---

## 6. Memory map

| Region | Address | Size |
|---|---|---|
| Boot ROM | `0x00000000` | 32 KB, mask ROM — **cannot be erased**, so the board cannot be bricked |
| XIP flash | `0x10000000` | 4 MB (→ `0x103FFFFF`) or 16 MB (→ `0x10FFFFFF`) |
| SRAM | `0x20000000` | 520 KB |
| Peripherals | `0x40000000` | |
| OTP | via bootrom | 8 KB |

Flash layout the arduino-pico build produces (confirmed from `pio run`
output):

| | 4 MB board | 16 MB board |
|---|---|---|
| max sketch | 4,190,208 B | 16,773,120 B |
| EEPROM emulation (last 4 KB) | `0x103FF000` | `0x10FFF000` |
| filesystem | 0 B unless `board_build.filesystem_size` is set; carved out below the EEPROM sector | same |

**Nothing on the board says whether it has 4 MB or 16 MB.** The build only
knows what `platformio.ini` claims. Read the flash chip's JEDEC ID at runtime
(`template/src/main.cpp`, recipe 3). `picotool info -d` in BOOTSEL is a
cross-check, but its flash-size field comes from OTP that a third-party board
leaves unprogrammed, so it often reports nothing.

---

## 7. Vendor material

Everything the vendor ships is in `WeActStudio.RP2350ACoreBoard`:

| Path | What it is | What is wrong with it |
|---|---|---|
| `RP2350A_V*/HDK/V*_SCH.pdf` | full schematic | vector, but with no part numbers on U2/U3/U8 — regulator and flash are identified only by function |
| `RP2350A_V10/HDK/V10_PinOUT.png`, `RP2350A_V20/HDK/介绍 - 1.png` | pinout cards | the V2.0 card omits the `GP15` label on pin 20; GP15 does exist. Both cards say "16xPWM", which is the number of *usable* channels, not the RP2350's 24 |
| `RP2350A_V*/HDK/尺寸图.pdf` | dimensions | 51 × 21 mm, rows 17.78 mm apart |
| `RP2350A_V*/HDK/V*_3D.step` | 3D model | |
| `RP2350A_V*/Doc/*.pdf` | Raspberry Pi's *Pico 2 datasheet* and *Getting started with Pico* | generic Raspberry Pi documents — they describe the **Pico 2**, so their power-chain section is right for V2.0 and wrong for V1.0 |

There is no vendor SDK, BSP or example code. There is no PlatformIO board
definition for this board, and arduino-pico's `weact_rp2350b` variant is the
**RP2350B** core board (QFN-80, 48 GPIO, 8 ADC) — a different product.

---

# Part II — development guide

## 8. Toolchain and project configuration

PlatformIO + `platform = raspberrypi@1.20.0` (the first registry release
carrying RP2350 boards) + the earlephilhower arduino-pico core 6.0.0.

```ini
[env]
platform = raspberrypi@1.20.0
board = rpipico2                 ; V2.0 is pin-for-pin a Pico 2
framework = arduino
board_build.core = earlephilhower
board_build.filesystem_size = 0MB
monitor_speed = 115200
```

- `board_build.core = earlephilhower` is not optional here the way it is on
  RP2040. Plain `framework = arduino` selects the Arduino **Mbed** core, which
  has no RP2350 support at all.
- `board = rpipico2` is right for V2.0 by construction. It is also the best
  available choice for V1.0 — same silicon, same crystal, same flash wiring —
  but it defines `LED_BUILTIN = 25` and assumes VSYS/VBUS sense on GP29/GP24,
  none of which V1.0 has. Use `include/board.h`, not the variant's aliases.
- 16 MB boards need `board_upload.maximum_size = 16777216` and nothing else.
  `board_build.f_flash` is an ESP-ism; this platform ignores it.
- The build reports `Flash size / Sketch size / Filesystem size / EEPROM
  start` — read those lines to confirm the env matches the board.

The RISC-V (Hazard3) build target the RP2350 also supports is not reachable
through this PlatformIO platform; everything here is the Cortex-M33 path.

## 9. Clock configuration

Nothing to configure. `board = rpipico2` sets `f_cpu = 150000000L`, the core
brings the 12 MHz crystal up through the PLL in its startup code, and
`set_sys_clock_khz()` is available if you need to move it. There is no
`HSE_VALUE`-style constant to get wrong on this platform.

## 10. Peripheral cookbook

Defaults come from the `rpipico2` variant (`pins_arduino.h`):

| Object | Pins | Notes |
|---|---|---|
| `Serial` | USB CDC | **not** UART0. Exists only once the host opens the port |
| `Serial1` | GP0 TX / GP1 RX | header pins 1/2 |
| `Serial2` | GP8 TX / GP9 RX | header pins 11/12 |
| `Wire` | GP4 SDA / GP5 SCL | i2c0 |
| `Wire1` | GP26 SDA / GP27 SCL | i2c1 — **the same pins as A0/A1**. `Wire1.setSDA()/setSCL()` before `begin()` |
| `SPI` | GP16 MISO / GP19 MOSI / GP18 SCK / GP17 CS | spi0 |
| `SPI1` | GP12 MISO / GP15 MOSI / GP14 SCK / GP13 CS | spi1 |
| ADC | A0=GP26, A1=GP27, A2=GP28, A3=GP29 | `analogReadResolution(12)`; `analogReadTemp()` for the die sensor |
| PWM | `analogWrite()` on any GPIO | 8-bit @ 1 kHz by default; `analogWriteFreq()`, `analogWriteRange()` |

ADC reference: +3V3 through R18 100 Ω, with R19 1 Ω and C15 2.2 µF feeding
ADC_AVDD. The ADC's own supply current flows through that 100 Ω, so ADC_VREF
sits roughly 10–20 mV below 3.3 V and moves slightly when you sample. On V2.0
you can measure ADC_VREF on header pin 35; on V1.0 only on test point T1.

## 11. RP2350-specific gotchas

**Erratum RP2350-E9 — input-mode leakage.** A GPIO configured as an input with
its input buffer enabled leaks up to ~120 µA while the pad voltage sits between
logic levels (IOVDD 3.3 V). The internal pull-down (~50–80 µA) cannot win, so a
pin that should read LOW latches at roughly 2 V and reads HIGH — permanently,
until the pad is driven or the buffer is disabled. Consequences:

- `INPUT_PULLDOWN` is not usable as a button pull-down. Wire buttons
  active-LOW with `INPUT_PULLUP`, or fit an **external pull-down of 8.2 kΩ or
  less**.
- Floating unused inputs cost ~120 µA each. On a battery project that is the
  whole sleep budget.
- Raspberry Pi's other workaround is to enable the input buffer only around the
  read and disable it afterwards.

V1.0's `23@KEY` is active-LOW with a 5.1 kΩ pull-up and is unaffected.

**PWM slice aliasing is wider than on paper.** The RP2350 has 12 slices, but
`PWM_GPIO_SLICE_NUM(gpio)` is `(gpio >> 1) & 7` for GPIO < 32 — so on this
board (GP0–GP29) only slices 0–7 are reachable, and **four** GPIOs land on each
slice:

| Slice | GPIOs | |
|---|---|---|
| 0 | 0, 1, 16, 17 | GP0 and GP16 are the *same channel* (A) |
| 1 | 2, 3, 18, 19 | |
| 2 | 4, 5, 20, 21 | |
| 3 | 6, 7, 22, 23 | |
| 4 | 8, 9, 24, 25 | |
| 5 | 10, 11, 26, 27 | |
| 6 | 12, 13, 28, 29 | |
| 7 | 14, 15 | the only slice without an alias pair |

Channel is `gpio & 1`. Two pins sharing a slice share the counter and therefore
the frequency; two pins sharing a *channel* (n and n+16) share the duty
register as well, so `analogWrite(0, x)` and `analogWrite(16, y)` fight over
one output and the last write wins for both.

**Flash writes stall both cores.** Code executes from the same QSPI chip via
XIP, so `EEPROM.commit()`, LittleFS writes and `flash_range_program()` pause
the whole processor. Keep them out of timing-critical windows.

**`flash_do_cmd()` must run early.** It stops XIP to talk to the flash chip.
The SDK's rule is to call it during startup with interrupts off, before core 1
or any flash-resident interrupt handler can run. Calling it from a running
application hangs the chip.

**UF2 files are not interchangeable with RP2040.** The RP2350 ARM-S UF2 family
ID differs, the bootrom drive is named `RP2350` rather than `RPI-RP2`, and an
RP2040 `.uf2` copied onto it is rejected rather than run.

## 12. Flashing and recovery

Normal path, nothing pressed:

```sh
pio run -t upload -t monitor
```

PlatformIO opens the USB CDC port at 1200 baud; a running arduino-pico sketch
sees the touch and reboots itself into BOOTSEL; picotool flashes and reboots.
The board comes back as a CDC port (`/dev/cu.usbmodem*`, `COMx`) within a
second — VID:PID `2E8A:000F`.

When the firmware cannot respond — crashed, USB disabled, power-only cable, or
a blank board:

1. hold **BOOT** (the button next to the USB connector),
2. plug or replug the USB-C cable,
3. release BOOT — a USB drive named **`RP2350`** appears,
4. re-run `pio run -t upload`, or drag
   `.pio/build/<env>/firmware.uf2` onto the drive; the board reboots when the
   copy finishes.

BOOT is sampled by the bootrom at power-up only: pressing it while the board
runs does nothing (it grounds the flash chip select). RESET + BOOT together —
hold BOOT, tap RESET, release BOOT — reaches the same place without unplugging.

The boot ROM is mask ROM: nothing you flash can stop the BOOT button from
working, so recovery is always the sequence above. `pio run -t erase` is
*not* the RP2350 answer — it copies PlatformIO's bundled `flash_nuke.uf2`,
whose UF2 blocks carry the RP2040 (`0xE48BFF56`) and absolute (`0xE48BFF57`)
family IDs with no RP2350 ARM-S family, and the bundled picotool 2.0.0 has no
`erase` subcommand at all. For a true wipe, build `flash_nuke` for RP2350 from
pico-examples and drop that `.uf2` on the drive; in practice, flashing a
known-good sketch over the top solves what an erase would.

The alternative route is SWD on the four pads at the far end (`3V3`, `DIO`,
`CLK`, `GND`) with a Debug Probe, a second Pico running picoprobe, or any
CMSIS-DAP adapter (`upload_protocol = cmsis-dap` / `picoprobe`). Needed only
for live debugging — never for recovery.

## 13. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Board dies the moment it is powered from a bench supply on the second pin | V1.0 LDO fed above 6.5 V, or V2.0 VSYS fed above 5.5 V — the two revisions take different ranges on the same physical pin | Check the silk: `5V` = 3.6–6.5 V, `VSYS` = 1.8–5.5 V |
| Runs on USB, browns out on a 1S Li-ion | V1.0's LDO drops out below ~3.6 V | V1.0 needs ≥ 3.6 V; only V2.0's buck-boost runs a Li-ion down |
| Board completely dead, PWR LED off, USB connected | `EN` (pin 37) shorted low | Release 3V3_EN |
| 5 V applied to pin 40 `VIN` does nothing useful | Pin 40 is USB VBUS, ahead of the Schottky, not the supply input | Feed pin 39 |
| Blink sketch "does not work" | The LEDs run through 5.1 kΩ and are very dim | Look at it in shade before debugging code |
| `Serial.print` output never appears on a USB-serial adapter on GP0/GP1 | `Serial` is USB CDC; GP0/GP1 are `Serial1` | Use `Serial1`, or open the CDC port |
| First lines of output are missing | CDC bytes printed before the host opens the port are dropped | Re-print the banner from `loop()`; never `while (!Serial)` |
| A button wired to 3V3 with `INPUT_PULLDOWN` always reads HIGH | Erratum RP2350-E9 input leakage latches the pad at ~2 V | External ≤ 8.2 kΩ pull-down, or wire active-LOW with `INPUT_PULLUP` |
| `analogRead()` saturates at 1023 | Default resolution is 10-bit | `analogReadResolution(12)` |
| I2C on `Wire1` corrupts as soon as `analogRead()` runs | `Wire1` defaults to GP26/GP27 = A0/A1 | `Wire1.setSDA()/setSCL()` to another i2c1 pair before `begin()` |
| Servo/stepper pair jitters, or one `analogWrite` overwrites another | PWM slice/channel aliasing — 4 GPIOs per slice, n and n+16 share a channel | See §11's slice table and move a pin |
| Sketch links fine, then the board hangs or corrupts flash past ~4 MB | 16 MB env selected on a 4 MB board | Read the JEDEC ID (recipe 3), then pick the matching env |
| `LittleFS.begin()` returns false | `board_build.filesystem_size` defaults to `0MB` | Set it, then `pio run -t uploadfs` once |
| EEPROM values vanish on reboot | `EEPROM.commit()` never called | `put()` stages, `commit()` burns the sector |
| `Cannot find BOOTSEL disk` during upload | The running firmware cannot do the 1200-baud reboot | Hold BOOT, replug USB, re-run |
| An RP2040 `.uf2` dropped on the drive is ignored | Wrong UF2 family; the drive is `RP2350`, not `RPI-RP2` | Rebuild for RP2350 |
| `flash_do_cmd()` hangs the board | Called from a running application instead of during startup | Call it first in `setup()`, interrupts off |
| Wrong pins used for LED2/KEY/VSYS | The revisions differ on GP23/GP24/GP29 | Build with `-DBOARD_REV=10|20`; `board.h` refuses to compile without it |
