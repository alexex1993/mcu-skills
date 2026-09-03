# Waveshare ESP32-C6-Touch-LCD-1.47 — hardware and development reference

Everything this board does with the chip, and everything needed to build for it.
Chip-level facts (IO MUX tables, electrical limits, peripheral feature lists) live in
[esp32c6-soc.md](esp32c6-soc.md); the touch controller has its own file,
[touch-axs5106l.md](touch-axs5106l.md); code to paste lives in [recipes.md](recipes.md).

Two markers are used throughout:

- **✅ Hardware** — observed on a real board running the firmware in `template/`.
- **⚠︎ Inference** — a conclusion drawn from the schematic plus a datasheet. Sound, but
  nobody has put a probe on it. Say so when you repeat it.

Netlist facts below come from `ESP32-C6-Touch-LCD-1.47-Schematic.pdf` (one sheet,
Waveshare). Where the vendor wiki's pinout table disagrees with the schematic, the
schematic is what is soldered — see §4.2.

---

## Part I — Hardware

## 1. What is on the board

| | |
|---|---|
| SKU | **31203** (bare) · **31201** = the `-M` variant, 22-pin header pre-soldered |
| SoC | **ESP32-C6FH8** — QFN32, **8 MB in-package flash**, 22 GPIOs, **no PSRAM** |
| CPU | RISC-V RV32IMAC HP core @ **160 MHz** + RISC-V LP core @ 20 MHz |
| Memory | 512 KB HP SRAM (~320 KB linkable) · 16 KB LP SRAM · 320 KB ROM · 8 MB flash |
| Display | **1.47" IPS, JD9853, 172×320, 262K colours**, 4-wire SPI on SPI2, write-only |
| Touch | **AXS5106L** capacitive, I²C **0x63**, with `TP_RST` and `TP_INT` |
| IMU | **QMI8658A** 6-axis, same I²C bus, **0x6B** (SA0 grounded), INT1/INT2 on GPIO5/6 |
| Storage | microSD (TF) slot on the **same SPI2 bus** as the panel, SPI 1-bit mode only |
| Power | **ETA6098** Li-ion charger + **ME6217C33M5G** 3.3 V LDO, `VBAT` pad, `VBUS` from Type-C |
| Battery sense | VBAT through a 200 k / 100 k divider into **GPIO0** (`ADC1_CH0`) |
| Crystal | 40 MHz (Y1). **No 32.768 kHz crystal** — those pads are GPIO0/GPIO1, both used |
| Radio | Wi-Fi 6 (802.11ax/b/g/n), BLE 5, 802.15.4, onboard ceramic antenna |
| Connector | USB Type-C straight to the SoC's USB Serial/JTAG (GPIO12/13) |
| Buttons | **RESET** (Key1 → `CHIP_PU`) · **BOOT** (Key2 → **GPIO9**, pressed = low) |
| Header | 22-pin, breaks out power, VBAT, USB, UART0, I²C, the TF pins and 2 free GPIOs |

There is **no USB-UART bridge chip** and **no debug header**. The Type-C port is
simultaneously the console, the flasher and the JTAG probe.

The display and the touch panel arrive on **one 17-pin FPC** (J3): `LCD_SCL`,
`LCD_SDA`, `LCD_RST`, `LCD_DC`, `LCD_CS`, `LCD_VDD`, `LEDK`, `LEDA`, `TP_VDD`,
`TP_SCL`, `TP_SDA`, `TP_RST`, `TP_INT` and four grounds. There is no way to use the
panel without the touch controller also being on the bus.

## 2. Master pin table

The QFN32 package has 22 GPIOs. **GPIO10, GPIO11 and GPIO24–GPIO30 do not exist** on
it; GPIO14 exists *only* on it. **The board consumes 20 of the 22.**

