---
name: esp8266-nodemcu-30pin
description: Firmware development for the 30-pin NodeMCU ESP8266 devkit — NodeMCU DevKit V1.0 / Amica (CP2102) and LoLin NodeMCU V3 (CH340G), built on the Ai-Thinker ESP-12E or ESP-12F module (ESP8266EX, 4 MB flash), with PlatformIO and the Arduino ESP8266 core (`board = nodemcuv2`). Use when working on a NodeMCU, ESP-12E, ESP-12F or ESP8266 devkit: project setup, platformio.ini, the D0-D10 vs GPIO numbering, which pins are safe and which break the boot, GPIO15/GPIO0/GPIO2 strapping, the A0 divider and 1 V ADC, software I2C and software PWM, analogWrite range, LittleFS and flash layouts, OTA, deep sleep and the missing GPIO16-to-RST link, Wi-Fi, heap exhaustion, watchdog and Soft WDT resets, exception decoding, flashing over CP2102/CH340, or debugging why the board does not boot, prints garbage, or reboots by itself.
---

# NodeMCU ESP8266 devkit — 30-pin

The board that made the ESP8266 usable: an Ai-Thinker ESP-12E/F module, a USB-UART
bridge, an LDO, two buttons, 15 pins a side. Everything hard about it is a pin with a
second job, a peripheral that is software rather than silicon, or a number that means
something different than it looks like.

Read `reference/board-hardware.md` before choosing pins. Most ESP8266 tutorials are
written for core 2.x and for a different board; several of their defaults are now wrong.

- `reference/board-hardware.md` — the board: full header map with D-number ↔ GPIO
  translation, the strapping resistors and their values, the two LEDs, the A0 divider,
  the auto-program circuit and its truth table, the power tree — **plus** a development
  guide (§9 toolchain and flash layouts, §10 flashing and recovery, §11 peripheral
  cookbook, §12 symptom → cause → fix table).
- `reference/esp8266-soc.md` — the silicon: the 17 GPIOs and which six are the flash
  bus, memory map and why a sketch cannot exceed ~1 MB, clocks, the ADC's two mutually
  exclusive modes, UART/SPI/HSPI/I2S pin tables, PWM as a timer service, sleep modes and
  their real currents.
- `reference/recipes.md` — code that compiles: `platformio.ini` variants, digital and
  analog IO, software I2C and its scanner, HSPI, LittleFS, Wi-Fi station with reconnect,
  ArduinoOTA, deep sleep with RTC memory, Ticker, WS2812 over I2S, and the watchdog and
  exception-decoder recipes.
- `template/` — a **project that builds clean and was run on hardware**, in two
  variants, plus a scaffold script. See `template/README.md`.

## Confirm the board first

All 30-pin NodeMCU boards share the module, the pin map and every rule below. Two things
differ, and both bite:

| | DevKit V1.0 / Amica ("V2") | LoLin NodeMCU V3 |
|---|---|---|
| Bridge | CP2102 (Silicon Labs VCP driver) | CH340G (WCH driver) |
| Size | 48 × 26 mm — fits a breadboard with a row free | 58 × 31 mm — **covers both rails**, needs two breadboards |
| Two pads next to A0 | `RSV`, `RSV` — not connected | `VU` (USB 5 V, pre-LDO) and `GND` |
| Port name | `cu.SLAB_USBtoUART` / `cu.usbserial-0001` | `cu.usbserial-*`, VID 0x1A86 PID 0x7523 |

Anything else labelled "NodeMCU 30-pin" — CH340 clones of the V1.0 layout are common — is
a V1.0 electrically. If the user needs to know which they have, `pio device list` names
the bridge chip.

## Orientation

