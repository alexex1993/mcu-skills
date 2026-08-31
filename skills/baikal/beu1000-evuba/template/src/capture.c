/**
 * @file    capture.c
 * @brief   Аппаратный захват фронта на PWMA1, канал 0.
 *
 * ## Метка обязана быть аппаратной
 *
 * Программное чтение времени в обработчике добавило бы к метке задержку входа
 * в прерывание, а она гуляет. Захват фиксирует счётчик аппаратно; программе
 * остаётся перенести его на общую шкалу.
 *
 * ## Захват и база — разные таймеры
 *
 * Захват умеет PWMA, и он **16-разрядный**, со своим счётчиком; база живёт на
 * 32-разрядном TIM. Перевод — вычитанием возраста защёлки:
 *
 *     метка = tb_now() − (счётчик_PWMA_сейчас − защёлкнутое)
 *
 * Аппаратная точность при этом сохраняется полностью, а собственная
 * неточность программного чтения вычитается вместе с задержкой обработчика.
 * Разность считается по модулю 2^16, поэтому переполнение счётчика PWMA между
 * защёлкой и обработчиком безвредно.
 *
 * PWMA1 сидит на PCLK2, а база на PCLK0. По умолчанию частоты равны, но
 * делители настраиваются, поэтому отношение считается при инициализации.
 *
 * ## Измерено на этой плате (25 МГц, сигнал 1 кГц)
 *
 * джиттер через прерывание — **80 нс** (2 тика), опросом — 40 нс; потерь ноль
 * за 3001 фронт. Предел режима «оба фронта» — **не чаще одного фронта в
 * 100 мкс**: полярность переключается в обработчике, и импульс короче задержки
 * пропускается целиком. Пропуск даёт не отказ, а правдоподобный мусор — период
 * становится кратным, скважность произвольной.
 */
#include "capture.h"

#include "board.h"
#include "timebase.h"

static volatile cap_stats_t g_st;
static volatile bool        g_have_prev;

/** Отношение частот PWMA и базы, Q16. */
static uint32_t g_ratio_q16 = 1U << 16;

/** Возраст защёлки в тиках базы. Разность по модулю 2^16. */
static uint32_t
age_in_base_ticks (uint16_t latched) {
    const uint16_t now = PWMA_GetCounter(CAP_PWMA);
    const uint16_t age = (uint16_t)(now - latched);
    return (uint32_t)(((uint64_t)age * (uint64_t)g_ratio_q16) >> 16);
}

bool
cap_init (bool rising) {
    CRU_Clocks_TypeDef clocks;
    CRU_GetSystemClocksFreq(&clocks);
    if ((clocks.PCLK2_Frequency == 0U) || (tb_hz() == 0U)) {
        return false;
    }
    g_ratio_q16 = (uint32_t)(((uint64_t)tb_hz() << 16) /
                             (uint64_t)clocks.PCLK2_Frequency);

    CRU_APB2_EnableClock(CAP_CLK);
    /* ⚠ Тактирование порта обязательно, и отдельно от тактирования таймера:
       без него вывод к периферии не подключается, а `CRU_PIN_Init()` при этом
       отрабатывает молча. Наружу отказ выглядит как «захват не видит сигнала»
       при исправном проводе и верных регистрах. */
    CRU_APB1_EnableClock(CAP_GPIO_CLK);

    PWMA_InitStruct_TypeDef t;
    PWMA_StructInit(&t);
    t.CounterMode       = PWMA_COUNTERMODE_UP;
    t.ClockDivision     = PWMA_CLOCKDIVISION_DIV1;
    t.Prescaler         = 0U;      /* тик в тик с шиной: перевод меток точен */
    t.Autoreload        = 0xFFFFU; /* полный диапазон, реже переполнения */
    t.RepetitionCounter = 0U;
    if (PWMA_Init(CAP_PWMA, &t) != SUCCESS) {
        return false;
    }

    /* Всё, на что мы не подписаны, гасится и запрещается **явно**. У PWMA один
       вектор на все источники, и необслуженный флаг при запуске по уровню — не
       «лишнее событие», а бесконечный шторм: обработчик вызывается снова сразу
       после выхода, и программа больше ничего не делает. Переполнение счётчика
       здесь наступает каждые 2,62 мс. */
    PWMA_DisableIT_UPDATE(CAP_PWMA);
    PWMA_DisableIT_TRIG(CAP_PWMA);
    PWMA_DisableIT_COM(CAP_PWMA);
    PWMA_DisableIT_BRK(CAP_PWMA);
    PWMA_ClearFlag_UPDATE(CAP_PWMA);
    PWMA_ClearFlag_TRIG(CAP_PWMA);
    PWMA_ClearFlag_BRK(CAP_PWMA);

    PWMA_EnableCounter(CAP_PWMA);

    /* Запуск по уровню, а не по фронту: линия запроса от PWMA держится, пока
       обработчик не подтвердит приём (`PWMA_ClearIT`). При запуске по фронту
       событие, пришедшее между гашением флага и подтверждением, пропало бы
       целиком — контроллер не увидел бы нового перепада. */
    CLIC_ConfigIRQ(CAP_IRQ,
                   CLIC_INTATTR_MODE_MACHINE,
                   CAP_IRQ_LEVEL,
                   CAP_IRQ_PRIO,
                   CLIC_INTATTR_SHV_VECTORED,
                   CLIC_INTATTR_TRIG_TYPE_LEVEL,
                   CLIC_INTATTR_TRIG_POL_P);
    CLIC_EnableIRQ(CAP_IRQ);

    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port          = CAP_PORT;
    p.Pin           = CAP_PIN;
    p.Pull          = CRU_PIN_PULL_NO;
    p.InputCtrl     = ENABLE; /* вход обязателен: иначе захватывать нечего */
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = CAP_AF;
    CRU_PIN_Init(&p);

    PWMA_IC_InitStruct_TypeDef ic;
    PWMA_IC_StructInit(&ic);
    ic.ICPolarity    = rising ? PWMA_IC_POLARITY_RISING : PWMA_IC_POLARITY_FALLING;
    ic.ICActiveInput = PWMA_IC_ACTIVEINPUT_DIRECTTI;
    ic.ICPrescaler   = PWMA_IC_PRESCALER_DIV1; /* делить фронты нельзя */
    ic.ICFilter      = CAP_FILTER;
    PWMA_IC_Config(CAP_PWMA, CAP_CH, &ic);

    cap_reset();
    PWMA_CC_EnableChannel(CAP_PWMA, CAP_CH);
    PWMA_EnableIT_CC0(CAP_PWMA);
    return true;
}

