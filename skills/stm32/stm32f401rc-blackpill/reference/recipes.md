# STM32F401RCT6 Black Pill recipes

Copy-paste-ready. Sections 1–6 are extracted verbatim from `template/`, which builds clean and was
flashed and run on the board. Section 7 is recovery, not exercised on the author's board.

## 1. `platformio.ini`

```ini
; PlatformIO Project Configuration File
;
; 40-pin "Black Pill" with an STM32F401RCT6 and the WeAct Studio HID bootloader,
; built against the STM32Cube HAL.
;
;   pio run -t upload             -> [env:hid]   app at 0x08004000, over USB, keeps the bootloader
;   pio run -e dfu -t upload      -> [env:dfu]   app at 0x08000000 via ROM DFU - ERASES the bootloader
;   pio run -e stlink -t upload   -> [env:stlink] app at 0x08000000 via SWD  - ERASES the bootloader
;
; https://docs.platformio.org/page/projectconf.html

[platformio]
default_envs = hid

[env]
platform = ststm32
framework = stm32cube
; Not coreboard_f401rc: that one defaults to stlink and lists `hid`, which is
; the wrong HID bootloader (see scripts/hid_upload.py).
board = genericSTM32F401RC
build_flags =
    ; 25 MHz crystal. The F4 hal_conf template also defaults to 25 MHz, by luck;
    ; a CubeMX-generated hal_conf for an 8 MHz board would silently skew every
    ; baud rate and timer by 3.125x.
    -DHSE_VALUE=25000000
    -DLSE_VALUE=32768
    -Wall
debug_tool = stlink

[env:hid]
; Link behind the 16 KB bootloader. Without this the image starts at
; 0x08000000, the bootloader writes it to 0x08004000 anyway, and it never runs.
board_build.ldscript = ld/STM32F401RCTX_APP_0x08004000.ld
board_upload.offset_address = 0x08004000
board_upload.maximum_size = 245760
; While the template firmware runs, upload opens its USB port at 1200 baud and
; the firmware reboots into the bootloader by itself. Otherwise (blank chip,
; crashed or foreign firmware): hold KEY, tap NRST, release KEY - PC13 blinks
; fast, 0483:572A appears. The flasher reboots into the new firmware when done.
upload_protocol = custom
extra_scripts = scripts/hid_upload.py
; `pio device monitor`: the USB CDC console. The baud rate is ignored over USB
; (any value but 1200, which means "enter the bootloader").
monitor_speed = 115200

[env:dfu]
; ROM bootloader: hold BOOT0, tap NRST, release -> 0483:DF11. Writes sector 0,
; so the WeAct HID bootloader is gone afterwards (and [env:hid] stops working).
upload_protocol = dfu

[env:stlink]
; SWD probe on SWDIO/SWCLK. Also writes from 0x08000000 - same caveat as dfu.
upload_protocol = stlink
```

## 2. Linker script: run behind the bootloader

`template/ld/STM32F401RCTX_APP_0x08004000.ld` is ST's `STM32F401RCTX_FLASH.ld` (from
`tool-ldscripts-ststm32/stm32f4/`) with one line changed — the FLASH region:

```ld
/* Memories definition */
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 64K
  FLASH    (rx)    : ORIGIN = 0x8004000,   LENGTH = 240K  /* 256K - 16K bootloader */
}
```

Everything else (sections, `_estack` = 0x20010000, heap/stack minimums) is untouched. To make it
for another part, copy that part's ST script and move `ORIGIN` up by 0x4000 / `LENGTH` down by 16K.

## 3. Hand-off `Reset_Handler` — `src/boot_handoff.c`

Overrides ST's weak `Reset_Handler`. Required whenever the image can be started by the bootloader;
harmless otherwise.