| | |
|---|---|
| Module | ESP-12E or ESP-12F (Ai-Thinker), ESP8266EX, **Tensilica L106 32-bit, single core** @ 80 MHz (160 MHz optional) |
| Memory | 4 MB flash on-module, ~80 KB usable SRAM. **~50 KB free heap in a bare sketch** — verified. No PSRAM, no data cache to speak of |
| Program space | **1,044,464 B** with the default `4m1m` layout + 1 MB LittleFS. A sketch cannot exceed ~1 MB whatever the flash size |
| Crystal | **26 MHz** (verified by esptool). This is why the ROM log is 74880 baud |
| Header | 30 pins, 15 per side. **9 usable GPIOs**, 6 more are the flash bus and must not be touched |
| USB | No USB peripheral in the chip. CP2102 or CH340G bridge on UART0 (GPIO1/GPIO3) with DTR+RTS auto-reset |
| Console | UART0 @ 115200 for your sketch; **74880 for the ROM bootloader**. No JTAG, no debug header |
| LEDs | GPIO2 on the module (`LED_BUILTIN`) and GPIO16 on the PCB (`LED_BUILTIN_AUX`), **both active LOW** |
| Buttons | **FLASH** = GPIO0, pressed low · **RST** acts on nRST, not readable |
| Safe pins | D1 (GPIO5), D2 (GPIO4), D5 (GPIO14), D6 (GPIO12), D7 (GPIO13) |
| Usable with care | D3 (GPIO0), D4 (GPIO2), D8 (GPIO15) — strapping pins, rule 3 · D0 (GPIO16) — rule 4 |
| Never use | SD0–SD3, CLK, CMD = GPIO6–11, the SPI flash bus |
| Analog | one input, A0, 10-bit. Header pin 0–3.2 V through a 220k/100k divider; the chip pad behind it is 0–1.0 V |
| Radio | Wi-Fi b/g/n 2.4 GHz only. **No Bluetooth** — that is the ESP32 |
| Power | 3V3 out (NCP1117, 800 mA) · VIN is a **5 V** input in practice, rule 13 |
| Toolchain | PlatformIO + `platform-espressif8266` 4.2.1 + Arduino core 3.1.2, `board = nodemcuv2` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **The D-numbers are not GPIO numbers, and they are not in order.**
   `D1 = GPIO5, D2 = GPIO4, D3 = GPIO0, D4 = GPIO2, D5 = GPIO14, D6 = GPIO12,
   D7 = GPIO13, D8 = GPIO15, D0 = GPIO16.` Note D1/D2 are *swapped* relative to
   intuition. `digitalWrite(2, …)` and `digitalWrite(D2, …)` are different pins and both
   compile. Pick one scheme per project and never mix them; when reading a user's code,
   check which one they meant before diagnosing anything else.

