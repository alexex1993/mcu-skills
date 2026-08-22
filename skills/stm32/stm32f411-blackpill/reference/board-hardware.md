# WeAct "Black Pill" (STM32F411CEU6) — Detailed Board Description

Part I is the hardware: what the silicon has and what the board maker wired to it.
Part II is how to actually work with it — environment, clocks, peripherals, flashing, pitfalls.

Sources, so you can tell how much to trust each number:

| Claim class | Source | Trust |
|---|---|---|
| MCU pin table, alternate functions, electrical limits, calibration addresses | `DS10314 Rev 8` (STM32F411xC/xE datasheet) | authoritative |
| Peripheral register behaviour | `RM0383` (STM32F411xC/E reference manual) | authoritative, not reproduced here |
| Clock tree, RTC, USB CDC, DFU, PC13 polarity | measured on a WeAct V3.x board | verified |
| K1 on PA0, SPI-flash footprint pins, header order | published WeAct V3.x pinout, **no schematic was consulted** | probable — check before betting a PCB on it |

---

## 1. Overview

The "Black Pill" is WeAct Studio's MiniSTM32F4x1: a 53 × 21 mm, 40-pin DIP-format core board built
around an STM32F411CEU6 in UFQFPN48. Clones exist under other names (DFRobot DFR0864 among them) and
are pin-compatible with the WeAct design.

| | |
|---|---|
| MCU | STM32F411CEU6 — Cortex-M4F @ up to 100 MHz, 512 KB flash, 128 KB SRAM, UFQFPN48 |
| Crystals | 25 MHz HSE (PH0/PH1), 32.768 kHz LSE (PC14/PC15) — **both fitted from the factory** |
| USB | USB-C, wired to USB_OTG_FS on PA11/PA12. No USB-serial chip on the board |
| Power | 5 V from USB-C or the `5V` header pin → on-board LDO → 3V3 |
| LED | PC13, **anode to 3V3** → low = lit |
| Buttons | NRST, BOOT0, K1 (user) |
| Debug | 4-pin SWD header: 3V3, PA13 (SWDIO), PA14 (SWCLK), GND |
| Expansion | two 20-pin rows, 0.1" pitch, 22.86 mm apart |
| Extra | unpopulated SOIC-8 footprint on the underside for an SPI flash (W25Qxx) on SPI1 |

