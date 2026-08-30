# NodeMCU 30-pin ESP8266 — board reference

Everything board-level: what the NodeMCU PCB adds to a bare ESP-12E/F module, and how to
build for it. Silicon-level detail (GPIO count, memory map, peripheral tables, sleep
currents) is in `esp8266-soc.md`; copy-paste code is in `recipes.md`.

Sources, in order of authority:

| Document | What it settles |
|---|---|
| NodeMCU DEVKIT V1.0 Schematic (NodeMCU Team, 2015-01-25) | every net on the board — headers, straps, LEDs, ADC divider, power, auto-reset |
| NodeMCU DEVKIT Instruction V1.0 (2015-01-28) | the silkscreen pin names, the boot-level requirements, the 5 V limit, the recovery procedure |
| ESP8266EX Datasheet v7.1 (Espressif) | pin functions, electrical limits, peripheral pin tables, power modes |
| ESP-12E / ESP-12F Datasheet (Ai-Thinker) | module pinout, 4 MB flash, TX power and current, boot-mode table |
| verified on hardware, this session | marked **[hw]** below |

---

## 1. Header pin map

30 pins, two 15-pin rows, numbered from the antenna end. Every row below is taken from
schematic sheet 5 (`J1` = the A0 side, `J2` = the D0 side).

### J2 — the usable side

| # | Silk | GPIO | Arduino | Notes |
|---|---|---|---|---|
| 1 | D0 | GPIO16 | `D0`, `LED_BUILTIN_AUX` | RTC domain. **No interrupt, no PWM, no I2C, no OneWire.** Pull-down only (`INPUT_PULLDOWN_16`). Drives the blue PCB LED, active LOW. The deep-sleep wake pin — but see §8 |
| 2 | D1 | GPIO5 | `D1` | Free. Default `SCL` |
| 3 | D2 | GPIO4 | `D2` | Free. Default `SDA` |
| 4 | D3 | GPIO0 | `D3` | 12 kΩ pull-up + FLASH button. **Low at reset ⇒ download mode** |
| 5 | D4 | GPIO2 | `D4`, `LED_BUILTIN` | 12 kΩ pull-up. Module LED, active LOW. Also UART1 TX. **Must be high at reset** |
| 6 | 3V3 | — | — | 3.3 V output from the LDO |
| 7 | GND | — | — | |
| 8 | D5 | GPIO14 | `D5` | Free. HSPI SCLK |
| 9 | D6 | GPIO12 | `D6` | Free. HSPI MISO |
| 10 | D7 | GPIO13 | `D7` | Free. HSPI MOSI |
| 11 | D8 | GPIO15 | `D8` | **12 kΩ pull-DOWN. Must be low at reset** or the chip does not boot at all. HSPI CS |
| 12 | RX | GPIO3 | `D9`, `RX` | UART0 RX, wired to the bridge through a 470 Ω series resistor (R6) |
| 13 | TX | GPIO1 | `D10`, `TX` | UART0 TX. Emits the ROM boot log at 74880 baud |
| 14 | GND | — | — | |
| 15 | 3V3 | — | — | |

### J1 — the side that is mostly not usable

| # | Silk | GPIO | Notes |
|---|---|---|---|
| 1 | A0 | — | ADC input **through a 220 kΩ / 100 kΩ divider** — see §5 |
| 2 | RSV | — | Reserved on V1.0/Amica. **On LoLin V3 this pad is `VU`** (USB 5 V, before the LDO) |
| 3 | RSV | — | Reserved on V1.0/Amica. **On LoLin V3 this pad is `GND`** |
| 4 | SD3 | GPIO10 | SPI flash `/WP`. Usable **only** on a DOUT-mode module, and not on this board's stock DIO configuration |
| 5 | SD2 | GPIO9 | SPI flash `/HOLD`. Same caveat |
| 6 | SD1 | GPIO8 | **SPI flash MOSI — never touch** |
| 7 | CMD | GPIO11 | **SPI flash CS — never touch** |
| 8 | SD0 | GPIO7 | **SPI flash MISO — never touch** |
| 9 | CLK | GPIO6 | **SPI flash CLK — never touch** |
| 10 | GND | — | |
| 11 | 3V3 | — | |
| 12 | EN | — | CHIP_EN, 12 kΩ pull-up (R3). Pull low to power the SoC down (~0.5 µA) |
| 13 | RST | — | nRST, 12 kΩ pull-up (R4) + 100 nF (C1) + the RST button |
| 14 | GND | — | |
| 15 | VIN | — | 5 V rail, shared with the USB input through a Schottky. See §6 |

