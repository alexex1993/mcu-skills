---
name: esp32-wroom-36pin
description: Firmware development for the 36-pin ESP32-WROOM-32 development board — the original DOIT ESP32 DevKit V1 with 18 pins per side, i.e. the 30-pin layout plus the six in-package flash pins (SD0 SD1 SD2 SD3 CMD CLK = GPIO6-11) broken out at the USB end, CH340G or CP2102 bridge, AMS1117 LDO, user LED on GPIO2, GPIO0 not on the header. Use when working on a 36-pin ESP32 devkit (ESP32-D0WDQ6 / ESP32-D0WD-V3, ESP-WROOM-32 module), or when identifying which ESP32 board size you have: project setup, platformio.ini and sdkconfig for ESP-IDF or Arduino, pin mapping and which GPIOs are safe, the flash-pin trap, strapping pins, ADC1 vs ADC2 with Wi-Fi, SPI speed and IO_MUX pins, deep sleep and RTC wake pins, flashing, BOOT/EN recovery, brownouts, or debugging why something on the board does not work.
---

# ESP32-WROOM-32 devkit — 36-pin

The original DOIT ESP32 DevKit V1, before the flash pins were dropped to make the shorter
30-pin board everyone sells now. Eighteen pins a side: the 30-pin layout plus six pins at
the USB end that go straight to the module's internal flash die.

Those six are the only functional difference from the 30-pin board, and they are a
liability rather than a feature. Everything else — the usable GPIO set, the strapping
pins, the analog budget — is identical.

- `reference/board-hardware.md` — the board: header pin map with every pin's alternate
  functions, the six flash pins, a 20-second procedure to confirm which board size you
  actually have, the USB bridge and auto-reset circuit, power tree, flash partitions —
  **plus** a development guide (§10 toolchain, §11 sdkconfig, §12 flashing and recovery,
  §13 peripheral cookbook, §14 symptom → cause → fix table).
- `reference/esp32-soc.md` — the silicon: pins that do not exist, strapping semantics and
  the `GPIO_STRAP_REG` bit order, ADC/DAC/touch maps and accuracy, IO_MUX vs GPIO Matrix
  and the SPI speed rule, power modes, memory map, deep-sleep wake pins.
- `reference/esp32-family.md` — the rest of the family, for "should this be a different
  ESP32?" questions: what does and does not port between chips, radio and USB capability
  per chip, the RMT generation table (WS2812 under Wi-Fi load), deep-sleep memory and
  ULP/LP-core availability, and a chip-selection table.
- `reference/recipes.md` — code that compiles: `platformio.ini`, `sdkconfig.defaults`,
  ADC1 with calibration, LEDC LED and servo, debounced input, the new I2C master driver,
  80 MHz SPI, UART2, Wi-Fi scan and station connect, NVS, deep sleep, RMT WS2812.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script.
  See `template/README.md`.

## Confirm the board first

"36-pin ESP32" is the least standardised of the three sizes, and Espressif never made one,
so there is no vendor pin table for it. Twenty seconds settles it:

| Pins per side | Flash pins on the header | `IO0`/`D0` pin | GND pins | Skill |
|---|---|---|---|---|
| 15 | no | no | 2 | `esp32-wroom-30pin` |
| **18** | **yes** (`SD2 SD3 CMD` / `SD1 SD0 CLK`) | **no** | **2** | **this one** |
| 19 | yes (`D2 D3 CMD` / `D1 D0 CLK`) | yes | 3 | `esp32-wroom-38pin` |

If the count does not match, switch skills — the pin map is the whole point of having
three. `reference/board-hardware.md` §1 and §2.1 have the detail, including which parts of
the row order are reconstructed rather than transcribed from a vendor document.

## Orientation

| | |
|---|---|
| Module | ESP32-WROOM-32 (ESP-WROOM-32), ESP32-D0WDQ6 or D0WD-V3, **Xtensa LX6 dual-core** @ 240 MHz |
| Memory | 4 MB in-package flash, 520 KB SRAM (**~320 KB linkable**), 8 KB RTC FAST + 8 KB RTC SLOW. **No PSRAM** |
| Header | 36 pins, 18 per side. 31 GPIOs exposed but **only 25 usable**, 2× GND, VIN, 3V3, EN |
| USB | **No USB peripheral in the chip.** A CH340G/CP2102 bridge on UART0 (GPIO1/GPIO3), with DTR/RTS auto-reset |
| Console | UART0 @ 115200 only. There is no USB-Serial-JTAG and no debug header |
| LED | one blue user LED on **GPIO2**, active **high** (a strapping pin) |
| Buttons | **BOOT** = GPIO0, pressed low, **not on the header** · **EN/RST** acts on CHIP_PU, not readable |
| Free pins | 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 |
| Input-only | 34, 35, 36 (VP), 39 (VN) — no output driver, **no internal pulls** |
| Strapping | 2, 5, 12 (MTDI), 15 (MTDO) |
| **Never use** | **6, 7, 8, 9, 10, 11** — the in-package flash bus, six pins by the USB connector |
| Analog | ADC1 on 32–36, 39 · ADC2 on 0, 2, 4, 12–15, 25–27 · DAC on **25 and 26** |
| Radio | Wi-Fi b/g/n + Bluetooth 4.2 BR/EDR + BLE, PCB antenna on the module |
| Power | 3V3 out (~600 mA budget) · VIN 5–12 V in, 5 V out. No ORing diode |
| Toolchain | PlatformIO 6.1.19 + `platform-espressif32` 7.0.1 + ESP-IDF 6.0.1, `board = esp32dev` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`SD2 SD3 CMD SD1 SD0 CLK` are the flash bus. Wire nothing to them, ever.** Those six
   pins at the USB end are GPIO9, 10, 11, 8, 7, 6, and they go to the module's internal
   4 MB flash die — which the CPU is executing out of. A pull-up, an LED, or even a scope
   probe with too much capacitance gives you one of: a hang just after the bootloader
   banner, a boot loop with `invalid header`, or — worst — a board that runs and corrupts
   flash sectors under load. They exist on the header for factory bring-up, not for you.
   **This is the only thing that makes a 36-pin board different from a 30-pin board, and
   it is a hazard, not a feature.**