The "V2.0" that PlatformIO's board name reports and the "V3.0/V3.1" silkscreen refer to different
things (board revision vs. the PlatformIO definition's age). Electrically, every revision that has a
USB-C connector and three buttons behaves the same for everything in this document.

### Why this board and not the F401 version

WeAct sells the same PCB with an STM32F401CCU6 (256 KB flash, 64 KB RAM, 84 MHz max). The differences
that break code moved between them:

| | F401CCU6 | **F411CEU6** |
|---|---|---|
| Max SYSCLK | 84 MHz | 100 MHz |
| Flash / SRAM | 256 KB / 64 KB | 512 KB / 128 KB |
| Internal temp sensor channel | **ADC_IN16** | **ADC_IN18**, shared with VBAT |
| SPI/I2S count | 4 | 5 (adds SPI5) |
| Batch Acquisition Mode | no | yes |

The temperature-sensor channel is the one that bites: an F401 snippet compiles fine on an F411 and
returns a plausible-looking wrong number. See §14.4.

---

## 2. STM32F411CEU6 — Technical Specifications

### Core and performance
- Arm Cortex-M4 with single-precision FPU, DSP instruction set, MPU.
- 100 MHz max, 125 DMIPS / 1.25 DMIPS/MHz (Dhrystone 2.1).
- ART Accelerator: zero-wait-state execution from flash via instruction cache + prefetch.
- Batch Acquisition Mode (BAM): keeps peripherals and DMA running with the core stopped.

### Memory
- 512 KB flash at `0x0800 0000`.
- 128 KB SRAM at `0x2000 0000`, bit-banded.
- System memory (ROM bootloader) at `0x1FFF 0000`, option bytes at `0x1FFF C000`.
- No external memory interface (no FSMC/FMC, no QSPI) — what is on the die is all there is.

### Peripherals (UFQFPN48 package)
| Block | Count / note |
|---|---|
| GPIO | 36 usable I/O; PC13/14/15 are special (§5.1) |
| ADC | one 12-bit ADC1, **10 external channels** in this package (PA0–PA7, PB0, PB1) + VREFINT (ch17) + temp/VBAT (ch18) |
| DAC | **none** |
| Timers | TIM1 (advanced, 16-bit, complementary outputs), TIM2/TIM5 (32-bit), TIM3/TIM4 (16-bit), TIM9/10/11 (16-bit), IWDG, WWDG, SysTick |
| USART | USART1, USART6 (APB2, up to 12.5 Mbit/s), USART2 (APB1, up to 6.25 Mbit/s) |
| SPI / I2S | SPI1–SPI5; SPI1/4/5 up to 50 Mbit/s, SPI2/3 up to 25 Mbit/s. I2S1–I2S5, I2S2/I2S3 full-duplex |
| I2C | I2C1, I2C2, I2C3 — standard/fast, up to 1 MHz |
| USB | one USB_OTG_FS with an integrated transceiver, device/host/OTG, 4 bidirectional endpoints, 320 × 35-bit FIFO |
| SDIO | one, 1/4/8-bit, up to 50 MHz — pins are mostly not brought out usefully in this package |
| DMA | DMA1 and DMA2, 8 streams each, with FIFOs |
| Other | CRC unit, 96-bit unique ID, RTC + 20 backup registers, PLLI2S audio PLL |

### Power and clocks
- VDD 1.7–3.6 V, VBAT 1.65–3.6 V, VDDA tied to VDD on this package (`VDDA/VREF+` is a single pin 9).
- Internal regulator, always ON in UFQFPN48 (no BYPASS_REG, no PDR_ON pin) — one 2.2 µF cap on VCAP_1.
- Voltage scaling: Scale 3 ≤ 64 MHz, Scale 2 ≤ 84 MHz, **Scale 1 ≤ 100 MHz**.
- HSE 4–26 MHz, HSI 16 MHz ±1%, LSE 32.768 kHz, LSI ~32 kHz.
- Main PLL: input 0.95–2.10 MHz, VCO 100–432 MHz, P output 24–100 MHz, Q output 48–75 MHz.
- Sleep / Stop / Standby low-power modes; RTC and backup registers run from VBAT.

---

## 3. Power

```
USB-C VBUS 5 V ─┬─► [Schottky] ─┬─► 5V header pin
                │               │
                │               └─► LDO (3.3 V, ~500 mA class) ─► 3V3 rail ─► VDD, VDDA
5V header pin ──┘
                                         VBAT header pin ─► VBAT (backup domain)
```

- Powering from the `5V` pin and from USB-C simultaneously is what the on-board diode is there to
  make survivable, but it is not a supply-sharing design — pick one.
- The `3V3` header pin is the LDO output; feeding a regulated 3.3 V into it bypasses the LDO and is
  the normal way to run the board from a battery. The 5 V pin then carries nothing.
- **VDDA is the ADC's reference.** There is no separate VREF+ pin on UFQFPN48, so full scale is
  whatever the LDO puts out — typically 3.25–3.30 V, not exactly 3.3 V. Correct for it with VREFINT
  (§14.4) rather than assuming.
- VBAT is broken out (`VB` on the silkscreen). Fit a coin cell or supercap there and the RTC and the
  20 backup registers survive VDD loss. With nothing attached, VBAT is tied to VDD internally on the
  board and the RTC stops when power does.

---

## 4. Clocking

| Source | Where | Used for |
|---|---|---|
| HSE 25 MHz crystal | PH0 (OSC_IN) / PH1 (OSC_OUT) | PLL input → SYSCLK and USB |
| LSE 32.768 kHz crystal | PC14 (OSC32_IN) / PC15 (OSC32_OUT) | RTC |
| HSI 16 MHz RC | internal | boot clock, fallback |
| LSI ~32 kHz RC | internal | IWDG, low-accuracy RTC |

Out of reset the chip runs on HSI at 16 MHz. Everything below has to be configured.

### The working clock tree

```
HSE 25 MHz ──/M=25──► 1 MHz ──×N=192──► VCO 192 MHz ──┬──/P=2──► SYSCLK 96 MHz
                                                       └──/Q=4──► 48 MHz  → USB_OTG_FS

SYSCLK 96 ──/1──► HCLK 96 MHz ──┬──/2──► PCLK1 48 MHz  (APB1 timers ×2 = 96 MHz)
                                └──/1──► PCLK2 96 MHz  (APB2 timers    = 96 MHz)
Flash latency: 3 wait states
Voltage scaling: Scale 1
```

Every divider in that tree is inside spec: PLL input 1 MHz (0.95–2.10), VCO 192 MHz (100–432),
P output 96 MHz (24–100), Q output 48 MHz (48–75), PCLK1 48 MHz (≤ 50), PCLK2 96 MHz (≤ 100).

### Why 96 MHz and not 100

USB_OTG_FS needs exactly 48 MHz, and it comes off PLLQ — the same VCO as PLLP. For 100 MHz SYSCLK
you would need a VCO of 200 (`/P=2`) or 400 (`/P=4`) MHz; neither divides by an integer Q into 48.
So on this board, **USB and 100 MHz are mutually exclusive**. 96 MHz is the standard answer, and it
is what CubeMX picks for this part too.

If you genuinely need 100 MHz and no USB: PLLM=25, PLLN=200, PLLP=2. Q is then meaningless.

### Running without the crystal

`RCC_OSCILLATORTYPE_HSI` with `PLLSource = RCC_PLLSOURCE_HSI` and `PLLM = 16` gives the same
1 MHz PLL input and hence the same 96/48 MHz. It works — but the HSI's ±1% is far outside USB
full-speed's ±0.25% requirement, so enumeration becomes flaky-to-impossible over temperature. Use it
only for boards with a damaged crystal, and only without USB.

### Flash wait states

At VDD 2.7–3.6 V the flash tolerates 30 MHz per wait state (datasheet Table 15):

| HCLK | Wait states |
|---|---|
| ≤ 30 MHz | `FLASH_LATENCY_0` |
| ≤ 60 MHz | `FLASH_LATENCY_1` |
| ≤ 90 MHz | `FLASH_LATENCY_2` |
| ≤ 100 MHz | `FLASH_LATENCY_3` |

96 MHz → 3. Set too few and the first instruction fetch past the boundary hard-faults.

---

## 5. MCU Pin Map (UFQFPN48) — Board Usage

Pin numbers are the UFQFPN48 column of datasheet Table 8. "FT" = 5 V-tolerant, "TC" = 3.3 V only.

### 5.1 Committed pins — do not reuse without knowing what breaks

| Pin | Signal | Board use | Notes |
|---|---|---|---|
| 2 | PC13 | **user LED** | anode to 3V3 → LOW = lit. Behind the backup power switch: max 3 mA, ≤ 2 MHz, ≤ 30 pF load, **never as a current source** |
| 3 | PC14 | LSE crystal (OSC32_IN) | unusable as GPIO while the crystal is fitted |
| 4 | PC15 | LSE crystal (OSC32_OUT) | same |
| 5 | PH0 | HSE crystal (OSC_IN) | 25 MHz |
| 6 | PH1 | HSE crystal (OSC_OUT) | 25 MHz |
| 7 | NRST | reset button | |
| 10 | PA0-WKUP | **user key K1** | shorts to GND when pressed → enable the internal pull-up. TC pin: **3.3 V only, not 5 V tolerant**. Also WKUP1 and ADC1_0 |
| 32 | PA11 | USB_FS_DM | USB-C data, AF10 |
| 33 | PA12 | USB_FS_DP | USB-C data, AF10 |
| 34 | PA13 | SWDIO | SWD header |
| 37 | PA14 | SWCLK | SWD header |
| 44 | BOOT0 | BOOT0 button | dedicated pin, not a GPIO. Input range 0–9 V. Also the VPP pin for factory programming |
| 1 | VBAT | `VB` header pin | backup domain supply |
| 8 | VSSA/VREF− | — | analog ground |
| 9 | VDDA/VREF+ | — | **combined pin: ADC reference is the 3V3 rail** |
| 22 | VCAP_1 | 2.2 µF cap | regulator stabilisation, not user-accessible |

Also worth knowing: **PB2 is BOOT1**. It has a pull-down on the board, so BOOT0-high selects the
system bootloader as intended. If PB2 has been driven or pulled high by an attached circuit, holding
BOOT0 boots from SRAM instead and no DFU device appears — a genuinely confusing failure.

### 5.2 SPI flash footprint (underside, U4)

An unpopulated SOIC-8 pad accepts a W25Qxx-class SPI flash, wired to **SPI1**:

| SPI1 signal | Pin | AF |
|---|---|---|
| NSS (chip select) | PA4 | GPIO, driven manually |
| SCK | PA5 | AF5 |
| MISO | PA6 | AF5 |
| MOSI | PA7 | AF5 |

Those four pins are also on the header. Soldering a chip there costs you PA4–PA7 for anything else,
including ADC1_4…ADC1_7. Unpopulated, they are ordinary GPIO. *(Wiring per the published WeAct
pinout; not verified against a schematic here.)*

### 5.3 Free I/O and what each pin can be

Everything not in §5.1. Selected alternate functions from datasheet Table 9 — this is the table to
consult before assigning a peripheral, because on a 48-pin part the useful combinations run out fast.

| Pin | # | 5 V tol. | ADC | Timers | SPI / I2S | USART | I2C | Other |
|---|---|---|---|---|---|---|---|---|
| PA0 | 10 | **no (TC)** | IN0 | TIM2_CH1/ETR, TIM5_CH1 | — | USART2_CTS | — | WKUP1, **K1** |
| PA1 | 11 | yes | IN1 | TIM2_CH2, TIM5_CH2 | SPI4_MOSI | USART2_RTS | — | |
| PA2 | 12 | yes | IN2 | TIM2_CH3, TIM5_CH3, TIM9_CH1 | I2S2_CKIN | **USART2_TX** | — | |
| PA3 | 13 | yes | IN3 | TIM2_CH4, TIM5_CH4, TIM9_CH2 | I2S2_MCK | **USART2_RX** | — | |
| PA4 | 14 | yes | IN4 | — | SPI1_NSS, SPI3_NSS | USART2_CK | — | flash CS |
| PA5 | 15 | yes | IN5 | TIM2_CH1/ETR | **SPI1_SCK** | — | — | flash SCK |
| PA6 | 16 | yes | IN6 | TIM1_BKIN, TIM3_CH1 | **SPI1_MISO** | — | — | flash MISO |
| PA7 | 17 | yes | IN7 | TIM1_CH1N, TIM3_CH2 | **SPI1_MOSI** | — | — | flash MOSI |
| PB0 | 18 | yes | IN8 | TIM1_CH2N, TIM3_CH3 | SPI5_SCK | — | — | |
| PB1 | 19 | yes | IN9 | TIM1_CH3N, TIM3_CH4 | SPI5_NSS | — | — | |
| PB2 | 20 | yes | — | — | — | — | — | **BOOT1** |
| PB10 | 21 | yes | — | TIM2_CH3 | SPI2_SCK, I2S3_MCK | — | **I2C2_SCL** | |
| PB12 | 25 | yes | — | TIM1_BKIN | SPI2_NSS, SPI4_NSS, SPI3_SCK | — | I2C2_SMBA | |
| PB13 | 26 | yes | — | TIM1_CH1N | SPI2_SCK, SPI4_SCK | — | — | |
| PB14 | 27 | yes | — | TIM1_CH2N | SPI2_MISO | — | — | |
| PB15 | 28 | yes | — | TIM1_CH3N | SPI2_MOSI | — | — | RTC_50Hz, RTC_REFIN |
| PA8 | 29 | yes | — | TIM1_CH1 | — | USART1_CK | **I2C3_SCL** | **MCO_1** |
| PA9 | 30 | yes | — | TIM1_CH2 | — | **USART1_TX** | I2C3_SMBA | OTG_FS_VBUS |
| PA10 | 31 | yes | — | TIM1_CH3 | SPI5_MOSI | **USART1_RX** | — | USB_FS_ID |
| PA15 | 38 | yes | — | TIM2_CH1/ETR | SPI1_NSS, SPI3_NSS | USART1_TX | — | JTDI |
| PB3 | 39 | yes | — | TIM2_CH2 | SPI1_SCK, SPI3_SCK | USART1_RX | I2C2_SDA | **JTDO-SWO** |
| PB4 | 40 | yes | — | TIM3_CH1 | SPI1_MISO, SPI3_MISO | — | I2C3_SDA | JTRST |
| PB5 | 41 | **no (TC)** | — | TIM3_CH2 | SPI1_MOSI, SPI3_MOSI | — | I2C1_SMBA | |
| PB6 | 42 | yes | — | TIM4_CH1 | — | USART1_TX | **I2C1_SCL** | |
| PB7 | 43 | yes | — | TIM4_CH2 | — | USART1_RX | **I2C1_SDA** | |
| PB8 | 45 | yes | — | TIM4_CH3, TIM10_CH1 | SPI5_MOSI | — | I2C1_SCL, I2C3_SDA | |
| PB9 | 46 | yes | — | TIM4_CH4, TIM11_CH1 | SPI2_NSS | — | I2C1_SDA, I2C2_SDA | |

Two pins on this package are **not** 5 V tolerant: **PA0** and **PB5**. Everything else that is a
GPIO is FT — but only while it is not in analog or oscillator mode.

**PA9 is dual-purpose.** It is `USART1_TX` (AF7) and also the USB core's `OTG_FS_VBUS` sensing input.
Those coexist only because the reference firmware sets `vbus_sensing_enable = DISABLE`; turn VBUS
sensing on and PA9 must be wired to VBUS or the device never enumerates.

**PB3 is SWO.** Leaving it as SWO gives you `printf` over SWD trace without spending a UART; taking
it as GPIO costs you that.

### 5.4 Practical peripheral assignments

Combinations that do not fight anything committed in §5.1:

- **I2C1** on PB6/PB7 (SCL/SDA) — the conventional choice; PB8/PB9 is the same peripheral remapped.
- **I2C2** on PB10 (SCL) + PB3 or PB9 (SDA) — note PB11 does not exist on this package, so the
  "natural" PB10/PB11 pair from bigger F4s is not available.
- **USART1** on PA9/PA10 — the pins the ROM bootloader also uses, so an attached USB-UART doubles as
  a recovery path.
- **USART2** on PA2/PA3.
- **SPI2** on PB13/PB14/PB15 (SCK/MISO/MOSI) — leaves SPI1 free for the flash footprint.
- **PWM**: TIM3 on PA6/PA7/PB0/PB1, TIM4 on PB6–PB9, TIM1 (with complementary outputs) on PA8–PA10.

---

## 6. Memory Map and Boot

| Region | Address | Size |
|---|---|---|
| Flash (user) | `0x0800 0000` | 512 KB |
| SRAM | `0x2000 0000` | 128 KB, bit-banded via `0x2200 0000` |
| System memory (ROM bootloader) | `0x1FFF 0000` | 30 KB |
| Option bytes | `0x1FFF C000` | 16 B |
| Flash size register | `0x1FFF 7A22` (`FLASHSIZE_BASE`) | 2 B, in KB |
| Unique device ID | `0x1FFF 7A10` (`UID_BASE`) | 12 B |
| VREFIN_CAL | `0x1FFF 7A2A` | 2 B |
| TS_CAL1 (30 °C) | `0x1FFF 7A2C` | 2 B |
| TS_CAL2 (110 °C) | `0x1FFF 7A2E` | 2 B |
| Peripherals | `0x4000 0000` | APB1 / APB2 / AHB1 / AHB2 |
| USB OTG FS | `0x5000 0000` | AHB2 |

Flash sector layout (matters for erase granularity and for hand-rolled EEPROM emulation):
sectors 0–3 are 16 KB, sector 4 is 64 KB, sectors 5–7 are 128 KB.

### Boot modes

| BOOT0 | BOOT1 (PB2) | Boots from |
|---|---|---|
| 0 | x | user flash at `0x0800 0000` |
| 1 | 0 | **system memory — the ROM bootloader** |
| 1 | 1 | embedded SRAM |

The ROM bootloader (AN2606) accepts firmware over USART1 (PA9/PA10), USART2 (PD5/6 — absent on this
package), **USB DFU (PA11/PA12)**, I2C1/2/3, and SPI1/2/3. On this board the USB path is the one that
matters: it is the same connector you already have plugged in.

---

## 7. SDK and Tooling

There is no board SDK to speak of — the Black Pill is a bare core board. What exists:

- **WeAct's repository** (`WeActStudio/WeActStudio.MiniSTM32F4x1`) — schematics, a hardware manual,
  and CubeIDE examples. The PlatformIO board definition links to it.
- **STM32CubeF4** — the HAL and CMSIS, which PlatformIO installs as `framework-stm32cubef4`. It does
  **not** include the USB device middleware; that has to be vendored (§14.5).
- **STM32CubeMX** — useful for generating a first `SystemClock_Config()` and MSP files even if you
  then build with PlatformIO. Pick the *STM32F411CEUx* part, not the board.
- **STM32CubeProgrammer** (`STM32_Programmer_CLI`) — ST's own flashing tool; an alternative to
  dfu-util if you want option-byte access or read-protection control.
- **dfu-util** — what PlatformIO actually invokes. Shipped as `tool-dfuutil`.

---

# Part II — Working With the Board

## 12. Development Environment: PlatformIO + STM32Cube HAL

### 12.1 The board definition is correct — for once

`board = blackpill_f411ce` reports 512 KB flash and 128 KB RAM, picks
`STM32F411CEUX_FLASH.ld`, and lists `dfu` among its upload protocols. Nothing needs overriding for
sizes or linker script. Two things are worth changing:

- `upload_protocol` defaults to `stlink`; set it to `dfu` unless you actually have a probe.
- `debug_tool` defaults to `blackmagic`; set it to `stlink` if that is what is on your SWD header.

### 12.2 A known-good `platformio.ini`

See `../template/platformio.ini`, reproduced in `recipes.md` §1. The one non-obvious line:

```ini
build_flags = -DHSE_VALUE=25000000
```

This is *not* strictly required — `stm32f4xx_hal_conf_template.h` in CubeF4 happens to default
`HSE_VALUE` to 25 MHz, which is exactly this board's crystal. That is a coincidence of ST's template
being written around a Discovery board with the same crystal, and it silently becomes wrong the
moment you supply your own `stm32f4xx_hal_conf.h` (CubeMX writes 8 MHz for many targets) or the
framework package changes. Set it explicitly; the cost is one line and the failure mode it prevents
is every `HAL_RCC_Get*Freq()`, every UART baud rate and every timer period being off by 3.125×.

### 12.3 Project layout

```
platformio.ini
include/           board.h and per-module headers  (on the include path)
src/               main.c and the modules
lib/USBDevice/     vendored ST USB Device Library — PlatformIO builds each lib/ subdir as a library
```

`lib/<name>/` subdirectories are picked up automatically by the Library Dependency Finder; there is
no `library.json` to write for a plain source drop.

### 12.4 Everyday commands

```sh
pio run                       # build
pio run -t upload             # flash over DFU (board must already be in DFU)
pio run -t clean
pio device list               # find the CDC port
pio device monitor            # USB CDC console; Ctrl-C to leave
pio run -t size               # per-section breakdown
pio debug                     # SWD, needs debug_tool and a probe
```

---

## 13. Bring-Up: the Order That Works

```c
int main(void)
{
    HAL_Init();                 /* SysTick at 1 ms, NVIC grouping, flash prefetch */
    SystemClock_Config();       /* before ANY peripheral init                     */
    /* ... GPIO, then peripherals ... */
}
```

Three ordering rules, each with a failure that looks like something else:

1. **`SystemClock_Config()` before peripheral init.** A UART initialised at 16 MHz HSI and then
   switched to 96 MHz keeps its old baud divisor: output becomes garbage, with no error.
2. **`__HAL_RCC_<PORT>_CLK_ENABLE()` before `HAL_GPIO_Init()` on that port.** Without it the writes
   land in a dead peripheral: the pin never changes and nothing reports a problem.
3. **Set the output level before switching a pin to output.** Otherwise the pin glitches through
   whatever was in ODR — visible as a flash of the LED, and worse on a reset line.

And the one that is specific to PlatformIO rather than to the chip:

4. **You must provide `SysTick_Handler()`.** `framework-stm32cube` ships no `stm32f4xx_it.c`. The
   startup file's weak `SysTick_Handler` is `B SysTick_Handler` — an infinite loop. Without your own:

   ```c
   void SysTick_Handler(void) { HAL_IncTick(); }
   ```

   the board boots, runs to the first `HAL_Delay()` or `HAL_GetTick()` comparison, and hangs there.
   The symptom — "it stops right after startup, but SWD says the core is running" — reads like a
   clock problem and is not.

---

## 14. On-Board Peripheral Cookbook

### 14.1 LED and button

```c
#define LED_Pin        GPIO_PIN_13   /* PC13, anode to 3V3 -> LOW = lit */
#define LED_GPIO_Port  GPIOC
#define KEY_Pin        GPIO_PIN_0    /* PA0, shorts to GND -> LOW = pressed */
#define KEY_GPIO_Port  GPIOA
```

PC13 is behind the backup-domain power switch, which sinks at most 3 mA (datasheet Table 8, note 2).
Configure it `GPIO_SPEED_FREQ_LOW`; the datasheet caps it at 2 MHz with a 30 pF load. Driving
anything but the LED from it — a transistor base, another board's input — is out of spec, and it must
never *source* current. If you need a second output, use any PB pin instead.

K1 needs `GPIO_PULLUP`: not every board revision fits an external pull-up, and a floating input reads
as noise. PA0 is also `WKUP1`, so the same button can wake the chip from Standby.

### 14.2 RTC on the LSE crystal

The crystal is fitted, so the RTC can be accurate rather than approximate. Three things trip people:

**Backup-domain write protection.** `RCC->BDCR` and the RTC registers are read-only until:

```c
__HAL_RCC_PWR_CLK_ENABLE();
HAL_PWR_EnableBkUpAccess();
```

Skip either and `HAL_RCC_OscConfig()` for the LSE appears to succeed while the clock never starts.

**LSE startup is slow.** A 32.768 kHz crystal takes on the order of *seconds* to stabilise (datasheet
Table 38: typical 2 s). The HAL's `LSE_STARTUP_TIMEOUT` is 5000 ms, so a cold start can genuinely
spend a couple of seconds inside `HAL_RCC_OscConfig()`. That is not a hang.

**Reading the calendar.** On the F4 the time and date registers are shadow copies, unlocked only
after *both* have been read. `HAL_RTC_GetTime()` **must** come before `HAL_RTC_GetDate()`. Reversed,
the date silently freezes at whatever it held when the shadow was last unlocked — a bug that only
shows up at midnight.

Prescalers: with 32768 Hz, `AsynchPrediv = 127` and `SynchPrediv = 255` give exactly 1 Hz, and
`SynchPrediv` also sets the subsecond resolution (1/256 s ≈ 3.9 ms). `SubSeconds` counts **down** from
`SecondFraction` to 0, so milliseconds are
`(SecondFraction - SubSeconds) * 1000 / (SecondFraction + 1)`.

Use one of the 20 backup registers as a "clock already set" flag; they survive system reset and
Standby, and are cleared only when the backup domain loses power.

### 14.3 USB CDC — the only serial port this board has

There is no CH340/CP2102 on the Black Pill. Every `printf` route is one of:

| Route | Cost | Notes |
|---|---|---|
| **USB CDC** | ~14 KB flash, the USB peripheral, PA11/PA12 | same cable as power and flashing |
| USART1 on PA9/PA10 | two pins + an external USB-UART dongle | also the ROM bootloader's UART |
| SWO on PB3 | one pin + an SWD probe that supports trace | no extra cable |
| Semihosting | none | very slow, halts on every write, needs a debugger attached |

For the USB path, see §14.5 and `recipes.md` §7–9. The clock requirement is absolute: PLLQ must
produce exactly 48 MHz.

### 14.4 Internal temperature sensor and VREFINT

**On the F411 the temperature sensor is ADC channel 18, not 16.** Channel 16 is where it lives on the
F401/F405/F407, and that is what most Black Pill tutorials show, because most of them were written
for the F401 version of this board. Worse, channel 18 on the F411 is **shared with the VBAT divider**
— only one of `TSVREFE` and `VBATE` may be enabled at a time. Enable VBAT sensing and your
"temperature" is VBAT/4.

The HAL handles the mutual exclusion for you: `ADC_CHANNEL_TEMPSENSOR` on this part is
`ADC_CHANNEL_18 | ADC_CHANNEL_DIFFERENCIATION_TEMPSENSOR_VBAT`, and that marker bit makes
`HAL_ADC_ConfigChannel()` clear VBATE and set TSVREFE. So: use `ADC_CHANNEL_TEMPSENSOR`, and never
call `HAL_ADCEx_EnableVBAT()` in the same firmware.

**Use the factory calibration, not the typical constants.** The datasheet's `V25 = 0.76 V` and
`2.5 mV/°C` are typical values, and §3.30 says plainly that the uncalibrated sensor is only good for
detecting *changes*. Two per-chip points are burned into system memory:

| Symbol | Address | Meaning |
|---|---|---|
| `VREFIN_CAL` | `0x1FFF 7A2A` | raw VREFINT at 30 °C, VDDA = 3.3 V |
| `TS_CAL1` | `0x1FFF 7A2C` | raw sensor at 30 °C, VDDA = 3.3 V |
| `TS_CAL2` | `0x1FFF 7A2E` | raw sensor at 110 °C, VDDA = 3.3 V |

VREFINT (channel 17) is a fixed ~1.21 V, so `VDDA = 3300 * VREFIN_CAL / raw_vrefint` recovers the
actual rail — worth doing on this board specifically, because VDDA *is* the ADC reference and it is
an unremarkable LDO output, not a precision reference. Then rescale the sensor reading to 3.3 V
before interpolating between the two cal points. Full code in `recipes.md` §6.

**Sampling time.** Both internal channels are high-impedance: the datasheet asks for ≥ 10 µs
(Tables 71 and 74). At a 24 MHz ADC clock that is `ADC_SAMPLETIME_480CYCLES` (20 µs). The 3-cycle
default is 0.125 µs and returns noise. This is the single most common reason an internal-channel
reading looks random.

**ADC clock.** PCLK2 is 96 MHz and the ADC maximum is 36 MHz, so `ADC_CLOCK_SYNC_PCLK_DIV4` (24 MHz)
is the fastest legal prescaler. DIV2 would be 48 MHz — out of spec, and it degrades accuracy before
it fails outright.

### 14.5 The USB device stack has to be vendored

`framework-stm32cubef4` ships the HAL, CMSIS and the low-level drivers. It does **not** ship
`Middlewares/ST/STM32_USB_Device_Library`. There is no package flag that adds it. So a USB project
needs four files copied into `lib/USBDevice/` — `usbd_core.c`, `usbd_ctlreq.c`, `usbd_ioreq.c` and
`usbd_cdc.c` with their headers — from STM32CubeF4 or a CubeMX-generated project. `template/`
carries them.

Then five files are yours to write (CubeMX generates equivalents):

| File | Holds |
|---|---|
| `usbd_conf.h` | interface counts, `USBD_malloc` → static allocator |
| `usbd_conf.c` | `HAL_PCD_MspInit` (PA11/PA12 AF10, clock, NVIC), all `HAL_PCD_*Callback`s, `USBD_LL_*`, `OTG_FS_IRQHandler`, the FIFO split |
| `usbd_desc.c` | device and string descriptors, serial number from the die UID |
| `usbd_cdc_if.c` | line coding, DTR tracking, RX/TX |
| `usb_device.c` | `USBD_Init` + `USBD_RegisterClass` + `USBD_Start` |

Three traps in that set:

- **`USBD_malloc`.** ST's default is `malloc()`, and the stock linker script reserves `0x200` of
  heap. The allocation fails and the device never enumerates — with no error path anywhere. Either
  raise `_Min_Heap_Size` to `0x600` in a custom linker script, or (better) point `USBD_malloc` at a
  static buffer. `template/` does the latter.
- **`pClassDataCmsit[0]` vs `pClassData`.** Current ST releases renamed the field; older tutorials
  use the old name and will not compile. Same pointer.
- **The FIFO split.** The core has 320 32-bit words. `HAL_PCDEx_SetRxFiFo(hpcd, 0x80)` plus
  `SetTxFiFo(0, 0x40)` and `SetTxFiFo(1, 0x80)` is a working CDC allocation. Undersize the RX FIFO
  and you lose data under load rather than getting an error.

### 14.6 Timing finer than 1 ms

SysTick gives you 1 ms. For µs resolution, the DWT cycle counter runs at HCLK:

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
/* elapsed_us = DWT->CYCCNT / (HAL_RCC_GetHCLKFreq() / 1000000) */
```

It wraps every 2^32 / 96 MHz ≈ 44.7 s; unsigned subtraction handles the wrap correctly as long as
the interval you are measuring is shorter than that. Note it needs `TRCENA`, which some debuggers
also toggle — do not assume it survives a debug session.

---

## 15. Cortex-M4 / F4 Behaviours That Bite

### 15.1 No data cache — which removes a whole class of problems

Unlike the M7 parts (H7, F7), the F411 has no D-cache. DMA buffers need no cache maintenance, no
32-byte alignment, no `SCB_CleanDCache_by_Addr()`. The ART Accelerator is an instruction cache only
and is transparent. Code ported *from* an H7 can drop all of that; code ported *to* one cannot.

### 15.2 The FPU is single-precision only

`float` is hardware, `double` is software. A stray `3.14` literal (a `double`) or a `printf("%f")`
(which promotes to `double`) drags in the soft-float library and turns a 20-cycle operation into
several hundred. Write `3.14f`, and prefer integer or fixed-point formatting in log output — the
reference firmware prints deci-degrees as `%ld.%ld` for exactly this reason.

### 15.3 512 KB of flash is not the constraint; 128 KB of SRAM might be

The reference firmware uses 3.8% of flash. With no external memory interface on this part, RAM is the
resource to watch: 128 KB, all of it contiguous at `0x2000 0000`, with the stack growing down from
the top and `_Min_Stack_Size = 0x400` as the linker's floor. Large frame buffers or audio buffers are
where this board runs out.

### 15.4 Assorted

- **PA13/PA14 are SWD.** Reconfigure them as GPIO in early startup and you lock yourself out — the
  probe can no longer attach after reset. Recovery is BOOT0 + DFU (§16.3).
- **`HAL_Delay(1)` can return in as little as ~0 ms** — it counts tick boundaries, not elapsed time.
  `HAL_Delay(2)` is the minimum for "at least 1 ms".
- **`__HAL_RCC_*_CLK_ENABLE()` has a read-back delay** built into the macro. Do not "optimise" it by
  writing `RCC->AHB1ENR |= ...` directly; the very next register access can be lost.
- **APB1 is capped at 50 MHz.** With HCLK 96 that means `RCC_HCLK_DIV2`. Timer clocks on a divided
  APB are doubled, so APB1 timers still see 96 MHz — a factor of 2 that is easy to get wrong in a
  period calculation.
- **Bit-banding** covers the whole 128 KB of SRAM (alias at `0x2200 0000`) and the peripheral space
  (alias at `0x4200 0000`). Useful for atomic single-bit flags without disabling interrupts.

---

## 16. Flashing, Debugging, Recovery

### 16.1 DFU over the USB-C cable (no probe needed)

1. Hold **BOOT0**.
2. Tap **NRST**.
3. Release BOOT0 (after ~0.5 s).
4. The board enumerates as `0483:DF11` ("STM32 BOOTLOADER").
5. `pio run -t upload`.

PlatformIO invokes:

```
dfu-util -d 0x0483:0xDF11 -a 0 -s 0x08000000:leave -D .pio/build/.../firmware.bin
```

`:leave` exits DFU and starts the application, so no reset is needed *after* flashing. Only entering
DFU is manual, every time — the board has no auto-reset circuit.

Check it enumerated before blaming the upload:

```sh
dfu-util -l                        # any platform
system_profiler SPUSBDataType | grep -i -A4 bootloader   # macOS
lsusb | grep 0483:df11             # Linux
```

On Linux, add a udev rule for `0483:df11` or run the upload as root. On Windows, DFU needs the
WinUSB/libusb driver bound to the device (Zadig) — the ST DFU driver will not work with dfu-util.

### 16.2 SWD

Four pins on the short header: 3V3, PA13 (SWDIO), PA14 (SWCLK), GND. An ST-Link V2 clone is enough.

```ini
debug_tool = stlink
upload_protocol = stlink     ; if you would rather flash this way too
```

SWD is the only way to get breakpoints, and the only way to recover from firmware that breaks USB.

### 16.3 When the board stops responding

Ordered by how much it costs you:

1. **Firmware wedged, DFU still reachable** — hold BOOT0, tap NRST. The ROM bootloader runs before
   any of your code, so this works no matter what the application does.
2. **Bad firmware runs before you can flash** (reconfigures SWD, or crashes hard) — same thing. BOOT0
   bypasses user flash entirely.
3. **Wipe it** — `dfu-util -d 0x0483:0xDF11 -a 0 -s 0x08000000:mass-erase:force -D firmware.bin`.
4. **No DFU device appears** — check BOOT1/PB2 is not being held high by attached hardware (§5.1); a
   high BOOT1 with BOOT0 high boots from SRAM, where nothing is loaded.
5. **Still nothing** — try the USART1 bootloader on PA9/PA10 with an external USB-UART, or attach
   SWD and use STM32CubeProgrammer under reset.
6. **Read protection engaged** — `STM32_Programmer_CLI -c port=usb1 -rdu` clears RDP, which
   mass-erases the flash.

### 16.4 Console

`pio device monitor` after flashing the full variant. The reference firmware watches DTR
(`monitor_dtr = 1`) and reprints its banner whenever a monitor attaches, so you do not have to catch
the first two seconds after reset.

---

## 17. Pitfalls Quick Reference

| Symptom | Cause | Fix |
|---|---|---|
| Boots, then hangs immediately; core still running under SWD | no `SysTick_Handler`; the startup file's weak one loops forever | define `void SysTick_Handler(void) { HAL_IncTick(); }` |
| Hard fault on the first instruction after the clock switch | flash wait states too low for 96 MHz | `FLASH_LATENCY_3` |
| `HAL_RCC_ClockConfig()` fails or the chip is unstable above 84 MHz | voltage scale left at default | `__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1)` after enabling the PWR clock |
| USB never enumerates, no error anywhere | PLLQ ≠ 48 MHz (e.g. SYSCLK set to 100 MHz) | PLLM 25 / PLLN 192 / PLLP 2 / **PLLQ 4** → 96 MHz + 48 MHz |
| USB enumerates on the bench, drops out warm or cold | running the PLL off HSI instead of the 25 MHz crystal | HSI is ±1%, USB FS needs ±0.25% — use HSE |
| USB device never appears, `USBD_Init` returned OK | `USBD_malloc` = `malloc`, heap is 0x200 | static allocator, or `_Min_Heap_Size = 0x600` |
| Compile error on `pClassData` | field renamed to `pClassDataCmsit[0]` in current ST releases | use the new name |
| Every baud rate / timer period off by ~3× | `HSE_VALUE` is 8 MHz somewhere (custom `hal_conf.h` from another board) | `-DHSE_VALUE=25000000` in `build_flags` |
| LED does nothing; the pin toggles under the debugger | driving PC13 high expecting "on" | anode is on 3V3 — LOW lights it |
| K1 reads pressed at random | no pull-up | `GPIO_PULLUP` on PA0 |
| Date stops advancing at midnight but time is right | `HAL_RTC_GetDate()` called before `HAL_RTC_GetTime()` | Get**Time** first, always |
| RTC never ticks; LSE config "succeeded" | backup domain still write-protected | `__HAL_RCC_PWR_CLK_ENABLE()` + `HAL_PWR_EnableBkUpAccess()` first |
| Startup pauses ~2 s before the first output | LSE crystal stabilising | expected; datasheet Table 38 |
| Temperature reads ~3× too high, or is nonsense | code copied from an F401 example uses ADC_IN16; or VBAT sensing is on (shared ch18) | `ADC_CHANNEL_TEMPSENSOR`, and never `HAL_ADCEx_EnableVBAT()` |
| Any internal-channel ADC reading is noise | 3-cycle sampling time on a high-impedance source | `ADC_SAMPLETIME_480CYCLES` (≥ 10 µs) |
| ADC readings drift with USB load / supply | VDDA is the reference and it is just the LDO output | correct with VREFINT + `VREFIN_CAL` |
| Probe cannot attach after flashing | firmware reconfigured PA13/PA14 | BOOT0 + DFU mass-erase |
| Holding BOOT0 produces no DFU device | BOOT1/PB2 pulled high by attached hardware → SRAM boot | free PB2, or let its board pull-down win |
| GPIO writes do nothing at all | port clock not enabled | `__HAL_RCC_GPIOx_CLK_ENABLE()` before `HAL_GPIO_Init()` |
| Float maths unexpectedly slow | `double` literals or `printf("%f")` on a single-precision FPU | `3.14f`, and format integers |
