# BE-U1000 / EVU-BA-2.1 — готовый код

Всё, что ниже, **собирается**: разделы 1–7 извлечены из `template/` (не
перенабраны), разделы 8–11 — из порта EHAL проекта ЭБУ, где они измерены на
этой же плате. Отличие важно: код шаблона проверяется одной командой `make`,
код разделов 8–11 приведён фрагментами и требует своих определений из `board.h`.

---

## 1. Makefile

```make
BAIKAL_SDK ?=
ifeq ($(BAIKAL_SDK),)
$(error Задайте BAIKAL_SDK — путь к SDK (каталог с Drivers/, BSP/, Tools/))
endif
SDK_DIR := $(BAIKAL_SDK)

BOARD = EVU_BA_2_1        # подключает BSP платы
SRC_AUTOSEARCH = 0        # никакого автопоиска: иначе variants/ даст второй main()

INC_DIR += $(CURDIR)/include
CSRC    += $(wildcard $(CURDIR)/src/*.c)

CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_cru.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_gpio.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_uart.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_tim.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_pwma.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_adc.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_dma.c
CSRC += $(SDK_DIR)/Projects/_template/syscalls.c

.DEFAULT_GOAL := all
include $(SDK_DIR)/Tools/build/common.mk

# Зависимости заголовков: common.mk ищет main.d, компилятор пишет main.o.d.
-include $(wildcard $(OBJ_DIR)/*.o.d)
```

Сборка под TCM — **после `make clean`**:

```sh
make clean BAIKAL_SDK=...
MEM_REG_ROM=TCMA MEM_REG_RAM=TCMB make BAIKAL_SDK=...
grep ORIGIN output/debug/generated.ld    # под TCM должно быть 0x40010000
```

## 2. Вывод: `board.h`

Смысл файла — единственное место, где записано «что чем делается». Полностью —
`template/include/board.h`; вот форма, которую стоит копировать:

```c
/* Светодиод LD1: PC0, активен высоким. Кнопка SB1: PC13, нажата = ноль. */
#define LED_GPIO      GPIO2
#define LED_GPIO_PIN  GPIO_PIN_0
#define LED_CRU_PORT  CRU_PORT_C
#define LED_CRU_PIN   CRU_PIN_0
#define LED_CLK       CRU_APB2_PERIPH_GPIO2

/* Консоль: UART0 на PA6/PA7, альтернативная функция #1, разъём XS2. */
#define CON_UART      UART0
#define CON_UART_CLK  CRU_APB0_PERIPH_UART0
#define CON_GPIO_CLK  CRU_APB0_PERIPH_GPIO0
#define CON_PORT      CRU_PORT_A
#define CON_PIN_TX    CRU_PIN_6
#define CON_PIN_RX    CRU_PIN_7
#define CON_AF        CRU_PIN_AF_1
#define CON_BAUD      115200U

/* Захват: PWMA1 канал 0 → PB8, АФ #4, XP10 контакт 13. */
#define CAP_PWMA        PWMA1
#define CAP_CH          PWMA_CH0
#define CAP_CLK         CRU_APB2_PERIPH_PWMA1
#define CAP_IRQ         CLIC_PWMA1_IRQn
#define CAP_IRQ_HANDLER PWMA1_IRQHandler
#define CAP_PORT        CRU_PORT_B
#define CAP_PIN         CRU_PIN_8
#define CAP_AF          CRU_PIN_AF_4
#define CAP_FILTER      PWMA_IC_FILTER_FDIV1_N8

void __attribute__((interrupt)) CAP_IRQ_HANDLER(void);
```

Прототип обработчика объявляется здесь потому, что вызывается он таблицей
векторов, а не кодом: без прототипа компилятор справедливо ругается, а имя
обязано совпадать с тем, что ждёт `startup_c0_c1.S`.

## 3. Вывод в GPIO