Driving any of GPIO6–GPIO11 while code is executing from flash crashes or corrupts the
running sketch, because the CPU fetches instructions over that bus. There is no
configuration that makes them safe. Nine usable GPIOs is the real budget: GPIO0, 2, 4, 5,
12, 13, 14, 15, 16.

---

## 2. Strapping and boot

The module samples three pins on the rising edge of reset. The board fits resistors to
put them in the right state (schematic sheet 2), all 12 kΩ:

| Pin | Resistor | Required at reset | Board provides |
|---|---|---|---|
| GPIO0 | R1, to 3V3 | **HIGH** for normal boot, LOW for UART download | pull-up |
| GPIO2 | R2, to 3V3 | **HIGH** always | pull-up |
| GPIO15 | R5, to GND | **LOW** always | pull-down |
| CHIP_EN | R3, to 3V3 | HIGH | pull-up |
| nRST | R4, to 3V3 | HIGH | pull-up + 100 nF |

**[hw]** On a booting board these read back `GPIO0 = 1, GPIO2 = 1, GPIO15 = 0`, which is
what the `--full` template prints.

Boot mode, from the ESP-12F datasheet Table 2.3:

| Mode | EN | RST | GPIO15 | GPIO0 | GPIO2 |
|---|---|---|---|---|---|
| Run from flash | H | H | L | **H** | H |
| UART download | H | H | L | **L** | H |

The ROM prints `boot mode:(M,N)` where `M` encodes those straps. `boot mode:(3,x)` is a
normal flash boot; `boot mode:(1,x)` means GPIO0 was low — the chip is sitting in the
download stub and your sketch never started.

**The failure this causes:** a 12 kΩ pull-down is weak. A 10 kΩ pull-up on a breakout
board, an LED to 3V3, or an SPI slave that idles CS high on GPIO15 all beat it, and the
chip then produces *no output at any baud rate* — it never leaves reset. This looks
exactly like a dead board. Unplug D3, D4 and D8 before concluding anything else.

---

## 3. The two LEDs

| LED | Pin | Where | Polarity | Present on |
|---|---|---|---|---|
| `LED_BUILTIN` | GPIO2 | on the ESP-12E/F module | **active LOW** | every ESP-12E/F |
| `LED_BUILTIN_AUX` | GPIO16 | on the NodeMCU PCB (LED1, anode to 3V3 via 470 Ω R9) | **active LOW** | NodeMCU boards only |

`digitalWrite(LED_BUILTIN, LOW)` lights it. Half the ESP8266 blink examples online are
written for an active-high board and appear to work inverted.

The schematic annotates LED1 "This LED SHOULD BE BLUE or WHITE to make sure enough
voltage drop" — with a 470 Ω resistor from 3V3, a red LED would keep GPIO16 from reading
a clean low.

---

## 4. USB bridge and the auto-program circuit

- V1.0 / Amica: **CP2102** (Silicon Labs). LoLin V3: **CH340G** (WCH, VID 0x1A86 /
  PID 0x7523 **[hw]**).
- UART0 only. The ESP8266 has **no USB peripheral at all** — there is no USB-CDC, no
  USB-JTAG, and no way to talk to the chip other than through this bridge.
- R6, 470 Ω in series on the bridge's TX → GPIO3 line, so an external driver on RX does
  not fight it.

Two S8050 NPN transistors (VT1, VT2) turn DTR and RTS into RST and GPIO0. Truth table
from schematic sheet 3:

