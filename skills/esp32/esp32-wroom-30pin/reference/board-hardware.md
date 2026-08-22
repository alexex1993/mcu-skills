# ESP32-WROOM-32 devkit, 30-pin — board reference

Part I is the board: what is wired where, and what it costs you. Part II is the
development guide: toolchain, flashing, peripheral recipes and the pitfall table.

Chip-level facts (ADC channels, strapping semantics, IO_MUX, power modes, memory map)
are in `esp32-soc.md` and are not repeated here.

---

# Part I — the board

## 1. Which board this is

There is no single vendor. The 30-pin ESP32 devkit is a form factor, sold as:

- **DOIT ESP32 DevKit V1** (the original, and the name PlatformIO's `esp32doit-devkit-v1`
  refers to)
- **ESP32 CH340 Type-C Development Board** — the current commodity version, USB-C,
  CH340G bridge
- **NodeMCU-ESP32 30-pin**, **ESP32 DevKit V1 30P**, and a dozen unbranded listings

They are electrically the same board: an **ESP32-WROOM-32** module, an AMS1117-3.3 LDO, a
USB-to-UART bridge, the two-transistor auto-reset circuit, EN and BOOT buttons, a power
LED and a user LED. Differences between vendors are the bridge chip (CH340G vs CP2102 vs
CP2102N), the USB connector (micro-B vs Type-C), and whether the user LED is fitted.

**How to tell a 30-pin board from its siblings at a glance:** count the pins on one side.
15 per side is the 30-pin board. The decisive test is the pins nearest the USB connector —
on a 30-pin board the last pins are `GND`/`VIN` and `GND`/`3V3`, with **no** `SD2`, `SD3`,
`CMD`, `CLK`, `SD0`, `SD1` labels anywhere. If you can see those six labels you have a
36- or 38-pin board and the wrong skill is loaded.

It also fits a standard breadboard with one row free on each side, which the 36- and 38-pin
boards do not. That is the reason this variant exists.

## 2. Header pin map

Oriented with the **USB connector at the bottom**, silkscreen labels as printed:

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
| 11 | **D14** | 14 | ADC2_CH6, TOUCH6, MTMS/JTAG, HSPICLK, RTC | | 11 | **D4** | 4 | ADC2_CH0, TOUCH0, RTC |
| 12 | **D12** | 12 | **strapping MTDI** — see below. ADC2_CH5, TOUCH5, RTC | | 12 | **D2** | 2 | **strapping**; user LED; ADC2_CH2, TOUCH2, RTC |
| 13 | **D13** | 13 | ADC2_CH4, TOUCH4, MTCK/JTAG, RTC | | 13 | **D15** | 15 | **strapping MTDO**; ADC2_CH3, TOUCH3, RTC |
| 14 | **GND** | — | | | 14 | **GND** | — | |
| 15 | **VIN** | — | 5 V in / 5 V out. See §4 | | 15 | **3V3** | — | LDO output, ~600 mA headroom. See §4 |

25 GPIOs, 2 GND, VIN, 3V3, EN. That is the whole header.

### Not on this header

| Pin | Why it is missing | Consequence |
|---|---|---|
| **GPIO0** | Wired to the BOOT button and the auto-reset transistor only | You cannot use GPIO0 for anything, but you also cannot accidentally break the boot strap with it — the 30-pin board's quiet advantage over the 36- and 38-pin ones |
| **GPIO6–11** | In-package SPI flash bus | Nothing to get wrong. These are the six pins that make 36- and 38-pin boards dangerous |
| GPIO20, 24, 28–31, 37, 38 | Not bonded out on the WROOM-32 package | Do not exist anywhere |

### Pins by usability

```
Free for anything (15):     4 13 14 16 17 18 19 21 22 23 25 26 27 32 33
Input only, no pulls (4):   34 35 36(VP) 39(VN)
Strapping — usable with care (4):  2 5 12 15
UART0 to the USB bridge (2):       1(TX0) 3(RX0)
```

The three that produce real bugs:

- **GPIO12 (D12)** — MTDI. Anything that holds it high at reset straps VDD_SDIO to 1.8 V
  and destabilises the module's 3.3 V flash. It is a perfectly good pin *after* boot; it
  just must not be pulled up through boot. A 10 kΩ pull-down makes it safe permanently.