```c
CRU_APB2_EnableClock(LED_CLK);

CRU_PIN_InitStruct_TypeDef p;
CRU_PIN_StructInit(&p);
p.Port          = LED_CRU_PORT;
p.Pin           = LED_CRU_PIN;
p.Pull          = CRU_PIN_PULL_NO;
p.InputCtrl     = DISABLE;          /* выходу входной буфер не нужен */
p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
p.Alternate     = CRU_PIN_AF_0;     /* 0 — обычный GPIO */
CRU_PIN_Init(&p);

GPIO_InitStruct_TypeDef g;
GPIO_StructInit(&g);
g.PinMask = LED_GPIO_PIN;
g.Mode    = GPIO_MODE_OUTPUT;
GPIO_Init(LED_GPIO, &g);

GPIO_ToggleOutputPin(LED_GPIO, LED_GPIO_PIN);
```

Для входа (кнопка) — `Pull = CRU_PIN_PULL_UP`, `InputCtrl = ENABLE`,
`Mode = GPIO_MODE_INPUT`, и «нажата» — это **ноль**.

## 4. Консоль UART0 (`printf` работает)

`syscalls.c` из SDK направляет `_write` в слабый `__io_putchar`, поэтому
достаточно определить его. Полностью — `template/src/console.c`.

```c
int
__io_putchar (int ch) {
    while ((UART_GetLineStatus(CON_UART) & UART_LINE_STATUS_THRE) == 0UL) {
    }
    UART_TransmitData8b(CON_UART, (uint8_t)ch);
    return ch;
}

void
con_init (void) {
    CRU_APB0_EnableClock(CON_UART_CLK);
    CRU_APB0_EnableClock(CON_GPIO_CLK);

    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port = CON_PORT; p.Pin = CON_PIN_TX;
    p.Pull = CRU_PIN_PULL_NO; p.InputCtrl = DISABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0; p.Alternate = CON_AF;
    CRU_PIN_Init(&p);

    CRU_PIN_StructInit(&p);
    p.Port = CON_PORT; p.Pin = CON_PIN_RX;
    p.Pull = CRU_PIN_PULL_UP;   /* оборванный кабель ⇒ тишина, а не мусор */
    p.InputCtrl = ENABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0; p.Alternate = CON_AF;
    CRU_PIN_Init(&p);

    (void)UART_DeInit(CON_UART);

    UART_InitStruct_TypeDef init;
    UART_StructInit(&init);
    init.BaudRate        = CON_BAUD;
    init.DataWidth       = UART_DATAWIDTH_8B;
    init.Parity          = UART_PARITY_NONE;
    init.StopBits        = UART_STOP_1BIT;
    init.CtrlFIFO        = ENABLE;
    init.TxFIFOThreshold = UART_TX_FIFO_EMPTY;
    init.RxFIFOThreshold = UART_RX_FIFO_CHAR_1;
    (void)UART_Init(CON_UART, &init);
}

int
con_getc (void) {   /* не блокирует: -1, если байта нет */
    if (UART_IsActiveFlag(CON_UART, UART_FLAG_RFNE) == 0UL) {
        return -1;
    }
    return (int)UART_ReceiveData8b(CON_UART);
}
```

⚠ Печать опросом блокирует ядро: 87 мкс на байт при 115200, то есть строка в
60 байт — 5 мс. В угловом домене так печатать нельзя; там нужна очередь и
прерывание, и обёртка обязана **дожидаться места** (неблокирующая запись берёт
столько, сколько влезло, и молча теряет остальное).

## 5. Временная база на TIM0: 64 разряда

Полностью — `template/src/timebase.c`.