| DTR | RTS | RST | GPIO0 |
|---|---|---|---|
| 1 | 1 | 1 | 1 |
| 0 | 0 | 1 | 1 |
| 1 | 0 | **0** | 1 |
| 0 | 1 | 1 | **0** |

The circuit deliberately cannot assert both at once — that is why it needs both lines and
why esptool toggles them in sequence. Consequences:

- Any terminal that asserts DTR or RTS when it opens the port **resets the board**, and
  you lose the first lines of output. `monitor_dtr = 0` / `monitor_rts = 0` in
  `platformio.ini` attaches without resetting.
- A terminal that asserts only RTS drops the board into download mode, where it looks
  hung.
- With the board powered from a computer and the port not open, plugging any other USB
  device into that host can reset it (NodeMCU instruction sheet, note 3).

---

## 5. The ADC and its divider

Schematic sheet 7: `A0` (header) —[R13 220 kΩ 1 %]— `TOUT` (module pin 2) —[R14 100 kΩ
1 %]— GND.

| | |
|---|---|
| Header `A0` range | 0 – **3.2 V** (= 1.0 V × 320/100) |
| Module `TOUT` pad range | 0 – **1.0 V**, damaged above ~1.1 V |
| Resolution | 10 bit, `analogRead()` returns 0–1023 |
| Source impedance seen | ≈ 320 kΩ — a high-impedance sensor reads low |
| Channels | **one**. There is no second analog input |

**[hw]** A floating A0 on this board reads ~10–11 counts, not 0.

`ADC_MODE(ADC_VCC)` at file scope repurposes the ADC to measure the 3V3 supply
(`ESP.getVcc()`). It is exclusive: after that `analogRead(A0)` returns nothing meaningful
and the header pin must be left open. The ESP8266EX datasheet §4.9 spells out that the
two modes cannot coexist.

Wi-Fi does **not** block the ADC on this chip — that is an ESP32 problem, not an ESP8266
one. But `analogRead()` briefly disables interrupts, so sampling in a tight loop degrades
Wi-Fi throughput.

---

## 6. Power tree

Schematic sheet 4:

```
USB 5 V ──[1N5819 Schottky D1]──┬── VDD5V ── header VIN
                                └── NCP1117ST33 ── VDD3V3 ── module + header 3V3
```

| | |
|---|---|
| Regulator | NCP1117ST33T3G, SOT-223, fixed 3.3 V |
| Rated | 800 mA working, 1.0 A limit, 1.2 V dropout @ 800 mA |
| Bulk | 100 µF tantalum (C2) at the module, 10 µF at the regulator |
| VIN | on the 5 V rail, **behind** the Schottky from USB |
| 3V3 | an **output**. Do not drive it |
| GPIO | 3.3 V, **not 5 V tolerant**, 12 mA max per pin (ESP8266EX Table 5-1) |

