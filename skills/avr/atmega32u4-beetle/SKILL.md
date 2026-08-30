---
name: atmega32u4-beetle
description: Firmware development for the Beetle — the CJMCU / DFRobot "Mini Arduino Leonardo" USB board (ATmega32U4 @ 16 MHz, 5 V, 21×28 mm, castellated pads) — its native USB (CDC serial + HID keyboard/mouse/joystick), Caterina bootloader and 1200 bps touch upload, ten exposed I/O pads, four PWM pins across Timer0/Timer1, five ADC channels, I2C on D2/D3, SPI on the back-side ICSP pads, 28 KB usable flash and 2.5 KB SRAM, and the PlatformIO + Arduino AVR core (`board = leonardo`) setup around them. Use when working on this board or any ATmega32U4 Leonardo/Micro/Pro-Micro-class clone: project setup, platformio.ini, why the board shows up as two different serial ports, uploads that fail or hang, recovering a bricked or HID-spamming board by shorting the ICSP reset pads, pin mapping and which pads are missing, PWM/timer conflicts, Servo and tone, USB HID keyboard projects, the SRAM budget, powering from the + pad, or debugging why something on the board does not work.
---

# Beetle — Mini Arduino Leonardo (ATmega32U4)

Board-specific firmware knowledge. The Beetle is an ATmega32U4 with **native
USB** and **ten pads**, and almost every trap on it comes from one of those
two facts: USB means the programming interface is the sketch you are about
to replace, and ten pads means most of the Leonardo pin numbering the
compiler accepts goes nowhere on this board.

