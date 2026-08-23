# WeAct Studio STM32H7xx Core Board (STM32H750VBT6) — Detailed Board Description

> This document is compiled from the `MiniSTM32H7xx` repository (WeAct Studio):
> schematic `Hardware/STM32H7xx SchDoc V12.pdf` (project "STM32H7XX Board V1.2", dated 2023-07-18),
> the repository README, CubeMX configurations (`.ioc`) and sources of the SDK examples, the OpenMV
> board configs (`SDK/openmv/Ports/…/omv/boards/WeActStudioSTM32H7xx/`),
> and the MCU datasheet `Datasheet/MCU/STM32H750VB_Datasheet.pdf` (DS12556 Rev 4).
> Russian version for the owner: `BOARD_RU.md` / `BOARD_RU.pdf`.
>
> **Provenance convention used below:** every net name, designator and resistor value comes from the
> V1.2 schematic; every clock/DMA/GPIO setting comes from the example that is named next to it.
> Where the schematic and a README disagree, the schematic wins and the discrepancy is called out.

---

## 1. Overview

| Item | Value |
|---|---|
| Product | WeAct Studio "STM32H7xx Core Board", schematic revision V1.2 |
| PCB | 4 layers, TG155, ENIG (immersion gold), lead-free soldering |
| Dimensions | 40.64 × 66.88 mm |
| MCU (default) | **STM32H750VBT6**, LQFP100 (14×14 mm) |
| PCB-compatible MCUs | STM32H743VIT6, STM32H723VGT6, STM32H7B0VBT6 (all LQFP100, same footprint U1) |
| Power supply | 3.3–5.5 V (5V pin, USB VBUS or external); DC-DC up to 1 A |
| Display | 0.96" TFT ST7735S, 80×160, SPI (8-pin FPC, mounts onto the board) |
| Camera | 8-bit DVP port (24-pin 0.5 mm FPC): OV7670 / OV2640 / OV7725 / OV5640(-AF) |
| Memory | 8 MB QSPI Flash (W25Q64, memory-mapped) + 8 MB SPI Flash (W25Q64, on SPI1) |
| Cards | MicroSD (TF) via SDMMC1, 4-bit bus, card-detect on PD4 |
| USB | Type-C 16P: USB 2.0 FS (OTG_FS), device mode (DFU/MSC/HID/UVC); Host power possible via SB6 |
| Firmware | OpenMV support (see `SDK/openmv`), Keil MDK HAL examples (`SDK/HAL/STM32H750` and `SDK/HAL/STM32H743`) |
| Debug | SWD: 4-pin header P3 (3V3, GND, SWDIO, SWCLK) |
| Buttons | K1 (user, PC13), NRST (reset), BOOT0 (enter system bootloader) |
| Indicators | Blue LED (PE3, via digital transistor VT2 PDTC114ET, active-high), red power LED D7 |
| ESD protection | MKESD0402MS05 on USB D+/D−, buttons, SD lines; LESD3Z5.0T1G on VBUS; SD03 TVS on 3V3 |

WeAct states that only genuine ST chips are used. Boards **do ship with a factory test firmware**
(screen test → camera test → ADC test → Flash/TF test, `K1` cycles through the items); what WeAct
does *not* publish is the source/binary of that firmware, to make cloning harder. Flashing anything
else overwrites it and it cannot be restored from this repository.

---

## 2. STM32H750VBT6 Microcontroller — Technical Specifications

Source: DS12556 Rev 4 (in repo: `Datasheet/MCU/STM32H750VB_Datasheet.pdf`). Reference manual: RM0433;
programming manual: PM0253; errata: ES0396 (all in `Datasheet/MCU/`).

### Core and performance
- 32-bit **Arm Cortex-M7**, **double-precision** FPU, L1 cache: 16 KB I-cache + 16 KB D-cache, MPU, DSP instructions
- Frequency **up to 480 MHz**, 1027 DMIPS (2.14 DMIPS/MHz, Dhrystone 2.1)
- Interconnect: 3 bus matrices (1 AXI + 2 AHB), 5 AHB2-APB bridges, 2 AXI2-AHB bridges

### Memory
- **Flash: 128 KB** (internal, at 0x0800 0000)
- **RAM: 1 MB**:
  - ITCM RAM 64 KB @ 0x0000 0000
  - DTCM RAM 128 KB @ 0x2000 0000 (192 KB TCM total)
  - AXI SRAM 512 KB @ 0x2400 0000
  - SRAM1 128 KB + SRAM2 128 KB @ 0x3000 0000 (contiguous; the OpenMV configs treat them as one block)
  - SRAM3 32 KB @ 0x3004 0000
  - SRAM4 64 KB @ 0x3800 0000
  - 4 KB SRAM in the backup domain @ 0x3880 0000
- **QUADSPI** (dual-bank, up to 133 MHz) — W25Q64 (8 MB) attached, memory-mapped window at **0x9000 0000**
- FMC (external memory controller, up to 32-bit bus: SRAM/PSRAM/NOR up to 133 MHz, SDRAM, 8/16-bit NAND) — routable to P1/P2
- CRC unit

### Security and cryptography
- ROP, PC-ROP, active tamper, secure firmware upgrade, secure access mode
- Hardware acceleration: AES 128/192/256, TDES, HASH (MD5, SHA-1, SHA-2), HMAC
- TRNG (true random number generator; used by example `01-GPIO` to randomise the blink)
- 96-bit unique ID (read at 0x1FF1E800 — the address the OpenMV port uses as `OMV_BOARD_UID_ADDR`)

### Communication peripherals (up to 35)
- 4× I2C (FM+, SMBus/PMBus)
- 4× USART + 4× UART (ISO7816, LIN, IrDA, up to 12.5 Mbit/s) + 1× LPUART
- 6× SPI (3 with muxed duplex I2S; 1 I2S in LP domain; up to 150 MHz)
- 4× SAI (serial audio interface)
- SPDIFRX, SWPMI (single-wire protocol master I/F), MDIO slave
- 2× SD/SDIO/MMC (up to 125 MHz)
- 2× CAN FD (1 with TT-CAN)
- 2× USB OTG (1 FS + 1 HS/FS), FS crystal-less (HSI48), LPM, BCD (battery charging detection)
- Ethernet MAC with DMA
- HDMI-CEC
- Camera interface DCMI 8–14 bit, up to 80 MHz

### Analog peripherals
- 3× ADCs, 16-bit, up to 3.6 MSPS, up to 36 channels total (fewer on LQFP100)
- 2× 12-bit DACs (1 MHz)
- 2× ultra-low-power comparators
- 2× operational amplifiers (7.3 MHz bandwidth)
- DFSDM: 8 channels / 4 filters
- Temperature sensor, VREFINT, VBAT/4 internal channels (all three used by example `07-ADC_Test`)
- VREFBUF (internal reference buffer; initialised by `07-ADC_Test`)

### Graphics
- LTDC (LCD-TFT controller, up to XGA) — **not** wired to anything on this board
- Chrom-ART (DMA2D) graphics accelerator
- Hardware JPEG codec (enabled in the OpenMV firmware, `OMV_JPEG_CODEC_ENABLE`)

### Timers (up to 22)
- 1× HRTIM (down to 2.1 ns resolution)
- 2× 32-bit (TIM2, TIM5), up to 240 MHz, 4× IC/OC/PWM, quadrature encoder input
- 2× 16-bit advanced motor-control timers (TIM1, TIM8, up to 240 MHz)
- 10× 16-bit general-purpose timers (up to 240 MHz)
- 5× 16-bit low-power timers (up to 240 MHz)
- 2× watchdogs (independent and window), SysTick, RTC (calendar, sub-seconds)

### Power/clock management
- Supply 1.62–3.6 V; POR/PDR/PVD/BOR; 3 independently gated power domains (D1/D2/D3)
- Embedded core LDO with configurable output; voltage scaling VOS0–VOS3 (480 MHz requires VOS0)
- Backup regulator, VBAT mode with battery charging
- Internal oscillators: HSI 64 MHz, HSI48 48 MHz (USB crystal-less), CSI 4 MHz, LSI 32 kHz
- External (on board): **HSE = 25 MHz (X1)**, **LSE = 32.768 kHz (X2)**
- 3× PLLs (with fractional dividers)
- Low-power modes: Sleep / Stop / Standby / VBAT. Measured on this board: **0.9 mA in Standby at 5 V input** (example `09-PWR_Test`)

### Debug
- SWD and JTAG, 4 KB Embedded Trace Buffer

### GPIO
- Up to 114 I/O ports on LQFP100; **76 of them are brought out to headers P1/P2** (see §5.7)

> Board variants from the README: **STM32H743VIT6** — same core but 2048 KB Flash and 1 MB RAM;
> **STM32H750VBT6** — 128 KB Flash, 1 MB RAM. This repository ships SDKs for both
> (`SDK/HAL/STM32H750`, `SDK/HAL/STM32H743`). On the H750 the working code lives in QSPI/SPI Flash.

---

## 3. Power

### Input and regulation
| Element | Description |
|---|---|
| Input voltage | 3.3–5.5 V: `5V` pin of headers P1/P2 or USB VBUS |
| Diode D10 | B5819WS SL (Schottky 40 V/1 A) — VBUS feed protection (VBUS → 5V rail) |
| DC-DC U2 | **SY8088AAC (1 A, default)**; alternative footprints: XT3410AFMR-G (1.5 A) / TLV62569DBVR (2 A) |
| Inductor L1 | 2.2 µH |
| Feedback divider | R10 680 kΩ (FB→OUT) / R11 150 kΩ (FB→GND) → 0.6 V × (1 + 680/150) ≈ **3.32 V** |
| Capacitors | C19 4.7 µF (input), C20 22 pF (feed-forward/compensation across R10), C21 10 µF (output) |
| D6 | **SD03 TVS on the 3.3 V rail** (output-side clamp) |
| VBUS network | TVS D5 LESD3Z5.0T1G; ESD D8/D9 MKESD0402MS05 on D+/D− |
| Type-C | Connector J1, 16 pins; R13/R14 5.1 kΩ pull-downs on CC1/CC2 (UFP/sink role); R12 5.1 kΩ series resistor of the power LED |
| SB6 | Jumper: feeds 5 V back to the connector VBUS (USB Host peripheral power) |
| Indicator | Red LED D7 (power) |

The 3.3 V output (rails `3V3`/`VDD-MCU`) powers the MCU, display, card socket and both flash chips.

### Camera power (DVP port)

Note the chain: only U5 hangs off 3V3; the 1.5 V core rail is derived **from 2V8**, not from 3V3.