```c
/*
 * Reset_Handler that survives being jumped to by the WeAct HID bootloader.
 *
 * The bootloader does not reset the chip before starting the app. It sets MSP
 * from 0x08004000 and branches to the reset vector at 0x08004004, leaving
 * SCB->VTOR at 0x08000000 - ITS vector table - and interrupts unmasked.
 * V1.2 (this board) jumps before it has started any clock or SysTick; V1.1,
 * still found on boards, jumps with HSE + PLL as SYSCLK and SysTick running.
 *
 * With ST's stock startup, HAL_Init() enables SysTick but the vector still
 * points into the bootloader, whose handler increments the bootloader's
 * uwTick - some random word of OUR RAM - so HAL_GetTick() never moves and the
 * first HAL_Delay() hangs forever. Under V1.1 the first tick can even land
 * while .data/.bss are being initialised.
 *
 * ST's Reset_Handler is weak, so this one replaces it: mask interrupts, stop
 * SysTick, point VTOR at our own table, THEN copy .data and zero .bss.
 * A V1.1 clock tree is taken back in SystemClock_Config() (HAL_RCC_DeInit first).
 *
 * Harmless when the chip boots straight into this image from 0x08000000
 * ([env:dfu] / [env:stlink]): VTOR then just gets the value it already had.
 */

#include "stm32f4xx.h"

extern uint32_t g_pfnVectors[];             /* startup_stm32f401xc.s */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
extern void __libc_init_array(void);
extern int main(void);

void Reset_Handler(void)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->VAL  = 0U;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;
    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    /* SystemInit() BEFORE the VTOR write: built with -DUSER_VECT_TAB_ADDRESS
       it writes VTOR = 0x08000000 itself (the framework's system file
       hard-codes VECT_TAB_OFFSET to 0; a -D cannot override it). */
    SystemInit();

    SCB->VTOR = (uint32_t)g_pfnVectors;     /* 0x08004000 behind the bootloader */
    __DSB();
    __ISB();

    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;)
    {
        *dst++ = *src++;
    }
    for (uint32_t *dst = &_sbss; dst < &_ebss;)
    {
        *dst++ = 0U;
    }

    __libc_init_array();

    __enable_irq();
    main();

    while (1)
    {
    }
}
```

## 4. Clock tree: HSE 25 MHz → 84 MHz SYSCLK, 48 MHz USB

From `src/main.c` (identical in `variants/minimal/main.c`). Note `HAL_RCC_DeInit()` first.

```c
/*
 * 84 MHz is the F401's maximum. PLLQ must give exactly 48 MHz for USB and
 * shares the VCO with PLLP: VCO 336 -> /4 = 84 SYSCLK, /7 = 48 USB.
 *
 * HAL_RCC_DeInit() first: behind a V1.1 bootloader the PLL is already running
 * as SYSCLK, and HAL_RCC_OscConfig() refuses (HAL_ERROR -> Error_Handler) to
 * touch a PLL that drives SYSCLK unless the requested values match exactly.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    if (HAL_RCC_DeInit() != HAL_OK)     /* back to HSI 16 MHz, PLL off */
    {
        Error_Handler();
    }

    /* F401: Scale 2 is its top range (84 MHz) and the reset value */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;            /* 25 MHz / 25 = 1 MHz          */
    osc.PLL.PLLN       = 336;           /* -> 336 MHz VCO (192..432)    */
    osc.PLL.PLLP       = RCC_PLLP_DIV4; /* -> 84 MHz SYSCLK             */
    osc.PLL.PLLQ       = 7;             /* -> 48 MHz for USB            */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();                /* HSE did not start            */
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  84 MHz              */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 42 MHz (42 max)     */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 84 MHz              */
    /* 2 wait states: 30 MHz per state at 2.7-3.6 V, 84 / 3 = 28 <= 30 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}
```

LED and KEY (pin macros from `include/board.h`):

```c
static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    LED_OFF();                          /* set the level before driving the pin */

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;   /* PC13: 2 MHz / 3 mA sink max */
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);
}

static void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin  = KEY_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;            /* KEY shorts to GND */
    HAL_GPIO_Init(KEY_GPIO_Port, &gpio);
}
```

And the two handlers every project needs — `framework-stm32cube` provides neither:

```c
/* Hang with the LED solid on, so a clock failure looks different from a blink.
   Register-level on purpose: it must work even if HAL state is broken. */
void Error_Handler(void)
{
    __disable_irq();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIOC->MODER = (GPIOC->MODER & ~(3U << 26)) | (1U << 26);
    GPIOC->BSRR  = GPIO_PIN_13 << 16;   /* PC13 low = lit */
    while (1)
    {
    }
}

/* framework-stm32cube ships no stm32f4xx_it.c and the startup file's weak
   SysTick_Handler is an infinite loop: without this, HAL_Delay() and every
   HAL timeout hang. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
```

## 5. USB CDC debug console

The full template (`new-project.sh --full`, the default) turns the USB-C port into a virtual COM
port: `0483:5740` "Black Pill F401RC VCP", serial number = chip UID, `/dev/cu.usbmodem<UID>` on
macOS, `/dev/ttyACM0` on Linux, `COMx` on Windows (driverless on Windows 10+). The USB Device
Library comes from `framework-stm32cubef4`; PlatformIO builds it because the sources include
`usbd_core.h` / `usbd_cdc.h`. The project's own files:

| File | Job |
|---|---|
| `src/usbd_conf.c`, `include/usbd_conf.h` | PCD glue for USB_OTG_FS (PA11/PA12), `OTG_FS_IRQHandler`, static class memory, **VBUS sensing off** |
| `src/usbd_desc.c`, `include/usbd_desc.h` | descriptors: VID/PID, product string, serial from the UID |
| `src/usb_device.c`, `include/usb_device.h` | `MX_USB_DEVICE_Init()` — core + CDC class + interface |
| `src/usbd_cdc_if.c`, `include/usbd_cdc_if.h` | RX ring, `CDC_Write()`, DTR tracking, 1200-baud request, `_write()` for `printf` |

`printf` → USB. newlib calls `_write()`; stdout is made line-buffered with a static buffer, so a
line costs one USB packet and no `malloc` from the 0x200-byte heap:

```c
/* newlib: printf/puts/putchar on stdout and stderr end up here. */
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    CDC_Write((const uint8_t *)ptr, (uint32_t)len);
    return len;                     /* claim success: never block printf */
}
```

```c
    MX_USB_DEVICE_Init();

    /* Line-buffered stdout with a static buffer: printf() reaches USB once per
       line (one packet) instead of once per format fragment, and newlib does
       not malloc a buffer out of the 0x200-byte heap. */
    setvbuf(stdout, stdout_buf, _IOLBF, sizeof(stdout_buf));
```

`CDC_Write()` drops output while no terminal holds DTR, and after 50 ms of the host not reading,
so `printf` never stalls the firmware with the port closed. Only call it (and `printf`) from thread
context — never from an interrupt handler.

Port open (DTR) and the bootloader request (1200 baud) arrive as control requests:

```c
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;

    switch (cmd)
    {
    case CDC_SET_LINE_CODING:
        LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |
                                           (pbuf[2] << 16) | (pbuf[3] << 24));
        LineCoding.format     = pbuf[4];
        LineCoding.paritytype = pbuf[5];
        LineCoding.datatype   = pbuf[6];
        if (LineCoding.bitrate == 1200U)
        {
            CDC_BootloaderRequest = 1U;     /* see weact_hid_flash.py */
        }
        break;

    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t)(LineCoding.bitrate);
        pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
        pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
        pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
        pbuf[4] = LineCoding.format;
        pbuf[5] = LineCoding.paritytype;
        pbuf[6] = LineCoding.datatype;
        break;

    /* DTR (bit 0 of wValue) goes high when a terminal opens the port. It is
       the only reliable "someone is listening" signal - without it, output
       sent to a closed port fills the endpoint and every write times out. */
    case CDC_SET_CONTROL_LINE_STATE:
        CDC_PortOpen = (((USBD_SetupReqTypedef *)pbuf)->wValue & 0x0001U) ? 1U : 0U;
        break;

    default:
        break;
    }

    return USBD_OK;
}
```

