# The ESP32 family — cross-chip reference

Everything in this repo is board-scoped: one skill knows one board, and its `*-soc.md`
knows that board's silicon in depth. This file is the other axis — **what is true across
the family**, and specifically the differences that are invisible in a marketing table and
that make code written for one ESP32 wrong on another.

Read this when the question is *which chip*, not *which pin*:

- someone asks whether to move a design from this board to a different ESP32
- a tutorial, library or answer written for a different ESP32 is being adapted
- the requirement is a protocol (Matter, Thread, USB mass storage, CAN FD, 5 GHz Wi-Fi)
  rather than a peripheral
- something works on one ESP32 and not on another, and the difference is not the pin map

**It is not a datasheet.** For any chip that has a skill in this repo, that skill's
`*-soc.md` wins over this file. Where a figure here comes from a secondary source rather
than an Espressif datasheet it is marked **⚠︎ second-hand** and should be checked against
`soc_caps.h` for the target before it is designed around.

---

## 1. Which chips this repo actually covers

| Chip | Skill | Board |
|---|---|---|
| ESP32-D0WDQ6 / D0WD-V3 | `esp32-wroom-30pin`, `-36pin`, `-38pin` | WROOM-32 devkits |
| ESP32-C3FH4 | `esp32c3-oled042` | ESP32-C3 0.42" OLED |
| ESP32-C6FH4 | `esp32c6-lcd147` | Waveshare ESP32-C6-LCD-1.47 |
| ESP32-S3-WROOM-1 | `esp32s3-cam-40pin` | 40-pin ESP32-S3 CAM |

**No skill exists for ESP32-S2, C2, C5, C61, H2, H4 or P4.** If the work is on one of
those, say so rather than reasoning from the closest skill — §2 and §9 say how far each
one's knowledge actually carries.

---

## 2. What does and does not transfer between ESP32s

Three things break when code moves between chips, in descending order of how much time
they cost:

1. **The GPIO numbers are different and the *set* of GPIO numbers is different.** Every
   chip skips numbers, and each skips different ones (ESP32: no 20/24/28–31; S3: no
   22–25; C3: 22 pins on QFN32; C6: 22 on QFN32). A loop over `0..N` faults somewhere.
2. **The core architecture is different.** ESP32/S2/S3 are Xtensa (`xtensa-esp32*-elf`,
   hardware FPU on ESP32/S3); C-, H- and P-series are RISC-V (`riscv32-esp-elf`, **no
   FPU on C3/C6/H2** — `float` is soft-float and roughly 50× slower than the integer path).
   Assembly, `IRAM_ATTR` sizing, ULP programs and stack-depth assumptions do not port.
3. **Peripherals silently disappear.** The DAC exists only on the original ESP32. Classic
   Bluetooth (BR/EDR) exists only on the original ESP32. Touch does not exist on C3/C6/H2.
   The pattern is that newer chips have *more* peripheral types and *fewer* of the analog
   legacy ones — a port forward loses DAC and BR/EDR, a port backward loses 802.15.4,
   USB-Serial-JTAG and the LP core.

What *does* port: ESP-IDF driver APIs (`driver/gpio.h`, `esp_wifi`, NVS, the event loop),
partition tables, and the general shape of `sdkconfig`. The IDF's job is exactly this
abstraction, and it does it well; the trap is the layer underneath.

---

## 3. Overview of the shipping family

Cores, memory and radio for the chips in volume production. Trimmed to what changes a
design decision — full electrical detail lives in the per-chip `*-soc.md`.

