# ESP32-WROOM-32 devkit, 36-pin — board reference

Part I is the board: what is wired where, and what it costs you. Part II is the
development guide: toolchain, flashing, peripheral recipes and the pitfall table.

Chip-level facts (ADC channels, strapping semantics, IO_MUX, power modes, memory map)
are in `esp32-soc.md` and are not repeated here.

---

# Part I — the board

## 1. Which board this is, and how confident this document is

The 36-pin ESP32 devkit is the **original DOIT ESP32 DevKit V1**, before the flash pins
were dropped to make the shorter 30-pin version everyone sells today. It is the 30-pin
board plus the six in-package flash pins broken out at the USB end.

Espressif never made a 36-pin board, so unlike the 38-pin variant there is no vendor
document with a numbered header table. **The GPIO *set* below is certain; the physical
*row order* is reconstructed** from the 30-pin layout, the 38-pin layout and vendor
descriptions of "the updated DOIT board with 6 extra pins". Marked **⚠︎ derived** where
that applies.

Nothing in your code should depend on the row order — only on which GPIO a silkscreen
label means, which is certain. Before wiring, do the 20-second check in §2.1.

**How to tell a 36-pin board apart:** 18 pins per side. Six of them are labelled `SD2`
`SD3` `CMD` / `SD1` `SD0` `CLK` at the USB end — those are absent on a 30-pin board. And
unlike the 38-pin board there is **no `IO0`/`D0` pin** and only two GND pins.

| Board size | Pins/side | Flash pins on header | GPIO0 on header | GND pins |
|---|---|---|---|---|
| 30-pin | 15 | no | no | 2 |
| **36-pin** | **18** | **yes** | **no** | **2** |
| 38-pin | 19 | yes | yes | 3 |

## 2. Header pin map

Oriented with the **USB connector at the bottom**. Silkscreen labels as printed on
DOIT-style boards.

| # | Left column | GPIO | Notes | | # | Right column | GPIO | Notes |
|---|---|---|---|---|---|---|---|---|
| 1 | **EN** | — | CHIP_PU. Pull low to reset. Not a GPIO | | 1 | **D23** | 23 | VSPID (MOSI), IO_MUX |
| 2 | **VP** | 36 | input-only, ADC1_CH0, RTC | | 2 | **D22** | 22 | VSPIWP; default I2C SCL |
| 3 | **VN** | 39 | input-only, ADC1_CH3, RTC | | 3 | **TX0** | 1 | UART0 TX → USB bridge |
| 4 | **D34** | 34 | input-only, ADC1_CH6, RTC | | 4 | **RX0** | 3 | UART0 RX ← USB bridge |
| 5 | **D35** | 35 | input-only, ADC1_CH7, RTC | | 5 | **D21** | 21 | VSPIHD; default I2C SDA |
| 6 | **D32** | 32 | ADC1_CH4, TOUCH9, XTAL_32K_P, RTC | | 6 | **D19** | 19 | VSPIQ (MISO), IO_MUX |
| 7 | **D33** | 33 | ADC1_CH5, TOUCH8, XTAL_32K_N, RTC | | 7 | **D18** | 18 | VSPICLK, IO_MUX |
| 8 | **D25** | 25 | **DAC_1**, ADC2_CH8, RTC | | 8 | **D5** | 5 | **strapping**; VSPICS0, IO_MUX |
| 9 | **D26** | 26 | **DAC_2**, ADC2_CH9, RTC | | 9 | **TX2** | 17 | UART2 TX |
| 10 | **D27** | 27 | ADC2_CH7, TOUCH7, RTC | | 10 | **RX2** | 16 | UART2 RX |
| 11 | **D14** | 14 | ADC2_CH6, TOUCH6, MTMS/JTAG, RTC | | 11 | **D4** | 4 | ADC2_CH0, TOUCH0, RTC |
| 12 | **D12** | 12 | **strapping MTDI** — see below | | 12 | **D2** | 2 | **strapping**; user LED |
| 13 | **D13** | 13 | ADC2_CH4, TOUCH4, MTCK/JTAG, RTC | | 13 | **D15** | 15 | **strapping MTDO**; ADC2_CH3 |
| 14 | **GND** | — | | | 14 | **GND** | — | |
| 15 | **SD2** | **9** | ⚠ **in-package flash** | | 15 | **SD1** | **8** | ⚠ **in-package flash** |
| 16 | **SD3** | **10** | ⚠ **in-package flash** | | 16 | **SD0** | **7** | ⚠ **in-package flash** |
| 17 | **CMD** | **11** | ⚠ **in-package flash** | | 17 | **CLK** | **6** | ⚠ **in-package flash** |
| 18 | **VIN** | — | 5 V in / out. See §4 | | 18 | **3V3** | — | LDO output. See §4 |

