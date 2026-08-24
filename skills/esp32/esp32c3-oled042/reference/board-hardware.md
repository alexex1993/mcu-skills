# ESP32-C3 0.42" OLED — hardware and development reference

The board sold as "ESP32-C3 0.42 OLED", "ESP32-C3 OLED development board",
"ABRobot ESP32-C3 0.42 OLED" and (for the near-identical 01Space revision)
"ESP32-C3-0.42LCD". 24.8 × 20.45 mm, two 8-pin 2.54 mm headers 17.78 mm
(700 mil) apart, USB-C on one end.

There is no vendor datasheet for the board. What exists is a schematic in a
third-party GitHub mirror, one Zephyr board port, and a handful of bring-up
write-ups. Everything below is sourced in §15; anything marked **⚠︎ Inference**
is a conclusion drawn across those sources rather than a printed statement.

Part I is the hardware. Part II (§10 onward) is the development guide.

---

## Part I — Hardware

## 1. What is on the board

| | |
|---|---|
| SoC | ESP32-C3FH4 (or FN4 on older stock) — RISC-V RV32IMC, 160 MHz, QFN32, 22 GPIO |
| Flash | **4 MB in-package**, on the SoC's SPI0/1 pins (GPIO12–17), not on the header |
| SRAM | 400 KB, of which 16 KB is cache → ~320 KB linkable |
| Display | 0.42" white OLED, SSD1306-compatible, **72 × 40 visible**, I2C 0x3C |
| LED | one blue LED on **GPIO8**, **LOW = lit** |
| Buttons | **BOO/BOOT** on GPIO9 (pressed = low) · **RST** on CHIP_EN, not readable |
| USB | USB-C straight to the SoC's USB Serial/JTAG on GPIO18/GPIO19. No bridge chip |
| Antenna | on-board ceramic chip antenna, Wi-Fi 802.11b/g/n + BLE 5.0 |
| Power | USB-C 5 V → 1N5819 → ME6211C33 LDO → 3V3. The `5V` header pin is **VUSB**, before the diode |
| Crystal | 40 MHz main crystal. **No 32.768 kHz crystal** — RTC slow clock is the internal RC |

Board draw is around 200 mA at 5 V over USB with Wi-Fi active (buyer report,
§15). That is unremarkable for a C3 — Wi-Fi TX peaks at 335 mA on the
datasheet — but it is more than a 100 mA USB port budget assumes.

### 1.1 Revisions and near-twins

Two boards share this description and are wired the same way for I2C, LED and
button:

- **ABRobot** — plain back, prints "ABrobot" on the OLED at first power-on.
  This is the one most bring-up write-ups describe.
- **01Space ESP32-C3-0.42LCD** — BOOT and RESET buttons on the underside,
  a WS2812B RGB LED, and a Qwiic I2C connector. Documented, with a schematic,
  in `01Space/ESP32-C3-0.42LCD` on GitHub; ported to Zephyr as
  `esp32c3_042_oled`.

**⚠︎ Inference:** the WS2812B and the Qwiic connector exist only on the 01Space
board. If a project needs an RGB LED, confirm which board is in hand before
writing RMT code.

---

## 2. Master pin table

Chip-level facts (IO MUX function, ADC channel, strapping role) come from the
ESP32-C3 datasheet and are in `esp32c3-soc.md`. The "on this board" column is
the part a datasheet cannot tell you.