| Chip | Core(s) | Max clock | SRAM | In-package flash/PSRAM | Radio |
|---|---|---|---|---|---|
| **ESP32** | 2 × Xtensa LX6 | 240 MHz | 520 KB | 0/2/4 MB flash, 0/2 MB PSRAM | Wi-Fi 4 (b/g/n), **BT 4.2 BR/EDR + LE** |
| **ESP32-S2** | 1 × Xtensa LX7 | 240 MHz | 320 KB + 16 KB RTC | 0/2/4 MB flash, 0/2 MB PSRAM | **Wi-Fi only, no Bluetooth** |
| **ESP32-S3** | 2 × Xtensa LX7 + PIE vector | 240 MHz | 512 KB + 16 KB RTC | 0/4/8/16 MB flash, 0–16 MB PSRAM | Wi-Fi 4, BLE 5 (**no BR/EDR**) |
| **ESP32-C2** | 1 × RISC-V | 120 MHz | 272 KB | none (external only) | Wi-Fi 4, BLE 5 |
| **ESP32-C3** | 1 × RISC-V | 160 MHz | 400 KB | 0/4 MB flash, no PSRAM | Wi-Fi 4, BLE 5 |
| **ESP32-C5** | RISC-V HP + LP | 240 MHz | 384 KB HP + 16 KB LP | 0/4 MB flash, 0/8 MB PSRAM | **Dual-band Wi-Fi 6 (2.4 + 5 GHz)**, BLE 5, 802.15.4 |
| **ESP32-C6** | RISC-V HP + LP | 160 MHz | 512 KB HP + 16 KB LP | 0/4/8 MB flash, no PSRAM | Wi-Fi 6 (2.4 GHz), BLE 5.3, 802.15.4 |
| **ESP32-C61** | 1 × RISC-V | 160 MHz | 320 KB | 0/4 MB flash, 0/2/8 MB PSRAM | Wi-Fi 6 (2.4 GHz), BLE 5 |
| **ESP32-H2** | 1 × RISC-V | 96 MHz | 320 KB + 4 KB LP | 0/2/4 MB flash, no PSRAM | **BLE 5.3 + 802.15.4 only — no Wi-Fi** |
| **ESP32-H4** | 2 × RISC-V | 96 MHz | 320 KB | none, 0/4 MB PSRAM | **BLE 5.4 + 802.15.4 only — no Wi-Fi** |
| **ESP32-P4** | 2 × RISC-V HP + 1 LP | 360 MHz | 768 KB + 32 KB LP | none, 0/16/32 MB PSRAM | **none — no radio at all** |

Three entries in that table are the ones people get wrong:

- **ESP32-S2 has no Bluetooth.** Not "reduced" — absent. A design that needs BLE
  provisioning cannot use an S2.
- **ESP32-H2 and H4 have no Wi-Fi.** They are Thread/Zigbee/BLE parts. An H2 cannot join a
  Wi-Fi network, cannot run `esp_wifi`, and cannot be the internet-facing half of a design.
- **ESP32-P4 has no radio.** It is an application processor. Every P4 design that needs
  connectivity pairs it with a second chip (usually a C6 over SDIO or SPI, running
  `esp_hosted`).

⚠︎ **second-hand** — the S2, C2, C5, C61, H2, H4 and P4 rows are compiled from Espressif's
product selector via the source in §11, not from a datasheet read for this repo. Treat the
memory-combo numbers as approximate and confirm the exact part-number suffix before
ordering; the *presence or absence* of a radio is safe.

Announced but with **no publicly indexed series datasheet**, so not usable as a design
assumption: ESP32-E22 (a Wi-Fi 6E radio co-processor), ESP32-H21 (BLE + 802.15.4,
ultra-low-power), ESP32-S31. If one of these comes up, say the register-level
specification is not published rather than quoting a figure from a press announcement.

---

## 4. Radio — what the keyword actually means

### Wi-Fi

| Generation | Chips | Note |
|---|---|---|
| Wi-Fi 4 (802.11 b/g/n, 2.4 GHz) | ESP32, S2, S3, C2, C3 | 20/40 MHz, 150 Mbps PHY ceiling |
| Wi-Fi 6 (802.11 ax, 2.4 GHz) | C6, C61 | 20 MHz only; the win is airtime efficiency and TWT, not raw rate |
| Wi-Fi 6 dual-band (2.4 + 5 GHz) | **C5 only** | the only Espressif part that reaches 5 GHz |
| none | H2, H4, P4 | |

**Target Wake Time (TWT)** is the reason to pick a Wi-Fi 6 part for a battery sensor: it
lets the AP schedule the station's wake-ups so the radio sleeps between negotiated slots
instead of on a DTIM beacon cadence. It needs an AP that supports it; without one, a C6
draws the same as a C3.

### Bluetooth

