---
name: esp32p4-jc-m3-dev
description: Firmware development for the Guition JC-ESP32P4-M3-DEV board (JC-ESP32P4-M3 module — ESP32-P4NRW32 with 32 MB in-package PSRAM, 16 MB flash, and an ESP32-C6 radio co-processor on SDIO) — its MIPI-DSI panel and MIPI-CSI camera connectors, IP101 Ethernet, ES8311 codec with speaker amplifier and microphone, microSD on SDMMC, RS485, three USB-C sockets, the 26-pin JP1 header, and the PlatformIO + ESP-IDF (pioarduino) setup around them. Use when working on this board or any ESP32-P4 devkit: project setup, platformio.ini and sdkconfig.defaults, the silicon revision split between rev v1.3 and rev v3.x, why the bootloader dies with "Illegal instruction", the pin map and which header pins are traps, Wi-Fi through esp_hosted and esp_wifi_remote, mounting the TF card and its on-chip LDO, bringing up a DSI panel, the shared I2C bus, audio direction and the amplifier enable, Ethernet RMII pins, choosing a free GPIO, flashing over any of the three USB-C ports, or debugging why something on the board does not work.
---

# Guition JC-ESP32P4-M3-DEV

An ESP32-P4 carrier with a MIPI display connector, a MIPI camera connector, Ethernet,
audio, a card slot, RS485, a battery charger and **three USB-C sockets** — and a radio
that is a second chip you talk to over SDIO.

Three things shape almost every task on this board. First, **the P4 has no radio**:
`esp_wifi` is not the API, `esp_wifi_remote` over `esp_hosted` is, and the ESP32-C6 inside
the module needs its own firmware. Second, **the silicon comes in two mutually incompatible
revisions** and ESP-IDF defaults to the wrong one for the boards being sold — the symptom
is a bootloader that dies with `Illegal instruction`, which reads like corrupt flash.
Third, **there is no user LED**: the only thing that lights is the power indicator, so a
board that "does nothing" is usually a board that is running fine with its console on the
wrong socket.

- `reference/board-hardware.md` — the board: the module pin list transcribed from the
  schematic, every P4 GPIO and what this board did with it, the JP1 header with its four
  trap positions, power tree, Ethernet, I2C, audio, storage, the silicon-revision table,
  the toolchain including the PlatformIO/SCons breakage, flashing, and a
  symptom → cause → fix table.
- `reference/esp32p4-soc.md` — the silicon: why no GPIO is stolen by flash, PSRAM, MIPI or
  the HS USB PHY; strapping pins; ADC1/ADC2 and touch maps; the three USB controllers;
  what the P4 has that no other ESP32 does.
- `reference/esp32-family.md` — the rest of the family, for "should this be a different
  ESP32?" questions.
- `reference/recipes.md` — code: `platformio.ini`, `sdkconfig.defaults`, silicon-revision
  and PSRAM detection, SDMMC with the on-chip LDO, I2C, ES8311, MIPI-DSI, GT911, Ethernet,
  RS485, a 16 MB partition table, the die temperature sensor, Wi-Fi through the C6. Each
  recipe says whether it was compiled.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script.
  See `template/README.md`.

## Confirm the board first

| | This board | Not this board |
|---|---|---|
| Module lid | `JC-ESP32P4-M3 · GUITION` · `SOC: ESP32P4NRW32` · `32M PSRAM` · `Flash: 16M` · `WiFi: ESP32-C6` | anything reading `ESP32-P4-WROOM`, `ESP32-P4-MINI`, or a Waveshare / M5Stack part number |
| Size and ports | 92 × 62 mm · **three USB-C** · RJ45 · TF slot · two 15-way FPC · 2×13 header on the left edge | ESP32-P4 Function EV Board (two USB-C, different header), Waveshare ESP32-P4-NANO, M5Stack Tab5 |
| Display | **none on board** — a DSI panel plugs into J2 | boards with a bonded panel |

Then find the **silicon revision**, because it changes the build:

```sh
esptool --chip esp32p4 -p <port> flash-id    # prints "revision v1.3" or "revision v3.x"
```

(esptool 5.3.0 ships with the pinned platform: commands are hyphenated and the binary is
`esptool`. `esptool.py` and `flash_id` still work, with a deprecation warning.)

Guition's own download archive splits every demo into `P4_V1.3` and `P4_V3X` for this
reason. Everything below assumes **rev v1.3** (rev < 3.0) and says where rev ≥ 3.0 differs.