| GPIO | QFN32 pin | Board net | Function | Analog | LP IO | Strap? | Notes |
|---|---|---|---|---|---|---|---|
| **GPIO0** | 6 | `BAT_ADC` | battery voltage sense | `ADC1_CH0` / `XTAL_32K_P` | `LP_GPIO0` | — | 200 k/100 k divider + 100 nF to GND, always connected |
| **GPIO1** | 7 | `IO1` | **SPI `SCLK`** (LCD + TF) | `ADC1_CH1` / `XTAL_32K_N` | `LP_GPIO1` | — | 10 k pull-up (R5); **no FSPI IO-MUX function** → GPIO Matrix |
| **GPIO2** | 8 | `IO2` | **SPI `MOSI`** (LCD + TF) | `ADC1_CH2` | `LP_GPIO2` | — | 10 k pull-up (R4); IO-MUX slot here is `FSPIQ` (MISO), not MOSI |
| **GPIO3** | 9 | `IO3` | **TF `MISO`** (`D0`) | `ADC1_CH3` | `LP_GPIO3` | — | 10 k pull-up (R6); no FSPI function |
| **GPIO4** | 10 | `IO4` | **TF `CS`** (`D3`) | `ADC1_CH4` | `LP_GPIO4` / `LP_UART_RXD` | ✅ SDIO edge | 10 k pull-up (R3) → card deselected at reset. JTAG `MTMS` |
| **GPIO5** | 11 | `IMU_INT1` | QMI8658A INT1 | `ADC1_CH5` | `LP_GPIO5` / `LP_UART_TXD` | ✅ SDIO edge | JTAG `MTDI`. On header pin 19 |
| **GPIO6** | 12 | `IMU_INT2` | QMI8658A INT2 | `ADC1_CH6` | `LP_GPIO6` / `LP_I2C_SDA` | — | JTAG `MTCK`. On header pin 21 |
| **GPIO7** | 13 | `IO7` | **free** | — | `LP_GPIO7` / `LP_I2C_SCL` | — | JTAG `MTDO`. Header pin 22. The one unencumbered pin |
| **GPIO8** | 14 | `IO8` | **free** | — | — | ✅ boot mode + ROM print | Header pin 20, **floating** — see §4.1 |
| **GPIO9** | 15 | `BOOT` | **BOOT button** | — | — | ✅ boot mode | 10 k pull-up (R10) + 100 nF (C22). Header pin 18 |
| **GPIO12** | 16 | `USB_N` | USB `D−` | `USB_D−` | — | — | 22 R series (R8). 40 mA default drive |
| **GPIO13** | 17 | `USB_P` | USB `D+` | `USB_D+` | — | — | 22 R series (R7). 40 mA default drive |
| **GPIO14** | 18 | `IO14` | **LCD `CS`** | — | — | — | QFN32-only pin. Not on the header |
| **GPIO15** | 19 | `IO15` | **LCD `DC`** | — | — | ✅ JTAG source | Not on the header. No internal pulls |
| **GPIO16** | 21 | `ESP_TXD` | UART0 TX | — | — | — | 499 R series (R2). Header pin 5 |
| **GPIO17** | 22 | `ESP_RXD` | UART0 RX | — | — | — | Header pin 7 |
| **GPIO18** | 23 | `ESP_SDA` | **I²C SDA** (touch + IMU) | — | — | — | 10 k pull-up (R11) on board. Header pin 12 |
| **GPIO19** | 24 | `ESP_SCL` | **I²C SCL** (touch + IMU) | — | — | — | 10 k pull-up (R12) on board. Header pin 10 |
| **GPIO20** | 25 | `IO20` | **`TP_RST`** (touch reset) | — | — | — | Active low. Not on the header |
| **GPIO21** | 26 | `IO21` | **`TP_INT`** (touch interrupt) | — | — | — | Active low. Not on the header |
| **GPIO22** | 27 | `IO22` | **LCD `RST`** | — | — | — | Not on the header |
| **GPIO23** | 28 | `IO23` | **LCD backlight** | — | — | — | 1 k (R14) to an SS8050 NPN base; `LEDK` via 10 R (R13). Active high |

### 2.1 The header (H1, 22 pins)

| Pin | Net | Pin | Net |
|---|---|---|---|
| 1 | `VBUS` | 2 | `VBAT` |
| 3 | `GND` | 4 | `GND` |
| 5 | `ESP_TXD` (GPIO16) | 6 | `GND` |
| 7 | `ESP_RXD` (GPIO17) | 8 | `3V3` |
| 9 | `ESP_RST` (`CHIP_PU`) | 10 | `ESP_SCL` (GPIO19) |
| 11 | `IO1` (SPI SCLK) | 12 | `ESP_SDA` (GPIO18) |
| 13 | `IO2` (SPI MOSI) | 14 | `USB_P` (GPIO13) |
| 15 | `IO3` (TF MISO) | 16 | `USB_N` (GPIO12) |
| 17 | `IO4` (TF CS) | 18 | `BOOT` (GPIO9) |
| 19 | `IMU_INT1` (GPIO5) | 20 | `IO8` (GPIO8) |
| 21 | `IMU_INT2` (GPIO6) | 22 | `IO7` (GPIO7) |

Pin **numbers** are from the schematic net list. ⚠︎ **Inference:** the two-column
layout above assumes the usual odd/even row split of a 2 × 11 header — count pads on
the board before wiring a connector to it.

**`VBAT` (pin 2) is a battery terminal, not an output to power things from.** Waveshare's
own warning: never tie it to `3V3` or `VBUS`.

## 3. Reset-time pin states

What each board signal does *before* firmware runs (ESP32-C6 datasheet Table 2-2):