void
cap_reset (void) {
    g_st.count      = 0U;
    g_st.lost       = 0U;
    g_st.last       = 0U;
    g_st.period     = 0U;
    g_st.period_min = 0xFFFFFFFFU;
    g_st.period_max = 0U;
    g_have_prev     = false;
}

void
cap_get (cap_stats_t *out) {
    out->count      = g_st.count;
    out->lost       = g_st.lost;
    out->last       = g_st.last;
    out->period     = g_st.period;
    out->period_min = g_st.period_min;
    out->period_max = g_st.period_max;
}

/**
 * Обработчик PWMA1: у блока одно прерывание на все каналы.
 *
 * Порядок здесь существеннее содержания. У PWMA между флагами состояния и
 * линией запроса к контроллеру стоит собственная защёлка, и гашение флагов её
 * **не снимает**: линия остаётся поднятой, сколько бы регистр состояния ни
 * читался нулём. Снимает только запись `INTR_CLEAR` — разряд 15 регистра
 * `DIER`, то есть регистра **разрешений**. В HAL это `PWMA_ClearIT()`, и она
 * обязана быть последней строкой.
 *
 * Без неё: запуск по уровню — плата зависает (обработчик вызывается
 * бесконечно), запуск по фронту — ровно одно срабатывание за всё время.
 */
void __attribute__((interrupt))
CAP_IRQ_HANDLER (void) {
    /* Флаги, на которые мы не подписаны, всё равно взводятся аппаратно и
       держат линию запроса. Гасим здесь же — дешевле, чем потом разбираться,
       почему плата перестала отвечать. */
    if (PWMA_IsActiveFlag_UPDATE(CAP_PWMA) != 0U) {
        PWMA_ClearFlag_UPDATE(CAP_PWMA);
    }
    if (PWMA_IsActiveFlag_TRIG(CAP_PWMA) != 0U) {
        PWMA_ClearFlag_TRIG(CAP_PWMA);
        PWMA_ClearFlag_BRK(CAP_PWMA);
    }
    /* ⚠ Флаг повторного захвата гасится, но наружу не отдаётся: на этой плате
       он стоит **на каждом** захвате, притом что не потеряно ничего (3001
       фронт, период ровно 25 000 тиков, разброс один тик). Расхождение с RM
       осталось невыясненным; принимать его за потерю нельзя. */
    if (PWMA_IsActiveFlag_CC0OVR(CAP_PWMA) != 0U) {
        PWMA_ClearFlag_CC0OVR(CAP_PWMA);
    }

    if (PWMA_IsActiveFlag_CC0(CAP_PWMA) != 0U) {
        const uint16_t latched = PWMA_IC_GetCapture(CAP_PWMA, CAP_CH);
        PWMA_ClearFlag_CC0(CAP_PWMA);

        const uint64_t stamp = tb_now() - (uint64_t)age_in_base_ticks(latched);
        if (g_have_prev) {
            const uint32_t d = (uint32_t)(stamp - g_st.last);
            g_st.period = d;
            if (d < g_st.period_min) {
                g_st.period_min = d;
            }
            if (d > g_st.period_max) {
                g_st.period_max = d;
            }
        }
        g_st.last   = stamp;
        g_have_prev = true;
        g_st.count++;
    }

    /* Подтверждение приёма. Фронт, пришедший между гашением флага и этой
       записью, не теряется: он взведёт флаг заново, запрос по уровню
       поднимется снова и будет обслужен следующим вызовом. */
    PWMA_ClearIT(CAP_PWMA);
}