## Orientation

| | |
|---|---|
| Module | **JC-ESP32P4-M3** (Guition). ESP32-P4NRW32, 2 × RISC-V HP + 1 LP |
| Clock | **360 MHz** on rev < 3.0 · 400 MHz on rev ≥ 3.0 · 40 MHz crystal |
| Memory | **16 MB** NOR flash · **32 MB in-package PSRAM** (HEX, 200 MHz) · 768 KB L2MEM · 32 KB LP SRAM · 8 KB TCM |
| Radio | **none on the P4.** An **ESP32-C6** inside the module, over **SDIO slot 1, 4-bit, 40 MHz**: CLK **18** · CMD **19** · D0-D3 **14 15 16 17** · reset **54, active high**. Driven by `esp_hosted` + `esp_wifi_remote` |
| GPIOs | **GPIO0-GPIO54, all of them exist.** Flash, PSRAM, MIPI DSI/CSI and the HS USB PHY are all on **dedicated pins** and cost nothing |
| Free GPIO | exactly eleven, all on JP1: **1, 2, 3, 4, 5, 20, 32, 33, 45, 46, 47** (+ 21, 22, 23 if you never fit a panel) |
| LED | **none on a GPIO.** D1 is a power indicator on the 3V3 rail |
| Buttons | **SW1** → `BOOTMODE` = **GPIO35**, active low, 10 K pull-up · **SW2** → `CHIP_PU` (reset) |
| Display | **MIPI-DSI**, 2 lanes, J2 15-way FPC. **No reset pin, no backlight pin.** PHY power = **on-chip LDO channel 3 @ 2500 mV** |
| Camera | **MIPI-CSI**, 2 lanes, J3 15-way FPC. SCCB on the shared I2C. No reset, no power-down |
| I2C | **one bus, everything on it**: SDA **7** · SCL **8**, 5.1 K pull-ups on board. ES8311 `0x18` · panel brightness `0x45` · GT911 `0x5D`/`0x14` · camera SCCB |
| Audio | MCLK **13** · BCLK **12** · LRCK **10** · **DOUT 9** (→ speaker) · **DIN 48** (← microphone) · **PA enable GPIO11, active high, 10 K pull-down** |
| Storage | microSD on **SDMMC slot 0, 4-bit**: CLK **43** · CMD **44** · D0-D3 **39 40 41 42**. **No CS, no card-detect.** VDD from **on-chip LDO channel 4** |
| Ethernet | IP101 PHY (per Guition's sdkconfig; the schematic says only `U4`), RMII, PHY address **1**. CRS_DV **28** · RXD0 **29** · RXD1 **30** · TXD0 **34** · TXD1 **35** · TXEN **49** · CLK **50** · MDC **31** · MDIO **52** · reset **51** — **exactly ESP-IDF 5.5.5's P4 defaults**, so no pin config is needed |
| RS485 | MAX485 on **UART1**: TX **26** · RX **27**. **DE//RE is generated in hardware — there is no direction GPIO** |
| USB | **three USB-C**: native USB-Serial/JTAG (GPIO24/25, `303A:1001`) · **CH340C** onto UART0 (GPIO37/38) · **USB 2.0 OTG HS** on dedicated pins |
| Battery | Li-ion on CN4, IP5306 charger. Sensed on **GPIO53** (`ADC2_CH4`) through a 68 K/100 K divider |
| Toolchain | PlatformIO Core 6.2.0 + **pioarduino** `platform-espressif32` **55.03.311** (ESP-IDF **5.5.5**) + `board = esp32-p4-evboard` + `framework = espidf`. **The official `espressif32` platform cannot build for the P4 at all** |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y` in
   `sdkconfig.defaults`, or a `V1.3` board does not boot at all.** ESP-IDF 5.5 defaults to
   building for rev ≥ 3.1, and `Kconfig.hw_support` says outright: *"Support of ESP32-P4
   rev. <3.0 and >=3.0 is mutually exclusive"*. Without the two lines the second-stage
   bootloader dies immediately after the `entry 0x...` line with **`Guru Meditation Error:
   Illegal instruction`** — which points at corrupt flash, a bad build, anything but a
   Kconfig setting. On a rev ≥ 3.0 board it is the reverse: `CONFIG_ESP32P4_REV_MIN_301=y`.
   In Arduino the same choice is the board name: `esp32-p4-evboard` (ES, < 3.0) versus
   `esp32-p4_r3-evboard` (≥ 3.0).

2. **`platform = espressif32` does not work. Pin the pioarduino URL.** The official
   PlatformIO platform, including 7.1.x, is on Arduino core 2.0.17 and carries **no
   ESP32-P4 board definition whatsoever**. The error is `Unknown board ID
   'esp32-p4-evboard'`, which reads like a typo. Use
   `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip`.

3. **`ModuleNotFoundError: No module named 'SCons.Tool.FortranCommon'` at link time is a
   packaging conflict, not your code.** PlatformIO Core 6.2.0 wants `tool-scons
   ~4.41101.0`; pioarduino pins `4.40801.0`, so the platform **deletes
   `~/.platformio/packages/tool-scons` mid-build**, removing SCons from under the running
   process. Either `uv pip install pioarduino` (their Core 6.1.19) or patch one word in
   `~/.platformio/platforms/espressif32/platform.json` — `board-hardware.md` §12.1 has the
   `sed` line. The patch is undone by any platform update.

4. **PlatformIO builds the partition image from `board_build.partitions` in
   `platformio.ini`, not from `CONFIG_PARTITION_TABLE_*`.** Setting the Kconfig option and
   expecting the image to follow gives you a 1 MB `factory` while the config says
   `SINGLE_APP_LARGE`. Keep both lines in agreement. And check what you actually got:
   `partitions_singleapp_large.csv` yields a **1,536 KB** app slot, stranding 14 MB of a
   16 MB part. `recipes.md` §12 has a 16 MB CSV that builds.

5. **There is no user LED on this board.** Only D1, a red power indicator on the 3V3 rail,
   not on a GPIO. So blink is not a valid first test unless you wire an LED to JP1 — and
   worse, in `GPIO_MODE_INPUT_OUTPUT` the input buffer reads the **pad**, and an unloaded
   pad follows its own driver, so **a passing read-back proves the pin drives, never that
   it reaches anything**. A self-test can report a pin working perfectly while it is
   wired to the Ethernet PHY and fighting it. Pick from the eleven free JP1 pins in the
   orientation table, and treat "the console prints" as the real first test.

6. **Four of the twenty-six JP1 positions belong to the ESP32-C6, not the P4.** Pins 20,
   22, 24 and 26 are the radio's own `U0RXD`, `U0TXD`, `IO9` boot strap and `CHIP_PU`,
   brought out so the C6 can be reflashed. Driving any of them takes Wi-Fi down mid-run;
   pin 26 holds the radio in reset permanently. Pin 8 is not connected at all.

7. **`esp_wifi_init()` alone is not enough, and a failure there is usually not a Wi-Fi
   problem.** The P4 has no radio. Add `espressif/esp_wifi_remote: ^0.14.2` to
   `idf_component.yml` (it pulls `esp_hosted` 2.0.13), configure the SDIO link — slot 1,
   4-bit, 40 MHz, CLK 18 / CMD 19 / D0-D3 14-17, **reset GPIO 54 with
   `CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH=y`** — and make sure the C6 is actually
   running slave firmware. A blank or held-in-reset C6 surfaces as a transport timeout
   inside `esp_hosted` init, not as an `esp_wifi` error.

8. **The microSD card is powered by the P4's own on-chip LDO channel 4.** Without
   `sd_pwr_ctrl_new_on_chip_ldo(.ldo_chan_id = 4)` on the `sdmmc_host_t` the card is simply
   unpowered and the mount times out, which reads as a bad card or bad wiring. It is
   **SDMMC slot 0**, the IO MUX slot, so the pins are *not* passed in the slot config —
   that absence is correct, not an omission. There is no chip select (it is not SPI), no
   card-detect and no write-protect: pass `SDMMC_SLOT_NO_CD` and `SDMMC_SLOT_NO_WP`.

9. **Acquire on-chip LDO channel 3 at 2500 mV before any MIPI-DSI call.** It powers
   `VDD_MIPI_DPHY`. Skip it and `esp_lcd_new_dsi_bus` fails or the panel stays dark with no
   useful error. `esp_ldo_acquire_channel({.chan_id = 3, .voltage_mv = 2500})`.

10. **GPIO11 low means silence whatever else is correct.** The NS4150 amplifier's enable is
    active high and held low by a 10 K pull-down (R19), so audio is muted until firmware
    raises it. Raise it *before* initialising the ES8311, or the first frames pop.

11. **I2S DOUT is GPIO9 and DIN is GPIO48, from the host's point of view.** The schematic
    names both from the *codec's* side, where the host's output is called `DSDIN` — which
    reads backwards. Worse, the net on GPIO48 is named **`ES7210_SDOUT`** and **there is no
    ES7210 on this board**: it is the ES8311's own ADC output feeding the on-board analog
    microphone. Getting the direction wrong gives a silent speaker and a microphone that
    records nothing, with the codec answering normally on I2C.

12. **There is exactly one I2C bus and it is already busy.** `SDA = 7`, `SCL = 8`, with
    5.1 K pull-ups on board. The ES8311 (`0x18`), the DSI panel's brightness register
    (`0x45`), a GT911 touch controller (`0x5D` or `0x14`) and the camera's SCCB all live on
    it, and it also reaches CN3 and JP1 pins 23/25. Nothing on this board defaults to
    GPIO7/8 in any framework, so a scan that finds nothing is almost always the pins. Do
    not open a second bus on the same pins for the touch controller — pass the same handle.

13. **Ethernet needs no pin configuration on this board, and the RMII pins are not
    free-for-all either.** `ETH_ESP32_EMAC_DEFAULT_CONFIG()` for the P4 in ESP-IDF 5.5.5
    already defaults to this board's exact wiring — `mdc 31`, `mdio 52`, clock
    `EMAC_CLK_EXT_IN` on GPIO50, `tx_en 49`, `txd0 34`, `txd1 35`, `crs_dv 28`, `rxd0 29`,
    `rxd1 30` — which is why Guition's demo sets only MDC and MDIO, to the values they
    already had. If you *do* move one: the RMII signals are **IO MUX pads with a short
    per-signal list** (`CRS_DV` 28/45/51, `RXD0` 29/46/52, `RXD1` 30/47/53, `TXD0` 34/41,
    `TXD1` 35/42, `TX_EN` 33/40/49, clock-in 32/44/50), set through
    `emac_dataif_gpio.rmii`; only MDC, MDIO and the PHY reset go through the GPIO matrix
    and can be anywhere. PHY address is **1** and the part is an **IP101** — both from
    Guition's demo sdkconfig, not from the schematic, which labels the PHY only `U4`.
    Finally, **`RMII_TXD1` is GPIO35, the boot strap and SW1**: holding SW1 at reset is
    download mode, which is fine, but anything else pulling GPIO35 low through reset looks
    like "the firmware stopped running".

14. **RS485 has no direction GPIO and you should stop looking for one.** DE and /RE are
    generated in hardware from the TX line by a 74LVC1G132 + an 8550 PNP one-shot. Pass
    `RTS = UART_PIN_NO_CHANGE`; Guition's own demo then still calls
    `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)`, which is harmless.

15. **Match `CONFIG_ESP_CONSOLE_*` to the socket you plugged into.** Three USB-C ports, two
    of which flash the board: the native USB-Serial/JTAG (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`,
    enumerates `303A:1001`) and the CH340C bridge onto UART0 (`CONFIG_ESP_CONSOLE_UART_DEFAULT`,
    needs the CH340 driver on the host). Pick the wrong one and the board flashes, runs,
    and prints nothing — with no LED to check instead.

