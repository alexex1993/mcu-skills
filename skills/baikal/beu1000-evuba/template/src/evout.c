/**
 * @file    evout.c
 * @brief   Импульс с аппаратными фронтами на PWMA2, канал 0.
 *
 * ## Импульс складывается из двух совпадений
 *
 * У CAPCOM некоторых микроконтроллеров на канал приходится два регистра
 * сравнения, и режим «установка/сброс» выдаёт готовый импульс без единого
 * прерывания. У PWMA регистр **один**, поэтому:
 *
 *   1. `CCR = t_on`, режим `ACTIVE`  → по совпадению выход встаёт;
 *   2. в обработчике `CCR = t_off`, режим `INACTIVE` → по совпадению падает.
 *
 * Оба фронта всё равно делает компаратор: задержка обработчика в джиттер не
 * превращается, она лишь ограничивает снизу длительность импульса.
 *
 * ## Предзагрузка регистра сравнения выключена — и это решение
 *
 * Событию нельзя ждать начала периода: цель, записанная за 100 мкс до
 * срабатывания, вступила бы в силу через 2,62 мс. У ШИМ то же поле включают по
 * обратной причине — новая скважность не должна вступать в силу посреди
 * импульса. Одно поле, противоположные решения, оба обязательны.
 *
 * ## Счётчик 16-разрядный
 *
 * На 25 МГц это 2,62 мс на оборот, и всё планирование обязано умещаться в
 * окно. Здесь окно проверяется и дальняя цель отвергается — так шаблон
 * остаётся коротким. Боевой код вместо отказа ставит **прицельное
 * пробуждение**: совпадение на заведомо достижимой точке, по которому остаток
 * пересчитывается. Ожидание переполнения счётчика вместо этого даёт слепую
 * полосу: цель чуть дальше окна перепрыгивает через ноль и оказывается «в
 * прошлом» (проверено — импульс выходил длиннее на 128 мкс).
 *
 * ## Измерено на этой плате (25 МГц)
 *
 * ошибка ширины **0 нс**, разброс 0…320 нс, постоянный сдвиг обоих фронтов
 * −1,8 мкс (он одинаков для включения и выключения, поэтому ширины не
 * касается). Зарядка канала стоит **38 мкс** — это цена одного вызова
 * `evo_pulse_us()`, и в плотном угловом домене она главный расход.
 */
#include "evout.h"

#include "board.h"
#include "timebase.h"

/** Окно планирования в тиках PWMA: с запасом внутри оборота счётчика. */
#define EVO_WINDOW_TICKS 0xF000U

typedef enum {
    EVO_IDLE = 0,
    EVO_WAIT_ON,  /**< заряжен фронт включения */
    EVO_WAIT_OFF  /**< импульс идёт, заряжен фронт выключения */
} evo_state_t;

static volatile evo_state_t g_state;
static volatile uint32_t    g_done;
static volatile uint16_t    g_width_ticks;

/** Отношение «тик базы → тик PWMA», Q16. */
static uint32_t g_inv_ratio_q16 = 1U << 16;
static uint32_t g_pwma_hz       = 25000000U;

static void
drive_passive (void) {
    PWMA_OC_SetMode(EVO_PWMA, EVO_CH, PWMA_OC_MODE_FORCED_INACTIVE);
}