| Element | Description |
|---|---|
| U5 XC6206P282MR | LDO 3.3 → 2.8 V — main rail `2V8` (sensor DOVDD / I-O). C28 1 µF, C29 10 µF, C30 1 µF, C32 100 nF |
| U4 XC6206P152MR | LDO **2.8 → 1.5 V** — rail `DVDD-1V5` (sensor core). Input via FB5, output via FB2. C25 1 µF, C26 10 µF, C27 100 nF |
| U3 XC6206P282MR | LDO 3.3 → 2.8 V — separate branch `AF-2V8` (OV5640 autofocus module supply), reaches J2 only through jumpers **SB4/SB5**. C22 1 µF, C23 10 µF, C24 100 nF |
| FB2, FB3, FB4, FB5 | Ferrite beads 1 kΩ @100 MHz: FB5 = 2V8→U4 in, FB2 = U4 out→DVDD-1V5, FB3 = 2V8→`AVDD-2V8`, FB4 = GND→`AGND` |
| R16, R17 | 4.7 kΩ SCCB pull-ups **to 3V3**: R16 on `DVP_SDA`, R17 on `DVP_SCL` (note the sensor I/O rail is 2.8 V — the pull-ups are to 3.3 V) |
| TP1 | Test point on `OV_STROBE` (J2 pin 1) |

> To use OV5640 autofocus, SB4 and SB5 must be soldered (see README): they connect J2 pin 23 to
> `AF-2V8` and J2 pin 24 to GND.

### VBAT and reset
- **VBAT**: header pin `VBAT_Pin` → FB1 (1 kΩ@100 MHz) → D4 BAT54C dual diode (battery/3V3 switchover) → MCU VBAT, with C7 1 µF + C8 100 nF and R9 0 Ω. **C39 100 nF sits directly at the MCU VBAT pin — this is the "0.1 µF added to VBAT" of revision V1.2.** No battery holder is fitted; a cell must be wired to `VBAT_Pin` (P2 pin 8).
- **Reset chain**: supervisor **U9 MAX809TEUR+T** (3.08 V threshold, active-low, open-drain) → R5 100 kΩ pull-up → R4 1.5 kΩ → `NRST` node, with C1 100 nF to GND, button SW3 and ESD diode D3 on the node.
- **`SYS_RESET`**: `NRST` → R1 330 Ω → `SYS_RESET`, a buffered copy of the reset line that drives **the display reset (J3 pin 3) and the camera reset (J2 pin 6)**. Consequence: both peripherals are reset together with the MCU and there is *no* software control over their RESET pins (which is why the `LCD_RST` / `DCMI_RESET` macros are empty in the BSP and in the OpenMV port).
- **BOOT0**: SW2 button to VDD-MCU + R7 10 kΩ pull-down to GND (idle BOOT0 = 0), ESD D1.
- **User button K1**: SW1 to VDD-MCU → R8 330 Ω → PC13, ESD D2. Examples configure PC13 as input with internal pull-down, so **pressed = logic 1**.
- **Blue LED E3**: PE3 → digital transistor VT2 PDTC114ET (integrated base resistors) → R6 1.5 kΩ → LED. **Active level: HIGH** (`GPIO_PIN_SET` = LED on).

### MCU decoupling
C11/C12 2.2 µF ×2 on VCAP1/VCAP2 (internal core LDO), C13–C17 100 nF + C18 4.7 µF on the VDD pins,
C9 1 µF + C10 100 nF on VDDA; VREF+ routed to header P2 pin 16.

---

## 4. Clocking

| Oscillator | Value | Usage |
|---|---|---|
| X1 (HSE) | **25 MHz**, C2/C3 10 pF load caps, metal can | System PLL1: M=5, N=192, P=2 → **480 MHz** (OpenMV, VOS0) or M=5, N=96 → **240 MHz** (HAL examples) |
| X2 (LSE) | **32.768 kHz**, C5/C6 12 pF | RTC |
| HSI48 | 48 MHz (internal) | USB FS (crystal-less), RNG; also the MCO1 source for the camera XCLK in the HAL examples |

- HAL examples: SYSCLK 240 MHz, HCLK/APB1..4 = 120 MHz, SPI1/2/3 = 240 MHz, SPI4/5/6 = 120 MHz,
  SDMMC = 240 MHz, QSPI = 120 MHz (D1HCLK). ADC in `07-ADC_Test` is clocked from **PLL3 at 75 MHz**.
- OpenMV: PLL1 480 MHz, PLL2 (M=5, N=80) and PLL3 (M=5, N=64) drive QSPI / ADC / SPI123 / SPI45.
- Camera XCLK on **PA8**: `RCC_MCO1` = HSI48 / 4 = **12 MHz** in the HAL examples
  (`HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_4)`), or **TIM1_CH1** at 12 MHz in
  OpenMV (`OMV_CSI_CLK_SOURCE_TIM`).
- **XCLK caveat** (documented in `08-DCMI2LCD/README.md` and `camera.c`): with a long FPC extension
  or high-frame-rate register sets, the MCO1 clock can produce corrupted frames on OV7725/OV2640.
  The workaround is `Camera_XCLK_Set(XCLK_TIM)`, which drives XCLK from **TIM1_CH1**; because that
  collides with the backlight PWM on TIM1_CH2N, the function automatically switches the LCD backlight
  to a **software PWM driven by TIM16**.

---

## 5. MCU Pin Map (LQFP100) — Board Usage

### 5.1 Debug and system pins
| MCU pin | Function | Notes |
|---|---|---|
| PA13 (SWDIO) | SWD data | To header P3 pin 2 through R2 22 Ω |
| PA14 (SWCLK) | SWD clock | To header P3 pin 3 through R3 22 Ω |
| NRST | Reset | NRST button SW3, MAX809T, header P2 pin 10 |
| BOOT0 | Boot selection | BOOT0 button SW2, R7 10 kΩ to GND |
| PA9 / PA10 | USART1 TX/RX | Used by the system bootloader (UART ISP); otherwise free, on P1 pins 27/26 |
| PA11 / PA12 | USB_OTG_FS DM / DP | Type-C connector (D−/D+), ESD protected; also on P1 pins 25/24 |
| PC13 | Button **K1** | Through R8 330 Ω; input with pull-down in the examples (pressed = high) |
| PE3 | Blue **LED** | Through VT2 PDTC114ET; active-high |
| PC14/PC15 | LSE 32.768 kHz | Not on headers |
| PH0/PH1 | HSE 25 MHz | Not on headers |
| PC2_C / PC3_C | Analog-capable (ADC3_INP0 / INP1) | Example `07-ADC_Test`: PC2_C = ADC3_INP0; on P2 pins 13/14 |

**Solder bridges SB1–SB3 (this is the part most often misread):** they are *not* on the SWD lines.
Each one sits in a single peripheral signal so the corresponding MCU pin can be freed:

| Jumper | In the line | Cut it to free |
|---|---|---|
| SB1 | PA7 ↔ `DVP_PWDN` (camera power-down, J2 pin 8) | PA7 |
| SB2 | PD4 ↔ `MicroSD_SW` (card-detect, socket mount pin MH1) | PD4 |
| SB3 | PD6 ↔ `SPIx_CS` (SPI-Flash chip select, U8 pin 1) | PD6 |
| SB4 / SB5 | `AF-2V8` → J2 pin 23 / GND → J2 pin 24 | (solder to *enable* OV5640-AF) |
| SB6 | 5 V rail → Type-C VBUS | (solder to *enable* USB-Host power out) |

SB1–SB3 are necessarily closed from the factory (camera, SD and SPI flash all work out of the box);
SB4/SB5 are open per the README, and SB6 is an opt-in feature added in V1.1 — check the board before
assuming, the schematic does not record the default fitting.

### 5.2 0.96" ST7735S TFT-LCD (SPI4, AF5)
| MCU pin | Function | LCD signal | Notes |
|---|---|---|---|
| PE11 | GPIO Output | **LCD_CS** | Pull-up **R39 100 kΩ** to 3V3 |
| PE13 | GPIO Output | **LCD_WR_RS** (D/C: 0=command, 1=data) | — |
| PE12 | SPI4_SCK (AF5) | **LCD_SCL** | SPI4 master, half-duplex TX (`SPI_DIRECTION_1LINE`), 8-bit, mode 0, prescaler 8 → **15 Mbit/s** @ 120 MHz APB2 |
| PE14 | SPI4_MOSI (AF5) | **LCD_SDA** | — |
| PE10 | TIM1_CH2N (PWM) | **LCD_LED** (backlight) | Through P-MOSFET FET1 SI2301, **active-low** (`TIM_OCNPOLARITY_LOW`); **R37 10 kΩ** gate pull-up to 3V3 keeps the backlight off while the pin floats; R38 22 Ω in series with LEDA |
| — | (`SYS_RESET`) | **LCD_RESET** | **Tied to the board reset net**, not to a GPIO — resets with the MCU, no software control (BSP `LCD_RST` macros are empty) |
| — | — | LCD_VCC / LCD_GND | 3V3 (C37 100 nF) / GND |

Backlight PWM in the examples: TIM1, prescaler 12, period 1000 → **1000 brightness steps**.
The software-PWM fallback uses TIM16 (prescaler 120, period 100).
*Frequency correction:* the vendor README quotes 20 kHz for both, which assumes a 240 MHz timer
kernel clock. With `TIMPRE = 0` (the HAL default — nothing in the SDK or in §13.2 below calls
`__HAL_RCC_TIMCLKPRESCALER`) and an APB2 prescaler of 1, `TIMxCLK = PCLK2 = 120 MHz`, so both
configurations actually run at **10 kHz**. Either rate is far above the eye's flicker threshold;
the number only matters if you are budgeting the timer for something else.

**Connector J3 — FPC 0.5 mm, 8 pins** (plus a mechanical pin 9 tied to GND):

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | LCD_LEDA (backlight anode, via R38 22 Ω) | 5 | LCD_SDA (PE14) |
| 2 | LCD_GND | 6 | LCD_SCL (PE12) |
| 3 | LCD_RESET (`SYS_RESET`) | 7 | LCD_VCC (3V3) |
| 4 | LCD_WR_RS (PE13) | 8 | LCD_CS (PE11) |

Display specs: ST7735S controller (datasheet `Datasheet/ST7735S_V1.5_20150303.pdf`), 80(RGB)×160
pixels, 65K/262K colors, SPI write-only, 3.3 V supply. The BSP (`Drivers/BSP/ST7735`) is the ST
`st7735.c` component driver extended by WeAct: it supports the 0.9" 80×160 and the 1.8" 128×160
panels (`ST7735_0_9_inch_screen` / `ST7735_1_8_inch_screen` / `ST7735_1_8a_inch_screen`), HannStar and
BOE panel variants, RGB/BGR order, RGB444/565/666 pixel formats and the four orientations
(portrait, portrait+180°, landscape, landscape+180°). Keil projects ship separate targets for the
0.96" and 1.8" panels (e.g. `08-DCMI2LCD0_96`, `08-DCMI2LCD1_8`, plus `…_W25Qxx` variants that link
the image for QSPI XIP).