| GPIO | On this board | Header | ADC | Notes |
|---|---|---|---|---|
| 0 | free | yes | ADC1_CH0 | also XTAL_32K_P; nothing is fitted, so it is a plain GPIO |
| 1 | free | yes | ADC1_CH1 | also XTAL_32K_N, same |
| 2 | free | yes | ADC1_CH2 | **strapping pin** — must be high at reset (see §4) |
| 3 | free | yes | ADC1_CH3 | |
| 4 | free | yes | ADC1_CH4 | JTAG MTMS |
| 5 | **OLED SDA** | yes | ADC2_CH0 | on-board pull-up; ADC2 is unusable anyway |
| 6 | **OLED SCL** | yes | — | on-board pull-up; JTAG MTCK |
| 7 | free | yes | — | JTAG MTDO |
| 8 | **blue LED**, LOW = lit | yes | — | **strapping pin**; pulled high by R1 |
| 9 | **BOOT button**, pressed = low | yes | — | **strapping pin**; external pull-up (R6) |
| 10 | free | yes | — | IO MUX FSPICS0 |
| 11 | — | no | — | VDD_SPI; powers the in-package flash |
| 12–17 | — | no | — | **in-package flash**. Never touch |
| 18 | **USB D−** | no | — | goes to the USB-C connector |
| 19 | **USB D+** | no | — | goes to the USB-C connector |
| 20 | free (UART0 RX) | yes | — | free for application UART; USB console is separate |
| 21 | free (UART0 TX) | yes | — | same |

Header pins additionally carry `5V` (= VUSB), `3V3` and `GND`.

**⚠︎ Inference:** several pinout images circulating for this board are wrong,
including ones that label the OLED bus as GPIO8/GPIO9. GPIO8/GPIO9 are the SoC's
*default* I2C pins in the arduino-esp32 variant header; this board routes the
panel to GPIO5/GPIO6 and puts the LED and the button on 8 and 9. This has been
confirmed three separate ways: the schematic, a user who unglued the panel and
probed the flex, and the fact that the working code uses 5/6.

---

## 3. Reset-time pin states

From the datasheet's Table 2-1, filtered to the pins that come out on this
board. Relevant when a pin drives something that must not twitch at boot.

| Pin | At reset | After reset |
|---|---|---|
| GPIO2, GPIO3 | input enabled | input enabled |
| GPIO4 (MTMS), GPIO5 (MTDI) | input enabled | input enabled |
| GPIO6 (MTCK) | input enabled + weak pull-up¹ | — |
| GPIO7 (MTDO) | input enabled | — |
| GPIO8 | input enabled | input enabled |
| GPIO9 | input enabled + weak pull-up | input enabled + weak pull-up |
| GPIO10 | input enabled | — |
| GPIO18, GPIO19 | USB pull-up active | — |
| GPIO20 (U0RXD) | input enabled + weak pull-up | — |
| GPIO21 (U0TXD) | output enabled + weak pull-up | — |

¹ depends on `EFUSE_DIS_PAD_JTAG`.

Power-up glitches (datasheet Table 2-2): MTCK/GPIO6, MTDO/GPIO7, GPIO10 and
U0RXD/GPIO20 emit a ~5 ns low glitch; **GPIO18 emits a 50 µs high glitch**.
Only the GPIO18 one is long enough to matter, and GPIO18 is USB D− here.

---

## 4. Strapping-pin analysis

Three strapping pins (datasheet §3), and this board uses two of them for
something else.

| Pin | Strapping role | On this board |
|---|---|---|
| GPIO2 | must be high at reset; "recommended to pull up due to glitches" | free header pin — **your problem** |
| GPIO8 | with GPIO9, selects boot mode; also gates ROM UART printing | blue LED, pulled high by R1 |
| GPIO9 | with GPIO8, selects boot mode | BOOT button, pulled high by R6 |

Boot mode table (datasheet Table 3-3):

| Mode | GPIO2 | GPIO8 | GPIO9 |
|---|---|---|---|
| SPI boot (normal) | 1 | any | 1 |
| Joint download boot | 1 | 1 | 0 |

So: pressing BOOT pulls GPIO9 low, and with GPIO8 held high by R1 the board
enters download mode. That is exactly the intended behaviour, and it is also
why **GPIO8 must not be pulled low by anything external at reset** — the LED
resistor is not strong enough to do that, but a load you add might be.

The one to actually watch is **GPIO2**. It is a free header pin, and if your
circuit holds it low at reset the chip does not boot. Nothing on the board
guards it.