| Board signal | GPIO | At reset | Consequence |
|---|---|---|---|
| SPI `SCLK` / `MOSI` / TF `MISO` / TF `CS` | 1, 2, 3, 4 | `IE`, plus 10 k board pull-ups | all four idle **high**; the card is deselected. Good |
| free | GPIO7 | `IE`, `WPU` (JTAG pad default) | weakly high |
| free | GPIO8 | `IE`, **floating** | see §4.1 — this is the one that bites |
| BOOT | GPIO9 | `IE`, `WPU` + board 10 k | high → SPI boot |
| USB `D−`/`D+` | 12, 13 | `IE` / `USB_PU` | USB enumerates from ROM with no firmware present — a bad image cannot brick the board |
| LCD `CS` / `DC` | 14, 15 | `IE`, no pull | floating; panel not selected. Harmless |
| I²C `SDA`/`SCL` | 18, 19 | `WPU` + board 10 k | idle high, as I²C wants |
| `TP_RST` | GPIO20 | `WPU` | touch controller **not** held in reset while the chip boots. Good |
| `TP_INT` | GPIO21 | `WPU` | input idle high (the controller pulls it low) |
| LCD `RST` | GPIO22 | `WPU` | panel **not** held in reset. Good |
| LCD backlight | GPIO23 | `WPU` | ⚠︎ **Inference:** the internal weak pull-up (~45 kΩ) feeds the 1 kΩ base resistor of the SS8050, giving roughly 55 µA of base drive — ample to turn it on. **The backlight is lit from power-on until firmware claims GPIO23**, showing whatever the panel's RAM held. Claim it with LEDC at 0 % duty as the *first* thing you do, and raise the duty only after the first full flush |

## 4. Strapping-pin analysis

All eFuse parameters below **default to 0 (not burnt)**, and eFuse is one-time
programmable.

| Pin | Board use | Strapping role | Verdict |
|---|---|---|---|
| **GPIO8** | *nothing* — header pin 20 only | chip boot mode (consulted when GPIO9 = 0); ROM message printing (consulted only when `EFUSE_UART_PRINT_CONTROL` ≠ 0) | ⚠️ **floating — see §4.1** |
| **GPIO9** | BOOT button | chip boot mode. 10 k pull-up → `1` → SPI boot; held low with GPIO8 = 1 → Joint Download Boot | ✅ this *is* the flashing mechanism |
| **GPIO4** (`MTMS`) | TF `CS` | SDIO **slave** sampling/driving clock edge | ✅ safe — the board is an SPI *host*, unrelated |
| **GPIO5** (`MTDI`) | IMU `INT1` | same | ✅ safe |
| **GPIO15** | LCD `DC` | JTAG signal source, consulted only when `EFUSE_JTAG_SEL_ENABLE` = 1 | ✅ safe at factory settings |

**Do not burn `EFUSE_UART_PRINT_CONTROL` or `EFUSE_JTAG_SEL_ENABLE` on this board.**
The first hands the boot decision to a floating pin; the second hands JTAG selection to
the LCD's data/command line. Neither bit can be un-burnt.

### 4.1 GPIO8 floats, and that is why manual download mode sometimes fails

Joint Download Boot requires **GPIO8 = 1 and GPIO9 = 0** (datasheet Table 3-3). On this
board GPIO9 has a 10 k pull-up and the BOOT button; **GPIO8 has neither, and reaches
only header pin 20.** Its strap value at reset is therefore whatever the pad happens to
float to.

Normally this never shows: the USB Serial/JTAG controller does host-controlled reset and
download-mode entry, so `esptool` never needs the strap. It matters when you fall back to
the manual **hold BOOT, tap RESET** dance — and it is exactly what Waveshare's FAQ is
describing when it says "temporarily connect GPIO8 to 3V3 and re-enter download mode".

➜ **If BOOT + RESET does not produce a download-mode device, jumper header pin 20
(GPIO8) to 3V3 (pin 8) and repeat.** Never pull GPIO8 *low* across a reset.

### 4.2 The wiki says the BOOT button is on GPIO8. It is not.

The vendor pinout table lists `GPIO8 — BOOT — Chip boot mode control pin, default
connected to BOOT button`. The schematic disagrees, unambiguously:

```
net BOOT  = Key2 · R10 (10 k pull-up) · C22 (100 n) · H1 pin 18 · U1 pin 15
net IO8   = U1 pin 14 · H1 pin 20
```

QFN32 pin 15 is **GPIO9**; pin 14 is GPIO8. So **the button is GPIO9**, which is also
what the C6 uses as its download strap by default. Firmware that reads GPIO8 for "is
BOOT held" reads a floating header pin and sees "not pressed" forever.

Both statements are half-true, which is why the confusion sticks: GPIO8 *is* a boot-mode
strapping pin, it is just not the button.

### 4.3 Pad-JTAG is unavailable

Three of the four JTAG pads are taken: `MTMS`/GPIO4 = TF `CS`, `MTDI`/GPIO5 = IMU INT1,
`MTCK`/GPIO6 = IMU INT2. Only `MTDO`/GPIO7 is free. The default JTAG source is the
built-in USB Serial/JTAG controller on GPIO12/13, so debugging goes over the Type-C cable
alongside the console.

## 5. What is left free

**Two pins: GPIO7 and GPIO8.** That is the headline fact about this board — it is a
finished product with a header, not a devkit with spare IO.

| Pin | Header | Caveats |
|---|---|---|
| **GPIO7** | 22 | none. LP-IO capable. No ADC channel |
| **GPIO8** | 20 | boot-mode strap: leave floating or pull **high**, never low (§4.1). No LP IO, no ADC |

