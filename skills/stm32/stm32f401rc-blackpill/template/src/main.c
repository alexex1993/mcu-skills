/*
 * STM32F401RCT6 Black Pill behind the WeAct HID bootloader: blink + USB CDC
 * debug console.
 *
 *   - PC13 LED blinks at 2 Hz, 10 Hz while KEY (PA0) is held;
 *   - the USB-C port enumerates as a virtual COM port (/dev/cu.usbmodem*,
 *     COMx on Windows) and printf() goes there;
 *   - opening the port prints a banner (clocks, chip, UID, reset cause, where
 *     the vector table is), then one heartbeat line per second;
 *   - KEY presses and releases are reported;
 *   - typed commands: 'i' banner again, 'r' reboot (the bootloader then starts
 *     this firmware again), 'b' reboot into the HID bootloader, anything else
 *     is echoed back;
 *   - opening the port at 1200 baud also reboots into the HID bootloader, so
 *     `pio run -t upload` needs no KEY + NRST while this firmware runs.
 *
 * Clock: HSE 25 MHz -> PLL -> 84 MHz SYSCLK, 48 MHz USB.
 */

#include <stdio.h>

#include "board.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#define BLINK_SLOW_MS   250U
#define BLINK_FAST_MS   50U
#define HEARTBEAT_MS    1000U

extern uint32_t g_pfnVectors[];

static uint32_t reset_flags;        /* RCC->CSR as found at boot */
static char     stdout_buf[128];

static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void KEY_GPIO_Init(void);
static void print_banner(void);
static void handle_input(void);
static void enter_hid_bootloader(void);

int main(void)
{
    /* Read the reset cause before anything can clear it, then clear it so the
       next boot reports only its own cause. */
    reset_flags = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;

    HAL_Init();             /* SysTick at 1 ms - on OUR vector table, see boot_handoff.c */
    SystemClock_Config();
    LED_GPIO_Init();
    KEY_GPIO_Init();
    MX_USB_DEVICE_Init();

    /* Line-buffered stdout with a static buffer: printf() reaches USB once per
       line (one packet) instead of once per format fragment, and newlib does
       not malloc a buffer out of the 0x200-byte heap. */
    setvbuf(stdout, stdout_buf, _IOLBF, sizeof(stdout_buf));

    uint32_t last_blink = HAL_GetTick();
    uint32_t last_beat  = HAL_GetTick();
    uint8_t  was_open   = 0U;
    uint8_t  key_was    = 0U;

    while (1)
    {
        /* Banner each time a terminal opens the port. Before `now` is read:
           the banner takes ~50 ms, and a `now` from before it would sit behind
           last_beat - (now - last_beat) then wraps to ~4e9 and the heartbeat
           fires on every pass. */
        if (CDC_PortOpen && !was_open)
        {
            HAL_Delay(50);  /* let the host finish opening, or the first line is lost */
            print_banner();
            last_beat = HAL_GetTick();
        }
        was_open = CDC_PortOpen;

        if (CDC_BootloaderRequest)
        {
            enter_hid_bootloader();
        }

        uint32_t now = HAL_GetTick();

        uint32_t period = KEY_PRESSED() ? BLINK_FAST_MS : BLINK_SLOW_MS;
        if ((now - last_blink) >= period)
        {
            last_blink = now;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }

        if ((now - last_beat) >= HEARTBEAT_MS)
        {
            last_beat = now;
            printf("[%7lu ms] alive, KEY %s\r\n", (unsigned long)now,
                   KEY_PRESSED() ? "down" : "up");
        }

        uint8_t key_is = KEY_PRESSED() ? 1U : 0U;
        if (key_is != key_was)
        {
            printf("KEY %s\r\n", key_is ? "pressed" : "released");
            key_was = key_is;
        }

        handle_input();
    }
}

static void handle_input(void)
{
    int c;

    while ((c = CDC_ReadByte()) >= 0)
    {
        switch (c)
        {
        case 'i':
            print_banner();
            break;
        case 'r':
            printf("rebooting...\r\n");
            HAL_Delay(20);  /* let the line leave before USB disappears */
            NVIC_SystemReset();
            break;
        case 'b':
            printf("rebooting into the HID bootloader...\r\n");
            HAL_Delay(20);
            enter_hid_bootloader();
            break;
        case '\r':
        case '\n':
            printf("\r\n");
            break;
        default:
            printf("%c", c);
            fflush(stdout);
            break;
        }
    }
}

static void print_banner(void)
{
    const uint32_t *uid = (const uint32_t *)UID_BASE;

    printf("\r\n=== STM32F401RCT6 Black Pill - USB CDC console ===\r\n");
    printf("SYSCLK %lu Hz, HCLK %lu, PCLK1 %lu, PCLK2 %lu\r\n",
           (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)HAL_RCC_GetHCLKFreq(),
           (unsigned long)HAL_RCC_GetPCLK1Freq(), (unsigned long)HAL_RCC_GetPCLK2Freq());
    printf("DEVID 0x%03lX rev 0x%04lX, flash %u KB, UID %08lX%08lX%08lX\r\n",
           (unsigned long)HAL_GetDEVID(), (unsigned long)HAL_GetREVID(),
           *(const uint16_t *)FLASHSIZE_BASE,
           (unsigned long)uid[2], (unsigned long)uid[1], (unsigned long)uid[0]);
    printf("vector table at 0x%08lX (%s)\r\n", (unsigned long)(uint32_t)g_pfnVectors,
           ((uint32_t)g_pfnVectors == 0x08004000U) ? "behind the WeAct HID bootloader"
                                                    : "owns the whole flash");
    printf("reset cause:%s%s%s%s%s%s (CSR 0x%08lX)\r\n",
           (reset_flags & RCC_CSR_LPWRRSTF) ? " low-power" : "",
           (reset_flags & RCC_CSR_WWDGRSTF) ? " WWDG" : "",
           (reset_flags & RCC_CSR_IWDGRSTF) ? " IWDG" : "",
           (reset_flags & RCC_CSR_SFTRSTF)  ? " software" : "",
           (reset_flags & RCC_CSR_PORRSTF)  ? " power-on" : "",
           (reset_flags & RCC_CSR_PINRSTF)  ? " NRST-pin" : "",
           (unsigned long)reset_flags);
    printf("commands: i = this info, r = reboot, b = HID bootloader; anything else echoes\r\n\r\n");
}

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