- **GPIO2 (D2)** — shares the user LED and a strapping latch. Fine as an output; as an
  input with an external pull-up it can block download mode.
- **GPIO5 (D5)** — strapped high at reset. Driving it low with something low-impedance
  during power-up changes SDIO slave timing (harmless here) but the bigger risk is that
  the pin's boot-time high level appears on whatever you attached, which for a chip-select
  is the deselected state anyway. In practice GPIO5 is the least troublesome of the three.

### Boot-time pin states

Between reset and `app_main()` the ROM bootloader and the IDF bootloader drive some pins.
This is not configurable, and it matters for anything that reacts to an edge:

- **GPIO1 (TX0)** carries the ROM log at 115200 baud unless MTDO/GPIO15 is strapped low.
- **GPIO2** may be driven briefly. A relay or MOSFET here can click on every reset.
- All other pins are inputs with their reset-default pulls (see `esp32-soc.md` §4) for
  roughly the first 200 ms.

## 3. USB, the bridge chip and auto-reset

The ESP32 has no USB peripheral (see `esp32-soc.md` §1). Everything USB on this board is
the bridge chip:

```
USB-C ── CH340G (or CP2102/CP2102N) ── UART0 (GPIO1 TX / GPIO3 RX)
              │  DTR ──┐
              │  RTS ──┤ two-transistor circuit ──> EN (CHIP_PU) and GPIO0
```

The DTR/RTS pair drives EN and GPIO0 through the classic two-transistor network, so
esptool can put the board in download mode and reset it out again with no buttons. When
that works, `pio run -t upload` just works. When it does not — a clone with the transistors
omitted, a bad capacitor, or a Windows driver that toggles the lines in the wrong order —
the manual sequence is in Part II §12.

Ports appear as:

- macOS: `/dev/cu.usbserial-*` (CH340) or `/dev/cu.SLAB_USBtoUART` / `/dev/cu.usbmodem*`
  (CP2102/CP2102N)
- Linux: `/dev/ttyUSB0` (CH340, CP2102) — needs `dialout` group membership
- Windows: a `COM` port; CH340G needs the WCH driver, CP2102 the Silicon Labs one

**There is no JTAG header and no USB-JTAG.** Debugging this board means either the console
or wiring an external JTAG probe to GPIO12/13/14/15 (MTDI/MTCK/MTMS/MTDO) — and GPIO12 is
the MTDI strapping pin, so a probe that idles it high will not let the board boot. In
practice, on this board, you debug over UART0.

## 4. Power

```
USB VBUS 5 V ──┬── VIN pin (bidirectional!)
               └── AMS1117-3.3 LDO ── 3V3 pin ── ESP32-WROOM-32
```

