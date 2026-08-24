---
name: esp32c3-oled042
description: Firmware development for the ESP32-C3 0.42" OLED board (ESP32-C3FH4, QFN32) — the ABRobot / 01Space "ESP32-C3-0.42LCD" mini board and its clones, sold as "ESP32-C3 SuperMini with OLED" and "ESP32 C3 OLED development board". Covers its 72x40 SSD1306 OLED on I2C, the single I2C bus, blue LED on GPIO8, BOOT button on GPIO9, USB Serial/JTAG console, ADC1, Wi-Fi/BLE, and the ESP-IDF + PlatformIO setup around them. Use when working on this board or a bare ESP32-C3: project setup, platformio.ini and sdkconfig, pin mapping and strapping pins, getting the 0.42-inch display to show something other than snow, U8g2 offsets, adding a second I2C device, flashing over USB-C, or debugging why something on the board does not work.
---

# ESP32-C3 0.42" OLED (ESP32-C3FH4)

A 25 × 20 mm ESP32-C3 board with a 0.42-inch OLED glued to it. Two things make
it worth a skill: **the panel is a 72 × 40 window inside a 128 × 64 controller
and does not start at column 0**, and **the C3 has exactly one I2C controller,
which the panel occupies**. Almost every failure on this board traces back to
one of those two facts, and neither is visible from the code.

There is no vendor datasheet for the board. The detail below is assembled from
the schematic, the Zephyr port, published bring-ups and a working project —
read the reference files rather than guessing.

- `reference/board-hardware.md` — the board reference: pin table, reset-time
  states, strapping analysis, free pins, peripheral availability, power tree,
  memory and flash layout, and the display's quirks in full (Part I), **plus**
  a development guide (Part II: §10 toolchain, §11 sdkconfig, §12 flashing,
  §13 peripheral cookbook, §14 symptom → cause → fix table, §15 sources).
- `reference/recipes.md` — copy-paste code: `platformio.ini`,
  `sdkconfig.defaults`, `board.h`, the OLED init sequence, the I2C bus, frame
  flushing, a second I2C device, bus scan, LED, BOOT button, ADC, UART, the
  U8g2 constructors, font regeneration.
- `reference/esp32c3-soc.md` — the ESP32-C3 datasheet digest, for chip-level
  questions: pin and IO MUX tables, analog functions, boot straps, memory,
  every peripheral's limits, electrical characteristics, current consumption.
- `template/` — a **project that builds clean**, in two variants, plus a
  scaffold script. See `template/README.md`.

## Orientation

| | |
|---|---|
| SoC | ESP32-C3FH4, RISC-V RV32IMC @ 160 MHz, **QFN32, 22 GPIO, no FPU** |
| Memory | **4 MB** in-package flash (1 MB default app partition), 400 KB SRAM of which 16 KB is cache → ~320 KB linkable |
| Clocks | 40 MHz main crystal. **No 32.768 kHz crystal** — RTC slow clock is the internal RC, and that is the correct default |
| Display | SSD1306, **72 × 40 visible**, I2C **0x3C**, **SDA GPIO5 / SCL GPIO6**, 400 kHz. Multiplex ratio 40, **column offset 28** |
| LED | one blue LED on **GPIO8**, **LOW = lit** |
| Buttons | **BOO/BOOT** GPIO9, **pressed = low** · **RST** acts on `CHIP_EN`, not readable in software |
| USB | USB-C straight to the SoC's USB Serial/JTAG on GPIO18/19. **No bridge chip.** Console, flashing and JTAG all share it |
| Free pins | GPIO0, 1, 2, 3, 4, 7, 10, 20, 21 on the headers. GPIO0–GPIO4 are also the only usable ADC channels |
| Power | USB 5 V → 1N5819 → ME6211C33 LDO → 3V3. The `5V` header pin is **VUSB, before the diode** |
| Radio | Wi-Fi 802.11 b/g/n + BLE 5, ceramic chip antenna, nothing to configure |
| Toolchain | PlatformIO + `platform-espressif32` 7.0.1 + ESP-IDF 6.0.1, `board = esp32-c3-devkitm-1` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`0xA8, 0x27` — multiplex ratio 40, not 64.** The panel bonds 40 rows of
   the SSD1306's 64 to glass. Initialised as a 128 × 64, the controller scans
   rows that do not exist and the screen fills with unstable noise. That "snow"
   reads as a dead panel or a bad I2C bus, and it is neither.