### 5.3 DVP camera (DCMI + I2C1/SCCB)
| MCU pin | Function | FPC signal | Notes |
|---|---|---|---|
| PC6 | DCMI_D0 (AF13) | DVP_D0 | Length-matched ±100 mil (D0–D7) |
| PC7 | DCMI_D1 (AF13) | DVP_D1 | |
| PE0 | DCMI_D2 (AF13) | DVP_D2 | |
| PE1 | DCMI_D3 (AF13) | DVP_D3 | |
| PE4 | DCMI_D4 (AF13) | DVP_D4 | |
| PD3 | DCMI_D5 (AF13) | DVP_D5 | |
| PE5 | DCMI_D6 (AF13) | DVP_D6 | |
| PE6 | DCMI_D7 (AF13) | DVP_D7 | |
| PB7 | DCMI_VSYNC (AF13) | DVP_VSYNC | `DCMI_VSPOLARITY_LOW` in example 05; EXTI line 7 in OpenMV |
| PA4 | DCMI_HSYNC (AF13) | DVP_HSYNC | |
| PA6 | DCMI_PIXCLK (AF13) | DVP_PCLK | `DCMI_PCKPOLARITY_RISING` (examples 05 and 08) |
| PA8 | RCC_MCO1 / TIM1_CH1 | DVP_XCLK | 12 MHz, through R15 33 Ω (see §4) |
| PB8 | I2C1_SCL (AF4) | DVP_SCL | Pull-up R17 4.7 kΩ to 3V3; `I2C1.Timing = 0x40805E8A` (100 kHz) |
| PB9 | I2C1_SDA (AF4) | DVP_SDA | Pull-up R16 4.7 kΩ to 3V3 |
| PA7 | GPIO Output | DVP_PWDN | Through SB1; pull-down R18 10 kΩ (PWDN = 1 → camera powered down) |
| — | (`SYS_RESET`) | DVP_RST | **Tied to the board reset net**, not to a GPIO (see §3) |
| TP1 | — | OV_STROBE | Test point (strobe/FREX of OV sensors), J2 pin 1 |

**Connector J2 — FPC 0.5 mm, 24 pins** (mechanical pins tied to GND). This is the standard
OpenMV-style camera pinout:

| Pin | Signal | Pin | Signal | Pin | Signal |
|---|---|---|---|---|---|
| 1 | OV_STROBE (TP1) | 9 | DVP_HSYNC (PA4) | 17 | DVP_PCLK (PA6) |
| 2 | AGND (via FB4) | 10 | DVDD-1V5 (via FB2) | 18 | DVP_D4 (PE4) |
| 3 | DVP_SDA (PB9) | 11 | 2V8 (I/O rail) | 19 | DVP_D0 (PC6) |
| 4 | AVDD-2V8 (via FB3) | 12 | DVP_D7 (PE6) | 20 | DVP_D3 (PE1) |
| 5 | DVP_SCL (PB8) | 13 | DVP_XCLK (PA8, R15 33 Ω) | 21 | DVP_D1 (PC7) |
| 6 | DVP_RST (`SYS_RESET`) | 14 | DVP_D6 (PE5) | 22 | DVP_D2 (PE0) |
| 7 | DVP_VSYNC (PB7) | 15 | GND | 23 | AF-2V8 **via SB4** |
| 8 | DVP_PWDN (PA7, via SB1) | 16 | DVP_D5 (PD3) | 24 | GND **via SB5** |

SCCB (I2C) device addresses used by the HAL BSP (`Drivers/BSP/Camera/camera.h`, 8-bit form):
**OV7670 0x42, OV7725 0x42, OV2640 0x60, OV5640 0x78**.

Sensors: HAL BSP drivers for OV7670, OV2640, OV7725, OV5640 (autofocus **not** implemented in the
HAL example). OpenMV ≤ V4.4.1: OV2640, OV5640, OV7690, OV7725, OV9650, MT9V034 (`OMV_ENABLE_OV5640_AF = 0`).
OpenMV V4.8.1 port: OV2640, OV5640 (**AF enabled**), OV7725, OV9650, MT9M114, MT9V0xx, Lepton,
PAG7920, PAJ6100, FrogEye2020, plus the FIR sensors MLX90621/90640/90641 and AMG8833.

DMA: DCMI → **DMA1_Stream0** (circular, word) in the HAL examples; the V4.8.1 OpenMV port uses
**DMA2_Stream1** plus MDMA channels 0/1.

### 5.4 QSPI Flash U7 (W25Q64, 8 MB, memory-mapped)
| MCU pin | Function | Series resistor | Notes |
|---|---|---|---|
| PB2 | QUADSPI_CLK | **R34** 33 Ω | |
| PB6 | QUADSPI_BK1_NCS | **R35** 33 Ω | **R36 100 kΩ pull-up to 3V3** on the chip's nCS |
| PD11 | QUADSPI_BK1_IO0 (DI/MOSI) | **R33** 33 Ω | |
| PD12 | QUADSPI_BK1_IO1 (DO/MISO) | **R32** 33 Ω | |
| PE2 | QUADSPI_BK1_IO2 (nWP) | **R31** 33 Ω | |
| PD13 | QUADSPI_BK1_IO3 (nHOLD) | **R30** 33 Ω | |

C35 100 nF decouples U7. Single Bank 1 mode; address window **0x9000 0000** (XIP). All six GPIOs are
configured `GPIO_SPEED_FREQ_VERY_HIGH`. Used by the `02-ExtMem_Boot` bootloader (APP at 0x9000 0000),
by OpenMV's "QSPI Flash" firmware variant and by the `SDK/QSPI_Flasher` algorithms. Compatible chips:
W25Q64JV (Winbond), PY25Q64HA (Puya), and per the flasher README any W25Qxx from 4 MB to 16 MB —
datasheets in `Datasheet/Spi Flash/` (incl. W25Q128JV).

### 5.5 SPI Flash U8 (W25Q64, 8 MB, on SPI1)
| MCU pin | Function | Signal | Notes |
|---|---|---|---|
| PB3 (JTDO) | SPI1_SCK (AF5) | SPI_Flash_CLK | — |
| PB4 (NJTRST) | SPI1_MISO (AF5) | SPI_Flash_MISO | — |
| PD7 | SPI1_MOSI (AF5) | SPI_Flash_MOSI | — |
| PD6 | GPIO Output | **F_CS** (SPI_Flash_CS) | Through **SB3**; pull-up **R40 100 kΩ** to 3V3 (added in V1.2); C36 100 nF decoupling |

> PB3/PB4 are JTAG pins: use SWD (2-wire) so SPI1 stays free. Example: `06-SPIFlash_Test`, which
> exposes **both** flash chips (QSPI + SPI) as **two USB MSC LUNs**. In the OpenMV builds ≤ V4.4.1
> this chip is the Python filesystem storage.

### 5.6 MicroSD U6 (SDMMC1, 4-bit bus)
| MCU pin | Function | Connector signal | Series R | Notes |
|---|---|---|---|---|
| PC8 | SDMMC1_D0 (AF12) | DAT0 (pin 7) | R26 22 Ω | 47 kΩ pull-ups: R19, R20, R21, R28, R29 |
| PC9 | SDMMC1_D1 (AF12) | DAT1 (pin 8) | R27 22 Ω | |
| PC10 | SDMMC1_D2 (AF12) | DAT2 (pin 1) | R22 22 Ω | |
| PC11 | SDMMC1_D3 (AF12) | CD/DAT3 (pin 2) | R23 22 Ω | |
| PC12 | SDMMC1_CK (AF12) | CLK (pin 5) | R25 22 Ω | Length-matched ±100 mil |
| PD2 | SDMMC1_CMD (AF12) | CMD (pin 3) | R24 22 Ω | |
| **PD4** | GPIO Input | **MicroSD_SW** (mount pin MH1) | — | Mechanical card-detect, through **SB2**, 47 kΩ pull-up. Present in hardware but **unused by the stock examples** |

3V3 supply (C33 1 µF + C34 100 nF), ESD D11–D16 (MKESD0402MS05). Example: `04-SD_Test`
(FatFS + USB MSC). From OpenMV V4.8.1 onward the TF card is the *only* Python code storage.

### 5.7 Headers P1/P2 (GPIO) — complete pinout

Two 44-pin (2×22) headers on a 2.54 mm pitch. **76 GPIOs** reach the headers; PA13/PA14 are on P3
only, and PC14/PC15 (LSE) and PH0/PH1 (HSE) are not brought out at all. Odd pins are the
left-hand column of each header in the schematic.

**P1** (power: only GND/5V, at pins 1/2):

| Pin | Net | Pin | Net | | Pin | Net | Pin | Net |
|---|---|---|---|---|---|---|---|---|
| 1 | GND | 2 | 5V | | 23 | PA15 | 24 | PA12 *(USB DP)* |
| 3 | PE1 *(DCMI_D3)* | 4 | PE0 *(DCMI_D2)* | | 25 | PA11 *(USB DM)* | 26 | PA10 *(USART1_RX)* |
| 5 | PB9 *(DVP_SDA)* | 6 | PB8 *(DVP_SCL)* | | 27 | PA9 *(USART1_TX)* | 28 | PA8 *(DVP_XCLK)* |
| 7 | PB7 *(DVP_VSYNC)* | 8 | PB6 *(QSPI_NCS)* | | 29 | PC9 *(SD D1)* | 30 | PC8 *(SD D0)* |
| 9 | PB5 | 10 | PB4 *(SPI1_MISO)* | | 31 | PC7 *(DCMI_D1)* | 32 | PC6 *(DCMI_D0)* |
| 11 | PB3 *(SPI1_SCK)* | 12 | PD7 *(SPI1_MOSI)* | | 33 | PD15 | 34 | PD14 |
| 13 | PD6 *(F_CS)* | 14 | PD5 | | 35 | PD13 *(QSPI_IO3)* | 36 | PD12 *(QSPI_IO1)* |
| 15 | PD4 *(SD detect)* | 16 | PD3 *(DCMI_D5)* | | 37 | PD11 *(QSPI_IO0)* | 38 | PD10 |
| 17 | PD2 *(SD CMD)* | 18 | PD1 | | 39 | PD9 | 40 | PD8 |
| 19 | PD0 | 20 | PC12 *(SD CK)* | | 41 | PB15 | 42 | PB14 |
| 21 | PC11 *(SD D3)* | 22 | PC10 *(SD D2)* | | 43 | PB13 | 44 | PB12 |

**P2** (carries the analog/reference and most power pins):

