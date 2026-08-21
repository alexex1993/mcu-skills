# Waveshare ESP32-C6-LCD-1.47 — hardware and development reference

Everything the board does with the chip, and everything you need to build for it.
Chip-level facts (IO MUX tables, electrical limits, peripheral feature lists) live in
[esp32c6-soc.md](esp32c6-soc.md); code you can paste lives in [recipes.md](recipes.md).

**⚠︎ Inference** marks a conclusion drawn by combining the Waveshare wiki with the
Espressif datasheet or with the template firmware. It is not printed in either
vendor document — verify against the schematic before betting hardware on it.

---

## Part I — Hardware

## 1. What is on the board

| | |
|---|---|
| SKU | **28563** (bare) · **30381** = the `-M` variant, pin header pre-soldered |
| SoC | **ESP32-C6FH4** — QFN32, **4 MB in-package flash**, 22 GPIOs |
| CPU | RISC-V RV32IMAC HP core @ **160 MHz** + RISC-V LP core @ 20 MHz |
| Memory | 512 KB HP SRAM · 16 KB LP SRAM · 320 KB ROM · 4 MB flash |
| Display | **1.47" TFT, ST7789, 172×320, 262K colours**, SPI2, write-only |
| LED | one **WS2812-family addressable RGB LED** on GPIO8 |
| Storage | microSD (TF) slot, **SPI mode only** — `SD_D1`/`SD_D2` are NC |
| Regulator | **ME6217C33M5G** LDO, 800 mA max |
| Radio | Wi-Fi 6 (802.11ax/b/g/n), BLE 5, 802.15.4, onboard ceramic antenna |
| Connector | USB Type-C wired straight to the SoC's USB-Serial-JTAG (GPIO12/13) |
| Buttons | **BOOT** (GPIO9, pressed = low) · **RESET** (acts on `CHIP_PU`, not a GPIO) |

There is **no USB-UART bridge chip** and **no debug header**. The Type-C port is
simultaneously the console, the flasher and the JTAG probe.

> The distributor sheet says Wi-Fi "802.11 b/g/n"; the wiki and Espressif both say
> **ax**/b/g/n. Espressif is authoritative — the SoC is 802.11ax-compliant.

> **262K colours** is the panel's maximum (18-bit). Drivers, including the template,
> address it in **RGB565 / 16-bit**, which the ST7789 also supports.

### 1.1 Soldering warning (verbatim from the wiki)

> Do not remove the screen when soldering the pin headers. Tilt the soldering iron
> and solder the headers directly.

### 1.2 Brightness warning (verbatim from the wiki, printed twice)

> Keep the screen brightness at **50 % or lower** during use, and **do not operate
> the screen at full brightness for extended periods**. Excessive brightness
> increases the screen temperature, and overheating may cause **dark shadows on the
> screen** and affect normal display.
>
> If the display is already abnormal, **allow the development board to cool down**,
> then flash a program with a **lower brightness setting**.

The damage is cumulative and permanent. This is why the template drives `LCD_BL`
from an LEDC channel and clamps at 50 % rather than using `gpio_set_level()`.

---

## 2. Master pin table

The QFN32 package has 22 GPIOs. **GPIO10, GPIO11 and GPIO24–GPIO30 do not exist**
on it; GPIO14 exists *only* on it. The board consumes 10 of the 22.