bool
evo_init (bool active_high) {
    CRU_Clocks_TypeDef clocks;
    CRU_GetSystemClocksFreq(&clocks);
    if ((clocks.PCLK2_Frequency == 0U) || (tb_hz() == 0U)) {
        return false;
    }
    g_pwma_hz       = clocks.PCLK2_Frequency;
    g_inv_ratio_q16 = (uint32_t)(((uint64_t)clocks.PCLK2_Frequency << 16) /
                                 (uint64_t)tb_hz());

    CRU_APB2_EnableClock(EVO_CLK);
    /* Тактирование порта — см. предупреждение в `capture.c`. Здесь оно
       включалось попутно (на том же порту светодиод), и потому отказ прятался. */
    CRU_APB2_EnableClock(EVO_GPIO_CLK);

    PWMA_InitStruct_TypeDef t;
    PWMA_StructInit(&t);
    t.CounterMode       = PWMA_COUNTERMODE_UP;
    t.ClockDivision     = PWMA_CLOCKDIVISION_DIV1;
    t.Prescaler         = 0U;      /* 40 нс на тик: разрешение важнее дальности */
    t.Autoreload        = 0xFFFFU;
    t.RepetitionCounter = 0U;
    if (PWMA_Init(EVO_PWMA, &t) != SUCCESS) {
        return false;
    }

    /* Урок захвата повторять незачем: всё, на что не подписаны, запрещаем и
       гасим. Переполнение нам тоже не нужно — подписка на него дала бы лишнее
       прерывание 381 раз в секунду. */
    PWMA_DisableIT_UPDATE(EVO_PWMA);
    PWMA_DisableIT_TRIG(EVO_PWMA);
    PWMA_DisableIT_COM(EVO_PWMA);
    PWMA_DisableIT_BRK(EVO_PWMA);
    PWMA_ClearFlag_UPDATE(EVO_PWMA);
    PWMA_ClearFlag_TRIG(EVO_PWMA);
    PWMA_ClearFlag_BRK(EVO_PWMA);

    PWMA_EnableCounter(EVO_PWMA);

    CLIC_ConfigIRQ(EVO_IRQ,
                   CLIC_INTATTR_MODE_MACHINE,
                   EVO_IRQ_LEVEL,
                   EVO_IRQ_PRIO,
                   CLIC_INTATTR_SHV_VECTORED,
                   CLIC_INTATTR_TRIG_TYPE_LEVEL,
                   CLIC_INTATTR_TRIG_POL_P);
    CLIC_EnableIRQ(EVO_IRQ);

    /* Полярность несёт всю разницу между активным высоким и активным низким:
       тогда «включить» — это всегда `ACTIVE`, а «снять» — всегда
       `FORCED_INACTIVE`, и ни одна ветка кода про уровень не знает. */
    PWMA_OC_InitStruct_TypeDef oc;
    PWMA_OC_StructInit(&oc);
    oc.OCMode       = PWMA_OC_MODE_FORCED_INACTIVE;
    oc.OCState      = PWMA_OC_STATE_ENABLE;
    oc.OCNState     = PWMA_OC_STATE_DISABLE;
    oc.CompareValue = 0U;
    oc.OCPolarity   = active_high ? PWMA_OC_POLARITY_HIGH : PWMA_OC_POLARITY_LOW;
    oc.OCNPolarity  = PWMA_OC_POLARITY_HIGH;
    /* Состояние вывода при снятом MOE обязано быть пассивным: аварийное
       отключение снимает MOE, и выход должен упасть, а не замереть как был. */
    oc.OCIdleState  = active_high ? PWMA_OC_IDLESTATE_LOW : PWMA_OC_IDLESTATE_HIGH;
    oc.OCNIdleState = PWMA_OC_IDLESTATE_LOW;
    PWMA_OC_Config(EVO_PWMA, EVO_CH, &oc);

    PWMA_OC_DisablePreload(EVO_PWMA, EVO_CH);
    PWMA_CC_EnableChannel(EVO_PWMA, EVO_CH);

    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port          = EVO_PORT;
    p.Pin           = EVO_PIN;
    p.Pull          = CRU_PIN_PULL_NO;
    p.InputCtrl     = DISABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = EVO_AF;
    CRU_PIN_Init(&p);

    g_state = EVO_IDLE;
    g_done  = 0U;
    PWMA_DisableIT_CC0(EVO_PWMA);
    drive_passive();

    /* Разрешение выходов — общее на блок, и снимается оно аварийным
       отключением. Без повторной установки выход больше не появится. */
    PWMA_EnableAllOutputs(EVO_PWMA);
    return true;
}

uint32_t
evo_window_us (void) {
    return (uint32_t)(((uint64_t)EVO_WINDOW_TICKS * 1000000U) / (uint64_t)g_pwma_hz);
}

bool
evo_busy (void) {
    return g_state != EVO_IDLE;
}

uint32_t
evo_done_count (void) {
    return g_done;
}