| Pin | Net | Pin | Net | | Pin | Net | Pin | Net |
|---|---|---|---|---|---|---|---|---|
| 1 | GND | 2 | 3V3 | | 23 | PA6 *(DVP_PCLK)* | 24 | PA7 *(DVP_PWDN)* |
| 3 | PE2 *(QSPI_IO2)* | 4 | PE3 *(LED)* | | 25 | PC4 | 26 | PC5 |
| 5 | PE4 *(DCMI_D4)* | 6 | PE5 *(DCMI_D6)* | | 27 | PB0 | 28 | PB1 |
| 7 | PE6 *(DCMI_D7)* | 8 | **VBAT_Pin** | | 29 | PB2 *(QSPI_CLK)* | 30 | PE7 |
| 9 | PC13 *(K1)* | 10 | **NRST** | | 31 | PE8 | 32 | PE9 |
| 11 | PC0 | 12 | PC1 | | 33 | PE10 *(LCD_LED)* | 34 | PE11 *(LCD_CS)* |
| 13 | PC2_C *(ADC3_INP0)* | 14 | PC3_C | | 35 | PE12 *(LCD_SCL)* | 36 | PE13 *(LCD_RS)* |
| 15 | GND | 16 | **VREF+** | | 37 | PE14 *(LCD_SDA)* | 38 | PE15 |
| 17 | PA0 | 18 | PA1 | | 39 | PB10 | 40 | PB11 |
| 19 | PA2 | 20 | PA3 | | 41 | 3V3 | 42 | 5V |
| 21 | PA4 *(DVP_HSYNC)* | 22 | PA5 | | 43 | GND | 44 | GND |

**Genuinely free GPIOs** (not used by any on-board function) — 35 pins:
`PB5, PD5, PD1, PD0, PA15, PD15, PD14, PD10, PD9, PD8, PB15, PB14, PB13, PB12` on P1 and
`PC0, PC1, PC2_C, PC3_C, PA0, PA1, PA2, PA3, PA5, PC4, PC5, PB0, PB1, PE7, PE8, PE9, PE15, PB10, PB11`
on P2, plus `PA9/PA10` (only claimed by the UART ISP bootloader). Everything else is shared with the
display, camera, SD socket, the two flash chips, USB, LED or K1 — usable, but only if that peripheral
is unused (and for PA7/PD4/PD6 the jumpers SB1/SB2/SB3 can isolate it physically).

For the mechanical placement of each pin see `Hardware/STM32H7xx_BoardShape Board Shape 外形 V12.pdf`
and sheet 1 of the schematic (P1/P2 "Header 22X2").

**The 1–44 pin numbers above are a schematic/datasheet convention, not board silkscreen.** The
physical board prints only the port/pin name next to each row (e.g. "A4", "B0"), with the leading
"P" dropped — nowhere on the PCB does the text "21" or "pin 21" appear. "PA4 = P2 pin 21" is correct
for wiring a schematic but useless for someone reading the physical board with a finger on it; read
off the printed port name instead, and count from a reference mark (e.g. the "K1" button label) if
two rows of similar-looking names are close together — the bottom of P1 ("B15 B14 B13 B12") is easy
to mistake at a glance for the low pins of P2 ("B0 B1"/"B10 B11") if you are not counting from a
fixed point.

### 5.8 Header P3 "SW" (debug)
4 pins, 2.54 mm: pin 1 `3V3 (VDD-MCU)`, pin 2 `SWDIO` (PA13 through R2 22 Ω), pin 3 `SWCLK`
(PA14 through R3 22 Ω), pin 4 `GND`.

---

## 6. Memory Map and Boot

| Region | Address | Size | Purpose |
|---|---|---|---|
| Internal Flash | 0x0800 0000 | 128 KB | Bootloader (OpenMV boot / QSPI bootloader / user code) |
| QSPI (XIP) | 0x9000 0000 | 8 MB | Main application (ExtMem_Boot, OpenMV QSPI variant) |
| ITCM | 0x0000 0000 | 64 KB | OpenMV stack |
| DTCM | 0x2000 0000 | 128 KB | OpenMV flash-filesystem cache |
| AXI SRAM | 0x2400 0000 | 512 KB | DMA buffers, framebuffer (400 KB FB + fb_alloc in OpenMV) |
| SRAM1+2 | 0x3000 0000 | 256 KB | data/bss/heap |
| SRAM3 | 0x3004 0000 | 32 KB | JPEG buffer |
| SRAM4 | 0x3800 0000 | 64 KB | GC block / VoSPI buffer |
| Backup SRAM | 0x3880 0000 | 4 KB | — |

### Entering ISP mode (system bootloader)
1. While powered: hold **BOOT0**, press and release **NRST**, release BOOT0 after ~0.5 s.
2. While unpowered: hold **BOOT0**, apply power, release BOOT0 after 0.5 s.
- **DFU**: connect USB Type-C to a PC (USB FS, PA11/PA12)
- **UART**: connect a USB-UART adapter to **PA9 (TX)/PA10 (RX)**
- Tools: **STM32CubeProg** (DFU/UART), the scripted `Soft/WeAct Studio Download Tool` (§7.3), or SWD
  (ST-Link, J-Link) via P3.
- ST-Link/SWD downloads of large images are size-limited by the 128 KB internal flash; for OpenMV-size
  images WeAct recommends the USB/DFU route.
- The 128 KB internal flash is small, so real projects execute from 0x9000 0000 (QSPI XIP) — see
  examples `02-ExtMem_Boot`, `11-ExtMem_Boot_USB`.

### The QSPI bootloader's own USB modes (`11-ExtMem_Boot_USB`)
- Hold **K1** while powering up/resetting; when the LED starts **blinking slowly**, release K1 →
  the board enumerates as a **USB Custom HID** device ("WeAct HID Flash") and accepts a
  0x9000 0000 image from the `WeAct_HID_Flash` host tool. QSPI clock source is D1HCLK, 120 MHz.
- Keep holding K1 until the LED **blinks fast**, then release → the board jumps to the **system DFU**
  bootloader instead.

---

## 7. SDK, Examples and Tools

### 7.1 HAL examples (`SDK/HAL/STM32H750`, mirrored in `SDK/HAL/STM32H743`)

Both trees contain the same 11 projects; the H743 copies target `STM32H743VITx`. Toolchain:
MDK-ARM (Keil), STM32Cube FW_H7 V1.9.0, SYSCLK 240 MHz in every `.ioc`.

| Example | Contents |
|---|---|
| 01-GPIO | LED on PE3 blinking with **RNG-generated** intervals, K1 (PC13), base init |
| 02-ExtMem_Boot | Bootloader that runs the APP from QSPI (APP @ 0x9000 0000) |
| 03-LCD_Test | ST7735S test (SPI4); separate Keil targets for the 0.96" and 1.8" panels |
| 04-SD_Test | MicroSD (SDMMC1 + FatFS) + USB MSC (1 LUN) |
| 05-DCMI_UVC | DCMI camera → USB **UVC** webcam, **160×120, ≈14 FPS**, OV2640/OV7670 |
| 06-SPIFlash_Test | Both flash chips (QSPI + SPI1) exposed as **two USB MSC LUNs** |
| 07-ADC_Test | ADC3 4-channel scan: **ADC3_INP0 (PC2_C), VREFINT, temperature sensor, VBAT**; 387.5-cycle sampling, ADC clock PLL3 75 MHz, VREFBUF enabled; result on the LCD |
| 08-DCMI2LCD | Camera → LCD (DCMI + DMA1_Stream0, I2C1, SPI4, TIM1 backlight, TIM16 software PWM); OV7725/OV7670/OV2640/OV5640; OV5640 AF not implemented |
| 09-PWR_Test | Standby-mode current test — **0.9 mA at 5 V** |
| 10-FLASH_EraseProgram | Internal flash erase/program |
| 11-ExtMem_Boot_USB | QSPI bootloader + **USB Custom HID** ("WeAct HID Flash") update, with fall-through to system DFU (see §6) |

Most projects ship several Keil targets, e.g. `…0_96` / `…1_8` (panel size) and `…_W25Qxx`
(image linked for QSPI XIP instead of internal flash).

### 7.2 OpenMV (`SDK/openmv`)

**Board ports** (drop-in board folders for the OpenMV tree):
`Ports/Below V4.4.1/…/WeActStudioSTM32H7xx/` (arch string `"WeAct H7xx 1024"`) and
`Ports/…/WeActStudioSTM32H7xx/` for V4.8.1+ (arch string `"WeAct H7 1024"`).

**OpenMV pin naming** (from `SDK/openmv/README.md` / `mpconfigboard.h`):

| Alias | Pin | Alias | Pin | Alias | Pin | Alias | Pin |
|---|---|---|---|---|---|---|---|
| P0 | PB15 | P5 | PB11 | P10 | PD15 | P15 | PA2 |
| P1 | PB14 | P6 | PA5 | P11 | PA13 | P16 | PA3 |
| P2 | PB13 | P7 | PD12 | P12 | PA14 | LED_BLUE | PE3 |
| P3 | PB12 | P8 | PD13 | P13 | PA0 | | |
| P4 | PB10 | P9 | PD14 | P14 | PA1 | | |

(Note P7/P8 alias PD12/PD13, which are also QSPI IO1/IO3 — usable only when the QSPI flash is idle.)

**Firmware variants and where they run:**

| Variant | Layout | Runs on |
|---|---|---|
| `Firmwares/legacy version/Vx.x.x/Internal Flash/` | `bootloader.*` → 0x0800 0000, `firmware.*` → 0x0804 0000, `openmv.*` (merged) → 0x0800 0000; Python files stored in the 8 MB SPI (or QSPI) flash | needs **2 MB internal flash → H743 only** |
| `Firmwares/legacy version/Vx.x.x/QSPI Flash/` | `0x08000000.hex` (128 KB bootloader that maps QSPI to 0x9000 0000) + `0x90000000.bin` (the firmware) | **H750** (and H743) |
| `Firmwares/V4.8.1/openmv_0x08000000.bin` (1.92 MB) + `romfs0.img` | single image at 0x0800 0000 | needs **2 MB internal flash → H743 only** |

> **V4.8.1+ breaking changes** (`Firmwares/README.md`): SPI-Flash and QSPI-Flash storage are **no
> longer supported** — scripts must live on a **TF card** — and the LCD API changed substantially
> (see `Example/lcd_0.96.py`). Combined with the 1.92 MB image size, V4.8.1 is effectively an
> H743-only firmware; H750 boards stay on the ≤ V4.4.1 "QSPI Flash" variant.

Legacy tree also contains `SPI_Flash_Erase_Firmware/` — recovery images
(`SPI_Flash_Erase_0x8000000.hex`, `SPI_Flash_Erase_0x8040000.bin`) that wipe both SPI flash chips when
a broken `main.py` makes the USB drive unmountable; the screen prints
`Please Burn Next Firmware` when done.