```c
static volatile uint32_t g_high;      /* старшие разряды, растит обработчик */

static inline uint32_t up_counter(void) {
    return ~TIM_GetCounter(TB_TIM, TB_CH);   /* TIM считает вниз */
}
static inline bool overflow_pending(void) {
    return (TB_TIM->RAWINTSTAT & (1UL << (uint32_t)TB_CH)) != 0UL;
}

bool tb_init(void) {
    CRU_Clocks_TypeDef clocks;
    CRU_GetSystemClocksFreq(&clocks);
    g_hz = clocks.PCLK0_Frequency;

    CRU_APB0_EnableClock(TB_CLK);
    TIM_DeInit(TB_TIM, TB_CH);

    TIM_InitStruct_TypeDef init;
    TIM_StructInit(&init);
    init.CounterMode = TIM_COUNTERMODE_FREE_RUNNING;
    init.LoadCount   = 0xFFFFFFFFUL;
    if (TIM_Init(TB_TIM, TB_CH, &init) != SUCCESS) { return false; }

    TIM_EnableIT(TB_TIM, TB_CH);
    CLIC_ConfigIRQ(TB_IRQ, CLIC_INTATTR_MODE_MACHINE, TB_IRQ_LEVEL, TB_IRQ_PRIO,
                   CLIC_INTATTR_SHV_VECTORED, CLIC_INTATTR_TRIG_TYPE_LEVEL,
                   CLIC_INTATTR_TRIG_POL_P);
    CLIC_EnableIRQ(TB_IRQ);
    TIM_EnableChannel(TB_TIM, TB_CH);
    return true;
}

uint64_t tb_now(void) {
    uint32_t hi, lo, hi2;
    do {                       /* пара «старшая-младшая» обязана быть согласованной */
        hi  = g_high;
        lo  = up_counter();
        hi2 = g_high;
    } while (hi != hi2);

    /* Окно между переполнением и его обработкой: без поправки время скакнуло
       бы назад на 2^32 тика. Регистр читается только вблизи нуля — он стоит
       ~37 тактов, а функция вызывается на каждом событии. */
    if ((lo < (1UL << 24)) && overflow_pending()) { hi++; }

    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

void __attribute__((interrupt)) TB_IRQ_HANDLER(void) {
    g_high++;
    TIM_ClearIT(TB_TIM, TB_CH);     /* у TIM подтверждение выглядит собой */
}
```

## 6. Захват фронта на PWMA

Полностью — `template/src/capture.c`. Ключевых мест три.

**Инициализация блока: гасить и запрещать всё, на что не подписан.** И первым
делом — тактирование: **и таймера, и порта, к которому принадлежит вывод**.

```c
CRU_APB2_EnableClock(CAP_CLK);        /* PWMA1 */
CRU_APB1_EnableClock(CAP_GPIO_CLK);   /* ⚠ порт B; без него захват молчит */

PWMA_InitStruct_TypeDef t;
PWMA_StructInit(&t);
t.CounterMode   = PWMA_COUNTERMODE_UP;
t.ClockDivision = PWMA_CLOCKDIVISION_DIV1;
t.Prescaler     = 0U;        /* тик в тик с шиной: перевод меток точен */
t.Autoreload    = 0xFFFFU;
PWMA_Init(CAP_PWMA, &t);

PWMA_DisableIT_UPDATE(CAP_PWMA);   PWMA_DisableIT_TRIG(CAP_PWMA);
PWMA_DisableIT_COM(CAP_PWMA);      PWMA_DisableIT_BRK(CAP_PWMA);
PWMA_ClearFlag_UPDATE(CAP_PWMA);   PWMA_ClearFlag_TRIG(CAP_PWMA);
PWMA_ClearFlag_BRK(CAP_PWMA);
PWMA_EnableCounter(CAP_PWMA);

PWMA_IC_InitStruct_TypeDef ic;
PWMA_IC_StructInit(&ic);
ic.ICPolarity    = PWMA_IC_POLARITY_RISING;
ic.ICActiveInput = PWMA_IC_ACTIVEINPUT_DIRECTTI;
ic.ICPrescaler   = PWMA_IC_PRESCALER_DIV1;   /* делить фронты нельзя */
ic.ICFilter      = PWMA_IC_FILTER_FDIV1_N8;  /* отсекает короче ~320 нс */
PWMA_IC_Config(CAP_PWMA, CAP_CH, &ic);
PWMA_CC_EnableChannel(CAP_PWMA, CAP_CH);
PWMA_EnableIT_CC0(CAP_PWMA);
```

**Перевод метки на общую шкалу** — вычитанием возраста защёлки:

```c
static uint32_t age_in_base_ticks(uint16_t latched) {
    const uint16_t now = PWMA_GetCounter(CAP_PWMA);
    const uint16_t age = (uint16_t)(now - latched);   /* по модулю 2^16 */
    return (uint32_t)(((uint64_t)age * (uint64_t)g_ratio_q16) >> 16);
}
/* g_ratio_q16 = (tb_hz() << 16) / PCLK2: PWMA на APB2, база на APB0 */
```

**Обработчик, и порядок в нём:**