Free with something given up:

| Pin(s) | Header | Give up |
|---|---|---|
| GPIO5, GPIO6 | 19, 21 | the IMU's interrupt lines (the IMU still works polled) |
| GPIO16, GPIO17 | 5, 7 | UART0 — harmless, the console is on USB |
| GPIO3, GPIO4 | 15, 17 | the TF card slot |
| GPIO0 | — | battery voltage sensing; and the 200 k/100 k divider stays wired whatever you do |
| GPIO1, GPIO2 | 11, 13 | the display **and** the card slot |

### 5.1 ADC: there is none left

`ADC1_CH0`–`CH6` are GPIO0–GPIO6, and the board takes **all seven**: battery sense, SPI
SCLK, SPI MOSI, TF MISO, TF CS, IMU INT1, IMU INT2. Neither free pin (GPIO7, GPIO8) has
an analog function.

➜ **To read an analog signal you must give something up** — the IMU interrupts (GPIO5/6)
are the cheapest, the TF card (GPIO3/4) the next. Otherwise use an external I²C ADC on
the existing bus.

The battery divider on GPIO0 presents ~66 kΩ of source impedance to the SAR ADC, far
above the ~10 kΩ it likes. The 100 nF cap (C20) at the node is what makes it work.
⚠︎ **Inference:** average a dozen samples and expect a fixed offset — calibrate against a
meter rather than trusting `VBAT = 3 × V(GPIO0)` to the millivolt. Nominal divider drain
is ~14 µA at 4.2 V, permanently.

### 5.2 Deep sleep and the LP system

LP IO covers only GPIO0–GPIO7; the board takes GPIO0–GPIO6.

➜ **GPIO7 is the only LP-IO / deep-sleep wake pin left.**

| LP peripheral | Needs | Board uses those for | Available? |
|---|---|---|---|
| **LP UART** | `LP_GPIO4` / `LP_GPIO5` | TF `CS` / IMU `INT1` | ❌ (unless the card slot goes) |
| **LP I2C** | `LP_GPIO6` / `LP_GPIO7` | IMU `INT2` / free | ⚠️ possible if you give up IMU INT2 |

Deep-sleep current: the SoC's 7 µA figure does **not** describe this board. The LDO's
quiescent draw, the ETA6098, the battery divider (~14 µA) and the backlight driver all
add on top. Do not quote the datasheet number for this product.

## 6. Peripheral availability

| Peripheral | Status | Notes |
|---|---|---|
| **SPI2** | ⚠️ in use — LCD + TF card | one bus, two chip selects; a third device needs a third CS, and GPIO7/GPIO8 are all there is |
| **I2C (`I2C_NUM_0`)** | ⚠️ in use — touch 0x63 + IMU 0x6B | 10 k pull-ups fitted. External devices go on the same bus; watch for address clashes |
| **USB Serial/JTAG** | ✅ in use — console, flashing, JTAG | fixed to GPIO12/13 |
| **UART0** | free on GPIO16/17 | broken out to the header, unused by the board |
| **UART1** | ✅ but only GPIO7/GPIO8 are free | |
| **LEDC** | ✅ 6 channels; channel 0 drives the backlight | low-speed mode only on the C6 |
| **ADC** | ❌ | §5.1 |
| **RMT / MCPWM / PCNT / I2S / TWAI / PARLIO** | ⚠️ pin-starved | each needs pins the board does not have; PARLIO and I2S are effectively out |
| **SDIO slave** | ❌ | fixed pins GPIO18–23 are the I²C bus, touch control and the display |
| **Temperature sensor** | ✅ | no pins needed, −40…125 °C |
| **Wi-Fi / BLE / 802.15.4** | ✅ onboard antenna | shared antenna, internal coexistence |

## 7. Power

```
USB-C VBUS ──┬── Q1 (AO3401 P-MOS) ──┬── VSYS ── U4 ME6217C33M5G LDO ── 3V3
             │                       │
             └── U2 ETA6098 charger ─┴── D1 (MBR230LSFT1G) ── VBAT pad
                    ISET = R18 82 k        L3 2.2 µH / 1.9 A
                    STAT → LED1 (R17 3 k)
```

| Item | Detail |
|---|---|
| Regulator | ME6217C33M5G, 3.3 V |
| Charger | ETA6098, switching, charge current programmed by **R18 = 82 kΩ** |
| Battery | single-cell Li-ion on the `VBAT` pad / header pin 2. **Check polarity; never tie `VBAT` to `3V3` or `VBUS`** |
| Battery sense | GPIO0 via 200 k (R21, high side) / 100 k (R22, low side) → `VBAT = 3 × V(GPIO0)`, with 100 nF (C20) |
| USB | `CC1`/`CC2` 5.1 k (R15/R16), `D±` 22 R (R7/R8), ESD5451N on the data pair |
| Indicators | LED1 = charger `STAT`; LED2 = power |

## 8. Memory and flash layout

