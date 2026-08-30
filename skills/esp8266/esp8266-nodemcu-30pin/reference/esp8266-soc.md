# ESP8266EX — silicon reference

What is true of the chip regardless of which board it is on. Board-level facts (headers,
LEDs, dividers, power, flashing) are in `board-hardware.md`.

Primary sources: **ESP8266EX Datasheet v7.1** (Espressif), **ESP-12E / ESP-12F Datasheet**
(Ai-Thinker). Figures marked **[hw]** were measured on a 30-pin NodeMCU in this session.

> Espressif marks the ESP8266EX **NRND** (not recommended for new designs) as of
> datasheet v7.1, 2025-11, and points new designs at the ESP8266-pin-compatible ESP8684
> (ESP32-C2). Existing boards are fine; say so if a user is choosing a chip for a new
> product rather than working with hardware they already have.

---

## 1. Core

| | |
|---|---|
| CPU | Tensilica L106, 32-bit Xtensa, **single core** |
| Clock | 80 MHz default **[hw]**, 160 MHz selectable |
| SRAM | ~80 KB usable by the sketch (`80 KB` reported by PlatformIO); the SDK reserves the rest of the 160 KB |
| Free heap | **~50–52 KB [hw]** in a bare Arduino sketch. Espressif quotes "< 50 kB in station mode" |
| ROM | boot ROM only — **no user program ROM**. Code lives in external SPI flash and executes XIP through a cache |
| FPU | none. `float` is software-emulated; `double` more so |
| RTC RAM | 768 B total: 256 B system + **512 B user**, survives deep sleep, cleared by power-off |

There is no MMU, no memory protection and no preemptive scheduler. The SDK runs its
Wi-Fi/TCP tasks in the gaps between your `loop()` calls; this is the single most important
structural fact about the chip.

**Flash cache.** Code is fetched from SPI flash through a 1 MB mapping window. That, not
the flash part, is why a sketch cannot exceed ~1,044,464 B on a 4 MB module.

**Interrupts must be in IRAM.** A handler living in flash cannot be fetched while the
flash bus is busy, so calling one during a flash read panics with `ISR not in IRAM!`.
Every `attachInterrupt` handler and everything it calls must be `IRAM_ATTR`, and IRAM is
only ~32 KB.

---

## 2. GPIO — 17 pins, and what happens to them

| GPIO | Chip pin | Alternate functions | Reality on a module |
|---|---|---|---|
| 0 | 15 | SPI_CS2 | **strapping** — low at reset = UART download |
| 1 | 26 | U0TXD, SPI_CS1 | UART0 TX, carries the ROM log |
| 2 | 14 | U1TXD, I2C_SDA, I2SO_WS | **strapping** — must be high at reset. Module LED |
| 3 | 25 | U0RXD, I2SO_DATA | UART0 RX |
| 4 | 16 | PWM3 | free |
| 5 | 24 | IR Rx | free |
| **6** | 21 | SPI_CLK / SD_CLK | **flash bus — unusable** |
| **7** | 22 | SPI_MISO / SD_D0 | **flash bus — unusable** |
| **8** | 23 | SPI_MOSI / SD_D1, U1RXD | **flash bus — unusable** |
| **9** | 18 | SPIHD / SD_D2 | flash `/HOLD`; usable **only** in DOUT flash mode |
| **10** | 19 | SPIWP / SD_D3 | flash `/WP`; usable **only** in DOUT flash mode |
| **11** | 20 | SPI_CS0 / SD_CMD | **flash bus — unusable** |
| 12 | 10 | MTDI, HSPI_MISO, I2SI_DATA, PWM0 | free |
| 13 | 12 | MTCK, HSPI_MOSI, U0CTS, I2SI_BCK | free |
| 14 | 9 | MTMS, HSPI_CLK, I2C_SCL, I2SI_WS, PWM2, IR TX | free |
| 15 | 13 | MTDO, HSPI_CS, U0RTS, I2SO_BCK, PWM1 | **strapping** — must be low at reset |
| 16 | 8 | XPD_DCDC | RTC domain, see §3 |

Electrical, from datasheet Table 5-1:

| | |
|---|---|
| Logic levels | `VIL ≤ 0.25 × VIO`, `VIH ≥ 0.75 × VIO`; VIO = 3.3 V |
| Output | `VOL ≤ 0.1 × VIO`, `VOH ≥ 0.8 × VIO` |
| **Max current** | **12 mA per pin** |
| Pulls | internal pull-**up** on every GPIO **except GPIO16**, which has pull-**down** only |
| 5 V tolerance | **none** |
| ESD | 2 kV HBM, 0.5 kV CDM |