```c
void __attribute__((interrupt)) CAP_IRQ_HANDLER(void) {
    if (PWMA_IsActiveFlag_UPDATE(CAP_PWMA)) { PWMA_ClearFlag_UPDATE(CAP_PWMA); }
    if (PWMA_IsActiveFlag_TRIG(CAP_PWMA))   { PWMA_ClearFlag_TRIG(CAP_PWMA);
                                              PWMA_ClearFlag_BRK(CAP_PWMA); }
    if (PWMA_IsActiveFlag_CC0OVR(CAP_PWMA)) { PWMA_ClearFlag_CC0OVR(CAP_PWMA); }

    if (PWMA_IsActiveFlag_CC0(CAP_PWMA)) {
        const uint16_t latched = PWMA_IC_GetCapture(CAP_PWMA, CAP_CH);
        PWMA_ClearFlag_CC0(CAP_PWMA);
        const uint64_t stamp = tb_now() - (uint64_t)age_in_base_ticks(latched);
        /* … разбор события … */
    }

    PWMA_ClearIT(CAP_PWMA);   /* ⚠ последней строкой, иначе линия не опустится */
}
```

⚠ Флаг повторного захвата (`CC0OVR`) на этой плате стоит **на каждом** захвате
при нулевых потерях — наружу его отдавать нельзя, иначе получится «потеря
события» на каждом фронте.

## 7. Импульс заданной ширины на PWMA

Полностью — `template/src/evout.c`. Импульс складывается **из двух совпадений**.

```c
bool evo_pulse_us(uint32_t lead_us, uint32_t width_us) {
    const uint64_t hz    = (uint64_t)g_pwma_hz;
    const uint64_t lead  = ((uint64_t)lead_us  * hz) / 1000000U;
    const uint64_t width = ((uint64_t)width_us * hz) / 1000000U;
    if ((lead + width) >= (uint64_t)EVO_WINDOW_TICKS) {
        return false;          /* дальше окна счётчика: нужен приём с пробуждением */
    }

    /* «Сейчас» берётся рядом с чтением счётчика, внутри этой функции:
       вынесенное наружу, оно разъезжается с опорой, и разность уходит
       целиком в ширину импульса. */
    const uint16_t base = PWMA_GetCounter(EVO_PWMA);
    const uint16_t on   = (uint16_t)(base + (uint16_t)lead);

    PWMA_OC_SetCompare(EVO_PWMA, EVO_CH, on);
    PWMA_OC_SetMode(EVO_PWMA, EVO_CH, PWMA_OC_MODE_ACTIVE);
    PWMA_ClearFlag_CC0(EVO_PWMA);
    PWMA_EnableIT_CC0(EVO_PWMA);

    /* Не ушёл ли счётчик мимо, пока мы считали: промах — это 2,62 мс лишнего
       импульса. Разность по модулю 2^16. */
    const uint16_t left = (uint16_t)(on - PWMA_GetCounter(EVO_PWMA));
    if ((left == 0U) || (left > EVO_WINDOW_TICKS)) {
        PWMA_DisableIT_CC0(EVO_PWMA);
        drive_passive();
        return false;
    }
    return true;
}
```

В обработчике по первому совпадению записывается второе:

```c
PWMA_OC_SetCompare(EVO_PWMA, EVO_CH, (uint16_t)(on + g_width_ticks));
PWMA_OC_SetMode(EVO_PWMA, EVO_CH, PWMA_OC_MODE_INACTIVE);
```

Настройка канала — с **выключенной** предзагрузкой и с явным пассивным
состоянием:

```c
PWMA_OC_InitStruct_TypeDef oc;
PWMA_OC_StructInit(&oc);
oc.OCMode      = PWMA_OC_MODE_FORCED_INACTIVE;   /* до первой команды — тишина */
oc.OCState     = PWMA_OC_STATE_ENABLE;
oc.OCPolarity  = active_high ? PWMA_OC_POLARITY_HIGH : PWMA_OC_POLARITY_LOW;
oc.OCIdleState = active_high ? PWMA_OC_IDLESTATE_LOW : PWMA_OC_IDLESTATE_HIGH;
PWMA_OC_Config(EVO_PWMA, EVO_CH, &oc);
PWMA_OC_DisablePreload(EVO_PWMA, EVO_CH);   /* ⚠ для ШИМ — наоборот, включить */
```

Снятие выхода в любой момент — `PWMA_OC_MODE_FORCED_INACTIVE`: работает
независимо от полярности и не требует знать состояние.