### 4.1 The eFuses not to burn

`EFUSE_UART_PRINT_CONTROL` makes ROM message printing depend on GPIO8's level
at reset. GPIO8 here is the LED. Burning that eFuse ties the boot log to
whatever the LED circuit happens to be doing, permanently — eFuse bits are
one-time programmable. Same reasoning for `EFUSE_DIS_USB_SERIAL_JTAG`, which
would remove the only console and the only flashing route this board has.

---

## 5. What is left free

Nine header GPIOs: **0, 1, 2, 3, 4, 7, 10, 20, 21**.

- **ADC:** ADC1 channels 0–4 are GPIO0–GPIO4, factory-calibrated, 12-bit,
  ~100 kSPS. **ADC2 is GPIO5 only** — the OLED SDA line — and ADC2 is
  non-functional on some C3 revisions per the SoC errata. Treat this board as
  having five ADC inputs, all on ADC1.
- **JTAG pads:** GPIO4–GPIO7 carry MTMS/MTDI/MTCK/MTDO. GPIO5 and GPIO6 are the
  OLED bus, so pad-JTAG is not available — but the USB Serial/JTAG controller on
  the USB-C port provides JTAG anyway, which is the better route.
- **Deep-sleep wake:** GPIO0–GPIO5 are the RTC-capable pins on the C3. Of those,
  GPIO0–GPIO4 are free here.
- **UART:** UART0's IO MUX pins (GPIO20/21) are free for an application serial
  port, because the console lives on USB Serial/JTAG instead. UART1 has no
  fixed pins and can go anywhere via the GPIO Matrix.

---

## 6. Peripheral availability

| Peripheral | Available | Notes |
|---|---|---|
| I2C | **one bus, already in use** | The C3 has exactly one I2C controller and it is on the panel. Extra devices share GPIO5/GPIO6 at a different address — confirmed working by two independent reports (§15) |
| SPI2 (FSPI) | yes, via GPIO Matrix | IO MUX fast pins are GPIO2/4/5/6/7/10; GPIO5 and GPIO6 are taken, so at least two signals route through the matrix |
| UART0 | yes | GPIO20/21, free |
| UART1 | yes | any free pins |
| ADC1 | yes, 5 channels | GPIO0–GPIO4 |
| ADC2 | effectively no | GPIO5 is the OLED bus; also errata-limited |
| LEDC (PWM) | yes, 6 channels | any GPIO |
| RMT | yes, 2 TX + 2 RX | any GPIO — this is how you would drive an external WS2812 |
| I2S, TWAI | yes | any GPIO via the matrix |
| USB Serial/JTAG | yes | GPIO18/19, fixed, non-negotiable |
| Wi-Fi / BLE | yes | ceramic chip antenna, nothing to configure |

---

## 7. Power

```
USB-C VBUS ──┬── "5V" header pin        (VUSB, ~5.1 V)
             │
          1N5819
             │
          VCC5V ── ME6211C33 LDO ── 3V3 ──┬── ESP32-C3FH4
                                          ├── OLED module
                                          └── "3V3" header pin
```

Two consequences worth knowing:

1. **The `5V` header pin is on the USB side of the diode, not the regulator
   side.** Measured: 5.1 V at USB and at the header, ~4.8 V after the diode.
   The diode protects the *regulator input* from an external supply, not the
   USB port from back-feed through the header. Feeding 5 V into that pin while
   USB is connected puts the two supplies in parallel.
2. Vendor specs claim 3.3–6 V into the `5V` pin. The ME6211C33 tolerates it;
   the 1N5819 drop means the LDO sees roughly 0.3 V less. **Untested by the
   author of this skill.**

---

## 8. Memory and flash layout