16. **Deleting or forgetting `sdkconfig.defaults` breaks two things silently.** Without
    `CONFIG_SPIRAM` the PSRAM component is compiled out entirely, so the failure is a
    *linker* error — `undefined reference to esp_psram_get_size` — rather than a runtime
    zero. And without the revision lines of rule 1 the board stops booting. Both were
    observed in this session.

17. **The die temperature sensor's range must be contained in one row of the P4 range
    table** (`{50..125} {20..100} {-10..80} {-30..50} {-40..20}`).
    `TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80)` works; a plausible `(-10, 100)` is inside
    none of them, and `temperature_sensor_install()` returns `ESP_ERR_INVALID_ARG` with the
    log line "Out of testing range" — which reads like a missing peripheral rather than a
    bad argument.

18. **Feed it more than 500 mA.** Guition's own burning instructions say so. The speaker
    amplifier and the backlight boost run from the IP5306's `VOUT-BAT` rail, so a thin
    cable shows up as distorted audio and random resets before it shows up as a power
    problem.

## When the task is choosing a pin

Count first, because the answer is smaller than the chip suggests. The P4 has 55 GPIOs and
**none of them are eaten by flash, PSRAM, MIPI or the HS USB PHY** — all of those are on
dedicated silicon pins, so the S3 reflex of "six pins belong to the flash, never touch
them" does not apply. This *board*, however, spends almost everything:

