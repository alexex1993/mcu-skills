---
name: esp32-wroom-38pin
description: Firmware development for the 38-pin ESP32-WROOM-32 development board — Espressif ESP32-DevKitC V4, Ai-Thinker NodeMCU-32S, DOIT ESP32 DevKit V1 38-pin and their CH340/CP2102 clones, 19 pins per side, with GPIO0 and the six in-package flash pins (GPIO6-11, labelled D0 D1 D2 D3 CMD CLK or SD0-SD3) broken out. Use when working on a 38-pin ESP32 devkit (ESP32-D0WDQ6 / ESP32-D0WD-V3, ESP-WROOM-32 or ESP32-WROVER module): project setup, platformio.ini and sdkconfig for ESP-IDF or Arduino, pin mapping and which GPIOs are safe, the flash-pin and IO0 traps, WROVER GPIO16/17 PSRAM conflict, strapping pins, ADC1 vs ADC2 with Wi-Fi, SPI speed and IO_MUX pins, deep sleep and RTC wake pins, flashing, BOOT/EN recovery, brownouts, or debugging why something on the board does not work.
---

# ESP32-WROOM-32 devkit — 38-pin

The reference form factor: Espressif's **ESP32-DevKitC V4** and the clones that copy its
J2/J3 header pin for pin. Nineteen pins a side, and every pad of the module is broken out —
including six that will hang the chip and one that will stop it booting.

That completeness is the whole story of this board. The 30-pin version protects you by
omission; this one hands you the flash bus and GPIO0 and expects you to know better.
Read `reference/board-hardware.md` before choosing pins.

- `reference/board-hardware.md` — the board: the Espressif J2/J3 header table with every
  pin's alternate functions, the six flash pins, the IO0 hazard, the `D2`-means-GPIO9
  silkscreen trap, the WROVER GPIO16/17 difference, the USB bridge and auto-reset circuit,
  power tree, flash partitions — **plus** a development guide (§10 toolchain, §11
  sdkconfig, §12 flashing and recovery, §13 peripheral cookbook, §14 symptom → cause → fix
  table).
- `reference/esp32-soc.md` — the silicon: pins that do not exist, strapping semantics and
  the `GPIO_STRAP_REG` bit order, ADC/DAC/touch maps and accuracy, IO_MUX vs GPIO Matrix
  and the SPI speed rule, power modes, memory map, deep-sleep wake pins.
- `reference/recipes.md` — code that compiles: `platformio.ini`, `sdkconfig.defaults`,
  ADC1 with calibration, LEDC LED and servo, debounced input, the new I2C master driver,
  80 MHz SPI, UART2, Wi-Fi scan and station connect, NVS, deep sleep, RMT WS2812.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script.
  See `template/README.md`.

## Orientation

| | |
|---|---|
| Module | ESP32-WROOM-32/-32D/-32E/-32U, ESP-WROOM-32S, or **ESP32-WROVER** — check the can. ESP32-D0WDQ6 or D0WD-V3, **Xtensa LX6 dual-core** @ 240 MHz |
| Memory | 4 MB in-package flash, 520 KB SRAM (**~320 KB linkable**), 8 KB RTC FAST + 8 KB RTC SLOW. **PSRAM only on a WROVER** |
| Header | 38 pins, 19 per side (Espressif J2 / J3). **32 GPIOs — all module pads**, EN, 3V3, 5V, 3× GND |
| USB | **No USB peripheral in the chip.** A CP2102N (DevKitC) or CH340G (clones) bridge on UART0 (GPIO1/GPIO3), with DTR/RTS auto-reset |
| Console | UART0 @ 115200 only. There is no USB-Serial-JTAG and no debug header |
| LED | **none on the Espressif DevKitC V4.** GPIO2, active high, on NodeMCU-32S and clones |
| Buttons | **BOOT** = GPIO0, pressed low, and **GPIO0 is also a header pin** · **EN/RST** acts on CHIP_PU, not readable |
| Free pins | 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 |
| Input-only | 34, 35, 36 (VP), 39 (VN) — no output driver, **no internal pulls** |
| Strapping | 0, 2, 5, 12 (MTDI), 15 (MTDO) |
| **Never use** | **6, 7, 8, 9, 10, 11** — the in-package flash bus, six pins by the USB connector |
| Analog | ADC1 on 32–36, 39 · ADC2 on 0, 2, 4, 12–15, 25–27 · DAC on **25 and 26** |
| Radio | Wi-Fi b/g/n + Bluetooth 4.2 BR/EDR + BLE, PCB antenna (or u.FL on `-U`/`-IE` modules) |
| Power | 3V3 out (~600 mA budget) · 5V in/out. **Three mutually exclusive supply routes** |
| Toolchain | PlatformIO 6.1.19 + `platform-espressif32` 7.0.1 + ESP-IDF 6.0.1, `board = esp32dev` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **GPIO6–11 are the flash bus. Wire nothing to them, ever.** The six pins nearest the
   USB connector — `D2 D3 CMD` on one side, `D1 D0 CLK` on the other — go to the module's
   internal 4 MB flash die, which the CPU is executing out of. A pull-up, an LED, or even
   a scope probe with too much capacitance gives you one of: a hang just after the
   bootloader banner, a boot loop with `invalid header`, or — worst — a board that runs
   and corrupts flash sectors under load. Espressif's own footnote says "avoid"; treat
   that end of the header as mechanical support.