Example scripts in `SDK/openmv/Example/`: `Blink.py`, `camera.py`, `lcd_0.96.py`, `uart1_test.py`.

Build (≤ V4.4.1): clone `https://github.com/WeActStudio/openmv.git -b WeActStudio`, init submodules,
then `make TARGET=WeActStudioSTM32H7xx -j` inside `openmv/src`.

Selected board-config values (V4.8.1 port): FB 400 KB + 32 KB streaming buffer + ≥80 KB fb_alloc in
AXI SRAM, GC blocks in SRAM4 (64 KB) and SRAM1 (267 KB), hardware JPEG (quality 50/90), MDMA channels
0/1 for DCMI and 6/7 for JPEG, `PWR_LDO_SUPPLY`, USB via OTG_FS.

### 7.3 Flashing tools shipped in the repo

| Tool | Path | Purpose |
|---|---|---|
| WeAct Studio Download Tool V2.22.0 | `Soft/WeAct Studio Download Tool_V2.22.0.7z` | One-click scripted download over **USB DFU** (`WeAct Studio USB Download Tool.bat`, with `DFU_Driver/STM32Bootloader.bat` for the Windows driver) or over **UART** (`WeAct Studio UART Download Tool.bat`, asks for file + COM port) |
| QSPI flash algorithm (Keil) | `SDK/QSPI_Flasher/STM32H7xx_W25Q128_WeActStudio.FLM` | Copy to `<Keil>\ARM\Flash\`; supports W25Qxx **4 MB–16 MB** |
| QSPI external loader (CubeProg) | `SDK/QSPI_Flasher/STM32H7xx_W25Q128_WeActStudio.stldr` | Copy to `<STM32CubeProgrammer>\bin\ExternalLoader\`; enable it to program 0x9000 0000 over ST-Link. Note CubeProgrammer needs the **arrow next to "Download"** to set a custom address for `.bin` files |
| Loader source | `SDK/QSPI_Flasher/STM32H7xx_W25Q128_Cube_QSPIFlasher-SourceCode.zip` | Sources of the two algorithms above |
| WeAct HID Flash | `SDK/openmv/Firmwares/legacy version/V4.4.1/QSPI Flash/WeAct_HID_Flash*` | Host tool (GUI `.exe`, CLI for Windows and Linux) for the Custom-HID bootloader of §6 |

Setup screenshots: `SDK/QSPI_Flasher/Images/KeilSetup.png`, `STM32CubeProgSetup01/02.png`.

---

## 8. Bill of Key Components

| Ref | Component | Purpose |
|---|---|---|
| U1 | STM32H750VBT6 (LQFP100) | MCU (footprint also fits H743VIT6/H723VGT6/H7B0VBT6) |
| U2 | SY8088AAC (or XT3410AFMR-G / TLV62569DBVR) | 5→3.3 V DC-DC, L1 2.2 µH |
| U3 | XC6206P282MR | 2.8 V LDO — `AF-2V8`, OV5640 autofocus (via SB4/SB5) |
| U4 | XC6206P152MR | 1.5 V LDO — camera core, fed from 2V8 |
| U5 | XC6206P282MR | 2.8 V LDO — main camera I/O rail `2V8` |
| U6 | MicroSD (push-push, card-detect on MH1) | Memory card |
| U7 | W25Q64 (8 MB) | QSPI Flash (R30–R35 33 Ω, R36 100 kΩ, C35) |
| U8 | W25Q64 (8 MB) | SPI Flash (R40 100 kΩ, C36) |
| U9 | MAX809TEUR+T | Reset supervisor (3.08 V), R4 1.5 kΩ, R5 100 kΩ, C1 100 nF |
| J1 | USB Type-C 16P | Power/USB, R13/R14 5.1 kΩ CC pull-downs |
| J2 | FPC 24P 0.5 mm | DVP camera |
| J3 | FPC 8P 0.5 mm | ST7735S display |
| P1, P2 | 2×22, 2.54 mm | GPIO headers (76 GPIOs) |
| P3 | 4-pin, 2.54 mm | SWD |
| X1 / X2 | 25 MHz / 32.768 kHz | HSE / LSE |
| VT2 | PDTC114ET,215 | Blue LED driver (R6 1.5 kΩ) |
| FET1 | SI2301 (P-MOSFET) | LCD backlight switch (R37 10 kΩ gate pull-up, R38 22 Ω) |
| D4 | BAT54C | VBAT switchover (FB1, C7/C8, C39, R9 0 Ω) |
| D5 | LESD3Z5.0T1G | VBUS TVS |
| D6 | SD03 | 3V3-rail TVS |
| D7 | Red LED (R12 5.1 kΩ) | Power indicator |
| D1, D2, D3, D8, D9, D11–D16 | MKESD0402MS05 | ESD on BOOT0/K1/NRST, USB D+/D−, SD lines |
| D10 | B5819WS SL | VBUS feed diode |
| SW1/SW2/SW3 | 3×4×2.5 buttons | K1 / BOOT0 / NRST |
| SB1–SB6 | Solder jumpers | DVP_PWDN (SB1), SD card-detect (SB2), SPI-Flash CS (SB3), OV5640 AF supply (SB4/SB5), USB Host 5 V (SB6) |

---

## 9. Revision History (Hardware/README.md)

- **V1.0** — first version
- **V1.1** — camera supply optimized; header copper pouring improved; USB-C Host power capability (SB6) added; button silkscreen moved
- **V1.2** (current) — 100 nF cap on VBAT (C39); reset moved to a MAX809 supervisor chip; 100 kΩ pull-up on the SPI-Flash CS (R40)

Schematics of the older revisions are kept in `Hardware/legacy version/` (V1.0, V1.1).

---

## 10. Mechanics and Assembly

- 40.64 × 66.88 mm (drawing: `Hardware/STM32H7xx_BoardShape Board Shape 外形 V12.pdf`; STEP 3D model in the same folder, V1.0 model under `legacy version/`)
- Display installation: bend the ribbon 90° down at the red line and 90° up at the blue line (never bend the green line — it breaks the traces), attach with double-sided tape, keep the screen frame off the components; see `Images/ST7735/Install-1/2.png`
- Camera installation: module into FPC socket J2 (`Images/ST7735/Install-3.png`); solder SB4/SB5 for OV5640-AF

## 11. Useful Links Inside the Repository

- Schematic: `Hardware/STM32H7xx SchDoc V12.pdf` (6 sheets: STM32 / POWER / DCMI / MicroSD / QSPI&SPI Flash / TFT-LCD); Altium library: `Hardware/STM32H7xx CoreBoard AltiumDesigner.IntLib`; camera module schematic: `Hardware/OV7725-M12-Camera SchDoc.pdf`
- Datasheets: `Datasheet/MCU/` (DS12556 for H750, H743 datasheet, RM0433, PM0253, errata ES0396), `Datasheet/ST7735S_V1.5_20150303.pdf`, `Datasheet/Spi Flash/` (W25Q64JV, W25Q128JV, PY25Q64HA), `Datasheet/Sensor/` (OV2640, OV5640 + its DVP autofocus application note, OV7725), `Datasheet/SY8088AAC.PDF`, `TLV62569DBVR.PDF`, `XT3410AFMR-G.PDF`, `MKESD0402MS05.PDF`
- Images: board photos `Images/STM32H750VB_1.jpg`, `Images/STM32H750VB_2.jpg`; outline `Images/BoardShape.png`; display/camera mounting `Images/ST7735/`; `Images/Flash Delay.png` (Keil download-delay setting for the QSPI algorithm)


---

# Part II — Working With the Board

> Everything above describes the hardware as WeAct built it. What follows is the practical layer:
> how to stand a project up, which HAL settings are known to work on this board, and the traps that
> cost real debugging time. Sources are this project's own working firmware
> (PlatformIO + STM32Cube HAL, USB CDC + ST7735 + ADC) and the vendor SDK it was derived from.
> Anything not verified on hardware is called out as such.

---

## 12. Development Environment: PlatformIO + STM32Cube HAL

### 12.1 The board definition has flash and RAM swapped

`~/.platformio/platforms/ststm32/boards/weact_mini_h750vbtx.json` ships with:

```json
"upload": { "maximum_ram_size": 131072, "maximum_size": 524288 }
```

Those are backwards. The H750VBT6 has **128 KB of flash** (`maximum_size` should be 131072) and the
linker puts `.data`/`.bss`/heap/stack in the **512 KB AXI SRAM** (`maximum_ram_size` should be
524288). Left uncorrected, PlatformIO prints a nonsense usage percentage and — worse — will not warn
you when an image outgrows the real 128 KB. Override it per project rather than editing the
platform file.

The definition also advertises `"f_cpu": "480000000L"`, which is the silicon's ceiling (at VOS0), not
what any of the HAL examples configure. It has no effect under `framework = stm32cube`.

### 12.2 A known-good `platformio.ini`

```ini
[env:weact_mini_h750vbtx]
platform  = ststm32
board     = weact_mini_h750vbtx
framework = stm32cube

; Correct the swapped sizes from the board definition (§12.1).
board_upload.maximum_size     = 131072    ; 128 KB internal flash
board_upload.maximum_ram_size = 524288    ; 512 KB AXI SRAM (RAM_D1)

; The BSP/Utilities/USB folders of framework-stm32cubeh7 contain their own
; ST7735 component. If you vendor a driver into lib/, they collide at link
; time -- skip the embedded libraries.
board_build.stm32cube.disable_embedded_libs = yes

build_flags =
    -DHSE_VALUE=25000000                  ; X1 is 25 MHz, matches the CMSIS default today but not
                                           ; guaranteed once a different hal_conf.h is dropped in
    -Wall

; The ROM bootloader enumerates as DFU (VID 0483, PID DF11) on the same
; Type-C cable, so no probe is needed. The stock definition omits dfu.
board_upload.protocols = dfu, stlink, jlink, cmsis-dap, blackmagic
upload_protocol        = dfu
debug_tool             = stlink

