/**
 * @file    timebase.c
 * @brief   Единая временная база на TIM0 канал 0.
 *
 * ## Почему TIM, а не счётчик тактов ядра
 *
 * TIM 32-разрядный, тактируется от PCLK0 и не останавливается. Счётчик ядра
 * (`mcycle`) считает такты процессора и поплывёт при любой смене частоты ядра
 * — а её меняет уже первая же настройка PLL.
 *
 * ## Две особенности TIM, которые придётся учесть
 *
 * **Он считает вниз.** В режиме free-running счётчик убывает от `LoadCount` и,
 * достигнув нуля, продолжает с `0xFFFFFFFF`. Наружу поэтому отдаётся
 * дополнение: `~counter`.
 *
 * **Он 32-разрядный.** На 25 МГц это 172 секунды. Старшие разряды ведутся
 * программно и наращиваются в обработчике переполнения.
 *
 * ## Гонка, которая стоит минуты времени
 *
 * Прерывание возникает в момент обнуления счётчика, а обработчик выполнится
 * позже. В этом окне младшая часть уже перескочила через ноль, а старшая ещё
 * нет — время скакнуло бы **назад** на 2^32 тика, то есть на минуты. Поэтому
 * чтение само проверяет необработанный флаг (`RAWINTSTAT`) и добавляет
 * старший разряд, если в это окно попало.
 *
 * Регистр периферии читается не всегда, а только когда младшая часть близка к
 * нулю: обращение стоит около 37 тактов, а `tb_now()` зовут на каждое событие.
 */
#include "timebase.h"

#include "board.h"

/** Старшие 32 разряда. Наращивает только обработчик. */
static volatile uint32_t g_high;

/** Частота базы, Гц. Читается один раз при инициализации. */
static uint32_t g_hz;

/** Возрастающий 32-разрядный счётчик из убывающего аппаратного. */
static inline uint32_t
up_counter (void) {
    return ~TIM_GetCounter(TB_TIM, TB_CH);
}

/** Ждёт ли необработанное переполнение. */
static inline bool
overflow_pending (void) {
    return (TB_TIM->RAWINTSTAT & (1UL << (uint32_t)TB_CH)) != 0UL;
}

bool
tb_init (void) {
    CRU_Clocks_TypeDef clocks;
    CRU_GetSystemClocksFreq(&clocks);
    g_hz   = clocks.PCLK0_Frequency;
    g_high = 0U;

    CRU_APB0_EnableClock(TB_CLK);
    TIM_DeInit(TB_TIM, TB_CH);

    TIM_InitStruct_TypeDef init;
    TIM_StructInit(&init);
    init.CounterMode = TIM_COUNTERMODE_FREE_RUNNING;
    /* Полный диапазон: чем длиннее период, тем реже переполнения и тем меньше
       поводов для гонки выше. */
    init.LoadCount = 0xFFFFFFFFUL;
    if (TIM_Init(TB_TIM, TB_CH, &init) != SUCCESS) {
        return false;
    }

    TIM_EnableIT(TB_TIM, TB_CH);
    CLIC_ConfigIRQ(TB_IRQ,
                   CLIC_INTATTR_MODE_MACHINE,
                   TB_IRQ_LEVEL,
                   TB_IRQ_PRIO,
                   CLIC_INTATTR_SHV_VECTORED,
                   CLIC_INTATTR_TRIG_TYPE_LEVEL,
                   CLIC_INTATTR_TRIG_POL_P);
    CLIC_EnableIRQ(TB_IRQ);

    TIM_EnableChannel(TB_TIM, TB_CH);
    return true;
}

uint64_t
tb_now (void) {
    /* Порядок чтения важен: старшая, младшая, старшая повторно. Если между
       ними отработал обработчик, пара рассогласована и чтение повторяется. */
    uint32_t hi;
    uint32_t lo;
    uint32_t hi2;
    do {
        hi  = g_high;
        lo  = up_counter();
        hi2 = g_high;
    } while (hi != hi2);

    if ((lo < (1UL << 24)) && overflow_pending()) {
        hi++;
    }
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

uint32_t
tb_hz (void) {
    return g_hz;
}

void
tb_delay_us (uint32_t us) {
    const uint64_t start = tb_now();
    const uint64_t need  = ((uint64_t)g_hz * (uint64_t)us) / 1000000U;
    while ((tb_now() - start) < need) {
        /* ожидание опросом */
    }
}

/**
 * Переполнение базы.
 *
 * У TIM подтверждение приёма прерывания называется тем, чем является:
 * `TIM_ClearIT()`. У PWMA то же самое спрятано битом `INTR_CLEAR` в регистре
 * **разрешений** — см. `capture.c`.
 */
void __attribute__((interrupt))
TB_IRQ_HANDLER (void) {
    g_high++;
    TIM_ClearIT(TB_TIM, TB_CH);
}