⚠︎ **derived**: rows 1–14 of each column are the confirmed 30-pin layout; rows 15–17 are
the six flash pins, whose *presence* is certain and whose *order within the group* is
reconstructed. Rows 18 (VIN / 3V3) are at the USB end on every DOIT-family board.

31 GPIOs on the header — but only **25 of them are usable**, because six are the flash bus.
The usable set is identical to the 30-pin board's.

### 2.1 Verify before you wire

Twenty seconds, and it settles every uncertainty in this document:

1. Count pins per side. 18 ⇒ this skill. 15 ⇒ `esp32-wroom-30pin`. 19 ⇒ `esp32-wroom-38pin`.
2. Read the six labels nearest the USB connector. They should be `SD2 SD3 CMD` on one side
   and `SD1 SD0 CLK` on the other. Those are GPIO9, 10, 11 and GPIO8, 7, 6 — **do not
   use any of them**, whatever order they are in.
3. Flash the minimal template variant. It prints `BOARD_NAME` and the LED pin, so a
   mismatch between this document and the board in your hand shows up immediately.

If the silkscreen disagrees with the table above, the silkscreen wins — and the GPIO
numbers behind the labels are still what this document says, because those come from the
module datasheet, not the board.

### Not on this header

| Pin | Why | Consequence |
|---|---|---|
| **GPIO0** | Wired to the BOOT button and the auto-reset transistor only | You cannot use GPIO0, but you also cannot break the boot strap with it — unlike on the 38-pin board |
| GPIO20, 24, 28–31, 37, 38 | Not bonded out on the WROOM-32 package | Do not exist anywhere |

### The six pins you must not use

`SD2`(9) `SD3`(10) `CMD`(11) and `SD1`(8) `SD0`(7) `CLK`(6) are the module's internal SPI
flash bus. The CPU executes code out of that flash through those wires. Loading one — a
pull-up, an LED, even a scope probe with too much capacitance — produces one of:

- the board hangs just after the bootloader banner,
- the board boot-loops with `invalid header`,
- worst: the board runs and corrupts flash sectors under load.

**They are broken out for hardware bring-up, not for you.** This is the only functional
difference between a 36-pin board and the 30-pin board most people buy, and it is a
liability rather than a feature. If you are choosing a board rather than using one you
already have, the 30-pin variant is strictly safer and fits a breadboard.

### Pins by usability

```
Free for anything (15):     4 13 14 16 17 18 19 21 22 23 25 26 27 32 33
Input only, no pulls (4):   34 35 36(VP) 39(VN)
Strapping — usable with care (4):  2 5 12 15
UART0 to the USB bridge (2):       1(TX0) 3(RX0)
In-package flash — DO NOT USE (6): 6 7 8 9 10 11
Not on the header:                 0
```

The three that produce real bugs:

- **GPIO12 (D12)** — MTDI. Held high at reset it straps VDD_SDIO to 1.8 V while the
  module's flash is a 3.3 V part: intermittent boots, presenting as flash corruption. Fine
  after boot. A 10 kΩ pull-down makes it permanently safe.
- **GPIO2 (D2)** — user LED plus a strapping latch. Fine as an output; an external pull-up
  can block download mode.
- **GPIO5 (D5)** — strapped high at reset, so whatever you attach sees a high level during
  boot. For a chip-select that is the deselected state, which is why it is the least
  troublesome of the three.

### Boot-time pin states

- **GPIO1 (TX0)** carries the ROM log at 115200 baud unless MTDO/GPIO15 is strapped low.
- **GPIO2** may be driven briefly by the bootloader — a relay here clicks on every reset.
- Everything else is an input with its reset-default pull for roughly the first 200 ms.