The datasheet also warns: *U0TXD (GPIO1) must not be pulled low externally during
power-up.*

---

## 3. GPIO16 is a separate thing

GPIO16 is `XPD_DCDC` (chip pin 8) and lives in the RTC power domain, on a different
register block from GPIO0–15. It is wired to nothing in the GPIO matrix, so:

- **no interrupt** — `attachInterrupt(16, …)` compiles and never fires
- **no PWM** — `analogWrite(16, …)` does nothing useful
- **no I2C, no OneWire, no Servo, no SPI, no UART**
- **pull-down only** — `pinMode(16, INPUT_PULLDOWN_16)`; `INPUT_PULLUP` is silently ignored
- `digitalRead` / `digitalWrite` work normally, and are all it does

Its actual job is deep-sleep wake: the RTC drives it low at the end of a sleep, and if it
is physically wired to `EXT_RSTB` the chip restarts. See `board-hardware.md` §8 — on a
stock NodeMCU that wire is **not fitted**.

---

## 4. Clocks

| | |
|---|---|
| Crystal | 24–52 MHz supported; ESP-12E/F fit **26 MHz** **[hw]** |
| CPU | 80 MHz (÷ from PLL) or 160 MHz |
| Flash | 40 MHz default **[hw]**, 80 MHz selectable |
| Tolerance | ±15 ppm required, over −25 … 75 °C |

The 26 MHz crystal has one loud consequence: the ROM bootloader computes its UART divider
as if the crystal were 40 MHz, so its log comes out at **115200 × 26/40 = 74880 baud**.
Everything else about the chip hides the crystal frequency from you.

---

## 5. Peripherals

### UART

| Port | TX | RX | Notes |
|---|---|---|---|
| UART0 | GPIO1 | GPIO3 | full duplex, flow control on GPIO15 (RTS) / GPIO13 (CTS) |
| UART0 swapped | GPIO15 | GPIO13 | `Serial.swap()` |
| UART1 | GPIO2 | — | **TX only**; its RX pin is GPIO8, on the flash bus |

Up to 4.5 Mbps. `Serial1` is the standard way to keep a debug log while UART0 does
something else — but it costs you the module LED pin.

### SPI

Two controllers.

| | Pins | Max clock |
|---|---|---|
| SPI (flash) | GPIO6/7/8/11 (+9/10) | — reserved for flash |
| **HSPI** | CLK GPIO14, MISO GPIO12, MOSI GPIO13, CS GPIO15 | **80 MHz master** (datasheet Table 4-3 says 20 MHz; the Arduino core drives it to 80 MHz and it works) |

HSPI's pins are fixed — there is no GPIO matrix on this chip. CS can be any GPIO you drive
yourself, which is usually better than using GPIO15.

### I2C

**There is no I2C peripheral.** `Wire` is a bit-banged software implementation
(`core_esp8266_si2c.cpp`). Consequences:

- any two GPIOs work: `Wire.begin(sda, scl)`; the `nodemcu` variant defaults to
  SDA = GPIO4, SCL = GPIO5
- transfers are **blocking** and consume CPU proportional to length
- the practical ceiling is ~400 kHz, and it degrades under Wi-Fi activity
- there are no internal pull-ups worth using: fit 4.7 kΩ to 3V3

### PWM

Also software — a timer ISR, not a peripheral.

| | |
|---|---|
| Frequency | `analogWriteFreq()`, clamped **100 Hz – 60 kHz**, default 1000 Hz |
| Range | `analogWriteRange()`, **default 255 in core 3.x** (it was 1023 in 2.x) |
| Resolution | traded against frequency; the datasheet claims >14 bits at 1 kHz |
| Channels | any GPIO except 16, but each one costs ISR time |
| Jitter | visible whenever Wi-Fi transmits, and not fixable |

For servos, motor control or flicker-free dimming, use an external PWM driver (PCA9685)
or a different chip.

### I2S

One input and one output, with linked-list DMA. Pins: BCK GPIO15, WS GPIO2, DATA GPIO3
(out); BCK GPIO13, WS GPIO14, DATA GPIO12 (in). Its most common non-audio use is driving
WS2812 LED strips glitch-free — the DMA engine is the only thing on this chip that can
meet WS2812 timing while Wi-Fi runs.

### ADC

One channel, `TOUT` (chip pin 6), 10-bit SAR, **0–1.0 V**. Two mutually exclusive modes:

| Mode | `ADC_MODE()` | Reads | Requires |
|---|---|---|---|
| External | `ADC_TOUT` (default) | the voltage on TOUT | `vdd33_const` set to the real supply in the RF init data |
| Supply | `ADC_VCC` | the internal VDD3P3 rail, via `ESP.getVcc()` | TOUT pin left **floating** |