2. **Column offset 28.** Panel column 0 is GDDRAM column 28, written into the
   page-address command for every page. Without it the image is shifted and
   wraps at the edge. Only the *column* needs offsetting — once the multiplex
   ratio is 40, row 0 is row 0.
3. **Wipe all eight pages × 128 columns once at init.** You only ever write 72
   of 128 columns; the rest keeps its power-on garbage, and it shows as junk
   pixels along the panel edge. This is a separate bug from rule 1 with the
   same appearance.
4. **Send `0xAF` (display on) *after* the first blank frame, not in the init
   sequence.** Otherwise every boot shows a fraction of a second of random
   GDDRAM before your first frame lands.
5. **SDA = GPIO5, SCL = GPIO6.** The arduino-esp32 variant header says GPIO8/9,
   and several published pinout images repeat it. That is the SoC default, not
   this board — GPIO8 and GPIO9 here are the LED and the BOOT button. Confirmed
   three ways: the schematic, a user who unglued the panel, and working code.
6. **`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`** in `sdkconfig.defaults`. Verified:
   without it the generated sdkconfig says `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`
   and the console goes to UART0 on GPIO20/21, which this board brings out to
   the header and connects to nothing. The monitor is silent while the firmware
   runs perfectly, and you debug blind.
7. **`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`.** Verified: ESP-IDF defaults to 2 MB
   regardless of the PlatformIO board definition's `flash_size`. Nothing fails
   at first — the trap springs later, when OTA or a filesystem will not fit in
   flash that physically exists.
8. **There is one I2C controller and the panel is on it.** Never call
   `i2c_new_master_bus()` twice; add a device to the existing bus. External I2C
   devices work fine on GPIO5/GPIO6 at a different address — two independent
   users confirm this — but they share the panel's bus and its pull-ups.
9. **The LED is active-low.** `gpio_set_level(8, 0)` lights it. `LED_BUILTIN`
   exists in the Arduino C3 variant and does **not** point at GPIO8.
10. **GPIO2 is a strapping pin and it is a free header pin.** It must be high
    at reset. Nothing on the board guards it, so a circuit you attach that
    holds it low stops the board booting — with no symptom other than silence.
11. **Never instantiate `Serial1` in Arduino code on this chip.** The
    arduino-esp32 core places it on GPIO18/19, which are the USB D−/D+ lines.
    The code compiles, and the USB serial port disappears from the host
    entirely. Core bug, not a board fault.
12. **Never burn `EFUSE_UART_PRINT_CONTROL` or `EFUSE_DIS_USB_SERIAL_JTAG`.**
    The first ties the boot log to GPIO8's level at reset — and GPIO8 here is
    the LED. The second removes the only console and the only flashing route
    this board has. eFuse bits are one-time programmable.
13. **ADC2 is unusable.** Its only channel is GPIO5, the panel's SDA line, and
    ADC2 is non-functional on some C3 revisions per the SoC errata. Five ADC1
    channels on GPIO0–GPIO4 are what this board has.
14. **Deep sleep is 5 µA for the SoC, not for the board.** The LDO's quiescent
    draw and the OLED module's standby current are on top. Do not quote the
    datasheet number for this board.

## When the task is the display

Two ways to drive the panel, and mixing them is where people get lost.

**A — reprogram the controller for 40 rows** (rules 1–4, what `template/`
does). Multiplex ratio 40, offset 0, start line 0, column offset 28 per page.
This is the approach to prefer: the geometry is stated once, in one place, and
every drawing call works in real panel coordinates.

**B — leave it as 128 × 64 and draw into the middle**, which is what U8g2's
`SSD1306_128X64_NONAME` constructors require. Then you need both an x and a y
offset, and published values disagree — 30/12 in the original forum code, 28/24
in MicroPython and in later reports — because the right numbers depend on which
init sequence ran. If you are on Arduino, sidestep it:
`U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5)` carries the
40-row init and needs no offset. Note the argument order — **clock first, then
data**, i.e. 6 then 5.

Performance is not a concern here and should not shape the design. The whole
framebuffer is 72 × 5 pages = **360 bytes**; a full flush at 400 kHz is about
**8 ms**, so roughly 120 fps. Partial redraws are not worth writing.

## When the task is adding hardware

The board's real constraint is peripheral inventory, not speed:

- **I2C** — one controller, occupied, shareable (rule 8).
- **SPI2** — available via the GPIO Matrix, but two of its IO MUX fast pins
  (GPIO5 = FSPIWP, GPIO6 = FSPICLK) are the panel's, so at least part of the
  bus routes through the matrix and will not reach the 80 MHz IO-MUX ceiling.
- **UART** — UART0's pins (GPIO20/21) are free for an application port,
  because the console is on USB. UART1 goes anywhere.
- **RMT / LEDC / I2S / TWAI** — no fixed pins, any free GPIO.
- **JTAG pads** — GPIO4–GPIO7, of which GPIO5 and GPIO6 are the panel's, so
  pad-JTAG is out. The USB-C port carries JTAG anyway.

Nine header GPIOs are free: **0, 1, 2, 3, 4, 7, 10, 20, 21** — with GPIO2's
strapping caveat (rule 10).

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/esp32c3-oled042/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — LED blink + console heartbeat. No display, no I2C.
  **140,080 B** flash (13.4 % of the 1 MB app partition), 10,216 B RAM.
  **Flash this first on a new board**: its two outputs fail independently, so
  between them they say which half of the chain is broken.
  `variants/minimal/main.c` documents which combination means what.
- `--full` (default) — OLED marquee on the 72 × 40 panel, printable-ASCII 5 × 7
  font, LED heartbeat, USB console. **163,356 B** flash (15.6 %), 10,608 B RAM.

Both figures are what `pio run` reports for ESP-IDF 6.0.1. Nothing is generated
at scaffold time and no paths are embedded, so copying the tree by hand works
identically. `template/README.md` maps files to subsystems so a `--full`
scaffold can be stripped back cleanly.

When the user already has a project, prefer bringing it in line with the
template's `platformio.ini`, `sdkconfig.defaults` and `board.h` over rewriting
their code.

## Flashing

Normally nothing to press:

```sh
pio run -t upload -t monitor        # or: idf.py -p <port> flash monitor
```

The USB Serial/JTAG controller supports host-driven reset and download-mode
entry, so esptool drives the whole cycle over the USB-C cable. The board
enumerates as CDC-ACM — `/dev/cu.usbmodem*`, `/dev/ttyACM*` (described as
"USB JTAG/serial debug unit"), or a COM port.

**Manual download mode — hold BOOT, tap RST, release BOOT.** Needed for the
first flash on a factory board (several bring-ups report this; later uploads go
automatically), and any time firmware has wedged USB. Because ROM code
enumerates with no valid application present, **bad firmware cannot brick this
board**. Last resort: `esptool.py -p <port> erase_flash`, which takes 7–20 s
for 4 MB — it is probably not hung.

There is no debug header. JTAG is the same USB-C port via the built-in
controller, and unlike some other Espressif targets PlatformIO's pinned
`tool-openocd-esp32` does ship `board/esp32c3-builtin.cfg` and
`target/esp32c3.cfg`, so `pio debug` has a config to work from. Untested here.

## Reporting

Say what is verified on hardware and what is derived.

**Verified on hardware** (from the firmware this skill was extracted from): the
whole display path — GPIO5/GPIO6, address 0x3C, 400 kHz, multiplex ratio 40,
column offset 28, page addressing, the GDDRAM wipe, and holding the panel off
until the first clean frame.

**Verified by building** (this toolchain, ESP-IDF 6.0.1): both template
variants and their flash/RAM figures, and rules 6 and 7 — each was confirmed by
removing the line and reading the generated sdkconfig.

**Derived, not confirmed by the author of this skill:** the LED polarity on
GPIO8 and the BOOT button on GPIO9 (schematic plus an independent bring-up);
the power tree in `board-hardware.md` §7; the U8g2 constructors; every recipe
marked **⚠︎ compile-checked only** or **⚠︎ untested** in `reference/recipes.md`;
and the 5 × 7 font, of which only the glyphs in `"Hello, world!"` have been
seen on glass. Anything marked **⚠︎ Inference** in `reference/board-hardware.md`
is a conclusion drawn across sources, not a printed vendor statement — flag it
as such rather than presenting it as fact.

Board revisions differ. If the user's board has buttons on the underside, an
RGB LED or a Qwiic connector, it is the 01Space revision rather than the
ABRobot one; the I2C, LED and button wiring is the same, but the extras are
not — see `board-hardware.md` §1.1.