| | |
|---|---|
| ROM | 384 KB |
| SRAM | 400 KB total, 16 KB configured as cache → ~320 KB the linker will hand out |
| RTC FAST | 8 KB, survives deep sleep |
| In-package flash | 4 MB, XMC part, on SPI0/1 |
| Cache | 16 KB, 8-way, **read-only**, 32-byte blocks |

With `CONFIG_PARTITION_TABLE_SINGLE_APP` (the default) the app partition is
1 MB at 0x10000. The template's full variant links at 163,356 B — 15.6 % of it.

**The flash size trap:** ESP-IDF defaults `CONFIG_ESPTOOLPY_FLASHSIZE` to 2 MB
regardless of what the PlatformIO board definition claims. Verified: build the
template without `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` in `sdkconfig.defaults` and
the generated `sdkconfig` says `CONFIG_ESPTOOLPY_FLASHSIZE="2MB"`. Nothing
fails at 163 KB, so this stays invisible until you add OTA or a filesystem and
run out of room that physically exists.

---

## 9. The display

The single most expensive thing to get wrong on this board.

### 9.1 It is a 72 × 40 window inside a 128 × 64 controller

The SSD1306 has 128 columns × 64 rows of GDDRAM. This panel bonds **72 columns
× 40 rows** of it to glass, and the window does **not** start at (0, 0).

The vendor's own sales copy says, verbatim: *"This screen is different from
other 0.42-inch screens. The starting point of the screen is 12864 (13, 14).
Please pay attention before buying, you must not directly replace other
0.42-inch screens."* That garbled sentence is a warning about exactly this.

Two ways to drive it, and they are not interchangeable:

**A. Reprogram the controller for a 40-row panel** — what the template does.

```
0xA8, 0x27          multiplex ratio = 40 rows (register takes rows-1)
0xD3, 0x00          display offset 0
0x40                start line 0
column offset 28    written into the page-address command, per page
```

Only a **column** offset is needed: once the multiplex ratio is 40, row 0 of
the framebuffer is row 0 of the glass. Verified on hardware.

**B. Leave it as 128 × 64 and draw into the middle** — what U8g2's
`U8G2_SSD1306_128X64_NONAME_*` constructors need.

Here you need both offsets, and reports disagree on the numbers because they
depend on which init the library ran:

| Source | xOffset | yOffset |
|---|---|---|
| Original review-comment code | 30 | 12 |
| MicroPython `ssd1306` module | 28 | 24 |
| Later blog commenters | 28 | 24 |

The template's approach sidesteps the disagreement entirely. If you are using
U8g2, prefer `U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5)`
— multiple users report it needs **no offset at all**, because that constructor
already carries the 40-row init and the offset. Note the argument order:
**clock first, then data**, i.e. 6 then 5.

### 9.2 Snow on the panel

Two distinct causes, and they look the same:

- **Wrong multiplex ratio.** Initialised as 128 × 64, the controller scans 64
  rows onto 40 rows of glass. The picture is unstable noise.
- **Un-wiped GDDRAM.** After power-on the controller's RAM holds random bits.
  You only write 72 of 128 columns, so the margin keeps its garbage and it
  shows at the panel edge. Wipe all eight pages × 128 columns once at init.

The template does both, and additionally keeps the display **off (0xAE) until
the first blank frame has been pushed** — otherwise the garbage is visible for
the fraction of a second between "display on" and "first flush".

### 9.3 Frame cost

360 bytes of framebuffer (72 × 5 pages). At 400 kHz, each page is 73 bytes plus
addressing ≈ 1.6 ms on the wire, so a full flush is **roughly 8 ms**, or a
ceiling near 120 fps. The template's 35 ms frame delay is a marquee speed
choice, not a hardware limit.

This is a small enough transfer that partial updates are not worth the
complexity — flush the whole thing.

---

## Part II — Development guide

## 10. Toolchain

Verified combination:

| | |
|---|---|
| PlatformIO Core | 6.x with `platform-espressif32` **7.0.1** |
| Framework | ESP-IDF **6.0.1** (`framework-espidf` 4.60001.0) |
| Toolchain | `toolchain-riscv32-esp` 15.2.0 |
| esptool | 4.11.0 |
| `board =` | `esp32-c3-devkitm-1` |