2. **`D2` on an Espressif silkscreen means GPIO9, not GPIO2.** DOIT-style boards use `D2`
   for GPIO2 and `D4` for GPIO4; Espressif uses `D0`–`D3` for the flash pins GPIO7–GPIO10.
   The same two characters mean different pins on different boards, and following a
   tutorial written for the other one puts your signal on the flash bus (rule 1). Resolve
   every silkscreen label to a GPIO number via `board-hardware.md` §2 before writing code.

3. **GPIO0 is on this header, and three things already own it**: the boot strap (low at
   reset ⇒ serial download mode), the BOOT button, and the bridge chip's auto-reset
   circuit. An LED to ground on IO0 is enough to drop the board into download mode on
   every reset — which presents as "my firmware stopped running", not as a pin problem —
   and a pull-up breaks `pio run -t upload`. If uploads started failing after you wired
   something, unplug IO0 first.

4. **`CONFIG_ESP_CONSOLE_UART_DEFAULT=y`, never `..._USB_SERIAL_JTAG`.** The original
   ESP32 has **no USB peripheral at all** — every byte comes out of UART0 through a
   separate bridge chip. Configuration copied from an ESP32-C3/C6/S3 project produces a
   firmware that runs perfectly and says nothing.

5. **GPIO12 must be LOW at reset.** It is MTDI, and high at reset straps VDD_SDIO to
   1.8 V while the flash is a 3.3 V part. The board boots intermittently and the symptom
   looks like *flash corruption*. A pull-up, an LED to 3V3, or a peripheral idling a line
   high all do it. Fine after boot; add a 10 kΩ pull-down if you need the pin.

6. **Check whether the module is a WROVER before touching GPIO16/17.** On WROVER modules
   those two are the 8 MB PSRAM's CS and CLK and are unusable — which also removes UART2's
   default pins. Espressif's footnote: available "only on the boards with the modules
   ESP32-WROOM and ESP32-SOLO-1". Read the can: `WROOM` ⇒ free, `WROVER` ⇒ taken. The same
   distinction decides whether `CONFIG_SPIRAM=y` is correct or fatal.

7. **ADC2 is dead while Wi-Fi is running.** The Wi-Fi PHY arbitrates for the same
   hardware and wins: `adc_oneshot_read()` on any ADC2 channel returns `ESP_ERR_TIMEOUT`
   until `esp_wifi_stop()`. That leaves **ADC1 — GPIO32, 33, 34, 35, 36, 39** — as the
   entire analog budget of a connected application. Plan the pinout around it.

8. **`SPI3_HOST` on GPIO18/19/23/5, or you get 26.67 MHz.** Those four are SPI3's IO_MUX
   pads and bypass the GPIO Matrix. Any other pin set — or the same pins on `SPI2_HOST` —
   is matrix-routed and the driver silently clamps full-duplex transfers to 80 MHz / 3.
   No error is reported; `spi_device_get_actual_freq()` is how you find out.

