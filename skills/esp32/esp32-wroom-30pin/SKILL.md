---
name: esp32-wroom-30pin
description: Firmware development for the 30-pin ESP32-WROOM-32 development board — the DOIT ESP32 DevKit V1 / ESP32 CH340 Type-C / NodeMCU-ESP32 30-pin form factor, 15 pins per side, CH340G or CP2102 USB-UART bridge, AMS1117 LDO, user LED on GPIO2. Use when working on a 30-pin ESP32 devkit (ESP32-D0WDQ6 / ESP32-D0WD-V3, ESP-WROOM-32 module): project setup, platformio.ini and sdkconfig for ESP-IDF or Arduino, pin mapping and which GPIOs are safe, strapping pins, ADC1 vs ADC2 with Wi-Fi, SPI speed and IO_MUX pins, I2C, deep sleep and RTC wake pins, flashing over the CH340 bridge, BOOT/EN recovery, brownouts, or debugging why something on the board does not work.
---

# ESP32-WROOM-32 devkit — 30-pin

The commodity ESP32 board: `ESP-WROOM-32` module, USB-serial bridge, LDO, two buttons,
15 pins a side. It is the one that fits a breadboard with a row free, which is why it
outsells the 36- and 38-pin versions.

Nearly every failure on this board comes from a pin that has a second job. Read
`reference/board-hardware.md` before choosing pins rather than guessing from a tutorial —
a lot of ESP32 tutorials were written for a different board size.

- `reference/board-hardware.md` — the board: header pin map with every pin's alternate
  functions, what is *not* broken out and why that helps, the USB bridge and auto-reset
  circuit, power tree, flash partitions — **plus** a development guide (§10 toolchain,
  §11 sdkconfig, §12 flashing and recovery, §13 peripheral cookbook, §14 symptom → cause
  → fix table).
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
| Module | ESP32-WROOM-32 (ESP-WROOM-32), ESP32-D0WDQ6 or D0WD-V3, **Xtensa LX6 dual-core** @ 240 MHz |
| Memory | 4 MB in-package flash, 520 KB SRAM (**~320 KB linkable**), 8 KB RTC FAST + 8 KB RTC SLOW. **No PSRAM** |
| Header | 30 pins, 15 per side. **25 GPIOs**, 2× GND, VIN, 3V3, EN |
| USB | **No USB peripheral in the chip.** A CH340G/CP2102 bridge on UART0 (GPIO1/GPIO3), with DTR/RTS auto-reset |
| Console | UART0 @ 115200 only. There is no USB-Serial-JTAG and no debug header |
| LED | one blue user LED on **GPIO2**, active **high** (a strapping pin) |
| Buttons | **BOOT** = GPIO0, pressed low, **not on the header** · **EN/RST** acts on CHIP_PU, not readable |
| Free pins | 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 |
| Input-only | 34, 35, 36 (VP), 39 (VN) — no output driver, **no internal pulls** |
| Strapping | 2, 5, 12 (MTDI), 15 (MTDO) — usable, with the caveats in rule 2 |
| Analog | ADC1 on 32–36, 39 · ADC2 on 0, 2, 4, 12–15, 25–27 · DAC on **25 and 26** |
| Radio | Wi-Fi b/g/n + Bluetooth 4.2 BR/EDR + BLE, PCB antenna on the module |
| Power | 3V3 out (~600 mA budget) · VIN 5–12 V in, 5 V out. No ORing diode |
| Toolchain | PlatformIO 6.1.19 + `platform-espressif32` 7.0.1 + ESP-IDF 6.0.1, `board = esp32dev` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`CONFIG_ESP_CONSOLE_UART_DEFAULT=y`, never `..._USB_SERIAL_JTAG`.** The original
   ESP32 has **no USB peripheral at all** — every byte you see comes out of UART0 through
   a separate bridge chip. Configuration copied from an ESP32-C3/C6/S3 project produces a
   firmware that runs perfectly and says nothing, and you debug blind. This is the single
   most common cross-chip mistake.

2. **GPIO12 must be LOW at reset.** It is MTDI, and high at reset straps VDD_SDIO to
   1.8 V while the module's flash is a 3.3 V part. The board then boots intermittently and
   the symptom looks like *flash corruption*, not like a pin problem. A pull-up resistor,
   an LED to 3V3, or a peripheral that idles a line high on GPIO12 all do it. It is a fine
   pin after boot; add a 10 kΩ pull-down if you must use it.

