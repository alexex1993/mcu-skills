# ESP32-WROOM-32 devkit, 38-pin — board reference

Part I is the board: what is wired where, and what it costs you. Part II is the
development guide: toolchain, flashing, peripheral recipes and the pitfall table.

Chip-level facts (ADC channels, strapping semantics, IO_MUX, power modes, memory map)
are in `esp32-soc.md` and are not repeated here.

---

# Part I — the board

## 1. Which board this is

The 38-pin ESP32 devkit is the reference form factor. Unlike the 30- and 36-pin variants
it has an authoritative source: **Espressif's own ESP32-DevKitC V4**, documented in
*esp-dev-kits* with a complete J2/J3 header table. The clones copy it pin for pin.

Boards that use this exact layout:

| Board | Vendor | Notes |
|---|---|---|
| **ESP32-DevKitC V4** | Espressif | the reference. **No user LED.** Micro-B USB, CP2102N |
| **NodeMCU-32S** | Ai-Thinker | ESP-WROOM-32S module; its datasheet numbers the same 38 pins 1–38 |
| **DOIT ESP32 DevKit V1, 38-pin** | DOIT | user LED on GPIO2 |
| unbranded "ESP32 38 pin" | — | CH340 or CP2102, micro-B or Type-C |

The header table in §2 is transcribed from the Espressif and Ai-Thinker documents, which
agree with each other pin for pin. It is the most trustworthy pin map of the three board
sizes.

**How to tell a 38-pin board apart:** 19 pins per side, and six pins near the USB connector
labelled with the flash bus. On the Espressif silkscreen those read `D2 D3 CMD` on one side
and `D1 D0 CLK` on the other; on Ai-Thinker/DOIT boards the same pins read `SD2 SD3 CMD`
and `SD1 SD0 CLK`. Both boards also break out `IO0`, which the 30- and 36-pin boards do
not. The board is too wide for a standard breadboard — it covers both rails.

### The silkscreen trap

On an Espressif-style 38-pin board, **`D2` means GPIO9 and `D0` means GPIO7.** On a
DOIT-style board, `D2` means GPIO2 and `D4` means GPIO4. The same two characters mean
different pins depending on who printed the board.

| Label | Espressif DevKitC V4 | DOIT-style |
|---|---|---|
| `D0` | GPIO7 (flash SD0) | — (GPIO0 is labelled `IO0`) |
| `D1` | GPIO8 (flash SD1) | — |
| `D2` | **GPIO9** (flash SD2) | **GPIO2** |
| `D3` | GPIO10 (flash SD3) | — |
| `D4` | — | GPIO4 |

Wiring "D2" from a tutorial written for the other board puts your signal on the flash bus.
Always resolve a silkscreen label to a GPIO number before writing code; §2 does that.

## 2. Header pin map

Oriented with the **USB connector at the bottom**. Numbering follows Espressif's J2 (left)
and J3 (right), which matches Ai-Thinker's 1–38 running down the left and back up the
right.

| J2 # | Left column | GPIO | Notes | | J3 # | Right column | GPIO | Notes |
|---|---|---|---|---|---|---|---|---|
| 1 | **3V3** | — | LDO output | | 1 | **GND** | — | |
| 2 | **EN** | — | CHIP_PU. Not a GPIO | | 2 | **IO23** | 23 | VSPID (MOSI), IO_MUX |
| 3 | **VP** | 36 | input-only, ADC1_CH0, RTC | | 3 | **IO22** | 22 | VSPIWP; default I2C SCL |
| 4 | **VN** | 39 | input-only, ADC1_CH3, RTC | | 4 | **TX** | 1 | UART0 TX → USB bridge |
| 5 | **IO34** | 34 | input-only, ADC1_CH6, RTC | | 5 | **RX** | 3 | UART0 RX ← USB bridge |
| 6 | **IO35** | 35 | input-only, ADC1_CH7, RTC | | 6 | **IO21** | 21 | VSPIHD; default I2C SDA |
| 7 | **IO32** | 32 | ADC1_CH4, TOUCH9, XTAL_32K_P, RTC | | 7 | **GND** | — | |
| 8 | **IO33** | 33 | ADC1_CH5, TOUCH8, XTAL_32K_N, RTC | | 8 | **IO19** | 19 | VSPIQ (MISO), IO_MUX |
| 9 | **IO25** | 25 | **DAC_1**, ADC2_CH8, RTC | | 9 | **IO18** | 18 | VSPICLK, IO_MUX |
| 10 | **IO26** | 26 | **DAC_2**, ADC2_CH9, RTC | | 10 | **IO5** | 5 | **strapping**; VSPICS0, IO_MUX |
| 11 | **IO27** | 27 | ADC2_CH7, TOUCH7, RTC | | 11 | **IO17** | 17 | UART2 TX. **WROVER: PSRAM** |
| 12 | **IO14** | 14 | ADC2_CH6, TOUCH6, MTMS/JTAG, RTC | | 12 | **IO16** | 16 | UART2 RX. **WROVER: PSRAM** |
| 13 | **IO12** | 12 | **strapping MTDI** — see below | | 13 | **IO4** | 4 | ADC2_CH0, TOUCH0, RTC |
| 14 | **GND** | — | | | 14 | **IO0** | 0 | **strapping + BOOT button** — see below |
| 15 | **IO13** | 13 | ADC2_CH4, TOUCH4, MTCK/JTAG, RTC | | 15 | **IO2** | 2 | **strapping**; user LED on clones |
| 16 | **D2** / SD2 | **9** | ⚠ **in-package flash** | | 16 | **IO15** | 15 | **strapping MTDO**; ADC2_CH3 |
| 17 | **D3** / SD3 | **10** | ⚠ **in-package flash** | | 17 | **D1** / SD1 | **8** | ⚠ **in-package flash** |
| 18 | **CMD** | **11** | ⚠ **in-package flash** | | 18 | **D0** / SD0 | **7** | ⚠ **in-package flash** |
| 19 | **5V** | — | 5 V in / out. See §4 | | 19 | **CLK** | **6** | ⚠ **in-package flash** |