## 3. USB, the bridge chip and auto-reset

The ESP32 has no USB peripheral (see `esp32-soc.md` §1). The USB port is the bridge chip:

```
USB ── CH340G (or CP2102/CP2102N) ── UART0 (GPIO1 TX / GPIO3 RX)
            │  DTR ──┐
            │  RTS ──┤ two-transistor circuit ──> EN (CHIP_PU) and GPIO0
```

Because GPIO0 is **not** on the 36-pin header, the auto-reset circuit has it to itself and
uploads are more reliable here than on a 38-pin board where users wire things to IO0.

Ports appear as `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` (macOS), `/dev/ttyUSB0`
(Linux, needs `dialout`), or a `COM` port (Windows, WCH or Silicon Labs driver).

**No JTAG header, no USB-JTAG.** External JTAG means wiring a probe to GPIO12/13/14/15
(MTDI/MTCK/MTMS/MTDO), and a probe that idles MTDI high stops the board booting. On this
board you debug over UART0.

If you do wire a probe: the board definition's `openocd_board` is `esp-wroom-32.cfg`, but
that file (shipped in `tool-openocd-esp32`) is upstream-deprecated — it just sets
`ESP32_FLASH_VOLTAGE 3.3` and sources `target/esp32.cfg`. Source `target/esp32.cfg` directly
with your interface config instead of chasing the deprecated board file.

## 4. Power

```
USB VBUS 5 V ──┬── VIN pin (bidirectional)
               └── AMS1117-3.3 LDO ── 3V3 pin ── ESP32-WROOM-32
```

| Rail | Direction | Range | Notes |
|---|---|---|---|
| **VIN** | in **or** out | 5 – 12 V in | With USB plugged in, VIN is an *output* at ~5 V. A second supply on it back-feeds the host |
| **3V3** | out (in, if you must) | 3.3 V | AMS1117 output; budget ~600 mA for peripherals |
| **GND** | — | — | Two pins, both connected |

1. **Never power VIN and USB at once.** No ORing diode is fitted.
2. **12 V into VIN is legal and stupid.** The LDO burns `(12 − 3.3) × I`; at 200 mA that is
   1.7 W in a SOT-223. Use 5 V.
3. **3V3 as an input bypasses the LDO** — the correct battery route, but it also
   back-powers the bridge chip, which on some clones then holds EN in an undefined state.

The 500 mA the datasheet demands is about the Wi-Fi TX burst, not the ~80 mA average. A
laptop USB-2 port or a thin cable gives the classic signature: everything works until the
first packet, then `ESP_RST_BROWNOUT`.

## 5. Onboard indicators and buttons

| Part | Wiring | Notes |
|---|---|---|
| Power LED | across 3V3 | always on. Not controllable |
| User LED | **GPIO2**, active **high** | blue. Some clones omit it — the minimal template tells you |
| **BOOT** | GPIO0 to GND, external pull-up | pressed = low. Usable as a runtime input |
| **EN / RST** | CHIP_PU to GND | not a GPIO, cannot be read |

## 6. Flash and partitions

4 MB in-package, on the six header pins you must not touch. The template's
`partitions.csv`:

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

Do **not** set `CONFIG_SPIRAM=y`. There is no PSRAM on a WROOM-32; the driver fails to find
it at boot and aborts before `app_main()`.

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
with DIO/40 MHz. On this board, also check that nothing is touching the six flash pins.

### Reading the boot banner

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

`boot:0x13` is a normal boot from flash. A different value means the strapping pins were
not where you expected; `board_report.c` decodes them properly.

## 13. Peripheral cookbook

Full code is in `recipes.md`. The board-specific decisions:

| Task | Use | Because |
|---|---|---|
| Status LED | GPIO2, LEDC channel | It is the only LED. A fade is distinguishable from a stuck pin; a blink is not |
| Button | GPIO0 (BOOT) | The only button that is a GPIO, and it is not on the header, so nothing can fight it |
| I2C | GPIO21 SDA / GPIO22 SCL | No hardware constraint — the community default, both unencumbered |
| SPI (fast) | **SPI3_HOST** on 18/19/23/5 | IO_MUX pads → 80 MHz. Any other pin set clamps to 26.67 MHz |
| SPI (second device) | same bus, any free pin as CS | A second bus means SPI2, whose IO_MUX pads are the JTAG group |
| Second UART | UART2 on GPIO17 TX / GPIO16 RX | UART1's defaults are flash pins |
| Analog in | ADC1: GPIO32–36, 39 | ADC2 dies when Wi-Fi starts |
| Analog out | DAC on GPIO25 / GPIO26 | The only two |
| Servo / motor | LEDC (servo) or MCPWM (bridge) | LEDC's 16 channels are plenty; MCPWM has dead-time |
| WS2812 strip | RMT on any free pin | Exact and DMA-fed, unlike bit-banging |
| Deep-sleep wake button | GPIO32/33/25/26/27/4/34/35 | Only RTC GPIOs wake. 21/22/23/18/19/5/16/17 cannot |
| Anything at all | **not** GPIO6–11 | In-package flash. Exposed on this header purely as a trap |

## 14. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| Board hangs right after the bootloader banner | Something wired to the six pins by the USB port (GPIO6–11) | Disconnect. Those are the flash bus |
| Board runs but corrupts flash under load | Capacitive loading on GPIO6–11 | Same. Even a scope probe is enough |
| The pin count or labels do not match §2 | You may have a 30- or 38-pin board | Run the check in §2.1 and switch skills |
| Monitor is silent, board seems dead | Console configured for USB-Serial-JTAG (copied from a C3/S3 project) | `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`. This chip has no USB peripheral |
| Monitor prints garbage at every baud rate | `CONFIG_ESP32_XTAL_FREQ_26` selected | Set 40 MHz |
| `Failed to connect to ESP32: Wrong boot mode detected` | Auto-reset circuit not driving GPIO0 | Manual BOOT+EN sequence (§12) |
| `Failed to connect` and the manual sequence also fails | Charge-only USB cable, or missing bridge driver | Try another cable first; check the port enumerates |
| Board resets the moment Wi-Fi starts | Brownout: the ~300 mA TX burst | Better cable / powered hub / 5 V into VIN |
| Boots intermittently, flash looks corrupt | GPIO12 (D12) pulled high at reset → VDD_SDIO strapped to 1.8 V | Remove the pull-up, or add a 10 kΩ pull-down on GPIO12 |
| `adc_oneshot_read()` returns `ESP_ERR_TIMEOUT` | ADC2 channel while Wi-Fi is running | Move to an ADC1 pin (32–36, 39) |
| ADC reads ±6 % off between boards | No calibration | `adc_cali_create_scheme_line_fitting` — the only scheme this chip has |
| ADC saturates near 3.3 V | 12 dB attenuation flattens above ~2450 mV | Divide so full scale lands at ~2.4 V |
| `gpio_set_level()` on GPIO34/35/36/39 does nothing | Input-only, no output driver | Use an output-capable pin; check `gpio_set_direction()`'s return |
| Pull-up on GPIO34 has no effect | Input-only pins have no internal pulls | Fit a physical 10 kΩ resistor |
| SPI will not go above ~26 MHz | Signals routed through the GPIO Matrix | `SPI3_HOST` on GPIO18/19/23/5 exactly |
| UART1 output goes nowhere and the board hangs | UART1 defaults to GPIO9/GPIO10 — flash pins | `uart_set_pin()` before `uart_driver_install()`, or use UART2 |
| Deep sleep draws ~15 mA, not 10 µA | LDO quiescent + USB bridge + power LED | Expected on a devkit |
| Wake-on-pin never fires after deep sleep | The pin is not an RTC GPIO | Use one of 2, 4, 12–15, 25–27, 32–36, 39 (GPIO0 is not on this header) |
| Output pin floats during deep sleep | Hold not enabled | `gpio_hold_en()` + `gpio_deep_sleep_hold_en()` |
| Firmware aborts at boot with a PSRAM error | `CONFIG_SPIRAM=y` on a WROOM-32 | Turn it off. There is no PSRAM |
| Board will not fit a breadboard | 18 pins per side covers both rails on a standard board | Expected. The 30-pin variant is the breadboard-friendly one |