; If the firmware exposes a USB CDC ACM port, `pio device monitor` works over
; that same cable. The baud rate is ignored on a virtual link; DTR must be
; asserted or most CDC stacks stay silent.
monitor_speed = 115200
monitor_dtr   = 1
```

**Do not rely on the CMSIS default.** As shipped in `framework-stm32cubeh7`, both
`system_stm32h7xx.c` and `stm32h7xx_hal_conf_template.h` unconditionally `#define HSE_VALUE
25000000` — so right now, on this exact package, leaving the flag off happens to work, the same
coincidence the Black Pill skill documents for `framework-stm32cubef4`. It stops being true the
moment a `stm32h7xx_hal_conf.h` generated by CubeMX for a *different* H7 board is dropped in —
most ST Nucleo/Eval H7 boards derive HSE from an 8 MHz ST-Link MCO, not a crystal, and CubeMX bakes
that into the generated header. Whichever `hal_conf.h` wins, `HAL_RCC_GetSysClockFreq()` and
everything derived from it (`SystemCoreClock`, `HAL_Delay`, UART baud dividers, timer periods) go
wrong by a factor of ~3 while the PLL itself still locks — the board runs, just with every timing
silently off. Setting `-DHSE_VALUE=25000000` in `build_flags` removes the dependency on which
header happens to be in the tree.

### 12.3 Project layout that maps cleanly onto CubeMX output

```
include/    board.h  lcd.h  temp_sensor.h  usb_device.h  usbd_conf.h  usbd_cdc_if.h  usbd_desc.h
src/        main.c  stm32h7xx_hal_msp.c  stm32h7xx_it.c  lcd.c  temp_sensor.c  usb*.c
lib/        ST7735/  USBDeviceCDC/        <- vendored drivers, one folder per library
platformio.ini
```

Keep `board.h` as the single place where pin names, port handles and `Error_Handler()` live — it is
the file you copy into the next project. Everything CubeMX would call `MX_*_Init()` keeps that name,
so generated code can be pasted in without translation.

`lib/` subfolders are built as separate static archives by PlatformIO's LDF. A driver dropped there
gets its own include path automatically; no `build_flags` needed.

### 12.4 Everyday commands

```bash
pio run                    # build
pio run -t upload          # build + flash (DFU by default, see §16)
pio run -t clean
pio device monitor         # USB CDC console
pio device list            # find the port / confirm DFU vs CDC enumeration
pio run -t size            # section sizes; watch FLASH against the real 128 KB
```

Other toolchains for the same board: **STM32CubeIDE** (import the vendor `.ioc` from
`SDK/HAL/STM32H750/*/`), or **Keil MDK**, which is what the vendor SDK ships as projects. CubeMX is
still worth running purely to generate clock trees and MSP code — paste the result into the layout
above.

---

## 13. Bring-Up: the Order That Works

### 13.1 `main()` skeleton

```c
int main(void)
{
    CPU_CACHE_Enable();      /* BEFORE HAL_Init -- see §15.1 */
    HAL_Init();              /* SysTick @ 1 ms, NVIC grouping, flash prefetch */
    SystemClock_Config();    /* §13.2 */

    MX_GPIO_Init();          /* clocks + pins first ... */
    MX_SPI4_Init();
    MX_TIM1_Init();
    MX_USB_DEVICE_Init();
    MX_ADC3_Init();

    LCD_Init();              /* ... then the things that talk over them */
    ...
}
```

Two ordering rules that are easy to get wrong:

- **Caches before `HAL_Init()`.** Enabling the D-cache after buffers are already live means the
  cache starts out inconsistent with whatever the DMA or the startup code wrote.
- **`SystemClock_Config()` before any peripheral init.** MSP functions call
  `HAL_RCCEx_PeriphCLKConfig()`, which computes dividers from the current tree.

### 13.2 Clock configuration (verified on this board)

HSE 25 MHz → PLL1 (M=5, N=96, P=2) → **SYSCLK 240 MHz**, AHB /2 → **HCLK 120 MHz**, all APBs /1 →
**120 MHz**. This is the vendor SDK's tree, and it is the conservative choice: 480 MHz is reachable
(M=5, N=192) but needs VOS0 *and* the D-cache tuned, and it doubles the power draw for a board whose
peripherals mostly sit at 120 MHz anyway.

```c
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);      /* the board has no SMPS -- LDO */

    /* VOS0 is only reachable by going through VOS1 first and waiting for
       VOSRDY at each step. Skipping the intermediate step hangs some parts. */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    __HAL_RCC_SYSCFG_CLK_ENABLE();               /* ODEN lives in SYSCFG on H7 */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;      /* X1, 25 MHz crystal */
    RCC_OscInitStruct.HSI48State     = RCC_HSI48_ON;    /* USB FS PHY, §14.6 */
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 5;    /* 25 / 5   = 5 MHz  PLL input   */
    RCC_OscInitStruct.PLL.PLLN       = 96;   /* 5 * 96   = 480 MHz VCO        */
    RCC_OscInitStruct.PLL.PLLP       = 2;    /* 480 / 2  = 240 MHz SYSCLK     */
    RCC_OscInitStruct.PLL.PLLQ       = 2;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1VCIRANGE_2;   /* 4..8 MHz input */
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1VCOWIDE;      /* 192..836 MHz   */
    RCC_OscInitStruct.PLL.PLLFRACN   = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2  |
                                       RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;   /* CPU  240 MHz */
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV2;     /* HCLK 120 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) { Error_Handler(); }
}
```

**`PWR_LDO_SUPPLY` is board-specific and getting it wrong bricks the boot.** The H7 latches its
supply configuration on the first write after reset; declaring an SMPS the board does not have leaves
the core under-supplied and the part stops responding to SWD until a power cycle with BOOT0 held.
This board is LDO-only.

**Flash wait states** (RM0433, `FLASH_LATENCY_x` counts AXI/HCLK wait states):

| VOS | 0 WS | 1 WS | 2 WS | 3 WS |
|---|---|---|---|---|
| VOS0 | ≤ 70 MHz | ≤ 140 MHz | ≤ 210 MHz | ≤ 240 MHz |
| VOS1 | ≤ 70 MHz | ≤ 140 MHz | ≤ 210 MHz | ≤ 225 MHz |
| VOS2 | ≤ 55 MHz | ≤ 110 MHz | ≤ 165 MHz | ≤ 225 MHz |
| VOS3 | ≤ 45 MHz | ≤ 90 MHz | ≤ 135 MHz | ≤ 180 MHz |

At HCLK = 120 MHz, `FLASH_LATENCY_1` is correct. Too few wait states produces hard faults on the
first flash read; too many merely costs performance — when in doubt, go up.

### 13.3 Clock frequencies you will need to quote

| Domain | Frequency | Feeds |
|---|---|---|
| CPU (`SystemCoreClock`, DWT) | 240 MHz | Cortex-M7, SysTick, DWT cycle counter |
| HCLK / AXI | 120 MHz | Bus matrix, flash wait-state budget, DMA |
| PCLK1 / PCLK2 / PCLK3 / PCLK4 | 120 MHz | Peripheral registers |
| TIMxCLK (`TIMPRE = 0`, APB /1) | 120 MHz | TIM1, TIM16 — see the §5.2 correction |
| SPI1/2/3 (PLL1Q default) | up to 240 MHz | SPI flash on SPI1 |
| SPI4/5 (`D2PCLK1`) | 120 MHz | **LCD on SPI4** |
| ADC (PLL3R) | 75 MHz, /4 → 18.75 MHz | ADC3 |
| HSI48 | 48 MHz | USB OTG FS |

Note that `SystemCoreClock` on the H7 is the **CPU** clock (240 MHz), not HCLK. Anything derived from
it — most importantly the DWT cycle counter (§15.4) — ticks at 240 MHz.

---

## 14. On-Board Peripheral Cookbook

### 14.1 LED and button

```c
#define LED_Pin        GPIO_PIN_3    /* PE3, through VT2 -- ACTIVE HIGH */
#define LED_GPIO_Port  GPIOE
#define KEY_Pin        GPIO_PIN_13   /* PC13, through R8 330R          */
#define KEY_GPIO_Port  GPIOC

GPIO_InitStruct.Pin  = KEY_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLDOWN;      /* K1 shorts to VDD, so pull DOWN */
HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

/* pressed == GPIO_PIN_SET  -- the inverse of most dev boards */
```

Both polarities are the opposite of the common convention (active-low LED, pull-up button). The LED
is driven through a PDTC114ET digital transistor and the button pulls *up* to VDD-MCU.

### 14.2 LCD bus: SPI4 half-duplex

Only SCK and MOSI are wired to the panel, so the peripheral must be configured **transmit-only
half-duplex** — `SPI_DIRECTION_2LINES` would drive MISO onto a pin the panel does not have.

```c
hspi4.Instance            = SPI4;
hspi4.Init.Mode           = SPI_MODE_MASTER;
hspi4.Init.Direction      = SPI_DIRECTION_1LINE;   /* half-duplex TX */
hspi4.Init.DataSize       = SPI_DATASIZE_8BIT;
hspi4.Init.CLKPolarity    = SPI_POLARITY_LOW;      /* SPI mode 0 */
hspi4.Init.CLKPhase       = SPI_PHASE_1EDGE;
hspi4.Init.NSS            = SPI_NSS_SOFT;          /* CS is a plain GPIO, PE11 */
hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;   /* 120/4 = 30 MHz */
hspi4.Init.FirstBit       = SPI_FIRSTBIT_MSB;
/* ...the remaining H7-only fields can stay at the CubeMX defaults... */
```

`SPI_BAUDRATEPRESCALER_4` puts the bus at **30 MHz**. The ST7735S datasheet specifies a 66 ns
minimum write cycle (≈15 MHz), so this is out of spec on paper; it works on this board because the
panel is on short on-board traces through an 8-pin FPC. **If pixels ever come out garbled or shifted,
the prescaler is the first thing to halve.** `_8` (15 MHz) is the in-spec, always-safe setting.

D/C and CS are ordinary GPIOs on PE13 and PE11 (`GPIO_SPEED_FREQ_VERY_HIGH`), and the reset line is
**not** software-controllable — it hangs off the board reset net (§3), which is why every ST7735 BSP
you will find for this board has an empty `LCD_RST` macro. The panel therefore resets only when the
MCU does; there is no way to recover a wedged controller except a full board reset.

### 14.3 Backlight: TIM1_CH2N is a *complementary* output

PE10 is `TIM1_CH2N`, not `TIM1_CH2`. Two consequences:

```c
/* 1. Start the complementary output, not the normal one: */
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);        /* NOT HAL_TIM_PWM_Start */

/* 2. HAL_TIMEx_ConfigBreakDeadTime() must be called, and AutomaticOutput /
      break settings must be sane, or MOE never gets set and the pin stays idle. */
```

Also required: `HAL_TIM_MspPostInit()` to configure PE10 as `GPIO_AF1_TIM1`. CubeMX emits this as a
separate function from `HAL_TIM_Base_MspInit()` and it is easy to drop when hand-porting.

With prescaler 12 and period 1000 the carrier is 10 kHz (§5.2 correction) with **1000 usable
brightness steps** — `LCD_SetBrightness()` writes CCR directly, so the full range is `0..1000`.
This project's `LCD_Light(100, 300)` therefore ramps to roughly a tenth of the panel's available
output; if a build looks dim, that is the reason, not the panel.

The gate has a 10 kΩ pull-up (R37) to keep the backlight **off** while PE10 floats, so the screen is
dark from reset until the timer is running — useful, because it hides the garbage frame the
uninitialised panel would otherwise show. Initialise, draw the first frame, *then* ramp the
backlight up.

### 14.4 Making the ST7735 fast — the trap in every stock driver

This is the single biggest performance mistake on this board, and it is inherited from ST's own
component driver.

`ST7735_FillRGBRect()` calls `ST7735_SetCursor()` **once per pixel row**, and `SetCursor()` sends
each address byte as its own CS-framed SPI transaction:

```
SetCursor  = write_reg(CASET) + 2 data + write_reg(RASET) + 2 data + write_reg(RAMWR)
           = 7 separate HAL_SPI_Transmit calls, each with CS and D/C GPIO toggles