All 32 module pads, plus EN, 3V3, 5V and three GND. Nothing on the WROOM-32 is hidden.

### The six pins you must not use

`D2`(9) `D3`(10) `CMD`(11) on the left, `D1`(8) `D0`(7) `CLK`(6) on the right — the six
nearest the USB connector — are the module's internal SPI flash bus. Espressif's own
footnote: *"They are grouped on both sides near the USB connector. Avoid using these pins,
as it may disrupt access to the SPI flash memory/SPIRAM."*

In practice "avoid" understates it. The CPU executes code out of that flash through those
wires. Attaching anything — even a scope probe with too much capacitance, certainly a
pull-up or an LED — produces one of: a board that hangs after the bootloader banner, a
board that boot-loops with `invalid header`, or, worst, a board that runs and corrupts
flash sectors under load.

**They are broken out for hardware bring-up, not for you.** Treat that whole end of the
header as mechanical support.

### GPIO0 is on this header, and that is a hazard

Unlike the 30- and 36-pin boards, the 38-pin header exposes **IO0** (J3 #14). GPIO0 is
simultaneously:

- the boot-mode strapping pin (low at reset ⇒ serial download mode),
- the BOOT button,
- one leg of the auto-reset circuit driven by the bridge chip's DTR/RTS.

Anything you connect there is in a three-way fight. An LED to ground is enough to hold it
below the threshold and drop the board into download mode on every reset — which presents
as "the board stopped running my firmware", not as a pin problem. A pull-up fights the
auto-reset circuit and breaks `pio run -t upload`.

If you need GPIO0 as an output (it is a legitimate ADC2/touch/RTC pin, and it can emit
CLK_OUT1), it must be high-impedance until well after boot. `board_report.c` prints the
GPIO0 strapping latch at every start so you can see when something is holding it.

### Pins by usability

```
Free for anything (15):     4 13 14 16 17 18 19 21 22 23 25 26 27 32 33
Input only, no pulls (4):   34 35 36(VP) 39(VN)
Strapping — usable with care (5):  0 2 5 12 15
UART0 to the USB bridge (2):       1(TX) 3(RX)
In-package flash — DO NOT USE (6): 6 7 8 9 10 11
```

GPIO12 (MTDI) is the one that silently damages boots: held high at reset it straps
VDD_SDIO to 1.8 V while the module's flash is a 3.3 V part. Symptoms look like flash
corruption. A 10 kΩ pull-down on GPIO12 makes it permanently safe.

### If the module is an ESP32-WROVER

The DevKitC V4 ships with a choice of modules, and the WROVER variants change two pins.
Espressif's footnote: *"GPIO16 and GPIO17 are available for use only on the boards with the
modules ESP32-WROOM and ESP32-SOLO-1. The boards with ESP32-WROVER modules have the pins
reserved for internal use."* On a WROVER they are the 8 MB PSRAM's CS and CLK.

So on a WROVER-populated 38-pin board:

- **IO16 and IO17 are gone.** UART2's default pins are gone with them; remap UART2 or use
  UART0.
- `CONFIG_SPIRAM=y` becomes correct instead of fatal, and you gain 4 MB of mappable
  external RAM.
- The rest of the header is unchanged.

Read the module can's silkscreen. `ESP32-WROOM-32`/`-32D`/`-32E`/`-32U` ⇒ 16/17 free;
`ESP32-WROVER`/`-B`/`-E`/`-IE` ⇒ 16/17 taken.

### Boot-time pin states

- **GPIO1 (TX)** carries the ROM log at 115200 baud unless MTDO/GPIO15 is strapped low.
- **GPIO0** is driven by the auto-reset circuit on every upload.
- **GPIO2** may be driven briefly by the bootloader.
- Everything else is an input with its reset-default pull for roughly the first 200 ms.

## 3. USB, the bridge chip and auto-reset

The ESP32 has no USB peripheral (see `esp32-soc.md` §1). The USB port is the bridge chip:

```
USB ── CP2102N (DevKitC) / CH340G (clones) ── UART0 (GPIO1 TX / GPIO3 RX)
            │  DTR ──┐
            │  RTS ──┤ two-transistor circuit ──> EN (CHIP_PU) and GPIO0
```

Ports appear as `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` (macOS), `/dev/ttyUSB0`
(Linux, needs `dialout`), or a `COM` port (Windows, WCH or Silicon Labs driver).

**No JTAG header, no USB-JTAG.** External JTAG means wiring a probe to GPIO12/13/14/15
(MTDI/MTCK/MTMS/MTDO) — and a probe that idles MTDI high stops the board booting. On this
board you debug over UART0.

If you do wire a probe: the board definition's `openocd_board` is `esp-wroom-32.cfg`, but
that file (shipped in `tool-openocd-esp32`) is upstream-deprecated — it just sets
`ESP32_FLASH_VOLTAGE 3.3` and sources `target/esp32.cfg`. Source `target/esp32.cfg` directly
with your interface config instead of chasing the deprecated board file.

### Note on C15 (early DevKitC V4 only)

Espressif documents a manufacturing issue on early ESP32-DevKitC V4 boards: a capacitor
`C15` that can make the board **boot into download mode by itself**, and that distorts a
clock output on GPIO0. The fix is to desolder C15. If a genuine Espressif DevKitC V4 keeps
landing in download mode with nothing attached to IO0, this is the first thing to check —
the clones do not have it.

## 4. Power

```
USB VBUS 5 V ──┬── 5V pin (bidirectional)
               └── LDO (AMS1117-3.3 or NCP1117) ── 3V3 pin ── ESP32-WROOM-32
```

Espressif's own warning applies to every clone too: **there are three mutually exclusive
ways to power the board — the USB port, the 5V/GND pins, or the 3V3/GND pins — and using
more than one at a time can damage the board or the supply.** No ORing diode is fitted.

| Rail | Direction | Notes |
|---|---|---|
| **5V** | in **or** out | ~5 V out when USB is connected. Feeding it while USB is plugged in back-feeds the host |
| **3V3** | out (in, if you must) | LDO output; budget ~600 mA for peripherals. Feeding it bypasses the LDO and back-powers the bridge chip |
| **GND** | — | three pins, all connected |

The 500 mA the datasheet demands is about the Wi-Fi TX burst, not the ~80 mA average. A
laptop USB-2 port or a thin cable produces the classic signature: everything works until
the first packet, then `ESP_RST_BROWNOUT`.

## 5. Onboard indicators and buttons

| Part | Wiring | Notes |
|---|---|---|
| Power LED | across 5V | on whenever the board is powered. Not controllable |
| User LED | **absent on the Espressif DevKitC V4**; **GPIO2, active high** on NodeMCU-32S and DOIT/CH340 clones | if the minimal template ticks but nothing lights up, your board has none — set `BOARD_HAS_USER_LED 0` |
| **BOOT** | GPIO0 to GND, external pull-up | pressed = low. Usable as a runtime input, with the caveats above |
| **EN / RST** | CHIP_PU to GND | not a GPIO, cannot be read |

## 6. Flash and partitions

4 MB in-package on GPIO6–11 (8 MB or 16 MB on some WROVER-E variants — check with
`esp_flash_get_size()`, which `board_report.c` prints). The template's `partitions.csv`:

| Partition | Type | Offset | Size |
|---|---|---|---|
| `nvs` | data/nvs | 0x9000 | 24 KB |
| `phy_init` | data/phy | 0xF000 | 4 KB |
| `factory` | app | 0x10000 | **1.75 MB** |
| `storage` | data/spiffs | 0x1D0000 | 2.19 MB |

The full self-test variant with the Wi-Fi stack links at 784 KB. For OTA, replace `factory`
with `ota_0`/`ota_1` at 1.5 MB each plus an `otadata`, and shrink `storage`.

---

# Part II — development guide

## 10. Toolchain

PlatformIO with the ESP-IDF framework. Verified combination:

| Component | Version |
|---|---|
| PlatformIO Core | 6.1.19 |
| `platform-espressif32` | 7.0.1 |
| ESP-IDF | 6.0.1 |
| `board` | `esp32dev` |
| Toolchain | `xtensa-esp32-elf` |

`board = esp32dev` rather than `esp32doit-devkit-v1`: both declare 4 MB flash and 320 KB
RAM, and `esp32dev` is what every ESP32 example assumes. The board definition only supplies
defaults that `platformio.ini` and `sdkconfig.defaults` override.

Arduino (`framework = arduino`) works and is a reasonable choice for this board — the pin
constraints in Part I apply identically, but the Arduino core's `analogRead()` hides the
ADC2/Wi-Fi conflict rather than reporting it.

## 11. sdkconfig that matters

From `template/sdkconfig.defaults`, with the reasoning:

```ini
CONFIG_IDF_TARGET="esp32"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y      # matches board_upload.flash_size in platformio.ini
CONFIG_ESPTOOLPY_FLASHMODE_DIO=y      # QIO needs all four data lines; WROOM-32 wires DIO
CONFIG_ESPTOOLPY_FLASHFREQ_40M=y      # 80M works on most boards but is not guaranteed
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESP_MAIN_TASK_STACK_SIZE=6144  # 3584 default overflows once Wi-Fi + printf appear
CONFIG_ESP_CONSOLE_UART_DEFAULT=y     # UART0 — the ONLY console on this board
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
CONFIG_ESP32_XTAL_FREQ_40=y           # the module's crystal. 26 MHz garbles every baud rate
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_BROWNOUT_DET_LVL_SEL_7=y       # ~2.43 V; catches a sagging USB rail before corruption
```

Two are the opposite of what an ESP32-C3/C6 project wants:

- **`CONFIG_ESP_CONSOLE_UART_DEFAULT`** — a C3/C6 would use
  `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`. This chip has no USB peripheral; selecting the USB
  console here produces a firmware that runs perfectly and says nothing.
- **`CONFIG_ESPTOOLPY_FLASHMODE_DIO`** — QIO is ~15 % faster at XIP but needs WP and HD
  bonded in quad mode, which WROOM-32 modules vary on. DIO always works.

`CONFIG_SPIRAM=y` is **fatal on a WROOM-32** (no PSRAM — the driver aborts before
`app_main()`) and **correct on a WROVER**. Check the module can before enabling it.

## 12. Flashing

Normal case — nothing to press:

```sh
pio run -t upload -t monitor
```

esptool drives DTR/RTS, the board enters download mode, flashes, and resets into the app.
`upload_speed = 921600` is set in the template; CH340G clones sometimes fail to negotiate
it — drop to `460800`.

### When auto-reset does not work

Symptom: `A fatal error occurred: Failed to connect to ESP32: Wrong boot mode detected`, or
`No serial data received`.

On this board, check **IO0** first: it is on the header, and anything attached to it fights
the auto-reset circuit. Disconnect it and retry before blaming the cable.

Manual sequence — the order matters:

1. Hold **BOOT**
2. Tap **EN**
3. Keep holding BOOT for another second
4. Release **BOOT**
5. Start the upload

Some clones need BOOT held for the whole upload.

### Recovery

**Bad firmware cannot brick this board.** The ROM bootloader is in mask ROM and always
answers the BOOT+EN sequence.

```sh
esptool.py --chip esp32 -p <port> erase_flash    # ~10 s for 4 MB
```

Then flash the minimal template variant to re-establish a baseline.

`rst:0x10 (RTCWDT_RTC_RESET)` in a loop, or `invalid header: 0xffffffff`, means the flash
contents or the image header's flash mode/frequency are wrong — `erase_flash`, then reflash
with DIO/40 MHz.

### Reading the boot banner

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

`boot:0x13` is a normal boot from flash. A different value means the strapping pins were
not where you expected; `board_report.c` decodes them properly, including the GPIO0 latch
that matters most on this board.

## 13. Peripheral cookbook

Full code is in `recipes.md`. The board-specific decisions:

| Task | Use | Because |
|---|---|---|
| Status LED | GPIO2, LEDC channel — **if fitted** | DevKitC V4 has none. A fade is distinguishable from a stuck pin; a blink is not |
| Button | GPIO0 (BOOT) | The only button that is a GPIO. Do not also wire the IO0 header pin |
| I2C | GPIO21 SDA / GPIO22 SCL | No hardware constraint — the community default, both unencumbered |
| SPI (fast) | **SPI3_HOST** on 18/19/23/5 | IO_MUX pads → 80 MHz. Any other pin set clamps to 26.67 MHz |
| SPI (second device) | same bus, any free pin as CS | A second bus means SPI2, whose IO_MUX pads are the JTAG group |
| Second UART | UART2 on GPIO17 TX / GPIO16 RX — **WROOM only** | UART1's defaults are flash pins; a WROVER has no 16/17 |
| Analog in | ADC1: GPIO32–36, 39 | ADC2 dies when Wi-Fi starts |
| Analog out | DAC on GPIO25 / GPIO26 | The only two |
| Servo / motor | LEDC (servo) or MCPWM (bridge) | LEDC's 16 channels are plenty; MCPWM has dead-time |
| WS2812 strip | RMT on any free pin | Exact and DMA-fed, unlike bit-banging |
| Deep-sleep wake button | GPIO32/33/25/26/27/4/34/35 | Only RTC GPIOs wake. 21/22/23/18/19/5/16/17 cannot |
| Anything at all | **not** GPIO6–11 | In-package flash. Exposed on this header purely as a trap |

## 14. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| Board hangs right after the bootloader banner | Something wired to GPIO6–11 (the six pins by the USB port) | Disconnect. Those are the flash bus |
| Board runs but corrupts flash under load | Capacitive loading on GPIO6–11 | Same. Even a scope probe is enough |
| Board sits in download mode on every reset | Something holding IO0 low, a stuck auto-reset transistor, or C15 on an early DevKitC V4 | Unplug IO0; `board_report.c` prints the GPIO0 latch. On a genuine DevKitC V4, check C15 |
| `pio run -t upload` fails since you wired IO0 | Your circuit fights the DTR/RTS auto-reset | Free IO0, or use the manual BOOT+EN sequence every time |
| Wired "D2" from a tutorial and the board died | On Espressif silkscreen `D2` is **GPIO9**, a flash pin | Resolve labels to GPIO numbers via §2 |
| Monitor is silent, board seems dead | Console configured for USB-Serial-JTAG (copied from a C3/S3 project) | `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`. This chip has no USB peripheral |
| Monitor prints garbage at every baud rate | `CONFIG_ESP32_XTAL_FREQ_26` selected | Set 40 MHz |
| GPIO16/17 do nothing | The module is an ESP32-WROVER; 16/17 are the PSRAM interface | Read the can. Move to other pins |
| Firmware aborts at boot with a PSRAM error | `CONFIG_SPIRAM=y` on a WROOM-32 | Turn it off, or fit a WROVER |
| Board resets the moment Wi-Fi starts | Brownout: the ~300 mA TX burst | Better cable / powered hub / 5 V into the 5V pin |
| Boots intermittently, flash looks corrupt | GPIO12 pulled high at reset → VDD_SDIO strapped to 1.8 V | Remove the pull-up, or add a 10 kΩ pull-down on GPIO12 |
| `adc_oneshot_read()` returns `ESP_ERR_TIMEOUT` | ADC2 channel while Wi-Fi is running | Move to an ADC1 pin (32–36, 39) |
| ADC reads ±6 % off between boards | No calibration | `adc_cali_create_scheme_line_fitting` — the only scheme this chip has |
| ADC saturates near 3.3 V | 12 dB attenuation flattens above ~2450 mV | Divide so full scale lands at ~2.4 V |
| `gpio_set_level()` on GPIO34/35/36/39 does nothing | Input-only, no output driver | Use an output-capable pin; check `gpio_set_direction()`'s return |
| Pull-up on GPIO34 has no effect | Input-only pins have no internal pulls | Fit a physical 10 kΩ resistor |
| SPI will not go above ~26 MHz | Signals routed through the GPIO Matrix | `SPI3_HOST` on GPIO18/19/23/5 exactly |
| UART1 output goes nowhere and the board hangs | UART1 defaults to GPIO9/GPIO10 — flash pins | `uart_set_pin()` before `uart_driver_install()`, or use UART2 |
| Deep sleep draws ~15 mA, not 10 µA | LDO quiescent + USB bridge + power LED | Expected on a devkit |
| Wake-on-pin never fires after deep sleep | The pin is not an RTC GPIO | Use one of 0, 2, 4, 12–15, 25–27, 32–36, 39 |
| Output pin floats during deep sleep | Hold not enabled | `gpio_hold_en()` + `gpio_deep_sleep_hold_en()` |
| Board will not fit a breadboard | It is 38 pins wide; it covers both rails | Expected. Use the 30-pin variant, or two half-breadboards |