bool
evo_pulse_us (uint32_t lead_us, uint32_t width_us) {
    if (g_state != EVO_IDLE) {
        return false;
    }
    if ((lead_us == 0U) || (width_us == 0U)) {
        return false;
    }

    const uint64_t hz    = (uint64_t)g_pwma_hz;
    const uint64_t lead  = ((uint64_t)lead_us * hz) / 1000000U;
    const uint64_t width = ((uint64_t)width_us * hz) / 1000000U;
    if ((lead + width) >= (uint64_t)EVO_WINDOW_TICKS) {
        return false; /* дальняя цель: нужен приём с прицельным пробуждением */
    }

    /* «Сейчас» берётся **рядом** с чтением счётчика, внутри этой функции.
       Вынесенное наружу, оно разъезжается с опорой, и разность целиком уходит
       в ширину импульса: проверено — ошибка ширины уезжала с +40 до −1920 нс,
       разброс с 520 до 7040 нс, притом что «оптимизированная» редакция была
       быстрее. У импульсного выхода важна ширина, а не сдвиг фронта. */
    const uint16_t base = PWMA_GetCounter(EVO_PWMA);
    const uint16_t on   = (uint16_t)(base + (uint16_t)lead);

    g_width_ticks = (uint16_t)width;
    g_state       = EVO_WAIT_ON;

    PWMA_OC_SetCompare(EVO_PWMA, EVO_CH, on);
    PWMA_OC_SetMode(EVO_PWMA, EVO_CH, PWMA_OC_MODE_ACTIVE);
    PWMA_ClearFlag_CC0(EVO_PWMA);
    PWMA_EnableIT_CC0(EVO_PWMA);

    /* Не ушёл ли счётчик мимо, пока мы считали и писали. Разность по модулю
       2^16: «впереди» — малая положительная, «позади» — большая. Промах мимо
       совпадения означал бы 2,62 мс лишнего импульса. */
    const uint16_t left = (uint16_t)(on - PWMA_GetCounter(EVO_PWMA));
    if ((left == 0U) || (left > EVO_WINDOW_TICKS)) {
        PWMA_DisableIT_CC0(EVO_PWMA);
        drive_passive();
        g_state = EVO_IDLE;
        return false;
    }
    return true;
}

/**
 * Обработчик PWMA2.
 *
 * Блок отдельный от захвата не только ради выводов: у PWMA один вектор на все
 * каналы, и на общем блоке перезарядка компаратора вывода исполнялась бы
 * внутри обработчика захвата — самого критичного по времени входа.
 */
void __attribute__((interrupt))
EVO_IRQ_HANDLER (void) {
    if (PWMA_IsActiveFlag_UPDATE(EVO_PWMA) != 0U) {
        PWMA_ClearFlag_UPDATE(EVO_PWMA);
    }

    if (PWMA_IsActiveFlag_CC0(EVO_PWMA) != 0U) {
        PWMA_ClearFlag_CC0(EVO_PWMA);

        if (g_state == EVO_WAIT_ON) {
            /* Фронт включения уже сформирован компаратором. Заряжаем
               выключение от **того же** значения сравнения: так ширина не
               зависит ни от задержки обработчика, ни от чтения счётчика. */
            const uint16_t on  = PWMA_OC_GetCompare(EVO_PWMA, EVO_CH);
            const uint16_t off = (uint16_t)(on + g_width_ticks);
            PWMA_OC_SetCompare(EVO_PWMA, EVO_CH, off);
            PWMA_OC_SetMode(EVO_PWMA, EVO_CH, PWMA_OC_MODE_INACTIVE);
            g_state = EVO_WAIT_OFF;
        } else if (g_state == EVO_WAIT_OFF) {
            /* Фронт выключения сформирован. Принудительный режим ставится
               следом не ради уровня, а ради определённости: канал не должен
               сработать снова, когда счётчик через оборот дойдёт до того же
               значения. */
            PWMA_DisableIT_CC0(EVO_PWMA);
            drive_passive();
            g_state = EVO_IDLE;
            g_done++;
        } else {
            PWMA_DisableIT_CC0(EVO_PWMA);
        }
    }

    /* Подтверждение приёма — последней строкой. См. `capture.c`. */
    PWMA_ClearIT(EVO_PWMA);
}