3. **ADC2 is dead while Wi-Fi is running.** The Wi-Fi PHY arbitrates for the same
   hardware and wins: `adc_oneshot_read()` on any ADC2 channel returns `ESP_ERR_TIMEOUT`
   until `esp_wifi_stop()`. There is no workaround. That leaves **ADC1 — GPIO32, 33, 34,
   35, 36, 39** — as the entire analog budget of a connected application. Plan the pinout
   around it before wiring, not after.

4. **`SPI3_HOST` on GPIO18/19/23/5, or you get 26.67 MHz.** Those four are SPI3's IO_MUX
   pads and bypass the GPIO Matrix. Any other pin set — or the same pins on `SPI2_HOST` —
   is matrix-routed, and the driver silently clamps full-duplex transfers to 80 MHz / 3
   because of the matrix's 25 ns delay. No error is reported;
   `spi_device_get_actual_freq()` is how you find out.

5. **GPIO34/35/36/39 are input-only and have no internal pull-up or pull-down.**
   `gpio_config()` accepts `GPIO_PULLUP_ENABLE` on them and returns `ESP_OK`. Nothing
   happens. A button on GPIO34 needs a physical 10 kΩ resistor, and a floating one reads
   as noise rather than as a stable level.

6. **A reset the moment Wi-Fi starts is the power supply, not your code.** The TX burst
   pulls ~300 mA; the datasheet asks for a 500 mA source. A laptop USB-2 port, a thin
   cable or a lying hub all produce the same signature — everything works until the first
   packet. `esp_reset_reason()` returns `ESP_RST_BROWNOUT`; say so rather than reading the
   stack trace.

7. **Never power VIN and USB at the same time.** There is no ORing diode. With USB
   connected, VIN is an *output* at ~5 V and a second supply back-feeds the host.

8. **UART1's default pins are GPIO9 and GPIO10 — the in-package flash bus.** Using UART1
   without `uart_set_pin()` first hangs the board. Use **UART2 on GPIO17/GPIO16** instead,
   and note that `uart_set_pin()` must come *before* `uart_driver_install()` — the reverse
   order compiles, runs, and leaves the peripheral on its defaults.

9. **Only RTC GPIOs wake the chip from deep sleep**: 0, 2, 4, 12–15, 25–27, 32–36, 39.
   GPIO5, 16, 17, 18, 19, 21, 22, 23 cannot, however you configure the interrupt. And the
   wake pin's pull must be set with `rtc_gpio_pullup_en()`, not `gpio_set_pull_mode()` —
   the digital pull registers are powered down in sleep.

10. **Deep sleep on this board is 8–20 mA, not 10 µA.** The LDO's quiescent draw, the USB
    bridge chip and the power LED are all still on. The datasheet's 10 µA is the bare
    chip. Never quote it for a devkit.

11. **Do not set `CONFIG_SPIRAM=y`.** A WROOM-32 has no PSRAM; the driver fails to find it
    and aborts before `app_main()`, which reads as a bricked board. That option belongs to
    WROVER modules.

12. **`CONFIG_ESP32_XTAL_FREQ_40=y`.** The module's crystal is 40 MHz. The 26 MHz setting
    exists for other modules and garbles the console at *every* baud rate, which reads as
    a broken cable.

13. **GPIO0 is not on this header** — it is wired only to the BOOT button and the
    auto-reset transistor. That is a feature: unlike a 38-pin board, nothing you wire can
    accidentally hold the boot strap and drop the board into download mode. Do not look
    for a `D0` pin; there isn't one.

14. **The internal I2C pull-ups are 45 kΩ.** Fine for two devices on a short jumper, not
    for a metre of ribbon or 400 kHz. A bus that scans clean at 100 kHz and fails at
    400 kHz needs external 4.7 kΩ resistors, not a driver change.

## When the task is choosing pins

Work outward from the constraints, in this order:

1. **Analog first.** If Wi-Fi is in the design, every analog input must land on GPIO32,
   33, 34, 35, 36 or 39 (rule 3). Four of those six are input-only, which suits sensors.
2. **Then the fast bus.** If anything needs SPI above 26 MHz, GPIO18/19/23/5 are spoken
   for (rule 4).
3. **Then outputs that must be quiet at boot.** GPIO2 is driven briefly by the bootloader
   and carries the LED; GPIO1 emits the ROM log at 115200 baud. A relay or a MOSFET gate
   on either clicks on every reset.
4. **Everything else** goes on 4, 13, 14, 16, 17, 19, 21, 22, 23, 25, 26, 27 — 15 pins,
   which is usually plenty, and the reason this board is enough for most projects.

If you run out, the 36- and 38-pin boards do **not** help: their extra pins are the flash
bus, which is unusable, plus GPIO0 on the 38-pin. The usable GPIO set is identical on all
three. Reach for an I2C GPIO expander instead.