| GPIO | QFN32 pin | Board function | IO MUX F0 | Analog | LP IO | Priority | Strap? | Notes |
|---|---|---|---|---|---|---|---|---|
| **GPIO0** | 6 | *free* | GPIO0 | `XTAL_32K_P` / `ADC1_CH0` | `LP_GPIO0` | **P2** | — | also the 32 kHz crystal pin if one were fitted |
| **GPIO1** | 7 | *free* | GPIO1 | `XTAL_32K_N` / `ADC1_CH1` | `LP_GPIO1` | **P2** | — | |
| **GPIO2** | 8 | *free* | GPIO2 | `ADC1_CH2` | `LP_GPIO2` | **P2** | — | also `FSPIQ` (SPI2 MISO fast path) |
| **GPIO3** | 9 | *free* | GPIO3 | `ADC1_CH3` | `LP_GPIO3` | **P2** | — | |
| **GPIO4** | 10 | **TF card `CS`** | `MTMS` | `ADC1_CH4` | `LP_GPIO4` / `LP_UART_RXD` | P3 | ✅ SDIO edge | JTAG `TMS`; also `FSPIHD` |
| **GPIO5** | 11 | **TF card `MISO`** | `MTDI` | `ADC1_CH5` | `LP_GPIO5` / `LP_UART_TXD` | P3 | ✅ SDIO edge | JTAG `TDI`; also `FSPIWP` |
| **GPIO6** | 12 | **SPI `MOSI`** (LCD + TF) | `MTCK` | `ADC1_CH6` | `LP_GPIO6` / `LP_I2C_SDA` | P3 | — | JTAG `TCK`; the IO MUX slot here is `FSPICLK`, **not** MOSI — see §9.1 |
| **GPIO7** | 13 | **SPI `SCLK`** (LCD + TF) | `MTDO` | — | `LP_GPIO7` / `LP_I2C_SCL` | P3 | — | JTAG `TDO`; IO MUX slot here is `FSPID`, **not** SCLK |
| **GPIO8** | 14 | **RGB LED data** | GPIO8 | — | — | P3 | ✅ boot mode + ROM print | see §4 |
| **GPIO9** | 15 | *free* — **BOOT button** | GPIO9 | — | — | P3 | ✅ boot mode | weak pull-up = `1` at reset |
| **GPIO12** | 16 | **USB `D−`** | GPIO12 | `USB_D−` | — | P3 | — | 40 mA default drive strength |
| **GPIO13** | 17 | **USB `D+`** | GPIO13 | `USB_D+` | — | P3 | — | 40 mA default drive strength |
| **GPIO14** | 18 | **LCD `CS`** | GPIO14 | — | — | **P2** | — | QFN32-only pin |
| **GPIO15** | 19 | **LCD `DC`** | GPIO15 | — | — | P3 | ✅ JTAG source | no internal pull resistors |
| **GPIO16** | 21 | *free* | `U0TXD` | — | — | P3 | — | UART0 TX; also `FSPICS0` |
| **GPIO17** | 22 | *free* | `U0RXD` | — | — | P3 | — | UART0 RX; also `FSPICS1` |
| **GPIO18** | 23 | *free* | `SDIO_CMD` | — | — | **P2** | — | also `FSPICS2` |
| **GPIO19** | 24 | *free* | `SDIO_CLK` | — | — | **P2** | — | also `FSPICS3` |
| **GPIO20** | 25 | *free* | `SDIO_DATA0` | — | — | **P2** | — | also `FSPICS4` |
| **GPIO21** | 26 | **LCD `RST`** | `SDIO_DATA1` | — | — | **P2** | — | also `FSPICS5` |
| **GPIO22** | 27 | **LCD backlight** | `SDIO_DATA2` | — | — | **P2** | — | |
| **GPIO23** | 28 | *free* | `SDIO_DATA3` | — | — | **P2** | — | |

Wiki-quoted groupings, for cross-checking:

- **LCD** — `MOSI` GPIO6 · `SCLK` GPIO7 · `LCD_CS` GPIO14 · `LCD_DC` GPIO15 ·
  `LCD_RST` GPIO21 · `LCD_BL` GPIO22. No MISO line to the panel.
- **TF card** — `MISO` GPIO5 · `MOSI` GPIO6 · `SCLK` GPIO7 · `CS` GPIO4 ·
  `SD_D1`/`SD_D2` **NC**.
- **RGB LED** — `RGB_Control` GPIO8.

**The TF card and the LCD share `MOSI` (GPIO6) and `SCLK` (GPIO7)**, separated only
by their chip selects. Anything you add to that bus is a third CS, not a third bus.

## 3. Reset-time pin states

What each board signal does *before* your firmware runs (datasheet Table 2-2):

