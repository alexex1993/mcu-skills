# STM32F401RCT6 "Black Pill" (WeAct HID bootloader) — hardware and development reference

Part I is the hardware and the bootloader that ships on it. Part II is how to work with it:
PlatformIO, clocks, flashing, recovery, pitfalls.

How much to trust each claim:

| Claim class | Source | Trust |
|---|---|---|
| DEVID 0x423, 256 KB, bootloader V1.2, HID flashing, app hand-off, 84 MHz from HSE, PC13 blink, KEY on PA0 active-low, USB CDC console, 1200-baud / DR19 bootloader entry | measured on the skill author's board (unbranded F401RCT6 clone) | **verified** |
| HSE = 25 MHz | inferred: the bootloader's PLL (M=25) only produces a valid 48 MHz USB clock from 25 MHz, and it enumerates; the app's 84 MHz tree from the same crystal runs | verified by consequence |
| Bootloader behaviour: entry conditions, KEY gestures, UID lock, protocol | disassembly of WeAct's **V1.2** `WeAct_HID_Bootloader_F4x1.axf` (with symbols, from the MiniSTM32F4x1 repo history) — the version on the board; V1.1 source for comparison. Protocol, KEY entry and DR19 entry confirmed on hardware | code-derived; marked items verified |
| MCU pinout, alternate functions, limits | ST DS10086 (STM32F401xB/C datasheet), RM0368 | authoritative |
| 40-pin header order, LED polarity, LSE, power circuit | the WeAct MiniF4 layout this clone copies — **no schematic exists for the clone** | unverified — read the silkscreen |

---

# Part I — hardware

## 1. What this board is