There is no PlatformIO board definition for this board. `esp32-c3-devkitm-1` is
the generic C3 one and is right on everything that matters: `mcu = esp32c3`,
4 MB flash, 160 MHz, `maximum_ram_size = 327680`. `dfrobot_beetle_esp32c3` also
works and is what the original bring-up used — under `framework = espidf` the
two are equivalent, since the Beetle definition's extra flags are Arduino-only.

Arduino-framework users: "ESP32C3 Dev Module" with **USB CDC On Boot =
Enabled**. Note that `LED_BUILTIN` is defined by that variant but does **not**
point at GPIO8.

---

## 11. sdkconfig essentials

```ini
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y   # or the monitor is silent
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y       # or IDF lays out for 2 MB
```

Both are verified: removing either from `sdkconfig.defaults` and rebuilding
produces `CONFIG_ESP_CONSOLE_UART_DEFAULT=y` and
`CONFIG_ESPTOOLPY_FLASHSIZE="2MB"` respectively.

Defaults that are already right and need no help: `CONFIG_XTAL_FREQ=40`,
`CONFIG_ESP32C3_DEFAULT_CPU_FREQ_MHZ=160`, `CONFIG_ESPTOOLPY_FLASHMODE="dio"`
(set from `board_build.flash_mode` in `platformio.ini`),
`CONFIG_RTC_CLK_SRC_INT_RC=y` (correct — there is no 32 kHz crystal).

---

## 12. Flashing, monitoring, recovery

Normal case, nothing to press:

```sh
pio run -t upload -t monitor
```

The USB Serial/JTAG controller supports host-driven reset and download-mode
entry, so esptool drives the whole cycle over the USB-C cable. The board
enumerates as CDC-ACM: `/dev/cu.usbmodem*` on macOS, `/dev/ttyACM*` on Linux
(`pyserial` describes it as "USB JTAG/serial debug unit"), a COM port on
Windows.

**Manual download mode** — hold **BOOT**, tap **RST**, release **BOOT**. Needed
when:

- it is the first flash on a factory board (several bring-up reports say the
  first upload needs the manual dance, and subsequent ones do not);
- firmware has wedged USB — a tight loop with interrupts off, or a
  console-config change that broke enumeration.

Because ROM code enumerates with no valid application present, **bad firmware
cannot brick this board**. Last resort:

```sh
esptool.py -p <port> erase_flash
```

A 4 MB erase takes ~7–20 s (datasheet Table 5-10 for a 32 Mb part). It is
probably not hung.

**Serial ports, Arduino-side.** With USB CDC on boot enabled, `Serial` maps to
`HWCDCSerial` over USB. `Serial0` is hardware UART0 on GPIO20/21 and prints to
the header pins, not the USB port. **Instantiating `Serial1` makes the USB
serial port disappear from the host entirely** — the arduino-esp32 core places
`Serial1` on GPIO18/19, which are the USB D−/D+ lines. This is a core bug for
the C3, not a board problem, and it is a spectacular way to lose an afternoon.

**JTAG.** The same USB-C port carries JTAG via the built-in controller.
PlatformIO's pinned `tool-openocd-esp32` 2.1100.20220706 does ship
`board/esp32c3-builtin.cfg`, so unlike the ESP32-C6 case `pio debug` has a
target config available here. Untested by the author of this skill.

---

## 13. Peripheral cookbook

Copy-paste code is in `recipes.md`. Summary of what needs care:

| Task | The thing to know |
|---|---|
| OLED | §9. Multiplex 40, column offset 28, wipe GDDRAM, display on last |
| Extra I2C device | Same bus, same pins, different address. Do **not** call `i2c_new_master_bus()` twice — add a device to the existing bus |
| Blue LED | `gpio_set_level(8, 0)` lights it |
| BOOT as a button | Input, external pull-up already fitted, pressed = 0. Debounce it |
| ADC | ADC1 only, GPIO0–GPIO4, `adc_oneshot` + `adc_cali` curve fitting |
| External WS2812 | RMT on any free GPIO. GRB byte order |
| Application UART | UART0 on GPIO20/21, or UART1 anywhere. The console is unaffected |
| Deep sleep | 5 µA for the SoC; the LDO's quiescent current and the OLED module's standby draw are on top. Do not quote the datasheet number for the board |

---

## 14. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Monitor silent, firmware clearly running | Console defaulted to UART0 on GPIO20/21, wired to nothing | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` |
| Panel shows unstable noise ("snow") | Initialised as 128 × 64; controller scans 64 rows onto 40 | Multiplex ratio 0xA8, 0x27 |
| Image is shifted, wraps at the edge | Column offset 28 not applied | Add it to the page-address command, per page |
| Junk pixels along one edge only | Un-wiped GDDRAM outside the 72-column window | Wipe all 8 pages × 128 columns at init |
| Flash of garbage at every boot | Display switched on before the first frame | Send 0xAF after the first `oled_flush()` |
| `i2c` timeout, nothing at 0x3C | Bus on GPIO8/GPIO9 (the arduino-esp32 default) instead of GPIO5/GPIO6 | GPIO5 = SDA, GPIO6 = SCL |
| U8g2 draws off-screen | 128 × 64 constructor without offsets | Use the `72X40_ER` constructor, arg order `(rot, reset, clock=6, data=5)` |
| USB serial port vanishes from the host after a code change | `Serial1` instantiated — the core puts it on GPIO18/19, the USB pins | Do not use `Serial1` on this chip |
| LED does nothing / is on when it should be off | Polarity assumed active-high | LOW = lit |
| Board does not boot with your circuit attached | GPIO2 held low at reset — it is a strapping pin | Leave GPIO2 high at reset, or move the signal |
| Partition table smaller than expected, OTA will not fit | IDF defaulted to 2 MB | `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` |
| ADC on GPIO5 reads nonsense | That is ADC2, and it is the OLED SDA line | Use ADC1 on GPIO0–GPIO4 |
| `LED_BUILTIN` blinks nothing | The Arduino variant defines it, but not as GPIO8 | Use `8` |

---

## 15. Sources

- **ESP32-C3 Series Datasheet v2.4**, Espressif — pin tables, strapping,
  boot modes, memory, electrical. Digested in `esp32c3-soc.md`.
- **ESP32-C3 Technical Reference Manual**, Espressif — peripheral registers.
- **Kevin (emalliab), "ESP32-C3 0.42 OLED"**, Feb 2025 — the fullest public
  bring-up: GPIO sweep, the schematic hunt, the serial-port analysis, the
  U8g2 offsets, the power-tree measurement. Source for the schematic-derived
  claims here (R1/R6/R8, the 1N5819, the ME6211C33) and for the `Serial1`
  finding.
- **`zhuhai-esp/ESP32-C3-ABrobot-OLED`** on GitHub — the schematic itself.
- **`01Space/ESP32-C3-0.42LCD`** on GitHub — the sibling board's schematic.
- **Zephyr board port `esp32c3_042_oled`** — independent confirmation of
  SDA = GPIO5, SCL = GPIO6, and of "no native USB, on-chip USB-serial instead".
- **Michiel van der Wulp, "ESP32-C3 SuperMini with 0.42 inch OLED display"** —
  Tasmota bring-up, the I2C scan showing exactly one device at 0x3C, the
  U8g2 constructor, the 200 mA figure, the `esptool` transcript identifying
  the chip as ESP32-C3 (QFN32) rev v0.4 with 4 MB embedded XMC flash.
- **The original bring-up firmware for this skill** — the ESP-IDF 6.0.1
  project the template was extracted from, run on a real board.