2. **`CONFIG_ESP_CONSOLE_UART_DEFAULT=y`, never `..._USB_SERIAL_JTAG`.** The original
   ESP32 has **no USB peripheral at all** — every byte comes out of UART0 through a
   separate bridge chip. Configuration copied from an ESP32-C3/C6/S3 project produces a
   firmware that runs perfectly and says nothing, and you debug blind.

3. **GPIO12 must be LOW at reset.** It is MTDI, and high at reset straps VDD_SDIO to
   1.8 V while the module's flash is a 3.3 V part. The board then boots intermittently and
   the symptom looks like *flash corruption*, not like a pin problem. A pull-up resistor,
   an LED to 3V3, or a peripheral idling a line high on GPIO12 all do it. Fine after boot;
   add a 10 kΩ pull-down if you must use it.

4. **ADC2 is dead while Wi-Fi is running.** The Wi-Fi PHY arbitrates for the same
   hardware and wins: `adc_oneshot_read()` on any ADC2 channel returns `ESP_ERR_TIMEOUT`
   until `esp_wifi_stop()`. There is no workaround. That leaves **ADC1 — GPIO32, 33, 34,
   35, 36, 39** — as the entire analog budget of a connected application. Plan the pinout
   around it before wiring, not after.

5. **`SPI3_HOST` on GPIO18/19/23/5, or you get 26.67 MHz.** Those four are SPI3's IO_MUX
   pads and bypass the GPIO Matrix. Any other pin set — or the same pins on `SPI2_HOST` —
   is matrix-routed, and the driver silently clamps full-duplex transfers to 80 MHz / 3
   because of the matrix's 25 ns delay. No error is reported;
   `spi_device_get_actual_freq()` is how you find out.

6. **GPIO34/35/36/39 are input-only and have no internal pull-up or pull-down.**
   `gpio_config()` accepts `GPIO_PULLUP_ENABLE` on them and returns `ESP_OK`. Nothing
   happens. A button on GPIO34 needs a physical 10 kΩ resistor, and a floating one reads
   as noise rather than as a stable level.

7. **A reset the moment Wi-Fi starts is the power supply, not your code.** The TX burst
   pulls ~300 mA; the datasheet asks for a 500 mA source. A laptop USB-2 port, a thin
   cable or a lying hub all produce the same signature — everything works until the first
   packet. `esp_reset_reason()` returns `ESP_RST_BROWNOUT`; say so rather than reading the
   stack trace.

8. **Never power VIN and USB at the same time.** There is no ORing diode. With USB
   connected, VIN is an *output* at ~5 V and a second supply back-feeds the host.

9. **UART1's default pins are GPIO9 and GPIO10** — two of the six from rule 1. Using
   UART1 without `uart_set_pin()` first hangs the board. Use **UART2 on GPIO17/GPIO16**
   instead, and note that `uart_set_pin()` must come *before* `uart_driver_install()` —
   the reverse order compiles, runs, and leaves the peripheral on its defaults.

10. **Only RTC GPIOs wake the chip from deep sleep**: 2, 4, 12–15, 25–27, 32–36, 39
    (GPIO0 is RTC-capable but not on this header). GPIO5, 16, 17, 18, 19, 21, 22, 23
    cannot, however you configure the interrupt. The wake pin's pull must be set with
    `rtc_gpio_pullup_en()`, not `gpio_set_pull_mode()` — the digital pull registers are
    powered down in sleep.

11. **Deep sleep on this board is 8–20 mA, not 10 µA.** The LDO's quiescent draw, the USB
    bridge chip and the power LED are all still on. The datasheet's 10 µA is the bare
    chip. Never quote it for a devkit.

12. **Do not set `CONFIG_SPIRAM=y`.** A WROOM-32 has no PSRAM; the driver fails to find it
    and aborts before `app_main()`, which reads as a bricked board. That option belongs to
    WROVER modules.

13. **`CONFIG_ESP32_XTAL_FREQ_40=y`.** The module's crystal is 40 MHz. The 26 MHz setting
    exists for other modules and garbles the console at *every* baud rate, which reads as
    a broken cable.