| Board signal | GPIO | At reset | After reset | Consequence |
|---|---|---|---|---|
| LCD `RST` | GPIO21 | `WPU` | `IE` | weakly pulled **high** → the panel is **not** held in reset while the chip boots. Good. |
| LCD backlight | GPIO22 | `WPU` | `IE` | weakly pulled **high**. ⚠︎ **Inference:** depending on the backlight driver's input impedance the backlight may glimmer between power-on and the moment firmware drives GPIO22. `gfx_init()` sets 0 % duty as its first act. |
| LCD `CS` | GPIO14 | `IE` | — | input, floating — panel CS undriven until SPI is configured. Harmless. |
| LCD `DC` | GPIO15 | `IE` | `IE` | input, no internal pull. Harmless at factory eFuse settings (§4). |
| TF `CS` | GPIO4 | `IE` | `IE` | input, floating → card not selected. Good. |
| RGB LED | GPIO8 | `IE` | `IE` | input, floating → data line idle. Good. |
| USB `D−`/`D+` | GPIO12/13 | `IE` / `USB_PU` | `IE` / `IE, USB_PU` | USB enumerates from ROM with no firmware present. This is what makes a bricked board recoverable over the same cable. |

## 4. Strapping-pin analysis

Four of the five strapping pins are tied to real hardware. All the eFuse parameters
below **default to 0 (not burnt)**, and eFuse is one-time programmable.

| Pin | Board use | Strapping role | Verdict |
|---|---|---|---|
| **GPIO8** | RGB LED data | chip boot mode (only consulted when GPIO9 = 0); UART0 ROM message printing (only consulted when `EFUSE_UART_PRINT_CONTROL` ≠ 0, default 0 = *ignored*) | ✅ safe at factory settings |
| **GPIO9** | BOOT button | chip boot mode. Weak pull-up → `1` → SPI boot. Held low + GPIO8 = 1 → Joint Download Boot | ✅ safe; this *is* the flashing mechanism |
| **GPIO4** (`MTMS`) | TF `CS` | SDIO **slave** sampling/driving clock edge | ✅ safe — the board is an SPI *host* here, unrelated |
| **GPIO5** (`MTDI`) | TF `MISO` | same | ✅ safe |
| **GPIO15** | LCD `DC` | JTAG signal source, only consulted when `EFUSE_JTAG_SEL_ENABLE` = 1 (default 0 → source is the USB Serial/JTAG controller, GPIO15 *ignored*) | ✅ safe at factory settings |

**Do not burn `EFUSE_UART_PRINT_CONTROL` or `EFUSE_JTAG_SEL_ENABLE` on this board.**
Either one hands a boot-time decision to a pin that already has a job — the RGB LED's
idle level, or the LCD's data/command line — and the bit can never be un-burnt.

### 4.1 Pad-JTAG is unavailable

All four JTAG pads (`MTMS`/GPIO4, `MTDI`/GPIO5, `MTCK`/GPIO6, `MTDO`/GPIO7) are
consumed by the TF card and the shared SPI bus. That is fine: the default JTAG source
is the built-in USB Serial/JTAG controller on GPIO12/13, so debugging goes over the
Type-C cable alongside the console.

## 5. What is left free

**Eight comfortable general-purpose pins: GPIO0, 1, 2, 3, 18, 19, 20, 23.** All are
Priority 2 with no strap, JTAG, UART or USB entanglement.

