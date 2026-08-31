/**
 * @file    app.c
 * @brief   Шаблон проекта BE-U1000 на плате EVU-BA-2.1: что и в каком порядке.
 *
 * Имя файла не `main.c` намеренно — см. предупреждение в `Makefile`: SDK везёт
 * свой `main.c` в каталоге `Projects/_template`, и при совпадении имён
 * собирается он.
 *
 * Программа поднимает пять подсистем и даёт консольное меню по UART0
 * (115200 8N1, разъём XS2 «JTAG UART0»):
 *
 *   h — помощь            l — переключить светодиод LD1
 *   p — выдать импульс    a — снять аналоговые входы
 *   c — статистика захвата (обнуление — C)
 *   s — состояние
 *
 * Импульс и захват проверяются **без внешнего генератора**: перемычка
 * PC5 (XP8 контакт 6) → PB8 (XP10 контакт 13) замыкает выход на вход. Блоки
 * при этом разные (PWMA2 и PWMA1), поэтому измерение не зависит от
 * измеряемого.
 *
 * ## Порядок инициализации, и почему он такой
 *
 * 1. Консоль — первой: без неё любая последующая неудача выглядит одинаково
 *    («плата молчит»).
 * 2. Временная база — до всего, что берёт метки времени.
 * 3. `CLIC_Config()` и `__enable_irq()` — **до** разрешения любого источника.
 *    Забытый `__enable_irq()` даёт молчащую плату, которая выглядит зависшей;
 *    забытый `CLIC_Config()` — то же самое.
 *
 * ⚠ Прерывания у этого ядра **не вкладываются**: обработчик GCC входит с
 * закрытым `mstatus.MIE` и наружу его не открывает. Уровни CLIC задают только
 * порядок выбора среди ожидающих. Бюджет по времени считается по самому
 * длинному обработчику в системе, а не по своему.
 */
#include <stdio.h>

#include "adc_dma.h"
#include "board.h"
#include "capture.h"
#include "console.h"
#include "evout.h"
#include "timebase.h"

#define BLINK_PERIOD_US 500000U

static void
led_init (void) {
    CRU_APB2_EnableClock(LED_CLK);

    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port          = LED_CRU_PORT;
    p.Pin           = LED_CRU_PIN;
    p.Pull          = CRU_PIN_PULL_NO;
    p.InputCtrl     = DISABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = CRU_PIN_AF_0;
    CRU_PIN_Init(&p);

    GPIO_InitStruct_TypeDef g;
    GPIO_StructInit(&g);
    g.PinMask = LED_GPIO_PIN;
    g.Mode    = GPIO_MODE_OUTPUT;
    GPIO_Init(LED_GPIO, &g);
}

static void
btn_init (void) {
    CRU_APB2_EnableClock(BTN_CLK);

    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port          = BTN_CRU_PORT;
    p.Pin           = BTN_CRU_PIN;
    /* Кнопка замыкает вывод на землю: без подтяжки вверх вход плавает и
       «нажатие» появляется само. */
    p.Pull          = CRU_PIN_PULL_UP;
    p.InputCtrl     = ENABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = CRU_PIN_AF_0;
    CRU_PIN_Init(&p);

    GPIO_InitStruct_TypeDef g;
    GPIO_StructInit(&g);
    g.PinMask = BTN_GPIO_PIN;
    g.Mode    = GPIO_MODE_INPUT;
    GPIO_Init(BTN_GPIO, &g);
}

/** SB1 «USER»: нажата = ноль на выводе. */
static bool
btn_pressed (void) {
    return (GPIO_ReadInputPort(BTN_GPIO) & BTN_GPIO_PIN) == 0UL;
}

static void
print_help (void) {
    printf("\r\n  h  помощь\r\n"
           "  l  светодиод LD1\r\n"
           "  p  импульс 500 мкс через 200 мкс (PC5 -> перемычка -> PB8)\r\n"
           "  a  аналоговые входы VIN0..VIN3\r\n"
           "  c  статистика захвата, C  сбросить\r\n"
           "  s  состояние\r\n");
}

static void
print_clocks (void) {
    CRU_Clocks_TypeDef c;
    CRU_GetSystemClocksFreq(&c);
    printf("clk: CCLK %lu  PCLK0 %lu  PCLK1 %lu  PCLK2 %lu  HCLK %lu\r\n",
           (unsigned long)c.CCLK_Frequency,
           (unsigned long)c.PCLK0_Frequency,
           (unsigned long)c.PCLK1_Frequency,
           (unsigned long)c.PCLK2_Frequency,
           (unsigned long)c.HCLK_Frequency);
}