| GPIO | Note |
|---|---|
| 1, 2, 3, 4, 5 | JP1 pins 7, 9, 11, 13, 15. LP/RTC-capable, touch channels. 2-5 are the pad-JTAG pins at reset |
| 20 | JP1 pin 17, `ADC1_CH4`. The cleanest pin on the header — no strap, no alternate use |
| 32, 33 | JP1 pins 19, 21 |
| 45, 46, 47 | JP1 pins 14, 12, 10. They are SD1 `D4`-`D6`, used only by 8-bit SD, which this board cannot do |

Then the ones you buy by giving something up: **39-44** cost the card · **9, 10, 11, 12,
13, 48** cost audio · **7, 8** are the shared bus · **26, 27** cost RS485 · **28, 29, 30,
31, 34, 35, 49, 50, 51, 52** cost Ethernet · **21, 22, 23** cost a panel's touch and
backlight · **37, 38** cost the UART console · **24, 25** cost the native USB port.

Never available: **14-19** and **6** (the C6 link, and 14-19 do not even reach a module
pin), **53** (battery divider), **54** (the C6's enable).

`36` is free and has a 10 K pull-up, but reaches no connector — module pad only.

If eleven pins is not enough, the answer is an **I2C expander on JP1 pins 23/25 or CN3**,
at an address outside `0x18`, `0x45`, `0x5D`, `0x14`.

## When the task is "the board does not work"

Diagnose in this order; each step only depends on the ones before it.

1. **Does the ROM banner (`ESP-ROM:esp32p4...`) appear?** It comes out before any of your
   configuration applies. No banner ⇒ cable, wrong socket, or something holding GPIO35 low.
2. **Does anything of yours print?** If the banner appears and your output does not, this
   is rule 15 — the console is built for the other socket. Check which port enumerated:
   `303A:1001` is the native one, a `1A86:*` is the CH340C.
3. **Does it die right after `entry 0x...` with `Illegal instruction`?** Rule 1. Stop here;
   nothing downstream matters.
4. **What does `esp_chip_info().revision` say?** `< 300` is the pre-rev-3 part. Confirm it
   matches the sdkconfig you built with.
5. **Does `esp_psram_get_size()` return 32 MB?** If the symbol does not *link*,
   `CONFIG_SPIRAM` is missing (rule 16). If it returns 0, the same.
6. **Wi-Fi failing?** Rule 7, in that order: component versions, SDIO slot and pins, reset
   polarity on GPIO54, then whether the C6 has firmware at all.
7. **Card not mounting?** Rule 8, in that order: LDO channel 4, then SDMMC not SPI, then
   slot 0, then `width = 4`, then FAT32, then another card.
8. **Panel dark?** Rule 9 first (LDO channel 3), then the panel's own timings — and note
   that Guition's archive ships two *different* sets of porches for the same 800 × 1280
   panel; see `recipes.md` §8.
9. **Audio silent?** GPIO11 first (rule 10), then the DOUT/DIN direction (rule 11).
10. **I2C scan empty?** Rule 12 — the pins. If some addresses answer and one does not, it
    is that device, not the bus.
11. **Ethernet link down?** Rule 13: MDC 31, MDIO 52, reset 51, PHY address 1.
12. **Still lost?** Flash `--minimal`. It touches only the toolchain, the silicon revision,
    the console route and the PSRAM mapping, so it separates "wrong build configuration"
    from "wrong peripheral code" without guessing.

`template/src/board_report.c` prints the revision, PSRAM state, flash size, reset reason,
measured clock and die temperature, which answers steps 4-5 in one paste.

## Starting a new project

Do not hand-assemble one: the `platformio.ini` has a platform URL that cannot be guessed
and the `sdkconfig.defaults` has two lines without which the board does not boot. Scaffold
from `template/`:

```sh
~/.claude/skills/esp32p4-jc-m3-dev/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — silicon revision, flash size, PSRAM check, USB console, blink on a free JP1
  pin. **207,224 B flash, 17,220 B RAM.** Flash this first on a board you have not used
  before: with no user LED it is the only way to separate a wrong build configuration from
  wrong peripheral code.
- `--full` (default) — board self-test: chip and revision, MAC, reset reason, the core
  clock **as measured**, flash and JEDEC id, partition layout, internal and PSRAM heaps,
  PSRAM/SRAM/CPU benchmarks, die temperature, an I2C scan, a GPIO read-back, then live
  telemetry once a second. **244,198 B flash, 17,520 B RAM.**

Both build with zero warnings at the toolchain versions in the orientation table. Nothing
is generated and no paths are embedded, so copying `template/` by hand works identically.
`template/README.md` maps files to subsystems.

The display, camera, audio, Ethernet, RS485 and Wi-Fi are deliberately **not** in the
template — they need hardware this board does not include, and each is in
`reference/recipes.md` marked with whether it was compiled.

When the user already has a project, prefer bringing their `platformio.ini` and
`sdkconfig.defaults` in line with the template over rewriting their code — on this board
that is where the failures live.

Arduino is a supported alternative (`framework = arduino`, arduino-esp32 3.2.x for rev
v1.3 and 3.3.11 for rev v3.x, board `ESP32P4 Dev Module` in the Arduino IDE), and every pin
rule above applies unchanged. Under Arduino the board name *is* the revision selector
(rule 1), so it matters more, not less.

## Flashing

```sh
pio run -t upload -t monitor
```

Three USB-C sockets; two of them can flash the board.

| | native USB-Serial/JTAG | CH340C | HS USB OTG |
|---|---|---|---|
| Enumerates | `303A:1001` · `/dev/cu.usbmodem*` · `/dev/ttyACM*` | `1A86:*` · `/dev/cu.usbserial*` · `/dev/ttyUSB*` | nothing, until firmware configures it |
| Console option | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` | `CONFIG_ESP_CONSOLE_UART_DEFAULT` | — |
| Auto-reset | yes | yes, RTS/DTR onto `BOOTMODE`/`CHIP_PU` | — |
| Host driver | none | **CH340 driver required** | — |

When upload fails, the manual sequence is:

> **hold SW1 (`BOOTMODE`) → tap SW2 (reset) → release SW1 → upload**

**Bad firmware cannot brick this board.** The ROM bootloader is mask ROM and always answers
that sequence:

```sh
esptool --chip esp32p4 -p <port> flash-id       # chip banner with revision, and 16 MB
esptool --chip esp32p4 -p <port> chip-id
esptool --chip esp32p4 -p <port> erase-flash
```

esptool **5.3.0** comes with the pinned platform; it renamed `esptool.py` to `esptool` and
the underscore commands to hyphens, keeping the old spellings working with a warning.

The one genuinely unrecoverable mistake is burning `EFUSE_0PXA_TIEH_SEL_0`, which switches
`VDDO_FLASH` to 1.8 V permanently on a 3.3 V flash. eFuses are one-time. Do not run
`espefuse` on this board.

## Reporting

Say what is verified and what is derived.

**Verified on hardware** (a rev v1.3 board, by the project this skill was extracted from):
the silicon-revision trap of rule 1 and its exact `Illegal instruction` symptom; the SCons
packaging conflict of rule 3 and the manifest patch that fixes it; the partition-table
source of rule 4; the temperature-sensor range trap of rule 17; that the board reports
ESP32-P4 rev v1.3, 32 MB PSRAM, 16 MB flash (JEDEC `0x684018`), 360 MHz measured; and the
memory bandwidth figures in `board-hardware.md` §10.

**Verified against ESP-IDF 5.5.5 itself** (the copy the pinned platform installs), not
just against the vendor's demos: the revision Kconfig and its "mutually exclusive" wording;
that `ESP32P4_REV_MIN_301` is the default; the SDMMC slot-0 IO MUX pins (`sdmmc_pins.h`
gives CLK 43, CMD 44, D0-D3 39-42, D4-D7 45-48 — the board's wiring exactly) and that slot 1
does not use IO MUX, which is why the C6 sits there; `sd_pwr_ctrl_by_on_chip_ldo.h`, whose
own comment names LDO channel 4 for SDMMC IO; `esp_ldo_regulator.h` and its four channels;
the P4 temperature range table; `ADC2_CHANNEL_4_GPIO_NUM 53`; `SOC_GPIO_PIN_COUNT 55`,
`SOC_I2S_NUM 3`, `SOC_TWAI_CONTROLLER_NUM 3`, `SOC_TOUCH_SENSOR_NUM 14`; the RMII pad lists
in `emac_periph.c`; and that `ETH_ESP32_EMAC_DEFAULT_CONFIG()` for the P4 is this board's
pin set. The esptool command spellings were checked by running the 5.3.0 binary.

**Verified in this session:** both template variants build clean with zero warnings at
PlatformIO Core 6.2.0 / pioarduino 55.03.311 / ESP-IDF 5.5.5, at the flash and RAM figures
quoted above; the generated sdkconfig carries the revision, PSRAM and console settings; the
16 MB partition CSV in `recipes.md` §12 builds and produces the partition sizes it claims;
and a missing `CONFIG_SPIRAM` fails at link, not at runtime (rule 16).

**Schematic-derived, high confidence.** Every pin in `board-hardware.md` §2 is traced on
Guition's own schematic sheets. That includes the facts most likely to be got wrong
elsewhere: the JP1 pinout and its four ESP32-C6 positions; that GPIO14-19 never reach a
module pin; that the TF card's VDD comes from on-chip LDO channel 4 through an
always-on switch (`R10` unpopulated, so GPIO45 is free); that `PA_CTRL` has a 10 K
pull-down; that RS485 direction is a hardware one-shot with no GPIO; and the ten Ethernet
RMII pins.

**Vendor code, high confidence.** The SDIO link parameters, the SD slot and LDO channel,
the DSI LDO channel and voltage, the I2C addresses, the ADC unit/channel for the battery
and the Ethernet MDC/MDIO/PHY-address all come from Guition's own demos in the product
archive, most of which vendor Espressif's `esp32_p4_function_ev_board` BSP unmodified.

**Primary-source.** The GPIO inventory, strapping tables, boot modes, ADC channel maps and
the three USB controllers in `reference/esp32p4-soc.md` are from the ESP32-P4 Series
Datasheet v0.5 — which is marked **PRELIMINARY** on every page.

**Corrected during a second verification pass — do not reintroduce these:**
- The RMII data pins are **not** "fixed in copper". They are IO MUX pads with a short list
  of alternatives each, selectable through `emac_dataif_gpio.rmii`; the board simply uses
  the same set ESP-IDF already defaults to.
- `temperature_sensor_install()` fails with **`ESP_ERR_INVALID_ARG`** ("Out of testing
  range"), not `ESP_ERR_NOT_FOUND`.
- A pin in `GPIO_MODE_INPUT_OUTPUT` reads the **pad**, not an output latch. The distinction
  matters: on an unloaded pad the read-back still passes, which is why the GPIO28 mistake
  went unnoticed.
- **IP101** and **PHY address 1** come from Guition's demo sdkconfig. The schematic labels
  the PHY only `U4`, and which strap resistors set the address is not shown.
- esptool 5.3.0 uses `esptool` and hyphenated commands (`flash-id`, `chip-id`,
  `erase-flash`).
- The ESP32-P4 datasheet describes **no CAN FD** — three TWAI controllers, CAN 2.0, up to
  1 Mbit/s — and ESP-IDF defines no FD capability for it, contradicting the CAN FD row in
  `reference/esp32-family.md`, which is second-hand from Espressif's product selector.
- The C6 variant inside the module is not named anywhere in Guition's material; say
  "an ESP32-C6", not a specific part number.

**Known contradictions — quote them as contradictions, not as facts:**
- **Guition's xiaozhi board file defines `BOOT_BUTTON_GPIO` as 21.** GPIO21 is the touch
  panel's interrupt line. The boot button is SW1 on **GPIO35**. Do not copy that define.
- **The schematic net `ES7210_SDOUT` names a chip this board does not have** (rule 11).
- **Two different DSI porch sets** for the same 800 × 1280 panel ship in the same archive —
  Guition's xiaozhi board file and Espressif's BSP disagree. `recipes.md` §8.
- **The specification PDF claims "up to 400 MHz"**; a rev v1.3 board runs 360, and reports
  360.
- **The Getting Started PDF is for a different Guition board** (`JC1060P470`) and says so
  in its own text. Its Arduino IDE steps still apply.

**Not verified anywhere.** No DSI panel, camera, SD card, Ethernet cable, RS485 device,
speaker, microphone or battery was attached in this session, and nothing was flashed here.
The display, audio, storage, Ethernet, RS485, battery and Wi-Fi recipes are marked
"not compiled in this session" individually — say so rather than presenting them as
certain.