2. **The ROM boot log is 74880 baud, not 115200.** The module's crystal is 26 MHz and the
   ROM derives its baud from it: 115200 × 26/40 = 74880. At 115200 you see a burst of
   garbage before your first line and reach for a different cable. That garbage is
   `ets Jan 8 2013,rst cause:N,boot mode:(M,N)` — the one line that explains a boot loop.
   `pio device monitor -b 74880` reads it. (Verified live: readable at 74880, garbage at
   115200, and vice versa for the sketch's own output.)

3. **GPIO15 must be LOW at reset, GPIO0 and GPIO2 HIGH.** The board fits 12 kΩ resistors
   to guarantee it (pull-down on GPIO15, pull-ups on GPIO0/GPIO2 — schematic sheet 2).
   Anything you wire that fights them stops the chip booting **before any code or any
   serial output**, so the board reads as dead or bricked:
   - a pull-up, an LED to 3V3, or an SPI slave whose CS idles high on **D8/GPIO15**
   - a peripheral holding **D3/GPIO0** low → the chip sits in flash download mode instead
   - anything pulling **D4/GPIO2** low
   A board that does nothing at all, with no serial at either baud, is nearly always this
   and not a dead chip. Unplug everything from D3/D4/D8 and retry before anything else.

4. **GPIO16 (D0) is not a GPIO like the others.** It is XPD_DCDC, on the RTC domain, with
   its own hardware path. No interrupt, no `analogWrite`, no I2C, no OneWire, no Servo,
   no `attachInterrupt` — all of those compile and silently never fire. Its only pull is
   a pull-**down**: `pinMode(16, INPUT_PULLDOWN_16)`; `INPUT_PULLUP` on pin 16 compiles
   and does nothing. `digitalRead`/`digitalWrite` only.

5. **`ESP.deepSleep()` never wakes on a stock NodeMCU.** Waking needs GPIO16 wired to
   RST, and the 0 Ω link for it (R10 on the schematic, marked `0(NC)`; the instruction
   sheet calls it the R3 position) is **not fitted from the factory** — verified on
   hardware here by driving GPIO16 low and watching the chip not reset. The board goes to
   sleep and stays there, which reads as a crash or a brick. Solder the link, or run a
   jumper from D0 to RST. After that GPIO16 is unusable for anything else, and every
   upload needs the jumper off (RST is held).
   `template/src/main.cpp` tests for the link and reports it; use that before writing
   sleep code.

6. **`analogWrite()` range defaults to 255 in core 3.x, not 1023.** It changed from 2.x
   and nothing warns you. Tutorial code that ramps `analogWrite(pin, 0..1023)` sits at
   full brightness for three quarters of its range. Call `analogWriteRange(1023)`
   explicitly, or write 0–255. (`core_esp8266_wiring_pwm.cpp:29`.)

7. **PWM and I2C are software, not peripherals.** PWM is generated from a timer ISR:
   `analogWriteFreq()` clamps to 100 Hz–60 kHz, every additional PWM pin costs interrupt
   time, and Wi-Fi activity puts visible jitter on the output that no amount of tuning
   removes. `Wire` is bit-banged for the same reason. If a project needs clean PWM —
   servos, motor control, LED dimming that must not flicker — say so early: this board is
   the wrong choice, or the PWM belongs on an external driver.

8. **The loop must return.** There is no preemption; the SDK runs the Wi-Fi and TCP stack
   between calls to `loop()`. Blocking for more than ~3 s trips the software watchdog
   (`Soft WDT reset`), ~8 s the hardware one (`wdt reset`). `delay()` and `yield()` feed
   it — a bare `while (!flag) {}` does not, and neither does a long `for` loop over
   flash. A board that reboots every few seconds while "doing nothing wrong" is this.

9. **~50 KB of heap is the whole budget** (verified: 50,408 B free in the full template,
   52,168 B in a blink). One TLS connection costs 16–24 KB of it, so HTTPS plus a web
   server plus a JSON buffer does not fit. `String` concatenation fragments the heap:
   watch `ESP.getMaxFreeBlockSize()`, not `ESP.getFreeHeap()` — allocations fail on the
   largest block long before free memory runs out.

10. **A sketch cannot exceed ~1 MB, whatever the flash size.** `board = nodemcuv2`
    defaults to `eagle.flash.4m1m.ld`: 1,044,464 B of program space and 1 MB of
    filesystem. The ceiling is the flash cache mapping window, not the 4 MB part.
    Changing to `4m2m` or `4m3m` buys filesystem, never sketch. And OTA needs the new
    image to fit *alongside* the running one, so a 600 KB sketch cannot OTA-update itself.

11. **Use LittleFS; SPIFFS is deprecated in core 3.x** and its symbols are marked
    `__attribute__((deprecated))`. Set `board_build.filesystem = littlefs`. A project that
    mounts SPIFFS on a filesystem uploaded as LittleFS fails at `begin()` with no
    explanation.

12. **A0 is not the chip's ADC pin.** The header pin goes through a 220 kΩ / 100 kΩ 1 %
    divider (schematic sheet 7), so it takes 0–3.2 V while the ESP8266's TOUT pad behind
    it takes 0–1.0 V and is destroyed above ~1.1 V. Two consequences: wiring a sensor
    directly to a bare ESP-12E/F ADC pad using NodeMCU code overvolts it, and the divider
    presents ~320 kΩ, so a high-impedance source reads low. Also `ADC_MODE(ADC_VCC)`
    repurposes the ADC to measure the 3V3 rail — after that `analogRead(A0)` is
    meaningless and the header pin must be left floating.

13. **VIN is a 5 V input, not a 7–12 V one.** The sources disagree and the optimistic one
    is wrong in practice: the schematic's power sheet says "Max Supply Voltage 20 V"
    (the NCP1117's rating), while NodeMCU's own instruction sheet says never exceed 5 V.
    The regulator is a linear SOT-223 part — at 12 V and 200 mA it burns 1.7 W and
    thermally shuts down, which presents as random reboots under Wi-Fi load. Feed VIN 5 V.
    A 1N5819 Schottky sits between USB 5 V and that rail, so VIN and USB together will not
    back-feed the host, but do not rely on it as a design.

14. **GPIOs are 3.3 V and not 5 V tolerant, 12 mA maximum per pin** (ESP8266EX datasheet
    Table 5-1). No 5 V sensor, no 5 V I2C bus, no 5 V shield without a level shifter.
    3V3 on the header is an *output*.

15. **A reset the moment Wi-Fi transmits is the supply, not the code.** The TX burst pulls
    ~500 mA peak (ESP-12F datasheet). A laptop USB-2 port, a thin cable or a hub all give
    the same signature: everything works until the first packet. Check
    `ESP.getResetReason()` before reading a stack trace.

16. **Opening the serial port resets the board.** The two-transistor auto-program circuit
    (schematic sheet 3) decodes DTR *and* RTS, so any terminal that asserts either on open
    restarts the chip and you lose the first lines. Set `monitor_dtr = 0` and
    `monitor_rts = 0` to attach without resetting. Separately: with the board powered from
    a computer and the port *not* open, plugging any other USB device in can reset it —
    a documented NodeMCU quirk, not a fault.

## When the task is choosing pins

There are nine usable GPIOs, and only five of them are free of conditions. Work in this
order:

1. **The five safe pins first**: D1 (GPIO5), D2 (GPIO4), D5 (GPIO14), D6 (GPIO12),
   D7 (GPIO13). Put anything that must be quiet at boot here.
2. **I2C** takes D1 + D2 by default (SCL = GPIO5, SDA = GPIO4) — but I2C is software, so
   any two safe pins work: `Wire.begin(sda, scl)`. Moving it off D1/D2 is free and often
   the right call.
3. **HSPI** is fixed in hardware: SCLK = D5, MISO = D6, MOSI = D7, CS = D8. That consumes
   every remaining safe pin plus a strapping pin. Drive CS from a different pin and leave
   D8 alone if you can; if you cannot, make sure the slave does not pull D8 high at reset
   (rule 3).
4. **Then the conditional pins.** D3 (GPIO0) is fine as a button to GND. D4 (GPIO2) is
   fine as an output that idles high — it already drives the module LED. D8 (GPIO15) is
   fine as an output that idles low.
5. **D0 (GPIO16) last**, and only for a plain input or output (rule 4), and only if you
   are not using deep sleep (rule 5).

If that is not enough: an I2C GPIO expander, or the ESP32, which has three times the
pins, real hardware PWM and I2C, and Bluetooth. Recommend the swap rather than
contorting the pinout — the ESP8266's limits here are structural.

## When the task is "the board does not work"

Each stage depends only on the ones before it:

1. **Does the port enumerate?** No port ⇒ a charge-only cable, or a missing CP2102 /
   CH340 driver. `pio device list` names the chip.
2. **Does anything appear at 74880 baud?** The ROM banner comes out before any of your
   code. Present ⇒ the chip and the flash bus are alive. Absent, with the port there ⇒
   power, or **GPIO15 held high** (rule 3).
3. **Read `rst cause:` from that banner.** `1` power-on, `2` external reset, `4` hardware
   watchdog, `3`/`0` exception or software watchdog. `boot mode:(1,x)` means GPIO0 was low
   — the chip is in download mode and your sketch never ran.
4. **Does it print at 115200 and then reboot?** Read `ESP.getResetReason()`.
   *Soft WDT reset* ⇒ rule 8, something blocks. *Exception (n)* ⇒ enable
   `monitor_filters = esp8266_exception_decoder` and re-read the trace with symbols.
   Random resets only under Wi-Fi ⇒ rule 15, then rule 13.
5. **Does it run but misbehave?** Flash `--minimal` to establish a baseline, then add one
   subsystem at a time. The `--full` variant prints the strapping levels, the flash
   identity, the heap and the GPIO16↔RST verdict, which covers most of what is left.

## Starting a new project

Do not hand-assemble one. `template/` builds clean and was run on hardware:

```sh
~/.claude/skills/esp8266-nodemcu-30pin/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — blink on GPIO2 and GPIO16 plus a serial heartbeat. **267,483 B flash,
  28,292 B RAM.** Flash this first on an unfamiliar board: it touches only the toolchain,
  the upload route, UART0 and two GPIOs, and its outputs fail independently.
- `--full` (default) — board self-test: report, boot-strap read-back, GPIO16↔RST link
  probe, ADC, LittleFS boot counter, Wi-Fi scan, dual-LED heartbeat, FLASH button re-runs
  it. **304,199 B flash, 29,792 B RAM.**

Both build with zero warnings on platform-espressif8266 4.2.1 / Arduino core 3.1.2.
Nothing is generated and no paths are embedded, so copying `template/` by hand works
identically. `template/README.md` maps files to subsystems.

**267 KB for a blink is the floor, not bloat** — the core links the non-OS Wi-Fi SDK into
every sketch. Do not go looking for what to strip.

When the user already has a project, prefer bringing their `platformio.ini` and pin
definitions in line with the template over rewriting their code.

## Flashing

```sh
pio run -t upload -t monitor
```

The bridge drives RST and GPIO0 through the auto-program transistors, so esptool runs the
whole cycle with no buttons. 460800 baud is reliable; 921600 is not on CH340 clones. The
board appears as `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` (macOS),
`/dev/ttyUSB0` (Linux — needs `dialout` membership), or a COM port (Windows).

When it fails with `Failed to connect` or `Wrong boot mode detected`:
**hold FLASH → tap RST → release FLASH → upload.** If auto-reset has stopped working
after an interrupted flash, hold FLASH while plugging the USB cable in; that is the
NodeMCU instruction sheet's own recovery step.

**Bad firmware cannot brick this board.** The ROM bootloader is in mask ROM and always
answers that sequence:

```sh
esptool.py --chip esp8266 -p <port> erase_flash    # ~10 s for 4 MB
```

Erasing also wipes the RF calibration sector; the SDK rewrites it on the next boot, so a
first boot after `erase_flash` may take a second longer and print `rf cal sector: …`.

There is no JTAG and no debug header. `GDBStub` (bundled with the core) gives source-level
debugging over UART0 at the cost of the serial console — that is the whole debug story on
this chip. In practice: `Serial`, the exception decoder, and the `--minimal` baseline.

## Reporting

Say what is verified and what is derived.

**Verified on hardware in this session**, on a 30-pin NodeMCU with a CH340G bridge
(VID 0x1A86 / PID 0x7523), ESP-12E/F module, chip id 0x7107F0, flash id 0x00164068,
4 MB, 26 MHz crystal: both template variants build with zero warnings, upload at 460800
and run; the 74880-baud ROM log and its garbling at 115200; strapping levels
GPIO0 = 1 / GPIO2 = 1 / GPIO15 = 0 after boot; ~50 KB free heap; `deepSleepMax` ≈ 3.2 h;
LittleFS mounts and persists; a Wi-Fi scan returns real APs without browning out; and
**GPIO16 is not tied to RST** (rule 5).

**Taken from primary documents:** ESP8266EX Datasheet v7.1, ESP-12E and ESP-12F
datasheets (Ai-Thinker), the NodeMCU DevKit V1.0 schematic and instruction sheet — the
12 kΩ strapping resistor values, the 220k/100k ADC divider, the LED polarity, the
auto-program truth table, the NCP1117 power tree, and the 500 mA TX peak.

**Not verified here:** which LED the user's board actually lights (the module LED on
GPIO2 is present on every ESP-12E/F; the PCB LED on GPIO16 is a NodeMCU addition and is
absent on bare module breakouts), the LoLin V3 `VU`/`GND` pads, and every current figure
in `reference/esp8266-soc.md` §7. Say so rather than presenting them as measured.