9. **GPIO34/35/36/39 are input-only and have no internal pull-up or pull-down.**
   `gpio_config()` accepts `GPIO_PULLUP_ENABLE` on them and returns `ESP_OK`. Nothing
   happens. A button there needs a physical 10 kΩ resistor.

10. **A reset the moment Wi-Fi starts is the power supply, not your code.** The TX burst
    pulls ~300 mA against a datasheet requirement of a 500 mA source.
    `esp_reset_reason()` returns `ESP_RST_BROWNOUT`; say so rather than reading the stack
    trace.

11. **Exactly one power route at a time.** USB, or the 5V pin, or the 3V3 pin — never two.
    Espressif warns that more than one "can damage the board and/or the power supply", and
    no ORing diode is fitted on the clones either.

12. **UART1's default pins are GPIO9 and GPIO10** — flash pins again (rule 1). Use
    **UART2 on GPIO17/GPIO16** (WROOM only, rule 6), and call `uart_set_pin()` *before*
    `uart_driver_install()`; the reverse order compiles, runs, and leaves the peripheral on
    its defaults.

13. **Only RTC GPIOs wake the chip from deep sleep**: 0, 2, 4, 12–15, 25–27, 32–36, 39.
    GPIO5, 16, 17, 18, 19, 21, 22, 23 cannot, however you configure the interrupt. The wake
    pin's pull must be set with `rtc_gpio_pullup_en()`, not `gpio_set_pull_mode()`.

14. **Deep sleep on this board is 8–20 mA, not 10 µA.** LDO quiescent + USB bridge + power
    LED. The datasheet's 10 µA is the bare chip; never quote it for a devkit.

15. **`CONFIG_ESP32_XTAL_FREQ_40=y`.** The module's crystal is 40 MHz; the 26 MHz setting
    garbles the console at *every* baud rate, which reads as a broken cable.

16. **Do not assume there is a user LED.** The genuine Espressif DevKitC V4 has none — only
    a power LED. `BOARD_HAS_USER_LED` in `include/board.h` defaults to 1 for the clones;
    if the minimal variant prints ticks and nothing lights up, set it to 0 rather than
    debugging the GPIO.

## When the task is choosing pins

The 38-pin board has 32 pads on the header but the same **25 usable GPIOs** as the 30-pin
board, because six are flash and GPIO0 is spoken for. Work outward:

1. **Analog first.** With Wi-Fi in the design, every analog input must land on GPIO32, 33,
   34, 35, 36 or 39 (rule 7).
2. **Then the fast bus.** SPI above 26 MHz claims GPIO18/19/23/5 (rule 8).
3. **Then outputs that must be quiet at boot.** GPIO2 is driven briefly by the bootloader;
   GPIO1 emits the ROM log; GPIO0 is driven by the auto-reset circuit on every upload.
4. **Everything else** goes on 4, 13, 14, 16, 17, 19, 21, 22, 23, 25, 26, 27.

If you run out, the extra pins on this board over the 30-pin one are **not** the answer —
five of the seven are unusable and the sixth is GPIO0. Reach for an I2C GPIO expander.

## When the task is "the board does not work"

Diagnose in this order — each stage only depends on the ones before it:

1. **Is anything wired to the six pins by the USB connector, or to IO0?** Unplug them
   first. Rules 1 and 3 account for most mysteries on this board specifically.
2. **Does the port enumerate?** No port ⇒ cable (try a known data cable) or bridge driver.
3. **Does anything print at 115200?** The ROM bootloader's `rst:0x1 (POWERON_RESET)`
   banner comes out before any of your configuration applies.
4. **Does it print but say nothing after?** Rule 4 — the console is misconfigured, or
   MTDO/GPIO15 is strapped low.
5. **Does it boot-loop?** Read `rst:`. `ESP_RST_BROWNOUT` ⇒ rule 10. `RTCWDT_RTC_RESET` or
   `invalid header` ⇒ `esptool.py erase_flash` and reflash the minimal variant.