| | |
|---|---|
| Flash | 8 MB in package (ESP32-C6FH8), **no PSRAM** |
| HP SRAM | 512 KB; the linker hands out **327,680 B** of DRAM |
| LP SRAM | 16 KB |

⚠️ **8 MB of flash does not mean 8 MB of application.** With the default
`CONFIG_PARTITION_TABLE_SINGLE_APP` the app partition is **1 MB** and the other 7 MB are
unpartitioned. The `template/` full variant reports `25.9 % (used 271,242 bytes from
1,048,576 bytes)` — that denominator is the 1 MB partition, not the chip. Add a custom
`partitions.csv` before you plan on the space; nothing warns you until the image
overflows.

A full 172 × 320 × 2 B framebuffer is 110,080 B — a third of usable DRAM. The template
deliberately does **not** keep one; see §9.3.

## 9. The display

### 9.1 JD9853, not ST7789

The panel controller is a **Jadard JD9853**. ESP-IDF has no driver for it, and the two
obvious shortcuts both fail:

- The registry component `mydazy/esp_lcd_jd9853` **does not compile on ESP-IDF 6.x**
  (it uses `rgb_endian`, which became `rgb_ele_order`) and its built-in init sequence
  targets a 240 × 284 BOE panel, not this one.
- Driving it as an ST7789 gets you a picture (Arduino_GFX does exactly that) but skips
  the vendor register batch — gamma, power timing and panel-specific settings.

`template/components/jd9853/` is a self-contained `esp_lcd` panel driver with the
Waveshare register batch built in. Use it.

### 9.2 Four things decide whether you get a picture

1. **The vendor init sequence**, verbatim: `0xDF 98 53` unlocks the vendor registers,
   `0xDE` selects the register page (0, 1, 2 and back to 0), `0xC8` carries 32 bytes of
   gamma. It must be bracketed by `SLPOUT` (0x11, 120 ms) at the start and `DISPON`
   (0x29) at the end.
2. **`INVON` (0x21)** — ✅ **this panel is inverted.** Without it every colour is a
   photographic negative, which reads as "my RGB565 packing is broken" and sends you
   debugging the wrong thing. It is the last command of the built-in batch.
3. **`esp_lcd_panel_set_gap(panel, 34, 0)`** — the visible 172 columns sit at column
   offset **34** of the controller's 240-column RAM: (240 − 172) / 2 = 34. Without it
   the image is shifted sideways and wraps.
4. **Big-endian RGB565.** The panel wants big-endian pixels and the CPU is
   little-endian. Swap **once, at compile time**, in the colour macro — never per pixel
   in a loop.

Do **not** also call `esp_lcd_panel_init()`'s usual companions (`SLPOUT`, `COLMOD`,
`MADCTL`) yourself: the vendor batch already contains them, and sending them twice
upsets the register paging.

### 9.3 `draw_bitmap` is asynchronous — the bug that costs a day

`esp_lcd_panel_draw_bitmap()` **queues** a DMA transfer and returns immediately. Compose
the next tile into the same buffer and the panel receives half-old, half-new bytes: the
screen fills with coloured garbage, intermittently, which looks exactly like a flaky
panel or a bad ribbon.

✅ Register `on_color_trans_done` in `esp_lcd_panel_io_spi_config_t`, give a binary
semaphore from it, and take that semaphore after every `draw_bitmap` before touching the
buffer again. `flush_rect()` in `template/src/main.c` is the pattern.

### 9.4 40 MHz is the ceiling, and it is a wiring fact

The IO MUX puts SPI2's fast path at fixed pins — `FSPICLK` on GPIO6, `FSPID` on GPIO7,
`FSPIQ` on GPIO2. This board wires `SCLK` to **GPIO1** and `MOSI` to **GPIO2**. GPIO1 has
no FSPI function at all (QFN32 IO MUX table: F0 = GPIO1, F1 = GPIO1, F2 empty), and
GPIO2's is MISO, not MOSI. Both signals therefore go through the **GPIO Matrix**.

⚠︎ **Inference:** GPIO-Matrix-routed SPI on ESP32 parts is conventionally limited to
about half the IO-MUX ceiling; SPI2 master tops out at 80 MHz, so **40 MHz** is the
practical wall. The template sets exactly that. No software change recovers it — the
panel is physically on the wrong pins.

```
172 × 320 × 2 bytes = 110,080 bytes = 880,640 bits
880,640 / 40,000,000 = 22.0 ms per full-screen flush  →  ~45 fps ceiling
```

### 9.5 Tiles, not a framebuffer

The template renders straight from the scene description into a **172 × 32 staging
buffer** (11,008 B, `MALLOC_CAP_DMA`) and pushes it band by band. That is why the full
variant links at 12.9 KB of static RAM instead of 123 KB.

The trade is CPU for RAM: `compose()` runs once per band, so painting the background of
the whole scene costs *n* times over. It is the right trade here — 110 KB of DRAM buys
nothing when the bus caps you at 45 fps anyway, and it leaves room for Wi-Fi.