| Chips | Support |
|---|---|
| ESP32 | BT 4.2 — **BR/EDR (classic) and LE**. The only Espressif part with classic |
| S3, C2, C3, C5, C6, C61, H2, H4 | **LE only** (5.0–5.4 depending on part) |
| S2 | none |

Anything needing A2DP, HFP, SPP or a classic serial profile needs the original ESP32.
There is no software route to classic BT on an LE-only chip — a very common wrong answer.

### 802.15.4, and the Zigbee / Thread / Matter stack

This layering is conflated constantly, and the confusion produces impossible requirements:

```
802.15.4          radio + MAC. 250 kbit/s O-QPSK, 2.4 GHz, 16 channels (11–26).
                  Short frames, mesh-friendly, ~1/100 the throughput of Wi-Fi.
   ↓
Thread  /  Zigbee network layer. Thread is IPv6 (6LoWPAN); Zigbee is not.
   ↓
Matter            application layer. Runs over Thread *or* over Wi-Fi — never over
                  Zigbee, and never directly on 802.15.4.
```

Consequences worth stating explicitly:

- **Having the radio is not having the stack.** C6, C5, H2 and H4 have 802.15.4 hardware.
  Whether a *certified* Thread or Zigbee stack is available for a given IDF version is a
  separate question, answered by `esp-thread-br` / `esp-zigbee-sdk` release notes, not by
  the datasheet.
- **Matter over Wi-Fi needs no 802.15.4 at all.** An ESP32-C3 is a perfectly valid Matter
  device. If someone says "I need a C6 for Matter", they usually mean Matter over Thread.
- **A Thread network needs a border router** to reach IP. That is a separate always-powered
  device (commonly an S3 + C6 pair, or a commercial hub).
- 802.15.4 channels 11–26 overlap the Wi-Fi 2.4 GHz band. On a chip running both, expect
  interference on top of the coexistence cost below.

### Coexistence

On every multi-radio ESP32 the radios are **time-sliced through one shared 2.4 GHz front
end**, not concurrent. Wi-Fi + BLE + 802.15.4 on a C6 means the software coexistence
arbiter is dividing airtime, and each protocol's achievable throughput and latency degrade
accordingly. Budget for it: a BLE connection held open while Wi-Fi is streaming will show
interval jitter, and this is normal behaviour, not a bug to chase.

---

## 5. USB — "USB OTG" is three different things

The single most misleading row in any ESP32 comparison table.

| Class | Chips | Speed | What it can do |
|---|---|---|---|
| **No USB peripheral at all** | **ESP32 (original)**, H4 | — | every devkit carries a CP2102/CH340 bridge; "flashing over USB" is really UART0 through a bridge |
| **USB-Serial-JTAG only** | C3, C2, C5, C6, C61, H2 | 12 Mbit/s | CDC console + JTAG debug + flashing, on fixed pins. **Not a general USB controller** — no HID, no MSC, no host mode |
| **USB OTG 1.1 full speed** | **S2, S3** | 12 Mbit/s | real device *and* host stacks — HID, CDC, MSC, host for a keyboard or a stick |
| **USB OTG high speed** | **P4** | 480 Mbit/s | plus a separate FS OTG and a Serial/JTAG controller — three USB controllers |

Two rules that fall out of this:

- **A USB HID keyboard/mouse, a USB mass-storage gadget, or USB host needs an S2, S3 or
  P4.** On a C3 or C6 the answer is not "use TinyUSB", it is "the hardware cannot".
- **12 Mbit/s is ~1 MB/s at best, realistically 600–900 kB/s.** Streaming camera frames or
  logging to a USB drive off an S3 is bounded there, not by the sensor. If the requirement
  is faster, the chip is a P4.

---

## 6. Interface speed classes

### UART

**5 MBaud** ceiling on the HP UART controllers across the whole family — it falls out of
the APB clock and the internal divider, and it is the same number on an ESP32 and a P4.
RTS/CTS hardware flow control on every general-purpose controller.

LP-UART (a separate low-power controller, usable while the HP core sleeps) exists on
**C5, C6, P4** and runs at much lower rates off an RTC clock source.

### I2C