## 8. АЦП группой через DMA

Полностью — `template/src/adc_dma.c`.

⚠ Массив-приёмник обязан лежать в SRAM: `static volatile uint32_t g_dma[N]`
попадёт туда только при `MEM_REG_RAM=SRAM`. Под `MEM_REG_RAM=TCMA/TCMB` тот же
код даёт ошибку шины и молчащий АЦП.

```c
/* 1. АЦП: секвенсор по рангам, запуск программный, выгрузка в DMA. */
ADC_InitStruct_TypeDef init;
ADC_StructInit(&init);
init.ConversionMode       = ADC_CONV_SINGLE;
init.SequencerScanMode    = ADC_SEQ_SCAN_ENABLE;
init.SequencerLength      = seq_length(ADC_N_CH);
init.SequencerDiscontMode = ADC_SEQ_DISCONT_DISABLE;
init.TriggerSource        = ADC_TRIG_SOFTWARE;
init.DMATransfer          = DISABLE;
ADC_Init(ADC_UNIT, &init);

ADC_SetInputMode(ADC_UNIT, ADC_INPUT_SINGLE_END);
ADC_SetOutputMode(ADC_UNIT, ADC_OUTPUT_DATA_UNSIGNED);
for (uint8_t i = 0U; i < ADC_N_CH; i++) {
    ADC_SetSequencerRanks(ADC_UNIT, rank_of(i), chan[i]);
    ADC_SetChannelSamplingTime(ADC_UNIT, chan[i], ADC_SMP_TIME);
}
ADC_Enable(ADC_UNIT);
ADC_DisableIT_EOC(ADC_UNIT);    /* ⚠ прерывание от АЦП не разрешать вовсе */
ADC_DMA_Enable(ADC_UNIT);

/* 2. DMA: периферия → память, запрос по каждому EOC. */
DMA_InitStruct_TypeDef d;
DMA_StructInit(&d);
d.SrcAddress              = (uint32_t)(uintptr_t)&ADC_UNIT->ADR;
d.SrcIncMode              = DMA_INC_MODE_NO_CHANGE;
d.SrcTransferWidth        = DMA_TR_WIDTH_32BITS;
d.SrcBurstLength          = DMA_BURST_LENGTH_1;
d.SrcHandshake            = DMA_HS_HARDWARE;
d.SrcHWHandshakeInterface = 13U;            /* RM табл. 8-1: ADC_0 RX */
d.DstAddress              = (uint32_t)(uintptr_t)g_dma;
d.DstIncMode              = DMA_INC_MODE_INCREMENT;
d.DstTransferWidth        = DMA_TR_WIDTH_32BITS;
d.DstHandshake            = DMA_HS_SOFTWARE;
d.Direction               = DMA_DIR_PERIPH_TO_MEMORY;
DMA_Init(ADC_DMA, ADC_DMA_CH, &d);
DMA_SetDataLength(ADC_DMA, ADC_DMA_CH, ADC_N_CH);
DMA_EnableIT_IntTfr(ADC_DMA, ADC_DMA_CH);
```

Перед каждым запуском канал DMA перезаряжается (адреса и длина), потом
`ADC_StartConversion()`. Единственное прерывание — от DMA, и **только там**
запись в `CR1` безопасна.

## 9. CAN FD (из порта проекта ЭБУ)

```c
/* Кванты считаются, а не берутся «как обычно»: тактовая 12,5 МГц. */
const uint32_t total = clk / bitrate;          /* 25 при 500 кбит/с */
if ((clk % bitrate) != 0U) { return ERR_RANGE; }
uint32_t tq = 0U, presc = 0U;
for (uint32_t t = 25U; t >= 8U; t--) {         /* наибольшее подходящее */
    if ((total % t) == 0U) { tq = t; presc = total / t; break; }
}
uint32_t ts2 = tq / 4U;                        /* точка выборки ~75 % */
if (ts2 == 0U) { ts2 = 1U; }
const uint32_t ts1 = tq - 1U - ts2;

/* ⚠ Обход дефекта SDK: RequestMode проверяет режим один раз и молча сдаётся. */
static bool set_mode(CANFD_TypeDef *can, CANFD_OpMode_TypeDef mode) {
    for (uint32_t attempt = 0U; attempt < 8U; attempt++) {
        CANFD_RequestMode(can, mode);
        for (uint32_t spin = 0U; spin < 10000U; spin++) {
            if (CANFD_GetMode(can) == mode) { return true; }
        }
    }
    return false;
}

can->ICR = 0xFFFFFFFFU;   /* ⚠ ICR до IER: разрешение при взведённом ISR
                             само поднимает линию запроса (RM §13.2.1.2) */
/* … CANFD_EnableIT(…) … */
```