```

Drawing one 16-pixel-tall glyph is 16 rows × (7 + 1) ≈ **128 SPI transactions**; a 13-character line
is ~1660. At that point the bus clock is irrelevant — you are paying HAL call overhead and GPIO
toggles, not shifting pixels.

**The fix: compose in RAM, blit once.** The panel's address window is left at full screen by
`Init()`/`FillRect()`, so a single `SetCursor(0, y)` followed by one continuous burst lets the
controller's own address counter walk the rows for you:

```c
/* Full-width scratch band, pixels pre-byte-swapped for the wire. */
static uint16_t strip[160 * 16];

void LCD_StripFlush(uint16_t y, uint16_t height)
{
    ST7735_SetCursor(&st7735_pObj, 0, y);                        /* 7 transactions */
    lcd_io_senddata((uint8_t *)strip, 160u * height * 2u);       /* 1 transaction  */
}
```

A 160×16 text band is 5120 bytes: **8 SPI transactions instead of ~1660**, and at 30 MHz the
transfer itself is ~1.4 ms. That is what makes a 120 Hz UI loop fit — two bands per frame is ~2.7 ms
out of an 8.33 ms budget, with no DMA and therefore no cache-coherency problem (§15.1).

Store the strip **byte-swapped**: the panel wants RGB565 big-endian and the M7 is little-endian, so
swapping at composition time (once per changed pixel) instead of at transmit time lets the buffer go
out as raw bytes.

The bundled font is column-major, 8 rows per byte: for a glyph cell of width `size/2` and height
`size`, pixel `(col,row)` is bit `0x80 >> (row & 7)` of byte `col*2 + (row >> 3)`.

Related: the panel's own refresh rate is programmable via **FRMCTR1 (0xB1)**:

```
f_frame = f_osc / ((RTNA * 2 + 40) * (LINE + FPA + BPA))     f_osc ~= 850 kHz, LINE = 160
```

The stock `(0x01, 0x2C, 0x2D)` gives 42 × 249 ≈ 80 Hz. `(0x00, 0x02, 0x02)` gives 40 × 164 ≈ 130 Hz.
Raising it is only worth doing if you are actually pushing frames that fast, and it is the first
thing to revert if the panel starts to flicker or lose contrast.

Finally, a reality check worth giving anyone who asks for a high frame rate here: this is a small
TN-class panel whose pixel response is on the order of a hundred milliseconds. The MCU can
comfortably feed it at 120 Hz, but the difference between 60 and 120 will be hard to see. An on-screen
fps readout is more convincing than the animation itself.

### 14.5 Internal temperature sensor (ADC3)

The die sensor is `ADC_CHANNEL_TEMPSENSOR` on **ADC3**, which lives in the D3 domain and is clocked
from **PLL3R** (25/10 = 2.5 MHz → VCO 150 MHz → R = 75 MHz), then divided by the ADC's own
prescaler. `ADC_CLOCK_ASYNC_DIV4` → 18.75 MHz, comfortably inside the 50 MHz limit.

Three things make the reading trustworthy:

```c
HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);  /* once, at init */

sConfig.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;   /* the sensor is SLOW -- do not shorten */

/* Read VREFINT too, and scale by it: the result stops depending on how far
   the 3V3 rail actually sits from nominal. */
vdda_mv = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(vrefint_raw, ADC_RESOLUTION_16B);
```

Then interpolate against the factory calibration at `TEMPSENSOR_CAL1_ADDR` / `TEMPSENSOR_CAL2_ADDR`.
**Check `cal2 != cal1` before dividing** — an unprogrammed or blank calibration area reads as equal
values and would otherwise divide by zero.

`__LL_ADC_CALC_TEMPERATURE()` returns whole degrees; scaling the same formula by 10 gives tenths,
which is what you want on a display.

The sensor measures die temperature, so it reads several degrees above ambient and tracks CPU load —
it is a health indicator, not a thermometer.

### 14.6 USB CDC over the same Type-C cable

The FS PHY runs **crystal-less off HSI48** — no external USB crystal is fitted:

```c
PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
PeriphClkInitStruct.UsbClockSelection    = RCC_USBCLKSOURCE_HSI48;
HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

HAL_PWREx_EnableUSBVoltageDetector();     /* required, or the device never enumerates */

/* PA11 = DM, PA12 = DP, GPIO_AF10_OTG2_FS */
```

`HAL_PWREx_EnableUSBVoltageDetector()` is the line that gets forgotten. Without it the USB
transceiver's supply is never validated and the host sees nothing at all — no error, just an
unresponsive port.

Remember `HSI48State = RCC_HSI48_ON` in `SystemClock_Config()` (§13.2), and that `OTG_FS_IRQHandler`
must call `HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS)`.

A CDC ACM port on this cable means `pio device monitor` needs no extra hardware — which is why it is
worth adding early, before you need to debug something. Set `monitor_dtr = 1`.

### 14.7 DAC1 on PA4/PA5, and the AF for other "genuinely free" pins

Neither DAC is used by any on-board function or vendor example — PA4/PA5 (`DAC1_OUT1`/`DAC1_OUT2`,
P2 pins 21/22, both already listed as "genuinely free" in §5.7) are plain analog pins, no AF register
involved. Full working recipe (channel config, TIM6 sample-rate ISR, no DMA) is in
`recipes.md` §13. Two board-independent traps worth knowing before reaching for it:

- Clock-enable macro is **`__HAL_RCC_DAC12_CLK_ENABLE()`**, not `__HAL_RCC_DAC1_CLK_ENABLE` — DAC1
  and DAC2 share one RCC bit.
- On H7, `DAC_ChannelConfTypeDef` has four fields beyond the F4-era struct
  (`DAC_ConnectOnChipPeripheral`, `DAC_UserTrimming`, `DAC_TrimmingValue`, `DAC_SampleAndHoldConfig`)
  that must all be set explicitly or `HAL_DAC_ConfigChannel()` fails its parameter check.

**Alternate-function table for free pins actually used in this project** (this skill does not yet
carry a full AF map for every one of the 35 free GPIOs in §5.7 — only these are build-and-flash
verified):

| Pin | Header | Function used | AF |
|---|---|---|---|
| PA4 | P2 pin 21 | DAC1_OUT1 | analog, no AF |
| PA5 | P2 pin 22 | DAC1_OUT2 | analog, no AF |
| PB0 | P2 pin 27 | TIM3_CH3 | `GPIO_AF2_TIM3` (0x02) |

PB1 (P2 pin 28) should by the same logic be `TIM3_CH4` on `GPIO_AF2_TIM3` too, but this project never
drove it — treat that one entry as unverified until someone builds against it. For any other free
pin, get the AF value from `stm32h7xx_hal_gpio_ex.h` (`GPIO_AFx_yyy` defines) or CubeMX's pinout view
rather than assuming a pattern.

If you add a base timer (TIM6/TIM7) for a periodic tick — as the DAC recipe does — note the shared
vector name: **`TIM6_DAC_IRQn`/`TIM6_DAC_IRQHandler`** (TIM6 shares its vector with DAC underrun
errors), but **`TIM7_IRQn`/`TIM7_IRQHandler`** (ordinary standalone vector). Naming the handler wrong
is a silent failure — no build error, the interrupt just never fires because the weak default handler
catches it instead.

---

## 15. Cortex-M7 / H7 Behaviours That Bite

### 15.1 D-cache and DMA do not agree by default

The M7 has a 16 KB write-back data cache. A DMA engine is a separate bus master that neither reads
nor updates it, so:

- **MCU → DMA (TX):** the data may still be sitting in the cache. Call
  `SCB_CleanDCache_by_Addr()` before starting the transfer.
- **DMA → MCU (RX):** the CPU may serve a stale line from the cache. Call
  `SCB_InvalidateDCache_by_Addr()` after the transfer completes.

Both take **32-byte-aligned** addresses and sizes rounded up to 32 bytes, so DMA buffers should be
declared `__attribute__((aligned(32)))` with a size that is a multiple of 32 — otherwise you will
clean or invalidate a neighbouring variable and corrupt it.

Symptom to recognise: a transfer that works with the D-cache off, works under a debugger with
breakpoints, and produces stale or partial data at full speed.

Your options, in order of preference:

1. **Avoid DMA where blocking is fast enough.** The strip blit of §14.4 moves 5 KB in 1.4 ms with the
   CPU polling — a 120 Hz UI does not need DMA, and skipping it removes the whole class of bug.
2. Put DMA buffers in a **non-cached MPU region** (configure an MPU region as `Device` or
   `Normal, non-cacheable`).
3. Do the clean/invalidate calls by hand, with the alignment rules above.

### 15.2 Which RAM to use for what

The default linker script (`STM32H750VBTX_FLASH.ld`) declares five regions and only populates one:

```
FLASH    0x08000000  128K     code
DTCMRAM  0x20000000  128K     unused by default -- fastest, CPU-only
RAM_D1   0x24000000  512K     .data / .bss / heap / stack  <-- everything lands here
RAM_D2   0x30000000  288K     unused by default
RAM_D3   0x38000000   64K     unused by default
ITCMRAM  0x00000000   64K     unused by default
```

**DTCM and ITCM are not reachable by DMA1/DMA2.** They connect only to the CPU's tightly-coupled
buses. Putting a DMA buffer in DTCM is a classic silent failure: the transfer completes and the
buffer never changes.

- **RAM_D1 (AXI SRAM)** — the default; reachable by every master. Fine for framebuffers.
- **RAM_D2 (SRAM1/2/3)** — the same domain as DMA1/DMA2 and the peripherals; the conventional home
  for DMA buffers, and it keeps them off the AXI bus that the CPU is using.
- **RAM_D3 (SRAM4)** — the D3 domain, reachable by BDMA; useful for low-power/Stop-mode work.
- **DTCM** — zero-wait-state CPU data. Best place for a hot stack or scratch structures. No DMA.
- **ITCM** — zero-wait-state code. Move an interrupt handler here if you need deterministic latency.

To place something explicitly, add an output section to the linker script and tag the variable:

```c
__attribute__((section(".dma_buffer"), aligned(32))) static uint8_t rx[512];
```

### 15.3 128 KB of flash is the real constraint

The H750 is a 480 MHz part with 1 MB of RAM and the flash of a mid-range STM32F1. The bare project in
this repository — HAL, USB device stack, CDC class, ST7735 driver, a font and the application — comes
to roughly 40 KB, so there is room, but not for an RTOS plus a filesystem plus a graphics library.

When you outgrow it, the board's answer is **QSPI XIP**: a small loader in internal flash maps the
8 MB W25Q64 to `0x90000000` and jumps there (vendor examples `02-ExtMem_Boot`,
`11-ExtMem_Boot_USB`). Both the linker script and the flashing procedure change — see §6 and §7.3 —
so decide early rather than porting a finished project.

Watch `pio run -t size` against the corrected 131072 (§12.1), not the board definition's number.

### 15.4 Timing finer than 1 ms: the DWT cycle counter

`HAL_GetTick()` has 1 ms granularity, which cannot express, say, a 120 Hz frame period (8.333 ms).
The M7's DWT cycle counter runs at the **CPU clock (240 MHz)** and costs one register read:

```c
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR          = 0xC5ACCE55U;   /* Cortex-M7 needs this unlock; M3/M4 do not */
    DWT->CYCCNT       = 0;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Fixed-rate loop. Signed comparison, so the 17.9 s wrap costs nothing. */
