/*
 * WeAct Studio "Black Pill" V3.x (STM32F411CEU6) reference firmware.
 *
 *   - PC13 LED blinks every 100 ms (non-blocking, timestamp compare);
 *   - the hardware RTC runs off the external 32.768 kHz LSE crystal (PC14/PC15)
 *     and prints the time with milliseconds every 200 ms over USB CDC;
 *   - the internal temperature sensor and VREFINT are sampled every second and
 *     scaled with the chip's factory calibration values;
 *   - K1 (PA0) is polled and reported on press;
 *   - anything typed into the monitor is echoed back (see CDC_Receive_FS).
 *
 * The LED's anode is on 3V3 and its cathode on PC13, so a LOW level lights it.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "temp_sensor.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#define BLINK_PERIOD_MS   100U
#define REPORT_PERIOD_MS  200U
#define SENSE_PERIOD_MS   1000U

/* Firmware build time in the local zone, used to seed the RTC on the very first
   run. Substitute `date +"%Y %m %d %H %M %S %u"` right before building and the
   only drift is however long `pio run -t upload` takes. */
#define SEED_YEAR     2026
#define SEED_MONTH    8
#define SEED_DAY      22
#define SEED_HOUR     12
#define SEED_MINUTE   0
#define SEED_SECOND   0
#define SEED_WEEKDAY  RTC_WEEKDAY_SATURDAY
#define SEED_TZ_LABEL "UTC+3"

/* Magic value in an RTC backup register. It distinguishes "the clock was
   already running, we merely went through a reset" from "the backup domain
   just came up (VBAT was lost) - reseed it". Backup registers survive a system
   reset and Standby; they do not survive losing both VDD and VBAT. */
#define RTC_BKP_MAGIC 0x32F4U

static RTC_HandleTypeDef hrtc;

static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void KEY_GPIO_Init(void);
static void RTC_ClockSource_Init(void);
static void RTC_Init(void);
static void cdc_printf(const char *fmt, ...);
static void print_banner(void);
static void print_time(void);

int main(void)
{
    HAL_Init();             /* reset peripherals, SysTick at 1 ms            */
    SystemClock_Config();   /* HSE 25 MHz -> PLL -> 96 MHz, PLLQ -> 48 MHz   */
    LED_GPIO_Init();
    KEY_GPIO_Init();
    RTC_ClockSource_Init(); /* LSE 32.768 kHz -> RTC clock                   */
    RTC_Init();
    TempSensor_Init();
    MX_USB_DEVICE_Init();

    uint32_t last_blink  = HAL_GetTick();
    uint32_t last_report = HAL_GetTick();
    uint32_t last_sense  = HAL_GetTick();
    uint8_t  banner_sent = 0;
    uint8_t  key_was     = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* Blink without blocking: compare timestamps, never sleep in HAL_Delay */
        if ((now - last_blink) >= BLINK_PERIOD_MS)
        {
            last_blink += BLINK_PERIOD_MS;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }

        /* Reprint the banner every time the host opens the port (starts a monitor) */
        if (CDC_PortOpen && !banner_sent)
        {
            banner_sent = 1;
            print_banner();
        }
        else if (!CDC_PortOpen && banner_sent)
        {
            banner_sent = 0;    /* port closed - show the banner again next time */
        }

        if ((now - last_report) >= REPORT_PERIOD_MS)
        {
            last_report += REPORT_PERIOD_MS;
            print_time();
        }

        if ((now - last_sense) >= SENSE_PERIOD_MS)
        {
            last_sense += SENSE_PERIOD_MS;
            int32_t deci = TempSensor_ReadDeciC();
            cdc_printf("die %ld.%ld C, VDDA %lu mV\r\n",
                       (long)(deci / 10), (long)(deci < 0 ? -deci % 10 : deci % 10),
                       (unsigned long)TempSensor_ReadVddaMv());
        }

        /* Crude 20 ms debounce is enough for a tactile switch polled this often */
        uint8_t key_is = KEY_PRESSED() ? 1U : 0U;
        if (key_is && !key_was)
        {
            cdc_printf("K1 pressed\r\n");
        }
        key_was = key_is;
    }
}

/*
 * The RTC lives in the backup domain, which is write-protected out of reset.
 * PWR's clock and HAL_PWR_EnableBkUpAccess() must both come first, or the
 * writes to RCC->BDCR are silently dropped and the RTC never ticks.
 */
static void RTC_ClockSource_Init(void)
{
    RCC_OscInitTypeDef       osc    = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();    /* LSE can take seconds to start; timeout is 5 s */
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    periph.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_RTC_ENABLE();
}

static void RTC_Init(void)
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    /* 32768 Hz = (127+1) * (255+1), i.e. exactly 1 Hz out of the two prescalers.
       SynchPrediv also sets the subsecond resolution: 1/256 s ~ 3.9 ms. */
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    /* Clock already set and the backup domain never lost power - leave it
       alone. The RTC keeps counting straight through a software reset. */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == RTC_BKP_MAGIC)
    {
        return;
    }

    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    time.Hours          = SEED_HOUR;
    time.Minutes        = SEED_MINUTE;
    time.Seconds        = SEED_SECOND;
    time.TimeFormat     = RTC_HOURFORMAT12_AM;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler();
    }

    date.WeekDay = SEED_WEEKDAY;
    date.Month   = SEED_MONTH;
    date.Date    = SEED_DAY;
    date.Year    = SEED_YEAR - 2000U;
    if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_BKP_MAGIC);
}