Фильтр «принимать всё» ставится **при открытии** (нулевая маска, `CtrlIDE`
выключен), иначе порт молча выбрасывает кадры до настройки фильтров.

## 10. Запись eFlash из TCM

Массив недоступен, пока идёт его собственная операция, поэтому примитивы живут
в секции с именем **без точек** — для таких компоновщик сам заводит границы.

```c
#define NVMFUNC __attribute__((section("beu_nvmfunc"), noinline))
extern uint8_t __start_beu_nvmfunc[];
extern uint8_t __stop_beu_nvmfunc[];

NVMFUNC static void raw_program_word(uint32_t addr, uint32_t data) {
    while ((EFLASH->SR & EFLASH_SR_BUSY) != 0U) { }
    EFLASH->ADDR  = (uint16_t)((addr & 0x3FFFFU) >> 2);
    EFLASH->WDATA = data;
    EFLASH->CR    = EFLASH_CR_OP_CODE_WRITE | EFLASH_CR_RUN;
    while ((EFLASH->SR & EFLASH_SR_BUSY) != 0U) { }
}

/* Перенос в TCM и вызов через указатель: прямой вызов исполнялся бы из флеша,
   то есть ровно оттуда, откуда нельзя. */
static bool tcm_init(void) {
    const uint32_t len = (uint32_t)(__stop_beu_nvmfunc - __start_beu_nvmfunc);
    uint8_t *dst = (uint8_t *)(uintptr_t)0x40010000U;
    for (uint32_t i = 0U; i < len; i++) { dst[i] = __start_beu_nvmfunc[i]; }
    __asm volatile("fence.i" ::: "memory");     /* память команд изменилась */

    const uint32_t base = (uint32_t)(uintptr_t)__start_beu_nvmfunc;
    tcm_program_word = (void (*)(uint32_t, uint32_t))(uintptr_t)
        (0x40010000U + ((uint32_t)(uintptr_t)&raw_program_word - base));
    return true;
}
```

Внутри примитивов **нет ни вызовов, ни обращений к глобальным переменным**:
только регистры по абсолютным адресам, которые компилятор материализует парой
`lui`/`addi`. Иначе перенесённый код сослался бы на то, чего в TCM нет.

## 11. Сторожевой таймер

```c
WDT_InitStruct_TypeDef w;
WDT_StructInit(&w);
w.Mode    = WDT_MODE_INTERRUPT;   /* первое истечение — обработчик,
                                     второе — перезагрузка */
w.Timeout = period_index;         /* период 2^(8+i)−1 тактов, потолок 335 мс */
WDT_Init(BEU_WDT, &w);
CLIC_ConfigIRQ(BEU_WDT_IRQ, /* … */);
CLIC_EnableIRQ(BEU_WDT_IRQ);
WDT_Enable(BEU_WDT);
WDT_ReloadCounter(BEU_WDT);
```

⚠ Обработчик истечения **не подтверждает** прерывание: снятый запрос отменил бы
перезагрузку. Это единственное место, где правило «подтвердить приём последней
строкой» нарушается намеренно. Причину сброса хранить в ОЗУ, которое стартовый
код не трогает (например по `_heap_start`): регистра причины у кристалла нет.

## 12. Что стоит запустить первым на незнакомой плате

```sh
variants/new-project.sh ~/проба --minimal      # мигает LD1 — тракт цел
variants/new-project.sh ~/проба2               # консоль, время, захват, импульс, АЦП
```

Полный вариант проверяет захват и импульс **без внешнего генератора**:
перемычка `PC5` (XP8 контакт 6) → `PB8` (XP10 контакт 13), команда `p` в меню,
затем `c` — статистика захвата. Блоки разные (PWMA2 и PWMA1), поэтому измерение
не зависит от измеряемого.