They cannot be used at the same time, and the datasheet's §4.9 is explicit that mixing
them corrupts the RF calibration.

### Other

- **SDIO slave** — 25 MHz v1.1 / 50 MHz v2.0, on the flash bus pins. Unusable on a module
  with in-package flash.
- **IR remote** — GPIO14 TX / GPIO5 RX, NEC coding, 38 kHz carrier, software-driven.

---

## 6. Wi-Fi

| | |
|---|---|
| Standards | 802.11 b/g/n (HT20), 2.4 GHz only |
| Max rate | 72.2 Mbps (MCS7, 400 ns GI) |
| Modes | station, SoftAP, station+AP, promiscuous; 2 virtual interfaces |
| Security | WEP / WPA / WPA2, TKIP / AES |
| TX power | +20 dBm 802.11b, +17 dBm g, +14 dBm n |
| RX sensitivity | −91 dBm (11 Mbps), −75 dBm (54 Mbps), −72 dBm (MCS7) |
| **Bluetooth** | **none.** The ESP8266 has no radio for it |

TLS is the practical constraint, not the radio: a `WiFiClientSecure` session costs 16–24 KB
of a ~50 KB heap. `setBufferSizes(512, 512)` and a single connection at a time is usually
the difference between working and `out of memory`.

---

## 7. Power modes

Chip-level figures from datasheet Table 3-4 and Table 5-3. **None of these apply to a
devkit**, where the LDO, the USB bridge and the power LED dominate — a NodeMCU in deep
sleep draws milliamps, not microamps.

| Mode | What runs | Current |
|---|---|---|
| Active, TX 802.11b @ +17 dBm | everything | 170 mA (**500 mA peak**, ESP-12F datasheet) |
| Active, TX 802.11g @ +15 dBm | everything | 140 mA |
| Active, RX | everything | 50–56 mA |
| Modem-sleep | CPU on, radio off, association kept | 15 mA |
| Light-sleep | CPU paused, wakes on MAC/RTC/GPIO | 0.9 mA |
| Deep-sleep | RTC only | 20 µA |
| Shutdown (CHIP_EN low) | nothing | 0.5 µA |

Operating voltage 2.5–3.6 V (3.3 V typical), average 80 mA.

**Deep sleep limits:** `ESP.deepSleepMax()` ≈ **3.2 h [hw]** (the RTC counter is 32-bit
against a calibrated ~5 µs tick). Longer intervals need a counter in the 512 B of user RTC
RAM and a chain of sleeps. Waking requires the GPIO16 → RST wire.

---

## 8. Memory map (Arduino core, 4 MB module, `4m1m`)

| Region | Address | Size |
|---|---|---|
| Flash, XIP window (code) | `0x40200000` | ~1 MB mapped, **1,044,464 B** available to a sketch |
| Sketch image | `0x00000` | grows up |
| eboot bootloader | `0x00000` | prepended by the build |
| OTA staging | after the sketch | the new image must fit beside the running one |
| LittleFS | top of the 4 MB part | **1,024,000 B [hw]** |
| RF calibration | sector 1020 (`0x3FC000`) | rewritten by the SDK after `erase_flash` |
| System parameters | last 4 sectors | Wi-Fi credentials, SDK state |
| IRAM | `0x40100000` | ~32 KB — ISRs and `IRAM_ATTR` code |
| DRAM | `0x3FFE8000` | ~80 KB — data, BSS, heap, stack |
| RTC RAM | `0x60001200` | 768 B, of which 512 B user |

`ESP.getFreeSketchSpace()` reports the OTA room (≈ 2.8 MB **[hw]** with `4m1m`), which is
*not* the same number as the 1,044,464 B link-time ceiling. A sketch that links fine can
still fail to OTA if it cannot fit twice.

---

## 9. Reset causes

`ESP.getResetReason()` / the ROM's `rst cause:N`:

| N | `getResetReason()` | Meaning |
|---|---|---|
| 0 | Power on | cold start |
| 1 | Hardware Watchdog | the hardware WDT fired (~8 s blocked) |
| 2 | Exception | an unhandled CPU exception — decode the trace |
| 3 | Software Watchdog | `Soft WDT reset`, ~3 s blocked in `loop()` |
| 4 | Software/System restart | `ESP.restart()` |
| 5 | Deep-Sleep Wake | woke via GPIO16 → RST |
| 6 | External System | the RST pin, or the auto-reset circuit |

The ROM's `rst cause:` numbering differs from the SDK's — the ROM prints `2` for an
external reset **[hw]**, where the SDK calls the same event `6`. Read one or the other,
not both interchangeably.