## When the task is "the board does not work"

Diagnose in this order — each stage only depends on the ones before it:

1. **Does the port enumerate?** No port ⇒ cable (try a known data cable) or bridge driver.
2. **Does anything print at 115200?** The ROM bootloader's `rst:0x1 (POWERON_RESET)`
   banner comes out before any of your configuration applies. If that is absent but the
   port exists, suspect the cable's data lines or a wedged bridge chip.
3. **Does it print but say nothing after?** Rule 1 — the console is misconfigured, or
   MTDO/GPIO15 is strapped low.
4. **Does it boot-loop?** Read `rst:`. `ESP_RST_BROWNOUT` ⇒ rule 6. `RTCWDT_RTC_RESET` or
   `invalid header` ⇒ `esptool.py erase_flash` and reflash the minimal variant.
5. **Does it reach `app_main()` and then misbehave?** Flash `--minimal` to establish a
   baseline, then add one subsystem at a time. The full variant's report prints the
   strapping latches, the reset reason and the heap, which answers most of what is left.

`template/src/board_report.c` runs steps 3–5 for you and is worth pasting into any project
that is misbehaving.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/esp32-wroom-30pin/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — LED blink on GPIO2 + console heartbeat. **144,493 B flash, 13,380 B RAM.**
  Flash this first on a board you have not used before: it touches only the toolchain, the
  flashing route, UART0 and one GPIO, and its two outputs fail independently — LED but no
  console means the serial side, console but no LED means this board has no LED on GPIO2.
- `--full` (default) — board self-test: chip/reset/strapping report, partition dump, ADC1
  with calibration, Wi-Fi scan, LEDC heartbeat, BOOT button to re-run. **784,417 B flash,
  37,036 B RAM.** The jump is the Wi-Fi stack.

Both build as-is with ESP-IDF 6.0.1 (verified, zero warnings). Nothing is generated and no
paths are embedded, so copying `template/` by hand works identically.
`template/README.md` maps files to subsystems so a `--full` scaffold can be stripped back.

When the user already has a project, prefer bringing it in line with the template's
`platformio.ini`, `sdkconfig.defaults` and `include/board.h` over rewriting their code.

Arduino (`framework = arduino`) is a perfectly reasonable choice for this board and every
pin rule above applies unchanged — but the Arduino core's `analogRead()` hides the
ADC2/Wi-Fi conflict of rule 3 instead of reporting it.

## Flashing

```sh
pio run -t upload -t monitor
```

The bridge chip's DTR/RTS drive EN and GPIO0 through the two-transistor auto-reset circuit,
so esptool handles the whole cycle with no buttons. The board appears as
`/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` (macOS), `/dev/ttyUSB0` (Linux, needs
`dialout` membership), or a `COM` port (Windows, WCH or Silicon Labs driver).

When it fails with `Wrong boot mode detected` or `No serial data received`, the manual
sequence is: **hold BOOT → tap EN → keep BOOT a second longer → release BOOT → upload.**
Some clones need BOOT held for the whole upload. If 921600 baud is unreliable, drop
`upload_speed` to 460800.

**Bad firmware cannot brick this board.** The ROM bootloader is in mask ROM and always
answers that sequence:

```sh
esptool.py --chip esp32 -p <port> erase_flash    # ~10 s for 4 MB
```

There is no debug header and no USB-JTAG. External JTAG means wiring a probe to
GPIO12/13/14/15 — and a probe that idles MTDI (GPIO12) high stops the board booting
(rule 2). In practice you debug this board over UART0.

## Reporting

Say what is verified and what is derived.

**Verified on this machine:** both template variants build clean with PlatformIO 6.1.19 /
platform-espressif32 7.0.1 / ESP-IDF 6.0.1, zero warnings, at the flash and RAM figures
quoted above. Every recipe in `reference/recipes.md` was compiled in the same project.

**Taken from primary documents:** everything in `reference/esp32-soc.md` (ESP32 Series
Datasheet v4.3, ESP32-WROOM-32 Datasheet v3.1, ESP32 TRM v5.8), and the 26.67 MHz SPI
figure, which is computed from `_GPIO_MATRIX_DELAY_NS` in the IDF HAL.

**Not verified on hardware:** none of this was run on a physical board in this session. The
board-level details that vary by vendor — the user LED's pin and polarity, which bridge
chip is fitted, whether the auto-reset transistors are present — are stated for the
DOIT/CH340 mainstream and should be confirmed with the `--minimal` variant on the user's
actual board. Say so rather than presenting them as certain.