Redrawing less is what actually buys speed. `flush_rect()` takes a rectangle:
`move_dot()` repaints two 19 × 19 boxes rather than the screen, and `set_coord_line()`
repaints one text band. Coordinates are `(x_start, y_start, x_end, y_end)` with the end
exclusive, and the source buffer's stride must equal the rectangle width.

**The wire is not the whole cost.** Every `esp_lcd_panel_draw_bitmap()` pays a fixed toll
on top of its pixels — `CASET`, `RASET`, `RAMWR` and the DMA round trip — of roughly
**450 µs**. ✅ The toll is real and large: it was measured on a board (ESP-IDF 6.1.0,
40 MHz, `esp_lcd_panel_io_spi` with `trans_queue_depth = 1` and an `on_color_trans_done`
semaphore). ⚠︎ The 450 µs figure itself is **derived from whole-frame timings, not from
timing one call in isolation** — treat it as the right order of magnitude, and as a
number that predicted frame times well enough to schedule against.

| Rectangle | Pixels on the wire | + the call | Total |
|---|---|---|---|
| 14 × 14 cell | 78 µs | 450 µs | **~0.53 ms** |
| 56 × 14, four cells | 314 µs | 450 µs | ~0.76 ms |
| 140 × 14, a full row | 784 µs | 450 µs | ~1.2 ms |
| 172 × 320, full screen | 22.0 ms | 450 µs | ~22.5 ms |

Pixel arithmetic alone therefore **under-predicts small rectangles by 2–6×**, and two
rules fall out of that:

- **Merge before you split.** 450 µs buys about six 14 × 14 cells, so one rectangle
  spanning a gap beats two rectangles either side of it whenever the gap is under about
  six cells. Repainting a whole changed row in one call is nearly always cheaper than
  repainting the two changed pieces of it.
- **Budget the frame; do not repaint on demand.** A 60 fps frame is 16.7 ms, and a
  10 × 20 matrix of 14 px cells is 15.7 ms of pure wire time, so a full repaint can never
  fit in one frame however it is sliced. Spend a fixed budget per frame, counted in
  cell-equivalents with every call charged its ~6 cells, and leave the remainder marked
  dirty for the next frame — resuming where you stopped so no region starves.

✅ Measured on a 60 fps Tetris build (10 × 20 matrix, 14 px cells) driven by an autoplay
that hard-dropped a piece three times a second: at a budget of 130 cell-equivalents the
worst frame was 19 ms and the loop lost a frame here and there; at **105** the worst frame
was 15 ms and 97.5 % of frames came in under 12 ms, holding a steady 60 fps.

Genuinely full-screen repaints cannot be made to fit — a screen change is 22 ms whatever
you do. Budget those as a deliberate dropped frame, not as something to optimise away.

Keep per-pixel loops integer-only: the RISC-V core here has **no FPU**. Float belongs in
one-time init paths, never in a redraw.

### 9.6 The backlight

GPIO23 → 1 kΩ (R14) → base of an SS8050 NPN; `LEDK` → 10 Ω (R13) → collector; emitter to
ground. **Active high**, and PWM-able directly from an LEDC channel: low-speed mode
(the C6 has no high-speed mode), 8-bit resolution, 5 kHz. ✅ verified at 90 % duty.

Waveshare prints a "keep brightness at 50 % or lower, overheating causes permanent dark
shadows" warning on the *non-touch* ESP32-C6-LCD-1.47 page. It is **not** on this
product's page and this is a different panel module, so it is not reproduced here as a
rule — but it is the same size and vendor, so treat sustained 100 % backlight as a
question worth asking rather than a settled one.

---

## Part II — Development guide

## 10. Toolchain

✅ Verified combination:

| | |
|---|---|
| PlatformIO Core | 6.1.19 |
| `platform-espressif32` | **7.1.0** |
| ESP-IDF | **6.1.0** (`framework-espidf` 4.60100.0) |
| Toolchain | `toolchain-riscv32-esp` 15.2.0 |
| `board` | `esp32-c6-devkitc-1` |

There is no PlatformIO board definition for this product. The generic C6 DevKitC one is
used; unlike the non-touch board, its **8 MB flash size is already correct** here, so no
`board_upload.flash_size` override is needed. Everything else it gets wrong is the pin
map, which lives in `include/board_pins.h`.

Arduino is also viable (Waveshare ships an AXS5106L Arduino library and Arduino_GFX
supports the panel as an ST7789 with a 34-column offset), but everything in this skill is
ESP-IDF.

✅ **Set `PROJECT_VER` before `project()`.** ESP-IDF otherwise derives the version with
`git describe`, which fails outright in a repository that has no commits yet — a brand new
`git init` is the common case for a fresh project. The build stops during configuration
with `fatal: not a git repository` and a CMake error about a missing `head-ref` file, in a
directory that plainly *is* a repository, which sends you looking in the wrong place. The
template's `CMakeLists.txt` carries the fix:

```cmake
cmake_minimum_required(VERSION 3.16.0)
set(PROJECT_VER "1.0.0")          # before project(): skips git describe
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my-project)
```

## 11. sdkconfig essentials

```ini
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y      # ESP32-C6FH8
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y  # console on the Type-C port, not UART0
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_FREERTOS_HZ=1000               # only if you pace frames -- see below
```

The console line is the one that costs an afternoon. Without it `printf` and `ESP_LOG`
go to UART0 on GPIO16/17, which only reach the header — the monitor is silent while the
firmware runs perfectly.

**`CONFIG_FREERTOS_HZ` defaults to 100**, so the shortest `vTaskDelay()` is 10 ms. Any
loop that paces itself on a frame period — 16.7 ms for 60 fps, 33.3 ms for 30 — cannot be
held at that granularity: the sleep quantises to 10 or 20 ms and the rate lands wherever
the rounding puts it, with no error and nothing to debug. ✅ 1 kHz gives a 1 ms quantum
and holds 60 fps; pace against `esp_timer_get_time()` and use the tick only to sleep. Do
not raise it if nothing in the project needs sub-10 ms sleeps — the tick interrupt is not
free.

## 12. Flashing, monitoring, recovery

```sh
pio run                        # build
pio run -t upload              # flash
pio run -t upload -t monitor   # flash and watch the log
```

Nothing to press. The USB Serial/JTAG controller supports host-controlled reset and
download-mode entry, so `esptool` drives the whole cycle over the Type-C cable. The board
enumerates as CDC-ACM: `/dev/cu.usbmodem*` (macOS), `/dev/ttyACM*` (Linux), a COM port
(Windows).

**Pin the port.** Without `monitor_port` / `upload_port`, PlatformIO takes the first
`/dev/cu.*` it finds, which on macOS is a Bluetooth or AirPlay pseudo-port; the monitor
then attaches to nothing and prints nothing.

**Close the monitor before uploading** — both want the port exclusively;
`Could not open … port is busy` means a monitor is still holding it.

Recovery, in order:

1. Nothing enumerates at all → almost always a charge-only USB cable. The ROM creates the
   USB device on a blank chip, so enumeration does not depend on your firmware.
2. Firmware has wedged USB, or a console config change broke enumeration → **hold BOOT,
   tap RESET, release BOOT**.
3. That still fails → **jumper header pin 20 (GPIO8) to 3V3** and repeat (§4.1).
4. Still stuck → `esptool.py -p <port> erase_flash`. An 8 MB erase takes a while; it is
   probably not hung.

There is no debug header. JTAG is the same Type-C port via the built-in controller — but
PlatformIO's pinned `tool-openocd-esp32` (`~2.1100.0`) predates the ESP32-C6 and ships no
`board/esp32c6-builtin.cfg`, so `pio debug` fails with what looks like a typo. Use a
current upstream `openocd-esp32` from Espressif, or an ESP-IDF export'd environment's
`idf.py openocd`.

## 13. Peripheral cookbook

Code lives in [recipes.md](recipes.md). What is where:

| Task | Recipe | Verified? |
|---|---|---|
| `platformio.ini` / `sdkconfig.defaults` / `board_pins.h` | §1–3 | ✅ |
| SPI bus + JD9853 panel bring-up | §4 | ✅ |
| Flushing a rectangle without corrupting the buffer | §5 | ✅ |
| Backlight PWM | §6 | ✅ |
| I²C bus + a scan | §7 | ✅ |
| AXS5106L touch | §8, and [touch-axs5106l.md](touch-axs5106l.md) | ✅ |
| Three-point touch calibration in NVS | §9 | ✅ |
| BOOT button | §10 | ⚠︎ compile-checked (the GPIO9 fix is schematic-derived) |
| Battery voltage | §11 | ⚠︎ compile-checked |
| QMI8658A IMU | §12 | ⚠︎ address is schematic + datasheet; no code run |
| TF card on the shared bus | §13 | ⚠︎ compile-checked |

## 14. Pitfall table

