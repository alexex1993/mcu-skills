# Blink + USB console — STM32F401RCT6 Black Pill with the WeAct HID bootloader

A PlatformIO + STM32Cube HAL project that builds clean with `-Wall`, flashes over the board's own
USB-C through the WeAct HID bootloader, starts correctly behind it, and gives you a `printf`
console on the same USB-C port. Every code block in `../reference/recipes.md` comes from here.

## Use it

```sh
variants/new-project.sh ~/path/to/MyProject            # blink + USB CDC console (default)
variants/new-project.sh ~/path/to/MyProject --minimal  # blink only, no USB
cd ~/path/to/MyProject && pio run
pio run -t upload
pio device monitor
```

The first upload onto a board that runs something else needs the manual entry — hold KEY, tap
NRST, release KEY (PC13 flickers, then "breathes"; USB `0483:572A` appears); the flasher waits 30 s
for it. From then on the full variant needs no buttons: `pio run -t upload` opens its USB port at
1200 baud, the firmware reboots into the bootloader, and the upload continues.

The upload prints the bootloader's identity (`V1.2 0x423 ROM: 256KB`), writes 1 KB pages from
0x08004000, and reboots the chip into the app. The first upload installs the `hidapi` Python package
into PlatformIO's environment.

## What it does

| | full (default) | `--minimal` |
|---|---|---|
| LED | PC13 at 2 Hz, 10 Hz while KEY is held | same |
| USB | CDC virtual COM port `0483:5740` "Black Pill F401RC VCP", serial = chip UID | none |
| Console | on port open: banner (clocks, DEVID, flash size, UID, vector table, reset cause); 1 `alive` line/s; KEY press/release; echo. Commands `i` banner, `r` reboot, `b` HID bootloader | — |
| Re-flash | buttonless (1200-baud touch) | KEY + NRST every time |
| Size | **20408 B flash, 4424 B RAM** | **3896 B flash, 44 B RAM** |

A solid LED means `Error_Handler` — the crystal or PLL did not start. Sizes with PlatformIO
`ststm32` 19.7.1, `framework-stm32cubef4` 1.28.1.

Copying the tree by hand works just as well: nothing is generated, no absolute paths, no project
name embedded anywhere. For the minimal variant by hand, drop the six `usb*` files and use
`variants/minimal/main.c` as `src/main.c`.

## Environments

| env | links at | upload | WeAct bootloader |
|---|---|---|---|
| `hid` (default) | 0x08004000 | `scripts/weact_hid_flash.py` over USB HID | kept |
| `dfu` | 0x08000000 | ROM DFU — hold BOOT0, tap NRST | **overwritten** |
| `stlink` | 0x08000000 | ST-Link SWD | **overwritten** |

## Files

| File | Job | Needed by |
|---|---|---|
| `platformio.ini` | board, HSE_VALUE, the three environments, monitor speed | all |
| `ld/STM32F401RCTX_APP_0x08004000.ld` | ST's linker script with FLASH moved behind the 16 KB bootloader | `hid` |
| `scripts/hid_upload.py` | PlatformIO hook: `upload_protocol = custom` → the flasher, installs `hidapi` | `hid` |
| `scripts/weact_hid_flash.py` | WeAct HID bootloader flasher with the 1200-baud touch; also standalone (`--info`, `--reboot`, `--port`) | `hid` |
| `src/boot_handoff.c` | `Reset_Handler`: mask IRQs, stop SysTick, VTOR → our table, then C runtime init | all (harmless without the bootloader) |
| `src/main.c` | clocks (84/48 MHz from 25 MHz HSE), LED, KEY, console loop, bootloader request, `SysTick_Handler`, `Error_Handler` | all |
| `src/usbd_cdc_if.c`, `include/usbd_cdc_if.h` | CDC interface: RX ring, `CDC_Write()`, DTR, 1200-baud request, `_write()` for `printf` | full |
| `src/usbd_conf.c`, `include/usbd_conf.h` | USB_OTG_FS ↔ HAL PCD glue, `OTG_FS_IRQHandler`, static class memory, VBUS sensing off | full |
| `src/usbd_desc.c`, `include/usbd_desc.h` | device/string descriptors (VID/PID, product, UID serial) | full |
| `src/usb_device.c`, `include/usb_device.h` | `MX_USB_DEVICE_Init()` | full |
| `include/board.h` | pin names with polarity, flash map | all |
| `variants/minimal/main.c` | the blink-only `main.c` | `--minimal` |
| `variants/new-project.sh` | scaffold script — not copied into new projects | — |

The ST USB Device Library itself is not in the tree: it comes from `framework-stm32cubef4`, and
PlatformIO builds it because the sources include `usbd_core.h` / `usbd_cdc.h`.

When growing the project, keep `boot_handoff.c`, `HAL_RCC_DeInit()` at the top of
`SystemClock_Config()` with PLLQ giving 48 MHz, and `SysTick_Handler()`; call `printf` only from
the main loop, never from an interrupt; add an `stm32f4xx_it.c` once more interrupt handlers are
needed (`OTG_FS_IRQHandler` already lives in `usbd_conf.c`).

## Third-party code

| Path | Origin | License |
|---|---|---|
| `ld/STM32F401RCTX_APP_0x08004000.ld` | ST `STM32F401RCTX_FLASH.ld` (PlatformIO `tool-ldscripts-ststm32`), FLASH origin/length changed | BSD-3-Clause, header kept |
| `scripts/weact_hid_flash.py` | protocol reimplemented from WeAct's `WeAct_HID_Flash_CLI.c` (GPL-3.0); no code copied | this repo's MIT |
| `src/usbd_*.c`, `src/usb_device.c`, `include/usbd_*.h`, `include/usb_device.h` | adapted from this repo's `stm32f411-blackpill` template (CubeMX structure, hand-written); here the library is the framework's instead of a vendored copy | this repo's MIT |

## Verification status

Hardware-verified on an unbranded STM32F401RCT6 Black Pill with WeAct HID bootloader V1.2 (macOS
host): build of both variants, HID upload via `pio run -t upload`, start-up behind the bootloader,
84 MHz clock from HSE, PC13 blink, KEY on PA0 (active low), KEY + NRST bootloader entry; full
variant: CDC enumeration as `/dev/cu.usbmodem<UID>`, banner, one heartbeat per second, echo, `i`
and `b` commands, button-free re-flash via the 1200-baud touch (several runs, ~2.6 s each). Not
tested: the console on Linux/Windows, `pio device monitor` itself (the port was read with
pyserial), and `[env:dfu]` / `[env:stlink]` uploads (no probe attached, and DFU would have erased
the bootloader).