uint32_t period = SystemCoreClock / 120u;
uint32_t next   = DWT->CYCCNT;
for (;;) {
    while ((int32_t)(DWT->CYCCNT - next) < 0) { }
    next += period;
    if ((int32_t)(DWT->CYCCNT - next) > (int32_t)period) {
        next = DWT->CYCCNT + period;      /* overran badly -- resync */
    }
    render();
}
```

`DWT->LAR = 0xC5ACCE55` is the M7-specific step that is missing from most M3/M4 code you will copy.
It is also the cheapest profiler available: bracket a block with `DWT->CYCCNT` reads and divide by
`SystemCoreClock / 1000000` for microseconds.

### 15.5 Assorted

- `HAL_Delay()` in a loop drifts: it delays *between* jobs rather than pacing them, so a 200 ms
  period built from `HAL_Delay(200)` plus work runs slow. Compare deadlines against `HAL_GetTick()`
  instead, and let the period accumulate (`next += 5000` rather than `next = now`).
- The board has **no software control over the LCD and camera reset lines** (§3). Design around it.
- PB3/PB4 carry SPI1 to the on-board SPI flash and are JTAG pins. Use SWD (2-wire) or lose the flash.
- ADC3 is in the D3 domain: it keeps running in Stop mode, but its clock has to be configured
  separately from D1/D2 peripherals.

### 15.6 `HAL_TIM_Base_MspInit` vs `HAL_TIM_PWM_MspInit` — not board-specific, but easy to hit here

This is a general HAL quirk, not a board fact, but every timer recipe in this skill (§6/§14.3 backlight
PWM, §14.7/recipes.md §13 DAC tick) runs into its shape. A `TIM_HandleTypeDef` has **two separate weak
MSP callbacks**: `HAL_TIM_Base_Init()` calls `HAL_TIM_Base_MspInit()`, while `HAL_TIM_PWM_Init()`
calls a *different* one, `HAL_TIM_PWM_MspInit()`. If the peripheral clock-enable is only placed inside
`HAL_TIM_Base_MspInit` (as in the TIM1 MSP in `recipes.md` §7) and `HAL_TIM_Base_Init()` is called
before `HAL_TIM_PWM_Init()` on the same handle, everything works — the empty weak
`HAL_TIM_PWM_MspInit` stub is harmless. Call `HAL_TIM_PWM_Init()` **without** a preceding
`HAL_TIM_Base_Init()`, and the clock is never enabled — silently: no HAL error, the timer just never
ticks. Always init Base before PWM on the same handle.

---

## 16. Flashing, Debugging, Recovery

### 16.1 DFU over the Type-C cable (no probe needed)

1. Hold **BOOT0**, tap **NRST**, release BOOT0 after ~0.5 s. (Or hold BOOT0 while plugging in.)
2. `pio device list` / `dfu-util -l` should show **0483:DF11**.
3. `pio run -t upload`.

The board does not reset itself into DFU — the sequence is manual every time. If `upload` reports no
DFU device, the board is still running the previous firmware.

**Benign error after 100% download.** `dfu-util` with the STM32 ROM bootloader routinely prints an
error on a perfectly successful flash:

```
Download done.
File downloaded successfully
Submitting leave request...
dfu-util: Error during download get_status
*** [upload] Error 74
```

This is not a failed flash. After reaching 100%, `dfu-util` sends the "leave DFU mode" command; the
MCU resets into the freshly written application immediately and never gets to answer the trailing
`get_status` request that `dfu-util` sends next, so it reports the timeout as an error. Check the
board (screen content, LED behavior, USB re-enumeration) before assuming the upload failed — it
almost certainly didn't.

### 16.2 SWD

Four pins on header **P3**: 3V3, SWDIO (PA13), SWCLK (PA14), GND. ST-Link, J-Link, CMSIS-DAP and
Black Magic Probe are all in the board definition; `debug_tool = stlink` plus `pio debug` gives
breakpoints and a live variable view, which DFU cannot.

For images destined for QSPI (`0x90000000`), STM32CubeProgrammer needs the external loader
`STM32H7xx_W25Q128_WeActStudio.stldr` from `SDK/QSPI_Flasher/` copied into
`<STM32CubeProgrammer>/bin/ExternalLoader/`, and the download address set via the arrow next to the
Download button.

### 16.3 When the board stops responding

- **After a bad `HAL_PWREx_ConfigSupply()`**: power-cycle holding BOOT0. The ROM bootloader runs
  before your code, so DFU still works.
- **After flashing something that hangs immediately**: same — BOOT0 always wins.
- **Factory test firmware**: overwritten on first flash and not published by WeAct. It cannot be
  restored.

### 16.4 Console

A USB CDC ACM port (§14.6) on the same cable is the cheapest observability you will get; `printf`
over it costs nothing in pins. Alternative: USART1 on PA9/PA10, which is also what the ROM
bootloader's UART ISP mode uses.

---

## 17. Pitfalls Quick Reference

| Symptom | Cause | Fix |
|---|---|---|
| Every timing off by ~3×, PLL still locks | a different H7 board's `hal_conf.h` (8 MHz HSE) shadowed the CMSIS 25 MHz default | `-DHSE_VALUE=25000000` in `build_flags`, not relying on either header's default |
| Board dead after first flash, SWD unresponsive | `PWR_SMPS_*` supply configured | This board is **LDO** — `PWR_LDO_SUPPLY`; recover via BOOT0 |
| Hard fault on first flash read after clock switch | Too few flash wait states | `FLASH_LATENCY_1` at HCLK 120 MHz (§13.2) |
| Nonsense flash/RAM percentages; no overflow warning | Board definition has the sizes swapped | Override both in `platformio.ini` (§12.1) |
| Link error: duplicate ST7735 symbols | Cube's embedded BSP collides with `lib/ST7735` | `board_build.stm32cube.disable_embedded_libs = yes` |
| LED logic inverted | PE3 drives a transistor — **active high** | `GPIO_PIN_SET` = on |
| Button never reads pressed | K1 shorts to VDD | Configure `GPIO_PULLDOWN`; pressed = `GPIO_PIN_SET` |
| Backlight stays off | `HAL_TIM_PWM_Start()` on a complementary output | `HAL_TIMEx_PWMN_Start()`, plus `ConfigBreakDeadTime` and `HAL_TIM_MspPostInit()` |
| Backlight dim at "maximum" | CCR range is 0..1000, code caps at 100 | Scale brightness against `Period` (§14.3) |
| LCD unreadably slow, bus clock does not help | `FillRGBRect` sets the cursor per row | Compose in RAM, one blit (§14.4) |
| Pixels garbled/shifted after speeding up SPI | 30 MHz exceeds the ST7735 write-cycle spec | Drop to `SPI_BAUDRATEPRESCALER_8` |
| Colours byte-swapped | Panel is big-endian RGB565 | Swap at composition time |
| Panel wedged, no way to reset it | LCD_RESET is on the board reset net | Reset the whole MCU |
| DMA transfer stale/partial at full speed, fine under a debugger | D-cache vs DMA | Clean/invalidate, 32-byte aligned (§15.1) |
| DMA completes but the buffer never changes | Buffer is in DTCM/ITCM | Move it to RAM_D1 or RAM_D2 (§15.2) |
| USB never enumerates | `HAL_PWREx_EnableUSBVoltageDetector()` missing | Call it in `HAL_PCD_MspInit()` (§14.6) |
| USB CDC silent on the host | DTR not asserted | `monitor_dtr = 1` |
| ADC temperature wildly wrong or a div-by-zero | Blank calibration, or sampling time too short | Check `cal2 != cal1`; 387.5 cycles (§14.5) |
| Periodic job drifts slow | `HAL_Delay()` used as a period | Deadline comparison against `HAL_GetTick()` |
| Sub-millisecond pacing impossible | SysTick is 1 ms | DWT cycle counter at 240 MHz (§15.4) |
| `DWT->CYCCNT` stays zero | M7 needs the lock unlocked | `DWT->LAR = 0xC5ACCE55` (§15.4) |
| `HAL_DAC_ConfigChannel()` fails its parameter check | H7's `DAC_ChannelConfTypeDef` has 4 fields beyond the F4-era struct, left at `{0}` | Set `DAC_ConnectOnChipPeripheral`/`DAC_UserTrimming`/etc. explicitly (§14.7, recipes.md §13) |
| DAC clock-enable macro not found | Guessing `__HAL_RCC_DAC1_CLK_ENABLE` | It's `__HAL_RCC_DAC12_CLK_ENABLE()` — DAC1/DAC2 share one RCC bit (§14.7) |
| A TIM6-based ISR never fires, no error | Vector named `TIM6_IRQHandler` instead of the shared one | `TIM6_DAC_IRQn`/`TIM6_DAC_IRQHandler`; TIM7 alone is the ordinary `TIM7_IRQn` (§14.7) |
| PWM channel silent, no HAL error | `HAL_TIM_PWM_Init()` called without a prior `HAL_TIM_Base_Init()` on the same handle | Always init Base before PWM — two separate MSP callbacks (§15.6) |
| `dfu-util: Error during download get_status` / `Error 74` right after "File downloaded successfully" | MCU resets into the app before answering the trailing `get_status` | Benign — the flash succeeded; verify on the board, not by the exit code (§16.1) |
| Can't find "pin 21" printed on the board | 1–44 numbering is a schematic convention; silkscreen shows only port names, no leading "P" | Read the printed port name (e.g. "A4") and count from a reference mark like "K1" (§5.7) |