| Symptom | Real cause | Fix |
|---|---|---|
| Monitor attaches to `/dev/cu.MacBookAir` or a Bluetooth port | `monitor_port` not pinned | `monitor_port = /dev/cu.usbmodem*` |
| Monitor silent, firmware clearly running | console defaulted to UART0 | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` |
| `Could not open … port is busy` on upload | a monitor still holds the port | Ctrl-C it first |
| Nothing enumerates on USB | charge-only cable | use a data cable |
| BOOT + RESET does not enter download mode | GPIO8 floats; Joint Download Boot needs it high | jumper header pin 20 to 3V3 (§4.1) |
| "Hold BOOT at reset" feature never triggers | firmware read GPIO8; the button is GPIO9 (§4.2) | read GPIO9 |
| Display is a photographic negative | missing `INVON` (0x21) | it is the last line of the vendor batch |
| Picture shifted sideways / wraps at the edge | missing `x_gap = 34` | `esp_lcd_panel_set_gap(panel, 34, 0)` |
| Coloured garbage after redraws, intermittent | reused the DMA buffer before the transfer finished | `on_color_trans_done` + semaphore (§9.3) |
| Colours inverted per channel but not negative | RGB565 not byte-swapped for the big-endian panel | swap in the colour macro |
| Display garbles while the TF card is idle | TF `CS` (GPIO4) not parked high | drive GPIO4 high before opening the SPI bus |
| `spi_bus_initialize` returns `ESP_ERR_INVALID_STATE` | SPI2 initialised twice — the LCD and the card share it | initialise once, add a second device |
| `AXS5106L not responding at 0x63` | presence checked by chip-ID contents, not address ACK | `i2c_master_probe()` |
| I²C reads fail forever although the address ACKs | used a combined write+read (repeated START) | write, STOP, then a separate read |
| Touch coordinates land in random places | read fewer than 14 bytes of the touch frame | always read all 14 |
| Touch dot mirrored, or in the opposite corner | rotation 0 for this panel is mirror-X only | `screen_x = 171 − raw_x`, `screen_y = raw_y` |
| Panel glows garbage for a moment at power-on | GPIO23's reset pull-up biases the backlight transistor on | claim GPIO23 at 0 % duty first (§3) |
| Image links fine, board does not boot | app exceeded the 1 MB default partition | custom `partitions.csv` (§8) |
| `analogRead`-style code finds no free ADC pin | all seven ADC channels are used (§5.1) | give up the IMU interrupts or the card slot |
| `pio run` fails to configure: `fatal: not a git repository` in a directory that *is* one | ESP-IDF ran `git describe` for the version; the repo has no commits yet | `set(PROJECT_VER "1.0.0")` before `project()` (§10) |
| A loop paced for 60 fps settles at 50, or the rate jumps between values | `CONFIG_FREERTOS_HZ` is 100, so the shortest sleep is 10 ms | `CONFIG_FREERTOS_HZ=1000` (§11) |
| Frame time spikes when many small rectangles are pushed, though few pixels moved | ~450 µs fixed cost per `draw_bitmap`, independent of size | merge rectangles, budget per frame (§9.5) |

## 15. Differences from the non-touch ESP32-C6-LCD-1.47

They share a name, a chip family and a panel size. Almost nothing else. If a project
was written for the other board, **every one of these lines is a bug**:

| | ESP32-C6-**LCD**-1.47 | ESP32-C6-**Touch**-LCD-1.47 |
|---|---|---|
| SoC | ESP32-C6FH4, 4 MB flash | **ESP32-C6FH8, 8 MB flash** |
| Panel controller | ST7789 | **JD9853** (needs a vendor register batch) |
| SPI `SCLK` | GPIO7 | **GPIO1** |
| SPI `MOSI` | GPIO6 | **GPIO2** |
| LCD `CS` / `DC` | GPIO14 / GPIO15 | same |
| LCD `RST` | GPIO21 | **GPIO22** |
| Backlight | GPIO22 (direct) | **GPIO23** (via an NPN) |
| TF `CS` / `MISO` | GPIO4 / GPIO5 | GPIO4 / **GPIO3** |
| Touch | none | **AXS5106L, I²C 0x63** |
| IMU | none | **QMI8658A, I²C 0x6B** |
| RGB LED | WS2812 on GPIO8 | **none** |
| Battery | none | ETA6098 charger, `VBAT` pad, sense on GPIO0 |
| Free GPIOs | 8 | **2** |
| `sdkconfig` flash size | `4MB` (+ `board_upload.flash_size`) | `8MB`, and the DevKitC default is already right |

Both have: 172 × 320, `x_gap = 34`, an inverted panel, big-endian RGB565, a 40 MHz
GPIO-Matrix SPI ceiling, USB Serial/JTAG console, BOOT on GPIO9.

## 16. Sources

- [Waveshare wiki — ESP32-C6-Touch-LCD-1.47](https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47)
  — pin map, peripheral quick reference, precautions. Wrong about the BOOT button (§4.2).
- `ESP32-C6-Touch-LCD-1.47-Schematic.pdf` (Waveshare, one sheet) — the authority for every
  net in §2, §2.1, §3 and §7.
- `Jd9853_datasheet.pdf` (Jadard) — controller register set.
- `AXS5106_DataSheet_EN.pdf` — touch controller.
- `QMI8658A.pdf` (QST) — IMU register set and the SA0 addressing rule.
- ESP32-C6 datasheet v1.5 and Technical Reference Manual — digested in
  [esp32c6-soc.md](esp32c6-soc.md).
- [Arduino_GFX discussion #693](https://github.com/moononournation/Arduino_GFX/discussions/693)
  — the working JD9853 register batch and the 34-pixel column offset.
- [toto04/axs5106l](https://github.com/toto04/axs5106l) — a faithful port of Waveshare's
  AXS5106L Arduino library; the source of truth for the touch protocol.
- [Adafruit GFX classic font](https://github.com/adafruit/Adafruit-GFX-Library) (BSD) —
  the 5 × 7 glyphs in `template/include/font5x7.h`.