/* Print to USB CDC. If the host is not draining the pipe the endpoint stays
   busy; give up after 20 ms so the blink does not drift. */
static void cdc_printf(const char *fmt, ...)
{
    static char buf[256];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
    {
        return;
    }
    if (len > (int)sizeof(buf))
    {
        len = (int)sizeof(buf);
    }

    uint32_t deadline = HAL_GetTick() + 20U;
    while (CDC_Transmit_FS((uint8_t *)buf, (uint16_t)len) == USBD_BUSY)
    {
        if (HAL_GetTick() > deadline)
        {
            return;     /* host is not reading - drop the line silently */
        }
    }
}

/* Time with milliseconds. GetTime *before* GetDate is mandatory: on the F4 the
   calendar sits behind shadow registers that stay locked until both have been
   read (RM0383, "Reading the calendar"). Read them the other way round and the
   date freezes at whatever it was when the shadow was last unlocked. */
static void print_time(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    /* SubSeconds counts DOWN from SecondFraction to 0 within the current second */
    uint32_t ms = ((time.SecondFraction - time.SubSeconds) * 1000U) /
                  (time.SecondFraction + 1U);

    cdc_printf("%04u-%02u-%02u %02u:%02u:%02u.%03lu (" SEED_TZ_LABEL ")\r\n",
               2000U + date.Year, date.Month, date.Date,
               time.Hours, time.Minutes, time.Seconds, (unsigned long)ms);
}

static void print_banner(void)
{
    uint32_t uid0 = *(uint32_t *)UID_BASE;
    uint32_t uid1 = *(uint32_t *)(UID_BASE + 4U);
    uint32_t uid2 = *(uint32_t *)(UID_BASE + 8U);

    HAL_Delay(50);  /* let the host finish opening the port, else this is lost */

    cdc_printf("\r\n=== STM32F411CEU6 Black Pill ===\r\n");
    cdc_printf("SYSCLK %lu Hz, HCLK %lu Hz, PCLK1 %lu Hz, PCLK2 %lu Hz\r\n",
               HAL_RCC_GetSysClockFreq(), HAL_RCC_GetHCLKFreq(),
               HAL_RCC_GetPCLK1Freq(), HAL_RCC_GetPCLK2Freq());
    cdc_printf("Flash %u KB, HAL v%lu, unique ID %08lX%08lX%08lX\r\n",
               *(uint16_t *)FLASHSIZE_BASE, HAL_GetHalVersion(), uid2, uid1, uid0);
    cdc_printf("RTC source: LSE 32.768 kHz, seeded on first flash.\r\n");
    cdc_printf("Blink every %u ms. Type something - it comes back.\r\n\r\n",
               (unsigned)BLINK_PERIOD_MS);
}

/*
 * 96 MHz, not the chip's 100 MHz maximum. USB_OTG_FS needs exactly 48 MHz off
 * PLLQ, and PLLQ divides the same VCO as PLLP: VCO 192 -> /2 = 96 SYSCLK and
 * /4 = 48 USB. There is no integer pair that yields 100 MHz and 48 MHz at once,
 * so on any Black Pill firmware that uses USB, 96 MHz is the ceiling.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Voltage scale 1 is required above 84 MHz (datasheet Table 14) */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* No crystal fitted? Swap this block for RCC_OSCILLATORTYPE_HSI:
       osc.HSIState = RCC_HSI_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSI; PLLM = 16;
       - same 96/48 MHz, but the HSI's +/-1% drift is outside USB's +/-0.25%. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;   /* 25 MHz / 25 = 1 MHz  (spec: 0.95 .. 2.10)   */
    osc.PLL.PLLN       = 192;  /* 1 MHz * 192 = 192 MHz VCO (spec: 100 .. 432) */
    osc.PLL.PLLP       = RCC_PLLP_DIV2; /* 192 / 2 = 96 MHz SYSCLK            */
    osc.PLL.PLLQ       = 4;    /* 192 / 4 = 48 MHz - mandatory for USB        */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 96 MHz                 */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 48 MHz (50 MHz max)    */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 = 96 MHz                 */
    /* 3 wait states: at VDD 2.7-3.6 V flash runs 30 MHz per state, and
       96 / (3+1) = 24 <= 30. Too few states hard-faults on the first fetch. */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    LED_OFF();  /* set the level before switching the pin to output */

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    /* PC13 is fed through the backup-domain power switch: <= 2 MHz, <= 3 mA */
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);
}

static void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin  = KEY_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;    /* K1 shorts to GND; nothing else holds it high */
    HAL_GPIO_Init(KEY_GPIO_Port, &gpio);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

/* framework-stm32cube ships no stm32f4xx_it.c. Without this, the weak default
   in the startup file spins forever and HAL_Delay()/HAL_GetTick() never advance
   - the classic "board boots, then hangs in the first HAL_Delay". */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