6. **Does it land in download mode by itself?** Something is holding GPIO0 low (rule 3).
   On a genuine early DevKitC V4, it may instead be the **C15** capacitor Espressif
   documents — see `board-hardware.md` §3.
7. **Does it reach `app_main()` and then misbehave?** Flash `--minimal` for a baseline,
   then add one subsystem at a time.

`template/src/board_report.c` prints the strapping latches (including the GPIO0 one),
the reset reason, the flash size and the partition table, which answers most of steps 4–7.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/esp32-wroom-38pin/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — LED blink on GPIO2 + console heartbeat. **144,493 B flash, 13,380 B RAM.**
  Flash this first on a board you have not used before: it touches only the toolchain, the
  flashing route, UART0 and one GPIO, and its two outputs fail independently — LED but no
  console means the serial side, console but no LED means your board is a DevKitC V4 with
  no user LED (rule 16).
- `--full` (default) — board self-test: chip/reset/strapping report, partition dump, ADC1
  with calibration, Wi-Fi scan, LEDC heartbeat, BOOT button to re-run. **784,417 B flash,
  37,036 B RAM.** The jump is the Wi-Fi stack.

Both build as-is with ESP-IDF 6.0.1 (verified, zero warnings). Nothing is generated and no
paths are embedded, so copying `template/` by hand works identically.
`template/README.md` maps files to subsystems so a `--full` scaffold can be stripped back.

When the user already has a project, prefer bringing it in line with the template's
`platformio.ini`, `sdkconfig.defaults` and `include/board.h` over rewriting their code.

Arduino (`framework = arduino`) is a reasonable choice for this board and every pin rule
above applies unchanged — but the Arduino core's `analogRead()` hides the ADC2/Wi-Fi
conflict of rule 7 instead of reporting it.

## Flashing

```sh
pio run -t upload -t monitor
```

The bridge chip's DTR/RTS drive EN and GPIO0 through the two-transistor auto-reset circuit,
so esptool handles the whole cycle with no buttons. The board appears as
`/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` (macOS), `/dev/ttyUSB0` (Linux, needs
`dialout` membership), or a `COM` port (Windows, WCH or Silicon Labs driver).

When it fails with `Wrong boot mode detected` or `No serial data received`, **check IO0
before the cable** (rule 3). Then the manual sequence: **hold BOOT → tap EN → keep BOOT a
second longer → release BOOT → upload.** Some clones need BOOT held for the whole upload.
If 921600 baud is unreliable, drop `upload_speed` to 460800.

**Bad firmware cannot brick this board.** The ROM bootloader is in mask ROM and always
answers that sequence:

```sh
esptool.py --chip esp32 -p <port> erase_flash    # ~10 s for 4 MB
```

There is no debug header and no USB-JTAG. External JTAG means wiring a probe to
GPIO12/13/14/15 — and a probe that idles MTDI (GPIO12) high stops the board booting
(rule 5). In practice you debug this board over UART0.

## Reporting

Say what is verified and what is derived.

**Primary-source, high confidence:** the header pin map in `reference/board-hardware.md`
§2 is transcribed from Espressif's ESP32-DevKitC V4 J2/J3 tables and cross-checked against
Ai-Thinker's NodeMCU-32S datasheet, which agree pin for pin. This is the best-documented
of the three board sizes. The flash-pin warning, the GPIO16/17 WROVER note and the C15
issue are Espressif's own text.

**Verified on this machine:** both template variants build clean with PlatformIO 6.1.19 /
platform-espressif32 7.0.1 / ESP-IDF 6.0.1, zero warnings, at the flash and RAM figures
quoted above. Every recipe in `reference/recipes.md` was compiled in the same project. The
26.67 MHz SPI figure is computed from `_GPIO_MATRIX_DELAY_NS` in the IDF HAL.

**Not verified on hardware:** none of this was run on a physical board in this session.
Details that vary by vendor — whether a user LED is fitted, which bridge chip, whether the
auto-reset transistors are present — should be confirmed with the `--minimal` variant on
the user's actual board. Say so rather than presenting them as certain.