- **100 kbit/s and 400 kbit/s work everywhere.** That is the portable assumption.
- Above 400 kbit/s the controller divider usually allows it (the ESP32 and S3 refs quote
  800 kHz–1 MHz), but the limit is physical: SCL rise time against the bus capacitance and
  pull-up value. Anything above 400 kHz needs measured pull-ups, not a config change.
  Check `SOC_I2C_*` in the target's `soc_caps.h` before promising a rate.
- **No ESP32 does I2C High-Speed mode (3.4 Mbit/s).**
- **ESP32-C2 is I2C master-only** — no slave role (`SOC_I2C_SUPPORT_SLAVE` is not defined
  for that target). Every other chip does both. ⚠︎ **second-hand**, but it matches C2's
  generally stripped feature set (no RMT, no MCPWM).
- **LP-I2C** on C5, C6, P4 — master-only, for polling a sensor while the HP core sleeps.
- **I3C** on **P4 only**.

### SPI

SPI0/SPI1 are the flash+PSRAM controllers on every chip and are not available to an
application. What is left is SPI2 (and SPI3 where present).

| Chip | Master max | Slave max | Widths |
|---|---|---|---|
| ESP32 | 80 MHz **on IO_MUX pads only** | 40 MHz | single/dual/quad |
| S2, S3 | 80 MHz | 80 MHz | + octal (OPI) on S3's SPI3 |
| C2, C3, C6, C5, H2 | 60–80 MHz | 40 MHz | single/dual/quad |
| P4 | 80–100 MHz | — | + octal, + LP-SPI |

The **original ESP32 is the one where pin choice costs clock rate**: routing SPI through
the GPIO matrix instead of the IO_MUX pads adds 25 ns and clamps full-duplex transfers to
26.67 MHz. On the S3 and the RISC-V parts the matrix is fast enough that this stops
mattering below ~40 MHz. See `esp32-soc.md` §8 for the derivation.

### I2S

One controller on S2, C2, C3, C5, C6, H2; two on ESP32, S3 and P4. 40 MHz on everything
except P4 (50 MHz, plus 16-channel TDM). PDM microphone support on ESP32, S2, S3, C3, C6,
H2. On the original ESP32 the I2S peripheral doubles as the parallel LCD/camera interface —
on the S3 that job moved to a dedicated LCD_CAM block.

### RMT — the peripheral behind every WS2812 driver

RMT changed generation between chips, and the differences decide whether an addressable-LED
chain glitches under radio load.

| Chip | Channels | Architecture | Symbols/block | DMA | Continuous RX |
|---|---|---|---|---|---|
| **ESP32** | 8 | **flexible** — any channel is TX or RX | 64 | no | no (v1) |
| **ESP32-S2** | 4 | flexible | 64 | no | no (v1) |
| **ESP32-S3** | 8 | dedicated 4 TX + 4 RX | 48 | **yes** | yes |
| **ESP32-C3** | 4 | dedicated 2 TX + 2 RX | 48 | no | yes |
| **ESP32-C6** | 4 | dedicated 2 TX + 2 RX | 48 | no | yes |
| **ESP32-H2** | 4 | dedicated 2 TX + 2 RX | 48 | no | yes |
| **ESP32-P4** | 8 | dedicated | 48 | **yes** | yes |
| **ESP32-C2, C61** | **none** | — | — | — | — |

"Continuous RX" is `SOC_RMT_SUPPORT_RX_PINGPONG` — whether the receiver wraps around its
buffer or simply stops when it fills, which decides whether a long capture or RMT-based
UART emulation is possible. Confirm the DMA and ping-pong columns in the target's
`components/soc/<target>/include/soc/soc_caps.h` before designing around them; the channel
counts and block sizes are stable, those two capability flags moved between IDF versions.

A "symbol" is one 32-bit entry (level + duration ×2). The per-channel block is what caps
how much waveform fits before the driver has to refill mid-transmission — a C3's 48 symbols
is 24 WS2812 bits, i.e. the driver refills 20 times for a 20-LED strip.

What this actually means:

- **Only the S3 (and P4) can drive long LED chains glitch-free while Wi-Fi is busy.** DMA
  decouples RMT refill from interrupt latency. Everywhere else the refill is interrupt-
  driven ping-pong, and a Wi-Fi or BT interrupt arriving mid-frame stretches a bit past the
  WS2812's timing window — the visible symptom is one wrong-coloured LED, intermittently,
  under network load. The fix on a non-DMA chip is the SPI-based driver, not more RMT
  tuning.