| Rail | Direction | Range | Notes |
|---|---|---|---|
| **VIN** | in **or** out | 5 – 12 V in (the LDO's limit; the manual for the CH340 Type-C board states 5–12 V) | With USB plugged in, VIN is an *output* at ~5 V. Feeding it from a second supply at the same time back-feeds the host |
| **3V3** | out (in, if you must) | 3.3 V | AMS1117 rated 800 mA–1 A but thermally limited on this board; budget ~600 mA for peripherals |
| **GND** | — | — | Two pins, both connected |

Three rules that come from this being an LDO board:

1. **Never power VIN and USB at once.** No ORing diode is fitted on most clones.
2. **12 V into VIN is legal and stupid.** The AMS1117 burns `(12 − 3.3) × I` as heat; at
   200 mA that is 1.7 W in a SOT-223. Use 5 V.
3. **3V3 as an input bypasses the LDO** and is the correct way to run from a battery + your
   own regulator, but it also back-powers the bridge chip, which then holds EN in an
   undefined state on some clones. Cut the USB trace or accept the risk.

The 500 mA the datasheet demands (`esp32-soc.md` §6) is not about average draw — average
is ~80 mA — but about the Wi-Fi TX burst. A laptop USB-2 port, a long thin cable or a
powered hub that lies about its budget all produce the same signature: everything works
until the first packet, then `ESP_RST_BROWNOUT`.

## 5. Onboard indicators and buttons

| Part | Wiring | Notes |
|---|---|---|
| Power LED | across 3V3 | always on, ~3 mA. Not controllable |
| User LED | **GPIO2**, active **high** on DOIT/CH340 boards | blue. Some clones omit it; a few wire it to GPIO16 — check with the minimal template |
| **BOOT** | GPIO0 to GND, external pull-up | pressed = low. Also usable as a general input at runtime |
| **EN / RST** | CHIP_PU to GND | not a GPIO, cannot be read. Resets the chip |

## 6. Flash and partitions

4 MB in-package, on GPIO6–11. The template's `partitions.csv`:

| Partition | Type | Offset | Size |
|---|---|---|---|
| `nvs` | data/nvs | 0x9000 | 24 KB |
| `phy_init` | data/phy | 0xF000 | 4 KB |
| `factory` | app | 0x10000 | **1.75 MB** |
| `storage` | data/spiffs | 0x1D0000 | 2.19 MB |

1.75 MB for the app is generous — the full self-test variant with the Wi-Fi stack links at
784 KB. If you need OTA, replace `factory` with two 1.5 MB `ota_0`/`ota_1` partitions plus
an `otadata`, and drop `storage` to ~800 KB.

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

`board = esp32dev` is the right choice even though `esp32doit-devkit-v1` exists: both
declare 4 MB flash and 320 KB RAM, and `esp32dev` is the one every ESP32 example assumes.
The board definition only supplies defaults that `platformio.ini` and `sdkconfig.defaults`
then override, so nothing depends on the choice.

Arduino works too (`framework = arduino`), and for this board it is a perfectly reasonable
choice — but the pin constraints in Part I apply identically, and the Arduino core's
`analogRead()` hides the ADC2/Wi-Fi conflict rather than reporting it.

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

Two of these are the opposite of what an ESP32-C3/C6 project wants:

- **`CONFIG_ESP_CONSOLE_UART_DEFAULT`** — on a C3/C6 you would select
  `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`. This chip has no USB peripheral; selecting the
  USB console here produces a firmware that runs perfectly and says nothing.
- **`CONFIG_ESPTOOLPY_FLASHMODE_DIO`** — QIO gives ~15 % faster XIP but needs WP and HD
  bonded to the flash die in quad mode. WROOM-32 modules vary; DIO always works. Only try
  QIO if you have measured that you need it.

Do **not** set `CONFIG_SPIRAM=y`. There is no PSRAM on a WROOM-32; the driver will fail to
find it at boot and abort before `app_main()`.

## 12. Flashing

Normal case — nothing to press:

```sh
pio run -t upload -t monitor
```

esptool drives DTR/RTS, the board enters download mode, flashes, and resets into the app.
`upload_speed = 921600` is set in the template; CH340G clones occasionally fail to
negotiate it, in which case drop to `460800`.

### When auto-reset does not work

Symptom: `A fatal error occurred: Failed to connect to ESP32: Wrong boot mode detected`,
or `No serial data received`.

Manual sequence — the order matters:

1. Hold **BOOT**
2. Tap **EN**
3. Keep holding BOOT for another second
4. Release **BOOT**
5. Start the upload

The board is now in ROM download mode and stays there until reset. Some clones need BOOT
held for the whole upload.

### Recovery

**Bad firmware cannot brick this board.** The ROM bootloader lives in mask ROM and always
responds to the BOOT+EN sequence, whatever is in flash. If the board boot-loops:

```sh
esptool.py --chip esp32 -p <port> erase_flash    # ~10 s for 4 MB
```

Then flash the minimal template variant to re-establish a known-good baseline.

If the console prints an endless `rst:0x10 (RTCWDT_RTC_RESET)` or
`invalid header: 0xffffffff`, the flash contents are wrong or the flash mode/frequency in
the image header does not match the part — `erase_flash` then reflash with DIO/40 MHz.

### Reading the boot banner

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

- `rst:` — the reset cause, same taxonomy as `esp_reset_reason()`
- `boot:0x13` — normal boot from flash. `boot:0x03` or anything ending in a low bit set
  differently means the strapping pins were not where you expected; `board_report.c`
  decodes them properly.

## 13. Peripheral cookbook

Full code is in `recipes.md`. The board-specific decisions:

| Task | Use | Because |
|---|---|---|
| Status LED | GPIO2, LEDC channel | It is the only LED. A fade is distinguishable from a stuck pin; a blink is not |
| Button | GPIO0 (BOOT) | The only button that is a GPIO. `GPIO_PULLUP_ENABLE` plus a 20 ms debounce |
| I2C | GPIO21 SDA / GPIO22 SCL | No hardware constraint — these are the community default, and both are unencumbered pins |
| SPI (fast) | **SPI3_HOST** on 18/19/23/5 | IO_MUX pads → 80 MHz. Any other pin set is matrix-routed and clamps to 26.67 MHz |
| SPI (second device) | same bus, any free pin as CS | Adding a bus means SPI2, whose IO_MUX pads are the JTAG group |
| Second UART | UART2 on GPIO17 TX / GPIO16 RX | UART1's defaults are flash pins |
| Analog in | ADC1: GPIO32–36, 39 | ADC2 dies when Wi-Fi starts |
| Analog out | DAC on GPIO25 / GPIO26 | The only two |
| Servo / motor | LEDC (servo) or MCPWM (bridge) | LEDC's 16 channels are plenty; MCPWM has dead-time insertion |
| WS2812 strip | RMT on any free pin | Not bit-banged; RMT is exact and DMA-fed |
| Deep-sleep wake button | GPIO32/33/25/26/27/4/34/35 | Only RTC GPIOs wake. GPIO21/22/23/18/19/5/16/17 cannot |

## 14. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| Monitor is silent, board seems dead | Console configured for USB-Serial-JTAG (copied from an ESP32-C3/S3 project) | `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`. This chip has no USB peripheral |
| Monitor prints garbage at every baud rate | `CONFIG_ESP32_XTAL_FREQ_26` selected | Set 40 MHz — the WROOM-32 crystal |
| `Failed to connect to ESP32: Wrong boot mode detected` | Auto-reset circuit not driving GPIO0 | Manual BOOT+EN sequence (§12) |
| `Failed to connect` and even the manual sequence fails | Charge-only USB cable, or missing bridge driver | Try another cable first; then check the port actually enumerates |
| Board resets the moment Wi-Fi starts | Brownout: the ~300 mA TX burst | Better cable / powered hub / 5 V into VIN. `esp_reset_reason()` says `ESP_RST_BROWNOUT` |
| Boots intermittently, flash looks corrupt | GPIO12 (D12) pulled high at reset → VDD_SDIO strapped to 1.8 V | Remove the pull-up, or add a 10 kΩ pull-down on GPIO12 |
| Board enters download mode by itself every reset | Something holding GPIO0 low, or a stuck auto-reset transistor | `board_report.c` prints the GPIO0 latch. Unplug peripherals and retest |
| `adc_oneshot_read()` returns `ESP_ERR_TIMEOUT` | ADC2 channel while Wi-Fi is running | Move the input to an ADC1 pin (32–36, 39) |
| ADC reads are ±6 % off between two boards | No calibration | `adc_cali_create_scheme_line_fitting` — the only scheme this chip has |
| ADC saturates near 3.3 V | 12 dB attenuation flattens above ~2450 mV | Divide so full scale lands at ~2.4 V |
| `gpio_set_level()` on GPIO34/35/36/39 does nothing | Input-only pins, no output driver | Use an output-capable pin; check the return value of `gpio_set_direction()` |
| Pull-up on GPIO34 has no effect | Input-only pins have no internal pulls | Fit a physical 10 kΩ resistor |
| SPI will not go above ~26 MHz | Signals routed through the GPIO Matrix | `SPI3_HOST` on GPIO18/19/23/5 exactly |
| UART1 output goes nowhere and the board hangs | UART1 defaults to GPIO9/GPIO10 — flash pins | `uart_set_pin()` to real pins before `uart_driver_install()`, or use UART2 |
| Deep sleep draws ~15 mA, not 10 µA | LDO quiescent + USB bridge + power LED | Expected on a devkit. The 10 µA figure is the bare chip |
| Wake-on-pin never fires after deep sleep | The pin is not an RTC GPIO | Use one of 0, 2, 4, 12–15, 25–27, 32–36, 39 |
| Output pin floats during deep sleep | Hold not enabled | `gpio_hold_en()` + `gpio_deep_sleep_hold_en()` |
| Firmware aborts at boot with a PSRAM error | `CONFIG_SPIRAM=y` on a WROOM-32 | Turn it off. There is no PSRAM |
| Board works on USB, dies on a battery | Below 2.3 V the chip must be held in reset | Add a supply supervisor, or a bigger cell |
