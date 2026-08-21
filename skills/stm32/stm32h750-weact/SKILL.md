---
name: stm32h750-weact
description: Firmware development for the WeAct Studio MiniSTM32H7xx core board (STM32H750VBT6) — its 0.96" ST7735 TFT, DVP camera, QSPI/SPI flash, MicroSD, USB, and the STM32Cube HAL + PlatformIO setup around them. Use when working on this board or any STM32H750/H743 WeAct core board: project setup, platformio.ini, clock/PLL configuration, pin mapping, peripheral bring-up, LCD performance, D-cache/DMA problems, DFU flashing, or debugging why something on the board does not work.
---

# WeAct STM32H750VBT6 core board

Board-specific firmware knowledge. The two reference files hold the detail — read the one you need
rather than guessing, because most of the failure modes below are silent (the board runs, just
wrongly).

- `reference/board-hardware.md` — complete hardware reference: schematic-level pin map, every
  connector, power tree, memory map, SDK/examples inventory, **plus** a full development guide
  (Part II: §12 PlatformIO, §13 clocks, §14 peripheral cookbook, §15 M7 gotchas, §16 flashing,
  §17 pitfall table).
- `reference/recipes.md` — copy-paste-ready code: `platformio.ini`, clock config, LCD driver with the
  fast blit path, backlight PWM, ADC temperature sensor, USB CDC, DWT frame pacing.
- `template/` — a **complete working project** that builds and flashes, plus a scaffolding script.
  It is where every recipe was extracted from; see `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | STM32H750VBT6, Cortex-M7, **128 KB flash**, 1 MB RAM, LQFP100 |
| Clocks | HSE 25 MHz (X1), LSE 32.768 kHz. Working tree: PLL1 M5/N96/P2 → SYSCLK 240, HCLK 120 |
| Display | ST7735S 160×80 on **SPI4** (PE12 SCK, PE14 MOSI, PE11 CS, PE13 D/C, PE10 backlight) |
| LED / button | PE3 **active-high** / PC13 **pressed = high** |
| Storage | 8 MB QSPI flash @ `0x90000000` (XIP), 8 MB SPI flash on SPI1, MicroSD on SDMMC1 |
| USB | OTG_FS on PA11/PA12, crystal-less off HSI48; ROM DFU bootloader on the same Type-C |
| Debug | SWD on header P3 (PA13/PA14) |

## Rules that prevent the expensive mistakes

Check these before writing code — each one produces a failure that looks like something else.

1. **`-DHSE_VALUE=25000000`** in `build_flags`. Without it the PLL still locks but every derived
   timing is wrong by ~3×.
2. **`PWR_LDO_SUPPLY`** — this board has no SMPS. Configuring one leaves the core under-supplied and
   unresponsive to SWD until a BOOT0 power cycle. The setting latches on first write after reset.
3. **Fix the board definition's swapped sizes** in `platformio.ini`:
   `board_upload.maximum_size = 131072` (flash), `board_upload.maximum_ram_size = 524288` (RAM).
   Otherwise nothing warns you when an image exceeds the real 128 KB.
4. **`board_build.stm32cube.disable_embedded_libs = yes`** if you vendor an ST7735 driver into
   `lib/` — Cube's embedded BSP ships its own and they collide at link time.
5. **`FLASH_LATENCY_1`** at HCLK 120 MHz. Too few wait states → hard fault on the first flash read.
6. **Cache enable before `HAL_Init()`**, clock config before any peripheral init.
7. **D-cache vs DMA**: buffers `__attribute__((aligned(32)))`, size a multiple of 32, clean before
   TX / invalidate after RX. And **DMA1/DMA2 cannot reach DTCM or ITCM** — a buffer there silently
   never updates. Default linker puts everything in RAM_D1 (AXI SRAM), which is fine.
8. **Backlight is TIM1_CH2N**, a complementary output: `HAL_TIMEx_PWMN_Start()`, not
   `HAL_TIM_PWM_Start()`, and `HAL_TIMEx_ConfigBreakDeadTime()` + `HAL_TIM_MspPostInit()` are
   mandatory or the pin never drives.
9. **USB needs `HAL_PWREx_EnableUSBVoltageDetector()`** plus `HSI48State = RCC_HSI48_ON`. Missing it
   means the host sees nothing at all.
10. **LCD and camera reset lines are tied to the board reset net** — no GPIO control, ever. A wedged
    panel needs a full MCU reset.

## When the task is LCD performance

The stock ST7735 driver is ~200× slower than the bus allows: `ST7735_FillRGBRect()` calls
`SetCursor()` per pixel row, and `SetCursor()` sends each address byte as its own CS-framed SPI
transaction. A 13-character line costs ~1660 SPI transactions.

Fix by composing a full-width band in RAM (pixels pre-byte-swapped — the panel is big-endian RGB565)
and pushing it with one `SetCursor` + one burst: 8 transactions, ~1.4 ms for 160×16 at 30 MHz. That
is what makes a 120 Hz UI loop fit. `reference/recipes.md` has the working implementation.

Bus speed is secondary but real: `SPI_BAUDRATEPRESCALER_4` = 30 MHz works on this board although the
ST7735S spec implies 15 MHz; halve it the moment pixels come out garbled.

Panel refresh itself is programmable via FRMCTR1 (`f = f_osc / ((RTNA*2+40) * (LINE+FPA+BPA))`,
f_osc ≈ 850 kHz, LINE = 160): stock ≈ 80 Hz, `(0, 2, 2)` ≈ 130 Hz. Worth setting an expectation
though — pixel response on this panel is ~100 ms, so an fps readout is more convincing than the
animation.

## Starting a new project

Do not hand-assemble one. `template/` is a verified-working project — scaffold from it:

```sh
~/.claude/skills/stm32h750-weact/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--full` (default) — ST7735 UI, USB CDC console, ADC temperature sensor. 40.9 KB of 128 KB flash.
- `--minimal` — LED + K1 only, same clock tree and startup order. 5.7 KB. Use it to prove the
  toolchain and the flashing route before adding anything.

Both variants build as-is (verified). Nothing is generated and no paths are embedded, so copying the
tree by hand works equally well. `template/README.md` lists which files belong to which subsystem, so
a `--full` scaffold can be stripped back cleanly.

When the user already has a project, prefer bringing it in line with the template's
`platformio.ini` and `board.h` over rewriting their code.

## Flashing

DFU over the Type-C cable, no probe: hold **BOOT0**, tap **NRST**, release after ~0.5 s → the board
enumerates as `0483:DF11` → `pio run -t upload`. The board never resets itself into DFU; the sequence
is manual every time. SWD via header P3 when you need breakpoints.

When 128 KB of flash runs out, the board's answer is QSPI XIP at `0x90000000` with a small loader in
internal flash — a decision to make early, since the linker script and flashing procedure both
change. See `reference/board-hardware.md` §6, §7.3, §15.3.

## Reporting

State honestly what was verified on hardware versus derived from the datasheet. Several settings here
(30 MHz SPI, raised FRMCTR1) are deliberately outside published specs and work on this specific
board — say so rather than presenting them as safe defaults.