- **ESP32-C2 and C61 have no RMT at all.** Every WS2812 example, `neopixelWrite()`, and
  most one-wire (DS18B20) code is unusable there; the SPI or PARLIO route is the only one.
- Flexible-vs-dedicated changes the API: on ESP32/S2 you allocate a channel and set its
  direction; on the S3-generation you request a TX or an RX channel and the driver picks
  from the fixed pool. Code that assumes it can make channel 4 an RX channel fails on a C3.

### Peripherals that exist on only one or two chips

| Peripheral | Where |
|---|---|
| **DAC** (2 × 8-bit, true analog out) | **original ESP32 only** — dropped on every later chip |
| Touch sensor | ESP32 (10 ch), S2/S3 (14 ch), H4, P4. **Not on C3, C6, C2, H2** |
| Ethernet MAC (RMII, needs a PHY) | ESP32, P4 |
| TWAI / CAN 2.0 | ESP32, S2, S3, C3, C5, C6, C61, H2, P4 (not C2) |
| **CAN FD** | **C5, P4** |
| MIPI CSI / DSI, JPEG codec, H.264, ISP | **P4 only** |
| PARLIO (parallel IO) | C5, C6, C61, H2, P4 |
| MCPWM (dead-time, motor bridges) | ESP32, S3, C6, C5, H2, P4 (not C2, not C3) |
| SDMMC host | ESP32, S3, P4 (SDIO *slave* on C6, C61) |
| ULP FSM coprocessor | ESP32 |
| ULP RISC-V coprocessor | S2, S3 |
| Dedicated LP core | C5, C6, H4, P4 |

**MCPWM's absence on the C3 is a recurring trap** — motor-control code moved from an ESP32
to a C3 compiles against `driver/mcpwm.h` only until link time. LEDC has no dead-time
insertion; an H-bridge driven from two LEDC channels shoot-throughs.

---

## 7. Deep sleep and retained memory

Every ESP32 supports deep sleep. What differs is *what stays alive* and *what can run*.

| Chip | Retained memory | Can execute during deep sleep? |
|---|---|---|
| ESP32 | 8 KB RTC FAST + 8 KB RTC SLOW | yes — ULP **FSM** (assembly-ish, awkward) |
| S2, S3 | 16 KB RTC SLOW | yes — ULP **RISC-V** (write it in C) or FSM, not both |
| C3, C2 | small RTC region (< 8 KB) | **no coprocessor** — storage only |
| C6, C5 | 16 KB LP SRAM | yes — a real **LP RISC-V core** |
| H2, C61 | 4 KB LP SRAM | **no LP core** — storage only, LP peripherals can poll |
| H4 | 16 KB LP SRAM | yes — LP core |
| P4 | 32 KB LP SRAM | yes — LP core |

The distinction that matters: **"has LP SRAM" ≠ "can run code asleep".** H2 and C61 have
the memory and no core to use it. If the requirement is "wake, read a sensor, decide, go
back to sleep without the main CPU", the shortlist is ESP32/S2/S3 (ULP) or C5/C6/H4/P4
(LP core).

`RTC_DATA_ATTR` works on all of them for variables that survive the sleep.

Typical deep-sleep current, chip alone, RTC timer + retained memory:

| Chip | Deep sleep | Source |
|---|---|---|
| ESP32 | **10 µA** | datasheet, via `esp32-soc.md` §6 |
| ESP32-S3 | **7 µA** | datasheet, via `esp32s3-soc.md` §7 |
| ESP32-C3 | **5 µA** | datasheet Table 5-9, via `esp32c3-soc.md` §9 |
| ESP32-C6 | **7 µA** | datasheet Table 5-11, via `esp32c6-soc.md` §10.6 |
| S2, C2, C5, C61, H2, H4, P4 | 5–25 µA range | ⚠︎ **second-hand**, not verified here |

> **Never quote any of these for a devkit.** The LDO's quiescent draw, the USB-UART bridge
> and the power LED dominate by three orders of magnitude: a typical CH340/AMS1117 board
> idles at 8–20 mA in deep sleep. This rule is in every board skill in this repo because it
> is the single most common wrong answer about ESP32 power.