A 40-pin (2 × 20, 0.1") board in the WeAct "Black Pill" / MiniF4 format, but populated with an
**STM32F401RCT6 in LQFP64** instead of WeAct's 48-pin STM32F401CCU6 / F411CEU6, and carrying no
brand on the silkscreen. It ships with the **WeAct Studio HID Bootloader V1.2** in the first flash
sector, so out of the box it enumerates as a driverless HID device rather than running a demo.

WeAct itself sells no F401RC Black Pill (its 64-pin F4 board is a different, larger product); treat
the board as a clone that reused WeAct's footprint and bootloader. That matters in two places: the
**header order** is copied, not guaranteed, and **nothing** about the board's power circuit is
documented.

Identify an unknown board in one command, without writing anything — put it in the bootloader
(§6.1) and run:

```sh
python scripts/weact_hid_flash.py --info
# > bootloader: V1.2 0x423 ROM: 256KB
```

`0x423` = STM32F401xB/C, `0x433` = F401xD/E, `0x431` = F411xC/E. The package (RC vs CC) is not
readable in software; it is printed on the chip.

## 2. MCU — STM32F401RCT6

| | |
|---|---|
| Core | Cortex-M4 with FPU, **84 MHz max** |
| Flash | 256 KB: sectors 0–3 = 16 KB, sector 4 = 64 KB, sector 5 = 128 KB |
| SRAM | 64 KB at 0x20000000 (no CCM) |
| Package | LQFP64: PA0–PA15, PB0–PB10, PB12–PB15, PC0–PC15, PD2, PH0/PH1. **No PB11** (its pin is VCAP_1) |
| ADC | ADC1, 12-bit, 16 external channels on LQFP64 (PA0–PA7, PB0–PB1, PC0–PC5); temperature sensor on **ADC_IN16**, VREFINT ADC_IN17, VBAT ADC_IN18 |
| Timers | TIM1 (advanced), TIM2/TIM5 (32-bit), TIM3/TIM4, TIM9/10/11 |
| Serial | USART1/2/6, I2C1–3, SPI1–4 (SPI2/3 with I2S), SDIO, USB OTG FS |
| No | DAC, CAN, Ethernet, RNG, CCM RAM |
| Factory calibration | VREFINT_CAL 0x1FFF7A2A, TS_CAL1 (30 °C) 0x1FFF7A2C, TS_CAL2 (110 °C) 0x1FFF7A2E; flash size 0x1FFF7A22; UID 0x1FFF7A10 |

Code written for the 48-pin F401CC Black Pill runs unchanged: same die (DEVID 0x423), same flash and
RAM, same PC13/PA0. The LQFP64 only *adds* PC0–PC12 and PD2 — which the 40-pin header does not
route anyway (§3).

## 3. Pin map

### 3.1 Pins with a fixed job on the board

| Pin | Function | Notes | Status |
|---|---|---|---|
| PC13 | user LED | WeAct convention: anode to 3V3, LOW = lit. Backup-domain pin: ≤ 3 mA sink, ≤ 2 MHz, never source current | toggling verified, polarity not |
| PA0 | KEY | to GND when pressed; enable `GPIO_PULLUP`. Held through reset → bootloader | **verified** |
| PH0 / PH1 | HSE 25 MHz | not usable as GPIO | verified (by consequence) |
| PC14 / PC15 | LSE 32.768 kHz | the bootloader starts LSE; not measured here | unverified |
| PA11 / PA12 | USB D− / D+ (OTG_FS, AF10) | to the USB-C connector | verified (HID and the template's CDC enumerate) |
| PA13 / PA14 | SWDIO / SWCLK | reconfiguring them locks out SWD until BOOT0 | datasheet |
| BOOT0 | ROM bootloader select | button; held at reset → ROM DFU `0483:DF11` | datasheet |
| NRST | reset | button | used |

### 3.2 The 40-pin header (unverified on this clone)

The WeAct Black Pill V2/V3 order, which this format copies. Pin 1 nearest the USB-C connector,
viewed from the component side. **Check every position against your board's silkscreen.**

| Left row | Right row |
|---|---|
| B12 | VB (VBAT) |
| B13 | C13 (LED) |
| B14 | C14 (LSE) |
| B15 | C15 (LSE) |
| A8 | R (NRST) |
| A9 | A0 (KEY) |
| A10 | A1 |
| A11 (USB D−) | A2 |
| A12 (USB D+) | A3 |
| A15 | A4 |
| B3 | A5 |
| B4 | A6 |
| B5 | A7 |
| B6 | B0 |
| B7 | B1 |
| B8 | B2 (BOOT1) |
| B9 | B10 |
| 5V | 3V3 |
| GND | GND |
| 3V3 | 5V |

The LQFP64-only pins (PC0–PC12, PD2) are on the chip but, in this layout, on no header pin. Leave
them unconfigured or set them to analog (`GPIO_MODE_ANALOG`) if current in Stop mode matters.

### 3.3 Useful alternate functions on header pins

| Peripheral | Pins |
|---|---|
| USART1 | TX PA9 / RX PA10 (also a ROM-bootloader port), or PB6 / PB7 |
| USART2 | TX PA2 / RX PA3 (also a ROM-bootloader port) |
| I2C1 | SCL PB6 / SDA PB7, or SCL PB8 / SDA PB9 |
| I2C2 | SCL PB10 / SDA PB3 (AF9) |
| SPI1 | SCK PA5, MISO PA6, MOSI PA7 (NSS PA4) |
| SPI2 | SCK PB13 (or PB10), MISO PB14, MOSI PB15 (NSS PB12) |
| TIM2 / TIM3 / TIM4 PWM | PA0–PA3 / PA6, PA7, PB0, PB1 / PB6–PB9 |
| ADC1 | PA0–PA7, PB0, PB1 |

## 4. Power

Unknown in detail — no schematic. The format provides 5V (USB VBUS through a diode on WeAct boards)
and 3V3 from an on-board LDO, plus a VBAT pin for the backup domain. Treat the LDO as ~100 mA of
headroom for external loads until measured; VDDA is tied to that same 3V3, so ADC readings are
relative to whatever it really outputs (use VREFINT_CAL to measure it).

## 5. Clocks

| Source | Value | Use |
|---|---|---|
| HSE | 25 MHz crystal | PLL input |
| LSE | 32.768 kHz crystal (expected) | RTC |
| HSI | 16 MHz | after reset, and after `HAL_RCC_DeInit()` |

Working tree (template, verified): PLLM 25 → 1 MHz, PLLN 336 → VCO 336 MHz, PLLP /4 → **84 MHz**
SYSCLK, PLLQ /7 → **48 MHz** USB. AHB /1 = 84, APB1 /2 = **42** (its maximum), APB2 /1 = 84.
`FLASH_LATENCY_2` (30 MHz per wait state at 2.7–3.6 V). Voltage scaling: F401 VOS **Scale 2** is
the reset value and covers 84 MHz. The bootloader runs the identical PLL.

## 6. The WeAct HID bootloader

Two versions matter. **V1.2** is on this board (`--info` says so) and is what this section
describes, from the disassembly of WeAct's V1.2 `.axf`. **V1.1** has public source
(`WeActStudio/WeAct_HID_Bootloader_F4x1`) and differs in when it starts the clocks and in the UID
lock; differences are noted.

Binaries: WeAct's repo `WeActStudio/WeActStudio.MiniSTM32F4x1`, commit `d36f5a1`, path
`Soft/WeAct_HID_FW_Bootloader/Bootloader/` — `WeAct_HID_Bootloader_F4x1.hex` (25 MHz HSE, this
board) and `..._8MHz.hex` (8 MHz HSE boards), plus `.axf` files with symbols. The directory was
later removed from the repo's current tree.

### 6.1 Boot sequence (V1.2)

After every reset:

1. GPIO init: PC13 output, PA0 input with pull-up. Still on HSI, no SysTick.
2. **UID lock** (§6.4).
3. **Jump to the app at 0x08004000** if KEY (PA0) reads high **and** RTC backup register
   **DR19 ≠ 0x1234** **and** the word at 0x08004000 looks like a stack pointer
   (`(word & 0x2FF80000) == 0x20000000`). The jump only sets MSP and branches — VTOR stays
   0x08000000, PRIMASK clear.
4. Otherwise: `HAL_Init`, clock config (HSE + PLL 84 MHz, LSE), PC13 toggles every 60 ms while KEY
   is still held; then RTC init, USB HID `0483:572A` "WeAct Studio HID Bootloader", DR19 cleared,
   PC13 "breathing".

Ways in, therefore: **KEY held through NRST/power-up** (verified); a blank app area (first word
0xFFFFFFFF — why a new board shows up as HID by itself; verified); the app writing **0x1234 to
RTC_BKP_DR19** (after `HAL_PWR_EnableBkUpAccess()`) and calling `NVIC_SystemReset()` — verified;
the template firmware does this on a 1200-baud port open and on its `b` command, which is how
`pio run -t upload` gets in without buttons.

One open observation: during early tests with a throwaway firmware, the app started after a DR19
entry was twice thrown back into the bootloader ~11 s after it started (a plain reboot then ran
normally). With the template's CDC firmware it did not recur in 7 DR19 entries, two of them watched
for 30 and 45 s with the heartbeat running. No IWDG in the bootloader; cause not found. If it ever
shows up, `weact_hid_flash.py --reboot` gets out, and the banner's reset cause tells what reset it.

Inside the bootloader: a short KEY press toggles the breathing; **KEY held > 0.65 s** makes PC13
toggle every 100 ms, and on release it de-initialises USB and jumps to ST's ROM bootloader at
0x1FFF0000 — ROM DFU without BOOT0 (from the code; not tried on hardware). NRST leaves.

V1.1 differs: it starts HSE + PLL + SysTick **before** deciding to jump, so the app inherits a
running 84 MHz PLL and a SysTick firing into the bootloader's handler (WeAct's V1.2 changelog:
"fixed a problem with systick not closing").

### 6.2 What the jump leaves behind — the hand-off problem

The app starts with **SCB->VTOR = 0x08000000**, interrupts unmasked, and (V1.1 only) SysTick
running and the PLL driving SYSCLK. With stock ST startup code, after `HAL_Init()` the SysTick
vector resolves through the bootloader's table: its handler increments the bootloader's `uwTick`
— in the app's memory map a random RAM word — and the app's own `uwTick` never moves, so the first
`HAL_Delay()` hangs. Under V1.1 the first ticks additionally arrive while `.data`/`.bss` are being
initialised, and `HAL_RCC_OscConfig()` refuses to reprogram the running PLL unless the values match.

The template's answer (`src/boot_handoff.c` + `SystemClock_Config()`):

1. its own `Reset_Handler` (ST's is weak): `cpsid i`, SysTick off and its pending bit cleared, all
   NVIC enables/pendings cleared, `SystemInit()`, **then** `SCB->VTOR = g_pfnVectors`, then the
   usual `.data` copy / `.bss` zero / `__libc_init_array()`, `cpsie i`, `main()`;
2. `HAL_RCC_DeInit()` as the first line of `SystemClock_Config()`.

Both are harmless when the chip boots straight into the image (DFU/SWD builds) or behind V1.2, so
one source tree serves every case.

`-DUSER_VECT_TAB_ADDRESS -DVECT_TAB_OFFSET=0x4000` is **not** an alternative: the framework's
`system_stm32f4xx.c` defines `VECT_TAB_OFFSET` as 0 unconditionally, so the define is overridden
(one warning) and `SystemInit()` then writes VTOR = 0x08000000 — checked in the disassembly.

### 6.3 Flash protocol

Reimplemented in `template/scripts/weact_hid_flash.py` from WeAct's CLI source and verified on V1.2.

- Every transfer is a 65-byte HID OUT report: report ID 0 + 64 bytes.
- Commands: `"WeAct:"` + one byte — `0x00` reset page counter (next data goes to 0x08004000),
  `0x01` reboot (`NVIC_SystemReset`), `0x02` version, `0x04` erase app area.
- Version reply (IN report): `"WeAct:V1.2 0x423 ROM: 256KB"` — bootloader version, DEVID, flash
  size in KB.
- Data: raw 64-byte reports, 16 per 1 KB page. The bootloader erases a sector when the first page of
  it arrives (sectors 1, 2, 3 at pages 16/32/48, the 64 KB sector 4 at page 64, the 128 KB sector 5
  at page 128), writes the page, and answers `"WeAct:"` + `0x03`. Wait for that ACK before the next
  page; a 128 KB erase stalls the CPU (and USB) for 1–2 s.
- The bootloader buffers exactly one report: pace them (500 µs, as WeAct's CLI does) or data is
  overwritten.
- There is no read-back and no checksum. Verification is "the app starts".

### 6.4 The UID lock (V1.2)

On every boot, before anything else, the bootloader compares 12 bytes at **0x08003A28** (inside its
own sector) with the chip's unique ID at 0x1FFF7A10:

- slot blank (0xFF — a freshly installed official `.hex`): it programs the UID, and that boot then
  pulses PC13 for 4 ms every 500 ms **forever** — tap NRST once and it proceeds normally;
- slot matches: continue;
- slot holds **another chip's UID** (a sector-0 dump copied between boards): it copies itself to
  RAM at 0x2000C000, runs `bootloader_erase` from there — `HAL_FLASHEx_Erase` with
  `TypeErase = MASSERASE` — and hangs. **The whole flash is gone**, app included. Recover with
  ROM DFU (BOOT0) and the official `.hex`.

(V1.1 kept the UID at 0x08003FF4, the top of sector 0, and only blinked forever on a mismatch.)

## 7. Memory map

| Address | Size | Content |
|---|---|---|
| 0x08000000 | 16 KB (sector 0) | WeAct HID bootloader |
| 0x08003A28 | 12 B | chip UID copy inside the bootloader (V1.2 lock, §6.4) |
| 0x08004000 | 240 KB (sectors 1–5) | application — vector table first |
| 0x1FFF0000 | 30 KB | ST ROM bootloader (DFU, USART1/2, I2C, SPI) |
| 0x1FFF7A10 | 12 B | unique device ID |
| 0x1FFF7A22 | 2 B | flash size in KB |
| 0x20000000 | 64 KB | SRAM; `_estack` = 0x20010000 |
| 0x4000289C | 4 B | RTC_BKP_DR19 (RTC->BKP19R) — bootloader "stay" flag (0x1234), §6.1 |

---

# Part II — development

## 8. PlatformIO

### 8.1 Board and framework

`platform = ststm32`, `framework = stm32cube`, **`board = genericSTM32F401RC`**. Not
`coreboard_f401rc` (defaults to `stlink` and advertises `upload_protocol = hid`, which is the wrong
bootloader), and not `blackpill_f401cc` (works, but describes the 48-pin chip).

Verified with PlatformIO `ststm32` 19.7.1, `framework-stm32cubef4` 1.28.1, toolchain
`toolchain-gccarmnoneeabi` 1.90301 (GCC 9.3.1).

### 8.2 Environments

| env | ldscript | upload | sector 0 |
|---|---|---|---|
| `hid` | `ld/STM32F401RCTX_APP_0x08004000.ld` | `custom` → `scripts/weact_hid_flash.py` | kept |
| `dfu` | stock `STM32F401RCTX_FLASH.ld` | ROM DFU via `tool-dfuutil` | overwritten |
| `stlink` | stock | OpenOCD + ST-Link | overwritten |

`[env:hid]` also sets `board_upload.offset_address = 0x08004000` and
`board_upload.maximum_size = 245760`, so the size report and any probe-based upload use the right
numbers.

### 8.3 The uploader

`scripts/hid_upload.py` (an `extra_scripts` hook) replaces `UPLOADCMD` with
`$PYTHONEXE scripts/weact_hid_flash.py $SOURCE` and, before the first upload, `pip install`s
`hidapi` into PlatformIO's own Python — checked with `importlib.util.find_spec`, not imported:
with `hid` loaded into the SCons process, `pio run -t upload` ended `FAILED` (exit 1) after a
successful flash on macOS. The flasher:

- checks the image: word 0 must be a RAM stack pointer, word 1 a reset vector **≥ 0x08004000**
  (otherwise it was linked for 0x08000000 — refused);
- if `0483:572A` is not there, looks for the template firmware's CDC port (`0483:5740`, product
  "Black Pill F401RC VCP"; `--port` to name one) and opens it at **1200 baud** — the firmware
  reboots into the bootloader (DR19, §6.1). Needs `pyserial`, which PlatformIO's Python has;
- otherwise waits up to 30 s for a manual KEY + NRST (`--wait N` to change, 0 = fail immediately);
- prints the version reply, checks the size against flash − 16 KB, streams the pages, reboots.

macOS and Windows need no driver. Linux needs read/write access to the hidraw node:

```
# /etc/udev/rules.d/49-weact-hid.rules
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="572a", MODE="0666"
SUBSYSTEM=="usb",    ATTRS{idVendor}=="0483", ATTRS{idProduct}=="572a", MODE="0666"
```

The CDC console is `/dev/ttyACM*` on Linux: the user needs the `dialout` (Debian/Ubuntu) or `uucp`
(Arch) group, and ModemManager may probe the port for a few seconds after it appears — add
`ENV{ID_MM_DEVICE_IGNORE}="1"` for `0483:5740` if the first lines go missing. Not tested on Linux.

### 8.4 USB CDC console

The full template enumerates as a CDC ACM virtual COM port, `0483:5740` "Black Pill F401RC VCP",
serial number = chip UID — `/dev/cu.usbmodem<UID>` on macOS (verified), `/dev/ttyACM*` on Linux,
`COMx` on Windows. There is no USB-serial chip on the board: **the port exists only while firmware
with a USB stack runs**. The HID bootloader, ROM DFU and the minimal blink show no serial port at
all — `ls /dev/cu.*` finding nothing in those states is correct.

Design points (code in `recipes.md` §5):

- the ST USB Device Library comes from `framework-stm32cubef4`; only the glue, descriptors and CDC
  interface live in the project. Class memory is static: ST's default `USBD_malloc` is `malloc`,
  and the stock 0x200-byte heap makes the allocation fail and enumeration never finish;
- VBUS sensing off (PA9 is not wired to VBUS);
- output is sent only while the host holds **DTR** — a terminal has the port open. Writes to a
  closed port are dropped, and a host that stops reading costs at most 50 ms per write;
- transmit only from thread context; the receive callback (USB IRQ) only fills a ring buffer;
- `printf` goes through newlib `_write()`; stdout is line-buffered with a static 128-byte buffer;
- baud rate is ignored, except **1200 = reboot into the HID bootloader**.

Cost: 20408 B flash / 4424 B RAM for the full template versus 3896 B / 44 B for the blink
(same toolchain).

## 9. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Upload succeeds, board shows nothing (LED dark or bootloader breathing), USB device gone or back to HID | image linked for 0x08000000 | `[env:hid]` with the 0x08004000 linker script (the template's flasher refuses such images) |
| App starts, then hangs in the first `HAL_Delay()` | VTOR still points at the bootloader's table, SysTick goes to its handler — or no `SysTick_Handler` defined | `boot_handoff.c` `Reset_Handler`; define `SysTick_Handler` |
| Rare, unreproducible corrupted globals right after boot (V1.1 bootloader) | bootloader's SysTick fired during `.data`/`.bss` init | same `Reset_Handler` — IRQs masked before init |
| `Error_Handler` (template: LED solid on) right after `HAL_RCC_OscConfig`, only when flashed via HID (V1.1 bootloader) | PLL already SYSCLK with other values | `HAL_RCC_DeInit()` first |
| Works via ST-Link, hangs via HID | any of the three above — the probe path starts from a real reset | — |
| `-DVECT_TAB_OFFSET=0x4000` has no effect, "redefined" warning | framework system file hard-codes 0 | remove it; use the `Reset_Handler` |
| `upload_protocol = hid`: "HID device not found" / `1209:BEBA` | PlatformIO's `hid` is a different bootloader | `custom` + `scripts/hid_upload.py` |
| PC13 pulses briefly every 0.5 s forever, no USB device | first boot after (re)installing the bootloader: UID being recorded | tap NRST once |
| Board dead after writing a sector-0 dump from another board: no HID, no app | V1.2 UID lock mass-erased the whole flash | BOOT0 + NRST → ROM DFU → official `.hex` (recipes §7) |
| Board always comes up as HID, never runs the app | KEY/PA0 held low by external hardware, DR19 left at 0x1234, or blank app area | free PA0; reflash; reset |
| HID route stopped working after a DFU/ST-Link flash | sector 0 overwritten | reinstall the official `.hex` (recipes §7), or stay on DFU/SWD |
| After an app-requested entry (DR19), the next app returns to the bootloader ~11 s after starting | seen twice with an early test firmware, not with the template (§6.1); cause unknown | `weact_hid_flash.py --reboot`; check the banner's reset cause |
| No `/dev/cu.usbmodem*` / `ttyACM` / COM port at all | the running firmware has no USB stack (minimal blink, bootloader, ROM DFU), or it hangs before `MX_USB_DEVICE_Init()` | flash the full template; a solid LED = `Error_Handler` |
| Port appears, terminal shows nothing | terminal does not assert DTR (template sends only with DTR high) | enable DTR in the terminal; `pio device monitor` and `screen` set it |
| Console prints a burst of thousands of lines, then normal | a timestamp taken before a blocking call compared with a newer one: `now - last` underflows | read `HAL_GetTick()` after the blocking code (template main loop) |
| `printf` output garbled or USB drops out under load | `printf` / `CDC_Write()` called from an interrupt while the main loop also sends | send from thread context only |
| `pio run -t upload` flashes fine but ends `FAILED` | `import hid` in the extra script (hidapi in the SCons process) | `importlib.util.find_spec("hid")` as in the template |
| No `0483:DF11` after BOOT0 + NRST | BOOT0 released too early, or PB2/BOOT1 pulled high by the circuit (selects SRAM boot) | hold BOOT0 through the NRST release; free PB2 |
| Temperature reads nonsense | ADC_IN18 from F411 code, or 3-cycle sampling | `ADC_CHANNEL_TEMPSENSOR` (IN16 on F401), `ADC_SAMPLETIME_480CYCLES` |
| Everything timing-related off by 3.125× | `HSE_VALUE` 8 MHz from a foreign `hal_conf` | `-DHSE_VALUE=25000000` |
| Code from an F411 Black Pill hard-faults or is unstable | 96/100 MHz clock tree, `FLASH_LATENCY_3`, Scale 1 | 84 MHz tree from §5 |
| Pin from the datasheet does nothing on the header | LQFP64-only pin (PC0–PC12, PD2) or PB11 (absent) | pick a header pin (§3.2) |
| `ModuleNotFoundError: hid` during upload | `hidapi` missing in PlatformIO's Python | `~/.platformio/penv/bin/python -m pip install hidapi` |