static void
print_capture (void) {
    cap_stats_t s;
    cap_get(&s);
    const uint32_t hz = tb_hz();
    printf("capture: n %lu  period %lu tk", (unsigned long)s.count,
           (unsigned long)s.period);
    if (s.count > 1U) {
        printf(" (%lu us)  min %lu  max %lu",
               (unsigned long)((uint64_t)s.period * 1000000U / hz),
               (unsigned long)s.period_min, (unsigned long)s.period_max);
    }
    printf("\r\n");
}

static void
print_adc (void) {
    if (!adc_start()) {
        printf("adc: занят\r\n");
        return;
    }
    /* Ожидание опросом: в шаблоне это нагляднее, чем обратный вызов. Группа из
       четырёх каналов занимает около 72 мкс (18 мкс на канал). */
    const uint64_t deadline = tb_now() + (uint64_t)tb_hz() / 100U; /* 10 мс */
    while (!adc_ready()) {
        if (tb_now() > deadline) {
            printf("adc: нет результата\r\n");
            return;
        }
    }

    uint16_t v[ADC_N_CH];
    uint32_t us = 0U;
    adc_get(v, &us);
    printf("adc: ");
    for (uint8_t i = 0U; i < ADC_N_CH; i++) {
        /* Шкала 12 разрядов на 3,3 В: 0,806 мВ на отсчёт. */
        printf("VIN%u %4u (%4lu mV)  ", (unsigned)i, (unsigned)v[i],
               (unsigned long)(((uint32_t)v[i] * 3300U) / 4095U));
    }
    printf("| %lu us\r\n", (unsigned long)us);
}

int
main (void) {
    con_init();
    printf("\r\n=== BE-U1000 / EVU-BA-2.1 template ===\r\n");
    print_clocks();

    led_init();
    btn_init();

    if (!tb_init()) {
        printf("FATAL: временная база не поднялась\r\n");
        for (;;) {}
    }

    /* Один разряд на уровень и один на приоритет. Больше не нужно: вложенности
       у этого ядра всё равно нет, а поля шире требуют согласования во всех
       вызовах CLIC_ConfigIRQ — значение больше разрядности молча ломает
       прерывание целиком. */
    CLIC_Config(1U, 1U);
    __enable_irq();

    if (!cap_init(true)) {
        printf("WARN: захват не поднялся\r\n");
    }
    if (!evo_init(true)) {
        printf("WARN: импульсный выход не поднялся\r\n");
    }
    if (!adc_init()) {
        printf("WARN: АЦП не поднялось\r\n");
    }

    printf("база %lu Гц, окно импульса %lu мкс\r\n",
           (unsigned long)tb_hz(), (unsigned long)evo_window_us());
    print_help();

    uint64_t next_blink = tb_now();
    bool     led_on     = false;
    bool     btn_prev   = false;

    for (;;) {
        const uint64_t now = tb_now();

        if (now >= next_blink) {
            next_blink = now + ((uint64_t)tb_hz() * BLINK_PERIOD_US) / 1000000U;
            led_on     = !led_on;
            if (led_on) {
                GPIO_SetOutputPin(LED_GPIO, LED_GPIO_PIN);   /* LD1 активен высоким */
            } else {
                GPIO_ResetOutputPin(LED_GPIO, LED_GPIO_PIN);
            }
        }

        const bool btn = btn_pressed();
        if (btn && !btn_prev) {
            printf("SB1 нажата\r\n");
        }
        btn_prev = btn;

        const int ch = con_getc();
        switch (ch) {
            case 'h': print_help(); break;
            case 'l':
                led_on = !led_on;
                if (led_on) {
                    GPIO_SetOutputPin(LED_GPIO, LED_GPIO_PIN);
                } else {
                    GPIO_ResetOutputPin(LED_GPIO, LED_GPIO_PIN);
                }
                printf("LD1 %s\r\n", led_on ? "включён" : "выключен");
                break;
            case 'p':
                if (evo_pulse_us(200U, 500U)) {
                    printf("импульс заряжен, всего выдано %lu\r\n",
                           (unsigned long)evo_done_count());
                } else {
                    printf("импульс отвергнут (занят либо вне окна)\r\n");
                }
                break;
            case 'a': print_adc(); break;
            case 'c': print_capture(); break;
            case 'C': cap_reset(); printf("статистика захвата обнулена\r\n"); break;
            case 's':
                print_clocks();
                printf("время %lu тиков, импульсов %lu, выход %s\r\n",
                       (unsigned long)now, (unsigned long)evo_done_count(),
                       evo_busy() ? "занят" : "свободен");
                print_capture();
                break;
            default: break;
        }
    }
    return 0;
}