Three more are free with conditions: **GPIO9** (BOOT button is on it — treat as an
input with an external pull-up, never drive it hard low across a reset unless you
want download mode), **GPIO16/GPIO17** (UART0 TX/RX — free only if you do not want a
hardware serial console; this board's console is on USB anyway).

### 5.1 ADC

The SoC's only ADC pins are GPIO0–GPIO6 (`ADC1_CH0`–`CH6`). The board takes GPIO4,
GPIO5, GPIO6.

➜ **Four channels remain: `ADC1_CH0`–`CH3` on GPIO0–GPIO3.** Calibrated total error
±12 mV at ATTEN0/1, ±23 mV at ATTEN2, ±40 mV at ATTEN3 over 0–3300 mV; ceiling
100 kSPS. A 100 nF cap on the input gets you the datasheet numbers, which were
measured **with Wi-Fi disabled**.

### 5.2 Deep sleep and the LP system

LP IO covers only GPIO0–GPIO7; the board takes GPIO4–GPIO7.

➜ **LP GPIO wake sources remain on GPIO0–GPIO3 only** — the same four pads as the
ADC. They compete.

| LP peripheral | Needs | Board uses those for | Available? |
|---|---|---|---|
| **LP UART** | `LP_GPIO4`/`LP_GPIO5` | TF `CS` / `MISO` | ❌ |
| **LP I2C** | `LP_GPIO6`/`LP_GPIO7` | shared SPI `MOSI` / `SCLK` | ❌ |
| LP UART flow control | `LP_GPIO0`–`LP_GPIO3` | free | ✅ but useless without RXD/TXD |

⚠︎ **Inference:** there is no configuration in which the LCD, the TF card and an LP
peripheral all coexist. An LP-CPU application that talks to a sensor while the HP CPU
sleeps must give up either the card slot (frees GPIO4/5 for LP UART) or the display
(frees GPIO6/7 for LP I2C).

## 6. Peripheral availability

| Peripheral | Status | Notes |
|---|---|---|
| **SPI2** | ⚠️ in use — LCD + TF card | shared bus, two chip selects; a third device means a third CS from the free pins |
| **USB Serial/JTAG** | ✅ in use — console, flashing, JTAG | P1-only peripheral on GPIO12/13; nothing else can go there |
| **UART0** | broken out on GPIO16/17, unused | console is on USB instead |
| **UART1** | ✅ any free GPIO | **LP UART ❌** |
| **I2C** | ✅ any two free GPIOs via the GPIO Matrix | **LP I2C ❌** |
| **I2S** | ✅ any free GPIOs | needs 3–4 pins; eight are free |
| **ADC** | ✅ 4 of 7 channels (GPIO0–3) | §5.1 |
| **LEDC** | ✅ 6 channels, any GPIO | channel 0 drives the backlight in the template |
| **MCPWM** | ✅ any free GPIOs | |
| **RMT** | ⚠️ one TX channel drives the RGB LED | 4 channels total |
| **PCNT** | ✅ any free GPIOs | 4 counters × 2 channels |
| **TWAI® (CAN)** | ✅ two controllers | needs an external transceiver |
| **PARLIO** | ⚠️ theoretically | an 8-/16-bit bus needs more free pins than exist |
| **SDIO slave** | ❌ | fixed pins GPIO18–23 collide with LCD `RST` (21) and backlight (22) |
| **Temperature sensor** | ✅ | no pins needed, −40…125 °C |
| **Wi-Fi / BLE / 802.15.4** | ✅ onboard antenna | shared antenna, internal coexistence |

## 7. Power

| Item | Figure | Source |
|---|---|---|
| LDO (ME6217C33M5G) max output | **800 mA** | wiki |
| SoC peak, Wi-Fi TX 802.11b @ 21 dBm | **354 mA** | Table 5-7 |
| BLE TX @ 20 dBm | 315 mA | Table 5-8 |
| 802.15.4 TX @ 20 dBm | 305 mA | Table 5-9 |
| Wi-Fi RX | 78–82 mA | Table 5-7 |
| Modem-sleep, 160 MHz, CPU running | 27 mA | Table 5-10 |
| Modem-sleep, 80 MHz, CPU idle | 14 mA | Table 5-10 |
| Light-sleep | 35 µA | Table 5-11 |
| Deep-sleep (RTC timer + LP memory on) | 7 µA | Table 5-11 |
| Recommended cumulative `I_VDD` | min **0.5 A** | Table 5-2 |

⚠︎ **Inference:** 800 mA against a 354 mA SoC peak leaves ~450 mA for the backlight,
the RGB LED and the header — generous, but **the backlight and the RGB LED are not in
any of these numbers**; the datasheet figures are SoC-only. Budget them from the panel
and LED datasheets if you run from a battery.

⚠︎ And the 7 µA Deep-sleep figure is **not the board's** sleep current. The LDO's
quiescent draw, the backlight driver and the RGB LED's standby current add on top.
Never quote 7 µA for this board.

Voltage limits worth remembering: recommended supply 3.0 / 3.3 / 3.6 V
(min/typ/max); **absolute maximum 3.6 V** on input power pins; when burning eFuses
`VDDPST2` must not exceed **3.3 V**; `CHIP_PU` must never float (the board handles
this — relevant only if you tap that net).

## 8. Memory and flash layout

| Region | Size | Notes |
|---|---|---|
| ROM | 320 KB | boot and core functions |
| HP SRAM | **512 KB** | ESP-IDF reports ~320 KB of it as linkable DRAM after the ROM/BT reservations |
| LP SRAM | 16 KB | retains data through Deep-sleep |
| In-package flash | **4 MB** | ESP32-C6FH4; the FH8 variant on other boards has 8 MB |
| eFuse | 4096 bits | 1792 available to users |

The default ESP-IDF partition table gives a **1 MB app partition** out of the 4 MB.
The template's full variant lands at 245 KB of that. If you outgrow 1 MB, add a
`partitions.csv` and `board_build.partitions =` rather than shrinking code.

⚠️ A full-chip erase of a 4 MB part takes **typically 20 s, up to 60 s** (Table 5-12).
If `esptool.py erase_flash` looks hung, it probably is not.

The GDMA controller can address up to **384 KB of internal RAM**, so a 107 KB
framebuffer is comfortably within its reach in one descriptor chain.

## 9. The display

172×320 ST7789 over SPI2, write-only, `LCD_X_GAP = 34` because the 172-pixel glass is
centred in the controller's 240-column RAM: (240 − 172) / 2 = 34.

Two panel quirks that cost an evening each if you do not know them:

- **The panel ships colour-inverted.** Without `esp_lcd_panel_invert_color(panel,
  true)` the image is a photographic negative — correct geometry, correct text,
  inverted colours, which reads as "my colour conversion is broken".
- **The panel wants big-endian RGB565.** The template configures
  `.data_endian = LCD_RGB_DATA_ENDIAN_BIG` and byte-swaps once at compile time inside
  the `GFX_RGB()` macro, so nothing swaps per pixel at runtime.

### 9.1 The SPI pins do not line up with the IO MUX fast path

The IO MUX puts SPI2's signals at fixed pins: `FSPICLK` on **GPIO6**, `FSPID` (MOSI)
on **GPIO7**, `FSPIQ` on GPIO2, `FSPICS0` on GPIO16. The board wires it **the other
way round** — `MOSI` → GPIO6, `SCLK` → GPIO7 — so neither signal can take the direct
route and both go through the **GPIO Matrix**, which the datasheet notes "might
affect the latency of routed signals".

⚠︎ **Inference:** on ESP32-family parts, GPIO-Matrix-routed SPI is conventionally
limited to about half the IO-MUX ceiling. SPI2 master maxes at 80 MHz, so expect a
practical ceiling near **40 MHz** here. The template already sets exactly 40 MHz —
nothing is being left on the table, but **there is no headroom above it**, and
swapping the pins in software cannot help because the panel is physically wired this
way.

### 9.2 The 45 fps wall

```
172 × 320 × 2 bytes = 110,080 bytes = 880,640 bits
880,640 / 40,000,000 = 22.0 ms per full-screen flush  →  ~45 fps ceiling
```

`gfx_present()` blocks on the transfer-done semaphore, so a full-screen effect is
hard-capped near 45 fps no matter how cheap it is. If an effect looks slow, this is
the wall it is hitting — not the CPU. The ways past it, in order of payoff:

1. **Redraw less.** Push only the dirty rectangle with
   `esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, buf)`. A 172×40 status strip is
   2.75 ms, not 22 ms.
2. **Double-buffer.** Two framebuffers cost another 110 KB of the ~320 KB of usable
   DRAM; drop the semaphore wait and render into the back buffer while the front one
   is on the wire. This overlaps CPU and DMA but does not raise the 45 fps ceiling.
3. Nothing else. The bus is the bus.

### 9.3 Framebuffer memory

`g_fb` is 110,080 bytes of `.bss` — about 21 % of HP SRAM, and roughly a third of the
DRAM ESP-IDF actually hands out. The full template links at 123 KB RAM total. Adding
a Wi-Fi stack costs another ~50 KB; adding a second framebuffer costs 110 KB. You
cannot have all three plus LVGL comfortably — pick two.

---

## Part II — Development guide

## 10. Toolchain

There is **no PlatformIO board definition for this board.** Use the generic DevKitC
one and correct the flash size:

```ini
[env:esp32-c6-lcd-1_47]
platform = espressif32
board = esp32-c6-devkitc-1
framework = espidf

board_upload.flash_size = 4MB
board_upload.maximum_size = 4194304

upload_protocol = esptool
monitor_speed = 115200
monitor_filters = esp32_exception_decoder, direct
```

Verified with PlatformIO Core 6.1.19, `platform-espressif32` 7.0.1, ESP-IDF 6.0.1.
Arduino-framework and plain ESP-IDF (`idf.py`) both work on this board too; the pin
map and every hardware fact above is framework-independent.

`monitor_filters = ... direct` matters: without it the ANSI colour codes ESP-IDF's
logger emits arrive as literal escape sequences.

## 11. sdkconfig essentials

```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

The first is not optional. The Type-C port goes straight to the SoC's USB
Serial/JTAG controller; there is no bridge chip. Leave the console on its default
UART0 and your output goes to GPIO16/17, which nothing on this board is connected to
— the monitor sits silent while the firmware runs perfectly.

The second overrides the DevKitC definition's 8 MB. Both the sdkconfig entry and the
`board_upload.*` lines in `platformio.ini` are needed: they feed different tools.

## 12. Flashing, monitoring, recovery

Normal case — **nothing to press**:

```sh
pio run -t upload -t monitor      # or: idf.py -p <port> flash monitor
```

The USB Serial/JTAG controller supports host-controlled reset and download-mode
entry, so esptool puts the board into download mode over the same cable, flashes, and
resets it. The port appears as a CDC-ACM device (`/dev/cu.usbmodem*` on macOS,
`/dev/ttyACM*` on Linux, a COM port on Windows).

Manual download mode, for when firmware has wedged the USB stack or a
`CONFIG_ESP_CONSOLE_*` change has broken enumeration:

1. hold **BOOT** (GPIO9 → GND)
2. tap **RESET**
3. release BOOT

That is Joint Download Boot (GPIO9 = 0, GPIO8 = 1). ROM code enumerates the USB
device with no valid application present, so **the board cannot be bricked by bad
firmware** — the ROM always comes up.

If flashing still fails, `esptool.py -p <port> erase_flash` and start again;
allow 20–60 s for the erase.

Console output goes over the same cable. `monitor_filters = esp32_exception_decoder`
turns a panic backtrace into file:line — keep it on.

## 13. Peripheral cookbook

Working code for each of these is in [recipes.md](recipes.md); this is what to know
before you open it.

**LCD.** `esp_lcd` with `esp_lcd_new_panel_st7789`. Panel config needs
`reset_gpio_num = 21`, `bits_per_pixel = 16`, `data_endian = BIG`,
`rgb_ele_order = RGB`; after init call `invert_color(true)` and
`set_gap(34, 0)`. `on_color_trans_done` + a counting semaphore is how you know the
DMA is finished with your framebuffer.

**Backlight.** LEDC, not GPIO. See §1.2 — this is a hardware-damage rule, not a
style preference. 5 kHz / 10-bit is inaudible and finer than the eye needs, and LEDC
keeps running through Light-sleep so the panel does not flash when you sleep.

**RGB LED.** RMT TX channel + a bytes encoder, 10 MHz resolution (0.1 µs ticks),
WS2812 bit cells 0.3/0.9 µs and 0.9/0.3 µs, **GRB wire order**, `msb_first`. Keep it
dim: it sits under clear acrylic beside the panel and is startlingly bright.

**TF card.** `esp_vfs_fat_sdspi_mount` on the *same* SPI2 bus the LCD uses —
`sdspi_device_config_t.gpio_cs = 4`, `host.slot = SPI2_HOST`. Do **not** call
`spi_bus_initialize()` twice; initialise the bus once (the LCD path already does) and
attach the card as a second device. 1-bit SPI mode only — `SD_D1`/`SD_D2` are NC.
The SPI driver serialises the two devices' transactions, but a 22 ms full-frame flush
and a card write will queue behind each other; do card I/O between frames.

**ADC.** `adc_oneshot` on `ADC_UNIT_1`, channels 0–3 = GPIO0–GPIO3 only.

**Wi-Fi.** Nothing board-specific — onboard antenna, no external-antenna switch to
configure. Budget ~50 KB of RAM and remember the ADC error figures were measured with
the radio off.

## 14. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Monitor is silent, firmware clearly runs | console defaulted to UART0 on GPIO16/17, which is not wired to anything | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` |
| Image is a photographic negative | the IPS panel ships inverted | `esp_lcd_panel_invert_color(panel, true)` |
| Image is shifted 34 px, wraps at the edge | the 172 px glass is centred in 240 columns of controller RAM | `esp_lcd_panel_set_gap(panel, 34, 0)` |
| Colours are wrong in a channel-swapped way | RGB565 byte order — the panel is big-endian | `.data_endian = LCD_RGB_DATA_ENDIAN_BIG` and byte-swap once when packing |
| Panel develops permanent dark blotches | backlight run at 100 % — Waveshare's documented failure mode | LEDC PWM at ≤ 50 %; let the board cool, reflash dimmer |
| Animation never exceeds ~45 fps | 110 KB framebuffer at 40 MHz = 22 ms per flush | redraw dirty rectangles; §9.2 |
| Raising `pclk_hz` above 40 MHz garbles pixels | board wires MOSI/SCLK onto the wrong IO MUX pads, so SPI is GPIO-Matrix routed | stay at 40 MHz; it is not a software fix |
| `spi_bus_initialize()` returns `ESP_ERR_INVALID_STATE` | the LCD already initialised SPI2 | initialise the bus once; attach the card as a second device |
| TF card mounts but the display corrupts | both devices on one bus with unarbitrated CS | let the SPI driver own both devices; do card I/O between frames, not inside one |
| 4-bit SD mode will not initialise | `SD_D1`/`SD_D2` are NC on this board | 1-bit SPI mode only |
| Firmware links but will not boot | image exceeded the real flash / the 1 MB default app partition | `board_upload.flash_size = 4MB` + `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`; add a `partitions.csv` if genuinely over 1 MB |
| RGB LED shows the wrong colour | WS2812 wire order is **GRB**, not RGB | swap the first two bytes |
| RGB LED flickers or shows garbage | RMT resolution or bit timings off | 10 MHz resolution, 3/9 and 9/3 ticks, `msb_first = 1` |
| Board vanished from USB after a flash | firmware reconfigured GPIO12/13 or disabled the USB console | hold BOOT, tap RESET, release — the ROM always enumerates |
| `erase_flash` seems hung | 4 MB chip erase is 20–60 s | wait |
| Deep-sleep current is nowhere near 7 µA | that is the SoC-only figure; LDO quiescent + backlight driver + LED standby add on top | measure the board, do not quote the datasheet |
| ADC readings drift when Wi-Fi is on | datasheet accuracy figures were taken with Wi-Fi disabled | average more samples, or sample with the radio idle |
| Attempted pad-JTAG debugging finds nothing | all four JTAG pads are consumed by the SPI bus and card slot | debug over USB-Serial-JTAG on the Type-C port |

## 15. Sources

- Espressif, *ESP32-C6 Series Datasheet* v1.5 (2026-03-31) —
  <https://www.espressif.com/documentation/esp32-c6_datasheet_en.pdf>
- Waveshare wiki — <https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47>
- Distributor datasheet, SKU WS-28563
- ESP-IDF Programming Guide (esp32c6 target) —
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/>