- `reference/board-hardware.md` — the complete board reference: pad
  inventory and full pin map, the sixteen I/O lines that are not brought
  out, power tree, clock and PLL, memory map, fuses and the Caterina
  bootloader **plus** a development guide (Part II: §8 toolchain, §9 USB and
  the two serial ports, §10 peripheral cookbook, §11 gotchas, §12 flashing
  and recovery, §13 symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini`, `board.h`,
  non-blocking timing, USB-CDC that never blocks, `Serial1` on the TX/RX
  pads, I2C and SPI, the ADC channel map, external and pin-change
  interrupts, PWM and the timer map, Servo vs `tone()`, HID keyboard with
  its safety guards, EEPROM, power-down sleep with USB.
- `template/` — a **project that builds clean**, in three variants, plus a
  scaffold script. See `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | ATmega32U4, 8-bit AVR @ **16 MHz** (crystal), 44-pin QFN. 26 I/O lines on the chip, **10 on the pads** |
| Memory | 32 KB flash, **28,672 B usable** (Caterina bootloader keeps 4 KB), **2,560 B SRAM**, 1 KB EEPROM, + 832 B of USB DPRAM that is *not* part of SRAM |
| Board | DFRobot: 20 × 22 × 3.8 mm; the larger clone this skill was written against: 21 × 28 mm. Castellated pads, micro-USB. **No reset button, no regulator, no VIN, no 3V3** |
| Exposed pads | `RX(D0) TX(D1) SDA(D2) SCL(D3) 9 10 11 A0 A1 A2` + `+` (VCC) and `-` (GND) — DFRobot's own pad↔pin table |
| LED | **D13 = PC7**, on = **HIGH**, no pad. DFRobot advertise a "magic light blue soft BLINK indicator" — the blue LED is D13 |
| PWM | 4 pads: **D3, D11 on Timer0** (the `millis()` timer, ~977 Hz) · **D9, D10 on Timer1** (the `Servo` timer, ~490 Hz) |
| ADC | 5 × 10-bit: A0 (PF7/ADC7), A1 (PF6/ADC6), A2 (PF5/ADC5), and D9 = **A9** (ADC12), D10 = **A10** (ADC13) |
| Interrupts | **4** external: D3→INT0, D2→INT1, D0→INT2, D1→INT3 · pin-change on D9/D10/D11 (PCINT5/6/7) |
| USB | **Native**, on-chip. `Serial` = USB CDC (no baud rate). HID keyboard/mouse/joystick supported |
| UART | `Serial1` on TX/RX (D1/D0) — a **genuinely free** hardware UART, unlike on an Uno/Nano |
| I2C / SPI | SDA=D2, SCL=D3 (SCL doubles as a PWM pad) · SPI on unlabelled **test pads**: MISO=D14/PB3, SCK=D15/PB1, MOSI=D16/PB2, SS=D17/PB0 |
| Power | **4.5–5 V reliable · 3–4.5 V "may work, not guaranteed" · 6 V destroys it** (DFRobot). `+` is unregulated. Motor/relay loads need ≥ 10 µF and their own supply feed |
| Debug | No SWD/JTAG in practice — the JTAG pins are A0–A3 and the Arduino fuse set disables them. **6-dot ICSP pad array on the back** = SPI + RESET |
| Toolchain | PlatformIO 6.1 + `atmelavr` 5.3.0 + Arduino AVR core, **`board = leonardo`** (there is no `beetle` board in PlatformIO) |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **The board is two serial ports, not one.** Running the sketch it
   enumerates as VID:PID `0x2341:0x8036`; in the bootloader it enumerates as
   `0x2341:0x0036` — a *different device* with a *different port name*.
   Uploading means: open the sketch's port at 1200 baud, the chip resets, the
   sketch's port **vanishes**, the bootloader's port appears, `avrdude`
   speaks `avr109` to that one. Consequences: never hard-code `upload_port`
   (it breaks the handshake); a serial monitor holding the sketch port open
   can block the touch; and "the port disappeared mid-upload" is the process
   working, not failing.
2. **The upload route runs through the sketch you are replacing.** The reset
   is performed by the *sketch's own* USB interrupt handler: on seeing the
   1200-baud port close it writes the magic key `0x7777` to `RAMEND-1` and
   calls `wdt_enable(WDTO_120MS)` on itself (core `CDC.cpp`). So a sketch that
   hangs, blocks interrupts or never attached USB **cannot be reset by any
   host** — nothing external is listening. The escape is manual and takes
   **one** touch, not a double-tap: **short ICSP pin 5 (RESET) to pin 6 (GND)
   on the six dots on the back.** Caterina enters the bootloader on any
   external reset and holds it `TIMEOUT_PERIOD = 8000` ms — eight seconds,
   from its own source. Locate those dots before you need them.
3. **`while (!Serial);` hangs the board forever off a PC.** On a 328P that
   line is a no-op; here `Serial` is a USB CDC port and the operator is false
   until a host opens it. On battery, in a prop, in a keyboard — nothing
   runs. Use `if (Serial) { … }` around prints, or a timed wait with an exit
   (`template/src/main.cpp`). But note the second edge: **`operator bool()`
   contains a `delay(10)`**, so every `if (Serial)` costs 10 ms whichever way
   it answers. One test per pass through `loop()` caps that loop at 100 Hz —
   cache it in a `bool` if you check it often.
4. **`Serial.begin(115200)` sets nothing.** USB CDC has no baud rate; the
   number is discarded. Chasing a "baud mismatch" on the USB port is chasing
   a symptom that cannot exist. The real UART, where baud *does* matter, is
   `Serial1` on the TX/RX pads.
5. **D3 and D11 PWM come out of Timer0 — the `millis()` timer.** This is the
   opposite of an Uno/Nano, where D3/D11 are Timer2. Touching `TCCR0A`/
   `TCCR0B` to "change the PWM frequency" on those two pads makes `delay()`,
   `millis()` and every baud-timed bit-bang lie, with no error anywhere. Two
   of the Beetle's four PWM pads are on that timer. They also run at a
   *different frequency* from the other two — ~977 Hz on D3/D11 (Timer0, fast
   PWM) against ~490 Hz on D9/D10 (Timer1, phase-correct), both at prescaler
   /64. Do not assume one pitch across the pad set.
6. **`Servo` takes Timer1 — that is D9 and D10, the other two PWM pads.**
   One `servo.attach()` and `analogWrite()` on D9/D10 is dead. With a Servo
   attached the Beetle has exactly two usable PWM pins left, both on the
   timer you must not reconfigure (rule 5). `tone()`, by contrast, takes
   Timer3, whose only output OC3A = D5 is *not* on a Beetle pad — so
   `tone()` costs you no PWM here, unlike on an Uno.
7. **`analogRead()` takes an analog channel index, not a pin number.** On
   this core `analogRead(2)` samples **A2 (PF5)**, not the SDA pad; the core
   subtracts 18 only from values ≥ 18 and then runs everything through
   `analog_pin_to_channel_PGM`. Always pass the constants: `A0`, `A1`, `A2`,
   and `A9`/`A10` for the D9/D10 pads. Any other bare integer silently reads
   the wrong pin, or a channel that reaches no pad at all.
8. **Nine of the pin numbers you know from an Uno reach nothing here.** D4,
   D5, D6, D7, D8, D12, D13, A3, A4, A5 all compile, all toggle real port
   bits, and none of them are on a Beetle pad (D13 drives the onboard LED
   and stops there). Library examples written for a Leonardo or Pro Micro
   will happily target them. Check `include/board.h` before wiring.
9. **A HID sketch that types at boot locks you out of your own machine.**
   The Beetle enumerates ~2 s after plug-in and starts typing into whatever
   window has focus — including the editor you need to fix it in — every
   time you plug it in. Gate every HID output behind a pin held to GND
   **and** a multi-second grace window after reset
   (`template/variants/hid/main.cpp`). Recovery once it happens is a RESET
   short of ICSP pin 5 to pin 6 while the host is being spammed.
10. **`Keyboard.h` / `Mouse.h` are not in the core PlatformIO installs.**
    Only the low-level `HID` library is bundled. `#include <Keyboard.h>` is
    a compile error until `lib_deps = arduino-libraries/Keyboard` is added —
    the Arduino IDE bundles them, which is exactly why the same sketch
    builds there and not here.
11. **The `+` pad is an unregulated rail and DFRobot say 6 V destroys the
    board.** Their numbers, verbatim: reliable at **4.5–5 V**, "may work" at
    3–4.5 V with reliability not guaranteed, and *"6V will damage the product
    by overvoltage."* A regulated input would absorb that extra volt, so this
    is as good as a statement that there is no regulator — there is also no
    VIN and no 3V3 output. Driving `+` from a bench supply while USB is
    plugged in ties your supply to the host's VBUS; pick one.
12. **A Beetle that reboots when a motor or relay switches is a power
    problem, not a firmware one.** DFRobot flag it explicitly: large loads
    must take their VCC and GND *from the supply directly* rather than through
    the board, with **≥ 10 µF** in parallel, "in order to prevent restart or
    malfunction caused by large load transient". Chasing this as a watchdog
    bug, a USB bug or a stack overflow is the classic wasted evening — an
    unregulated 5 V rail feeding both a motor and an MCU browns out the MCU.
13. **All I/O are 5 V push-pull, and there is no 3.3 V anywhere.** Into a
    3.3 V-only sensor that is damage, not a level mismatch. Level-shift or
    power the peripheral from its own rail with grounds common.
14. **`wdt_disable()` first in `setup()`** (and clear `MCUSR`). Caterina
    uses the watchdog itself to jump from bootloader to sketch, so a sketch
    that leaves the WDT armed with a short timeout reset-loops faster than
    the 8 s window can be caught — and re-uploading does not fix it, only a
    caught ICSP reset does.
15. **A factory-fresh ATmega32U4 has JTAG enabled, and JTAG is A0–A3.**
    Default `JTAGEN` is *programmed* (enabled); the Arduino Leonardo fuse
    set (HIGH = `0xD8`) turns it off. On a re-fused or badly-fused clone,
    A0/A1/A2 — three of your ten pads — read garbage and cannot be driven.
    Symptom: exactly PF4–PF7 dead, everything else fine. Fix by re-burning
    the bootloader, or at runtime by writing `JTD` in `MCUCR` twice within
    four cycles.
16. **Budget flash from ~28 KB, and expect ~4 KB gone before you write a
    line.** Caterina keeps 4 KB of the 32 KB, and the core enumerates USB
    from `main()` whether or not you mention `Serial`: the template's bare
    Blink is **3,956 B** here against 924 B for the same sketch on a Nano.

## When the task is USB HID

This is what the board is bought for — macropads, foot pedals, cosplay props
— and the whole difficulty is that HID and the upload path share one USB
device.

- `Keyboard.begin()` adds a HID interface *alongside* the CDC one; the board
  stays a composite device and uploads keep working. It does re-enumerate,
  so the host may briefly re-assign the port.
- HID carries **key positions, not characters**. `Keyboard.print("y")` on a
  host set to a non-US layout produces whatever key sits at that position.
  Restrict yourself to letters and digits, or map layouts yourself.
- `Keyboard.releaseAll()` on every exit path. A key left down by a reset
  mid-press repeats into the host until it is unplugged.
- The arm-pin + grace-window shape in `template/variants/hid/main.cpp` is
  not a nicety; it is the difference between a board you can iterate on and
  one you recover with tweezers. Keep both guards while developing, even if
  the shipped firmware drops them.
- `Mouse.h` is a separate dependency (`arduino-libraries/Mouse`) from
  `Keyboard.h`. Joystick/gamepad needs a third-party descriptor library —
  none is bundled.

## When the task is memory

2,560 B of SRAM, of which the USB stack has already spent ~150 B before
`setup()` runs — the template's blink reports 149 B used with no globals of
its own. The arithmetic that matters:

- Every `Serial.print("…")` literal lands in SRAM unless wrapped in `F()`.
  The full template — heartbeat, CDC report, ADC, EEPROM, free-RAM watch —
  totals 5,214 B flash and 164 B RAM precisely because every literal is
  `F()`-wrapped.
- The 832 B of USB DPRAM is *separate* silicon and does not come out of the
  2,560 B. Endpoint buffers are not your SRAM budget; CDC ring buffers are.
- `String` fragments the heap and there is no MPU: the heap grows up, the
  stack grows down, and the collision shows up as random corruption minutes
  or days later, never as an error. Use `snprintf()` into fixed `char[]`.
- Instrument, don't estimate. The `free_ram()` probe in `template/src/main.cpp`
  costs ~20 B of flash. Below ~250 B free, restructure instead of adding.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/atmega32u4-beetle/template/variants/new-project.sh <target-dir> [--full|--minimal|--hid]
cd <target-dir> && pio run
```

- `--minimal` — blink the onboard D13 LED. **3,956 B flash, 149 B RAM.**
  Flash this first on an unfamiliar Beetle: it touches one GPIO and nothing
  else, so a failure is the toolchain, the port, the bootloader window or a
  clone with no LED fitted — not your code.
- `--full` (default) — non-blocking heartbeat, USB-CDC report guarded by
  `if (Serial)`, free-RAM watch, ADC on A0, EEPROM boot counter. **5,214 B
  flash, 164 B RAM.**
- `--hid` — USB keyboard with the arm pin and boot grace window from the
  section above; the script appends `lib_deps = arduino-libraries/Keyboard`.
  **6,206 B flash, 231 B RAM.**

All three build as-is with platform-atmelavr 5.3.0 (verified — those are the
build's own figures). Nothing is generated and no paths are embedded, so
copying the tree by hand works identically. `template/README.md` maps files
to subsystems.

When the user already has a project, prefer bringing it in line with the
template's `platformio.ini` and `include/board.h` over rewriting their code.

## Flashing

Over the micro-USB cable, nothing to press *while the sketch is healthy*:

```sh
pio run -t upload -t monitor
```

PlatformIO opens the sketch's CDC port at 1200 baud (`use_1200bps_touch`),
the chip resets into Caterina, PlatformIO waits for the bootloader's port to
appear (`wait_for_upload_port`) and `avrdude` flashes it over `avr109` at
57600. Leave `upload_port` unset so that dance can happen (rule 1).

Recovery ladder, in order — stop at the first that works:

1. **Retry**, watching `pio device list` before and after. Two different port
   names appearing is the handshake working.
2. **Close the serial monitor** and any other program holding the port.
3. **Short ICSP pin 5 (RESET) to pin 6 (GND) once, then upload inside eight
   seconds.** No button on this board — the six dots on the back are the
   reset. One touch is enough; Caterina enters the bootloader on any external
   reset and waits `TIMEOUT_PERIOD = 8000` ms. This is the fix for a crashed
   sketch, a WDT boot-loop (rule 14) and a HID sketch spamming the host
   (rule 9). DFRobot's tip for a board whose sketch killed USB: click Upload
   *first*, then touch the pads — "you need try several times, because the
   correct time is not easy to be caught".
4. **Pass the bootloader port explicitly** for one upload:
   `pio run -t upload --upload-port <the port that appeared>`.
5. **ISP.** The same six dots are a standard ICSP footprint — a second Arduino
   running `11.ArduinoISP` or a USBasp. This bypasses USB and the bootloader
   entirely, so **no firmware permanently bricks this board**. Burning the
   bootloader also restores the fuse set — LOW `0xFF`, HIGH `0xD8`, EXT
   `0xCB` (Arduino's `boards.txt` values; some tools read the extended byte
   back differently because its top four bits are unimplemented) — which is
   the fix for a clone with JTAG wrongly enabled (rule 15) or a mis-set
   clock fuse.

## Reporting

Three tiers of evidence, in descending order. Pass the distinction on rather
than presenting it all in one voice.

**1. Read out of the sources the toolchain itself uses — exact.**
`variants/leonardo/pins_arduino.h` (pin↔port map, PWM↔timer map, the
`analog_pin_to_channel_PGM` table, the interrupt map), `wiring.c` (Timer0
fast-PWM and Timer1 phase-correct at /64, hence 977 Hz vs 490 Hz),
`Tone.cpp` (Timer3), `ServoTimers.h` (Timer1 only), `CDC.cpp` (the 1200-baud
close trigger, magic key, `wdt_enable(WDTO_120MS)`, `operator bool` and its
`delay(10)`, `begin()` discarding its argument), `boards.txt` (VID/PIDs,
fuses, `avr109`, 28672/2560), the bundled-library list, and
`bootloaders/caterina/Caterina.c` (`TIMEOUT_PERIOD = 8000`, `bootKey =
0x7777` at `0x0800`, the EXTRF/PORF/WDRF reset-cause logic — and the absence
of any double-tap detection). Plus the ATmega16U4/32U4 datasheet Atmel-7766J
for the memory map, `JTAGEN` defaulting to *programmed* (Table 28-4), BOD
levels (Table 8-1), the PLL and the package pin numbers (Figure 1-1). All
three template variants were **built**; every flash/RAM figure is that
build's own report.

**2. From the vendor's documentation — authoritative for the board, but a
wiki page, not a schematic.** DFRobot's Beetle SKU:DFR0282 documentation
supplies the pad↔pin table transcribed in `reference/board-hardware.md` §2.1,
the supply limits (4.5–5 V reliable, 3–4.5 V unguaranteed, **6 V destroys
it**), the ≥ 10 µF load-transient warning, the six-dot ICSP array on the back
and its use as the reset, and the "magic light blue soft BLINK indicator"
that establishes the D13 LED exists.

**3. Unresolved — say so when it matters.**

- **Clone variance.** DFRobot's board is 20 × 22 × 3.8 mm; the reseller
  listing for the board this skill was written against says 21 × 28 mm. Same
  advertised interface counts, so the pin table almost certainly transfers,
  but the outline is redrawn and the ICSP dots or LED may sit elsewhere.
  `reference/board-hardware.md` §2.4 confirms a pad map in ten minutes.
- **No schematic exists** for any Beetle, DFRobot's included. "No regulator"
  is inferred from "6 V will damage the product" — sound, but inference.
- **TX/RX LEDs.** The core drives D30/D17 with known polarity; whether a
  Beetle fits those diodes is undocumented, and there is little room at
  20 × 22 mm. Do not rely on them.

**Nothing here was run on a physical Beetle.** Everything marked
**⚠︎ compile-checked only** in `reference/recipes.md` was compiled against
this core with `-Wall` and no warnings, and not executed. If the board
disagrees with this skill, the board is right — that is a gap to report, not
a fault to debug around.