Rebooting into the HID bootloader from the app — the template does it on the 1200-baud request and
on the `b` command:

```c
/* The WeAct bootloader stays resident when RTC backup register DR19 holds
   0x1234 after a reset (and clears it). Backup registers are write-protected
   until DBP is set; without HAL_PWR_EnableBkUpAccess() the write is silently
   dropped and the reset just restarts this firmware. */
static void enter_hid_bootloader(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP19R = 0x1234U;
    HAL_Delay(20);          /* finish the USB control transfer that asked for it */
    NVIC_SystemReset();
}
```

Received bytes come from `CDC_ReadByte()` (-1 when none) in the main loop. To read the console:

```sh
pio device monitor                      # picks the port; Ctrl-C to quit
screen /dev/cu.usbmodem* 115200         # macOS alternative; Ctrl-A K to quit
```

The baud rate is meaningless over USB — except **1200, which reboots the board into the
bootloader**. Opening the port prints a banner (clocks, DEVID, flash size, UID, vector-table
address, reset cause); then one `alive` line per second, KEY events, and echo. Commands: `i` banner,
`r` reboot, `b` HID bootloader.

## 6. Flashing through the HID bootloader

`pio run -t upload` does all of this. While the template firmware runs, the flasher first opens its
CDC port at 1200 baud, the firmware reboots into the bootloader, and the upload continues — no
buttons. By hand, with any Python that has `hidapi` (and `pyserial` for the touch):

```sh
python scripts/weact_hid_flash.py .pio/build/hid/firmware.bin   # touch, flash, reboot
python scripts/weact_hid_flash.py --info                 # V1.2 0x423 ROM: 256KB
python scripts/weact_hid_flash.py firmware.bin --no-reboot   # stay in the bootloader
python scripts/weact_hid_flash.py --reboot                   # leave it, start the app
python scripts/weact_hid_flash.py firmware.bin --port /dev/cu.usbmodemXXXX   # several boards
```

If neither the bootloader nor the template's CDC port is there (blank chip, crashed firmware,
firmware without the 1200-baud handler) it asks for KEY + NRST and waits `--wait N` seconds
(default 30; 0 = fail at once).

## 7. Reinstall the HID bootloader

After `[env:dfu]` / `[env:stlink]` overwrote sector 0, or after the UID lock wiped the chip. Use
WeAct's **official** V1.2 image — its UID slot (0x08003A28) is blank, so it adopts whichever chip
it lands on. **Never** write a sector-0 dump taken from another board: V1.2 sees a foreign UID and
mass-erases the whole flash (`board-hardware.md` §6.4). The conversion below was checked (14888 B,
ending at the blank UID slot); the DFU write was not run on the author's board.

```sh
# 25 MHz HSE build (this board). For 8 MHz-crystal boards use ..._F4x1_8MHz.hex instead.
curl -L -o weact_hid_v12.hex \
  "https://raw.githubusercontent.com/WeActStudio/WeActStudio.MiniSTM32F4x1/d36f5a1ca4a9f73917dfddb9d8a3b31b5621f94a/Soft/WeAct_HID_FW_Bootloader/Bootloader/WeAct_HID_Bootloader_F4x1.hex"

# The .hex also carries records at 0x2000xxxx (RAM) - keep only the flash part.
~/.platformio/packages/tool-sreccat/srec_cat weact_hid_v12.hex -Intel \
  -crop 0x08000000 0x08004000 -offset -0x08000000 -o weact_hid_v12.bin -Binary

# hold BOOT0, tap NRST, release  ->  0483:DF11
~/.platformio/packages/tool-dfuutil/bin/dfu-util -d 0483:df11 -a 0 \
  -s 0x08000000:leave -D weact_hid_v12.bin
```

`dfu-util` erases only the sectors it writes (sector 0 here), so an app at 0x08004000 survives.
First boot afterwards records the UID and then pulses PC13 every 0.5 s forever: tap NRST once.

With an ST-Link instead of DFU: `st-flash write weact_hid_v12.bin 0x08000000`.