**The VIN contradiction.** The schematic's power sheet says "Max Supply Voltage: 20 V"
(the NCP1117's own rating). The NodeMCU instruction sheet, note 8, says never exceed 5 V.
The instruction sheet is the one to follow: this is a linear regulator in a SOT-223, and
at 12 V in / 3.3 V out / 200 mA it dissipates 1.74 W with no heatsink. It thermally
shuts down and restarts, which presents as random reboots that correlate with Wi-Fi load
rather than with anything in the code.

**Current.** The ESP-12F datasheet gives ~71 mA average while transmitting continuously
and a **500 mA peak**. The regulator handles it; a laptop USB-2 port, a thin cable or a
cheap hub often does not, and the signature is a reset on the first packet with
`ESP.getResetReason()` reporting a brownout or an external reset.

---

## 7. What the module is

| | ESP-12E | ESP-12F |
|---|---|---|
| Vendor | Ai-Thinker | Ai-Thinker |
| Size | 24 × 16 × 3 mm | 24 × 16 × 3 mm |
| Pins | 22 (SMD) | 22 (SMD) |
| Flash | 4 MB | 4 MB (32 Mbit) |
| Antenna | PCB trace, 3 dBi | PCB trace, improved layout |
| Difference | — | better RF layout and range; drop-in identical otherwise |

Either module makes a 30-pin NodeMCU; nothing in this skill changes between them.

**[hw]** This board: flash id `0x00164068`, 4,194,304 B real, DIO mode @ 40 MHz, chip id
`0x7107F0`, 26 MHz crystal.

---

## 8. Deep sleep and the GPIO16 ↔ RST link

`ESP.deepSleep(us)` powers everything down except the RTC. The RTC's only way to restart
the chip is to drive **GPIO16 (XPD_DCDC) low**, which must physically reach nRST.

The NodeMCU board provides a footprint for that link and **leaves it empty**: R10 on the
schematic, value `0(NC)`, annotated "Use this resistor only in sleep mode". The
instruction sheet (note 6) calls the same footprint the R3 position and adds: after
fitting it, GPIO16 must not be used for anything else.

**[hw]** Verified absent on the board tested here: the probe drove GPIO16 low for 5 ms and
the chip did not reset. `template/src/main.cpp` runs that test on every boot and prints
the verdict — the test uses RTC user memory, which survives a reset but not a power cycle,
so it can tell "GPIO16 low rebooted me" from "GPIO16 low did nothing".

Consequences:

- On a stock board, `ESP.deepSleep()` sleeps forever. It looks like a hang or a brick, and
  only RST or a power cycle recovers it.
- With the link fitted, **every upload fails** unless you lift it — GPIO16 idles low
  during parts of boot and holds the chip in reset. Use a jumper wire, not solder, if you
  are still developing.
- `ESP.deepSleepMax()` is ~3.2 h **[hw]** — not unlimited. Longer intervals need a chain
  of sleeps with a counter in RTC memory (512 B of user RTC RAM survives deep sleep).
- Deep sleep on this *board* is milliamps, not the datasheet's 20 µA: the LDO's quiescent
  draw, the bridge chip and the power LED are all still on. Never quote 20 µA for a devkit.

---

## 9. Toolchain and flash layouts

```ini
[env:nodemcuv2]
platform  = espressif8266        ; 4.2.1 verified
board     = nodemcuv2            ; ESP-12E, variant "nodemcu", 4 MB
framework = arduino              ; core 3.1.2 verified, xtensa-gcc 10.3.0
```

`board = nodemcuv2` sets `-DARDUINO_ESP8266_NODEMCU_ESP12E`, the `nodemcu` pin variant
(which is what defines `D0`–`D10`, `LED_BUILTIN = 2`, `LED_BUILTIN_AUX = 16`,
`SDA = 4`, `SCL = 5`), DIO flash mode at 40 MHz, and `eagle.flash.4m1m.ld`.

| `board_build.ldscript` | Sketch space | Filesystem | OTA possible up to |
|---|---|---|---|
| `eagle.flash.4m.ld` | 1,044,464 B | none | ~1 MB |
| **`eagle.flash.4m1m.ld`** (default) | **1,044,464 B** | 1,024,000 B **[hw]** | ~1 MB |
| `eagle.flash.4m2m.ld` | 1,044,464 B | 2 MB | ~1 MB |
| `eagle.flash.4m3m.ld` | 1,044,464 B | 3 MB | ~1 MB |

Sketch space does not change, because it is bounded by the flash cache mapping window,
not by the part. Choosing a layout is choosing a filesystem size, nothing else.

Other settings worth knowing:

| Setting | Effect |
|---|---|
| `board_build.filesystem = littlefs` | required — SPIFFS is deprecated in core 3.x |
| `board_build.f_cpu = 160000000L` | 160 MHz; roughly doubles compute, adds ~15 mA |
| `board_build.f_flash = 80000000L` | 80 MHz flash; faster XIP, and not all modules are stable at it |
| `board_build.flash_mode = dout` | frees GPIO9/GPIO10 as real GPIOs, at some XIP throughput |
| `monitor_filters = esp8266_exception_decoder` | turns a crash dump into symbolised source lines. Always set it |
| `build_flags = -D DEBUG_ESP_PORT=Serial -D DEBUG_ESP_WIFI` | SDK-level Wi-Fi logging |

**Sizes [hw]**, platform 4.2.1 / core 3.1.2, zero warnings:

| | Flash | RAM (static) | Free heap at runtime |
|---|---|---|---|
| `template --minimal` | 267,483 B | 28,292 B | 52,168 B |
| `template --full` | 304,199 B | 29,792 B | 50,408 B |

The 267 KB floor is the non-OS Wi-Fi SDK, which the core links into every sketch. It
cannot be stripped.

---

## 10. Flashing, monitoring and recovery

```sh
pio run -t upload -t monitor            # normal, no buttons
pio device monitor -b 74880             # the ROM boot log
pio run -t uploadfs                     # write the LittleFS image from data/
esptool.py --chip esp8266 -p <port> erase_flash
```

**Baud rates.** 460800 uploads reliably **[hw]**; 921600 is flaky on CH340 clones. The
sketch console is whatever `Serial.begin()` says (115200 by convention). The ROM
bootloader is fixed at **74880** — 115200 × 26/40, because the crystal is 26 MHz and the
ROM assumes 40.

**What the ROM prints [hw]:**

```
 ets Jan  8 2013,rst cause:2, boot mode:(3,6)
load 0x4010f000, len 3424, room 16
tail 0
chksum 0x2e
...
csum 0x2b
v00045240
~ld
rf cal sector: 1020
```

`rst cause:` — `1` power-on, `2` external reset (the RST pin or the auto-reset circuit),
`3` software reset, `4` **hardware watchdog**, `5` deep-sleep wake, `6` external system
reset. `boot mode:(3,x)` = flash boot, `(1,x)` = UART download.

After the ROM's lines the SDK switches UART0 to your `Serial.begin()` rate, so a 74880
monitor shows the banner then garbage, and a 115200 monitor shows garbage then your
output. Both are correct behaviour.

**When upload fails** (`Failed to connect to ESP8266`, `Wrong boot mode detected`):

1. hold **FLASH**, tap **RST**, release FLASH, upload;
2. if that fails, hold **FLASH** while plugging the USB cable in (instruction sheet,
   note 5 — the documented recovery after an interrupted flash);
3. lower `upload_speed` to 115200;
4. check nothing is wired to D3/D4/D8 or to RX/TX;
5. `esptool.py erase_flash`, then upload the `--minimal` variant.

**You cannot brick this board with bad firmware.** The bootloader is in mask ROM.
`erase_flash` also clears the RF calibration sector; the SDK rewrites it on the next boot.

**Debugging.** No JTAG, no debug header, no SWD. The options are `Serial`, the exception
decoder, and `GDBStub` (bundled with the core: `#include <GDBStub.h>` + `gdbstub_init()`,
which takes over UART0 and gives source-level break/step at 115200).

---

## 11. Peripheral cookbook

Full code in `recipes.md`; this is what to reach for and what it costs.

| Job | Use | Cost / caveat |
|---|---|---|
| Digital IO | `pinMode`/`digitalWrite` | 12 mA per pin. GPIO16 is read/write only |
| Interrupt | `attachInterrupt(digitalPinToInterrupt(pin), …)` + `IRAM_ATTR` handler | works on every GPIO **except 16**. Handler must be `IRAM_ATTR` or the chip crashes when it fires during a flash read |
| PWM | `analogWrite` | **software**, timer-driven. Range defaults to **255**; `analogWriteFreq` clamps 100 Hz–60 kHz. Jitters under Wi-Fi |
| Analog in | `analogRead(A0)` | one channel, 10-bit, 0–3.2 V at the header |
| I2C | `Wire.begin(sda, scl)` | **software**, blocking. ~400 kHz ceiling. Any two safe pins |
| SPI | `SPI.begin()` → HSPI on D5/D6/D7, CS on D8 | hardware, up to 80 MHz. CS on D8 is a strapping pin |
| UART | `Serial` (UART0), `Serial1` (TX only, GPIO2) | `Serial.swap()` moves UART0 to GPIO15/GPIO13 |
| Filesystem | `LittleFS` | 1 MB by default. `pio run -t uploadfs` writes `data/` |
| Wi-Fi | `ESP8266WiFi` | costs ~1–2 KB heap idle, far more with TLS |
| OTA | `ArduinoOTA` or `ESP8266httpUpdate` | new image must fit beside the running one |
| Timers | `Ticker` | callbacks run in interrupt context — no `Serial`, no `delay`, no heap |
| WS2812 | I2S (`NeoPixelBus` `Esp8266x1I2sDma`) | the only glitch-free route; occupies GPIO3 (RX) |
| Deep sleep | `ESP.deepSleep` | **needs the GPIO16↔RST link, §8** |

---

## 12. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| Nothing at all: no serial at 74880 or 115200, board seems dead | GPIO15 pulled high at reset by something you wired | Disconnect everything from D8. The 12 kΩ pull-down loses to any 10 kΩ pull-up |
| Garbage before the first line at 115200 | The ROM boot log at 74880 baud | Not a fault. `pio device monitor -b 74880` to read it |
| Everything is garbage at every baud | Sketch `Serial.begin()` disagrees with `monitor_speed`, or GPIO2 is being driven | Match the two; check nothing loads D4 |
| Board boots but the sketch never runs; `boot mode:(1,x)` | GPIO0 held low at reset | Remove whatever pulls D3 low. A momentary button is fine, a peripheral is not |
| Uploads fail, `Failed to connect` | Auto-reset not working, or the port is held open | Hold FLASH + tap RST. Close other monitors. Drop `upload_speed` |
| Uploads used to work, now never do | An interrupted flash left the chip in a bad state | Hold FLASH while plugging in the USB cable |
| `Soft WDT reset` every few seconds | `loop()` blocks for > ~3 s without `delay`/`yield` | Break the work up; call `yield()` inside long loops |
| `wdt reset` in the ROM banner (`rst cause:4`) | Hardware watchdog: blocked > ~8 s, often inside an ISR or with interrupts off | Same, and check for a non-`IRAM_ATTR` ISR |
| `Exception (28)` / `(3)` / `(9)` with an address dump | Null or unaligned dereference; often a `String` freed twice or a stack overflow | `monitor_filters = esp8266_exception_decoder`, then read the decoded trace |
| `ISR not in IRAM!` panic | An interrupt handler in flash was called during a flash read | Mark it `IRAM_ATTR` |
| Reboots the instant Wi-Fi connects | Supply cannot deliver the ~500 mA TX peak | Better cable, powered hub, or a 5 V supply on VIN |
| Random reboots under load with a 9–12 V supply on VIN | NCP1117 thermal shutdown | Feed VIN 5 V |
| `ESP.deepSleep()` never wakes | GPIO16↔RST link not fitted | §8 — solder R10 / jumper D0 to RST |
| Uploads stopped working after adding deep sleep | The GPIO16↔RST link holds the chip in reset | Lift the jumper to upload |
| `analogWrite(pin, 700)` is full brightness | Core 3.x default range is 255 | `analogWriteRange(1023)` or write 0–255 |
| `digitalWrite(2, …)` toggles the wrong pin | D-numbers ≠ GPIO numbers; `D2` is GPIO4 | Pick one scheme; see §1 |
| I2C scan finds nothing | Wrong pins (D1/D2 are GPIO5/GPIO4, not 1 and 2), or no pull-ups | `Wire.begin(4, 5)`; add 4.7 kΩ to 3V3 |
| LittleFS `begin()` fails | Filesystem never uploaded, or the sketch mounts SPIFFS | `pio run -t uploadfs`; `board_build.filesystem = littlefs` |
| Heap looks fine but `new`/`String` fails | Fragmentation — the largest free block is small | Watch `ESP.getMaxFreeBlockSize()`; reserve buffers once at startup |
| Sketch will not link, "section overflow" | Over the ~1 MB program ceiling | It is not the flash size; reduce the sketch (§9) |
| ADC reads low with a real sensor | The 320 kΩ divider loads the source | Buffer with an op-amp, or use a lower-impedance divider |