---

## 8. Choosing a chip

Read top to bottom; the first match wins.

| Requirement | Chip |
|---|---|
| Classic Bluetooth (A2DP, SPP, HFP) | **ESP32** — nothing else has BR/EDR |
| True analog output (DAC) | **ESP32** — nothing else has it |
| Ethernet without an extra chip | ESP32, or P4 |
| USB HID / mass storage / USB host | **S2, S3** (or P4) |
| Camera + PSRAM + on-device inference | **S3** |
| MIPI camera, video codec, display pipeline | **P4** (+ a C6 for radio) |
| 5 GHz Wi-Fi | **C5** — the only one |
| Thread / Zigbee / Matter-over-Thread | C6, C5 (with Wi-Fi), or H2/H4 (without) |
| Battery sensor, Wi-Fi, long sleep | C6 (TWT) or C3 (cheaper, no TWT) |
| Battery sensor, no Wi-Fi, years on a coin cell | **H2** |
| Cheapest thing that speaks Wi-Fi + BLE | C2, then C3 |
| Motor control with dead-time PWM | ESP32, S3, C6 — **not C3** |
| CAN FD | C5, P4 |
| Maximum library/tutorial compatibility | **ESP32** — a decade of code assumes it |

The last row is not a joke. The original ESP32 is the worst chip in the family on power,
security and peripheral generation, and it is still often the right answer because an
Arduino library written in 2018 works on it unmodified and needs porting everywhere else.

---

## 9. Multi-chip designs

The pattern is now standard rather than exotic, because P4 has no radio and because
protocol coverage does not fit on one die:

- **P4 + C6** over SDIO or SPI, with `esp_hosted` — the P4 runs the application, the C6 is
  a Wi-Fi 6 / Thread modem. This is Espressif's own reference topology.
- **S3 + C6** — S3 does camera/AI/display, C6 adds Thread and Wi-Fi 6.
- **anything + H2** as a Thread/Zigbee side-radio on a Wi-Fi-only main chip.

If a design does this, the **link between the two chips is part of the threat model**. An
SPI or UART bus between an application chip and a radio/crypto chip is tappable and
spoofable by anyone with physical board access — the same attacker the second chip is
usually there to defend against. It needs authentication (a boot-time session key or a
pre-provisioned shared secret) and encryption if the traces are exposed. Without that the
security boundary quietly relocates to the weakest point of the internal bus.

---

## 10. Security features, briefly

Every current ESP32 has: AES, SHA, RSA, an RNG, flash encryption, and Secure Boot v2
(RSA-3072 or, on newer parts, ECDSA). Newer chips add HMAC and a Digital Signature
peripheral that signs without exposing the private key to software, and C5/C6 carry a
PSA Certified Level 2 evaluation.

Two things worth saying plainly when this comes up:

- **Flash encryption and Secure Boot are eFuse-burned and irreversible.** Enabling either
  in `sdkconfig` on a development board is a one-way door — the board can stop accepting
  ordinary `idf.py flash` afterwards. Never enable them casually to "make the example more
  secure".
- **Post-quantum cryptography is not available in hardware on any shipping ESP32.** The
  accelerators are RSA and ECC. ML-KEM / ML-DSA (FIPS 203/204) would have to run in
  software, and ML-DSA's working set does not fit comfortably alongside a TLS stack on a
  chip with 320–520 KB of SRAM. If a requirement mentions PQC or crypto-agility, the honest
  answer is that it is a software problem on these parts today, with a real RAM cost.

---

## 11. Sources

Per-chip electrical, pin and register detail in this repo comes from Espressif datasheets
and technical reference manuals, digested in each skill's `*-soc.md`.

The cross-chip comparison material in §3–§7 — the family overview table, USB speed classes,
interface speed classes, the RMT generation table, and the deep-sleep memory taxonomy — is
adapted from **[ESP32Features](https://github.com/artkeller/ESP32Features)**, an
independent survey of the Espressif SoC family, used under **CC BY 4.0**. Its own text
flags several rows as unconfirmed; those are the ones marked ⚠︎ **second-hand** here, and a
few figures (RMT block sizes given in bytes, some 2026 chip announcements) were corrected
or dropped rather than carried over.