14. **GPIO0 is not on this header** — it is wired only to the BOOT button and the
    auto-reset transistor. That is a quiet advantage over the 38-pin board: nothing you
    wire can hold the boot strap and drop the board into download mode, so uploads are
    more reliable. Do not look for a `D0`/`IO0` pin; there isn't one.

15. **The internal I2C pull-ups are 45 kΩ.** Fine for two devices on a short jumper, not
    for a metre of ribbon or 400 kHz. A bus that scans clean at 100 kHz and fails at
    400 kHz needs external 4.7 kΩ resistors, not a driver change.

## When the task is choosing pins

The 36-pin board exposes 31 GPIOs but has the same **25 usable** ones as the 30-pin board.
Work outward from the constraints:

1. **Analog first.** If Wi-Fi is in the design, every analog input must land on GPIO32,
   33, 34, 35, 36 or 39 (rule 4). Four of those six are input-only, which suits sensors.
2. **Then the fast bus.** If anything needs SPI above 26 MHz, GPIO18/19/23/5 are spoken
   for (rule 5).
3. **Then outputs that must be quiet at boot.** GPIO2 is driven briefly by the bootloader
   and carries the LED; GPIO1 emits the ROM log at 115200 baud. A relay or a MOSFET gate
   on either clicks on every reset.
4. **Everything else** goes on 4, 13, 14, 16, 17, 19, 21, 22, 23, 25, 26, 27.

The six extra pins over a 30-pin board are **not** spare capacity (rule 1). If you run out
of GPIOs, reach for an I2C expander.

## When the task is "the board does not work"

Diagnose in this order — each stage only depends on the ones before it:

1. **Is anything wired to the six pins by the USB connector?** Unplug them first (rule 1).
   On this board size that is the failure the 30-pin owners never see.
2. **Does the port enumerate?** No port ⇒ cable (try a known data cable) or bridge driver.
3. **Does anything print at 115200?** The ROM bootloader's `rst:0x1 (POWERON_RESET)`
   banner comes out before any of your configuration applies. If that is absent but the
   port exists, suspect the cable's data lines or a wedged bridge chip.
4. **Does it print but say nothing after?** Rule 2 — the console is misconfigured, or
   MTDO/GPIO15 is strapped low.
5. **Does it boot-loop?** Read `rst:`. `ESP_RST_BROWNOUT` ⇒ rule 7. `RTCWDT_RTC_RESET` or
   `invalid header` ⇒ `esptool.py erase_flash` and reflash the minimal variant.
6. **Does it reach `app_main()` and then misbehave?** Flash `--minimal` to establish a
   baseline, then add one subsystem at a time.

`template/src/board_report.c` runs steps 4–6 for you and is worth pasting into any project
that is misbehaving — it prints the strapping latches, the reset reason, the flash size
and the partition table.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/esp32-wroom-36pin/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — LED blink on GPIO2 + console heartbeat. **144,493 B flash, 13,380 B RAM.**
  Flash this first on a board you have not used before: it touches only the toolchain, the
  flashing route, UART0 and one GPIO, and its two outputs fail independently — LED but no
  console means the serial side, console but no LED means this board has no LED on GPIO2.
  It also prints `BOARD_NAME`, so a board-size mismatch shows up immediately.
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
ADC2/Wi-Fi conflict of rule 4 instead of reporting it.

## Flashing

```sh
pio run -t upload -t monitor
```

The bridge chip's DTR/RTS drive EN and GPIO0 through the two-transistor auto-reset circuit,
so esptool handles the whole cycle with no buttons — and because GPIO0 is not on this
header (rule 14), nothing of yours can interfere. The board appears as
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
(rule 3). In practice you debug this board over UART0.

## Reporting

Say what is verified and what is derived — this skill has more derived content than its
siblings, because Espressif never made a 36-pin board.

**Certain:** the GPIO *set* on the header, and everything each GPIO can do. Those come
from the ESP32-WROOM-32 and ESP32 datasheets, not from the board.

**⚠︎ Derived:** the physical *row order* in `reference/board-hardware.md` §2 is
reconstructed from the confirmed 30-pin layout, the Espressif-documented 38-pin layout, and
vendor descriptions of the DOIT board's six extra pins. It is marked as such in that file.
Nothing in the code depends on it — only on which GPIO a silkscreen label means, which is
certain. Point the user at the check in §2.1 rather than asserting the row order.

**Verified on this machine:** both template variants build clean with PlatformIO 6.1.19 /
platform-espressif32 7.0.1 / ESP-IDF 6.0.1, zero warnings, at the flash and RAM figures
quoted above. Every recipe in `reference/recipes.md` was compiled in the same project. The
26.67 MHz SPI figure is computed from `_GPIO_MATRIX_DELAY_NS` in the IDF HAL.

**Not verified on hardware:** none of this was run on a physical board in this session.
Vendor-dependent details — the user LED's pin and polarity, which bridge chip is fitted,
whether the auto-reset transistors are present — are stated for the DOIT/CH340 mainstream
and should be confirmed with the `--minimal` variant.
