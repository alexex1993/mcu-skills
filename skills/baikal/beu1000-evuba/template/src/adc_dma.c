/**
 * @file    adc_dma.c
 * @brief   Аналоговые входы: ADC0, секвенсор рангов, выгрузка через DMA0.
 *
 * ## Сбор по прерыванию от самого АЦП здесь невозможен
 *
 * Два требования железа несовместимы, оба проверены на плате:
 *
 *   1. Запрос к контроллеру прерываний снимается **только** записью
 *      `CR1.CLRINTRPT` (`ADC_ClearIT_EOC`). Без неё обработчик вызывается
 *      бесконечно, и плата перестаёт отдавать даже очередь UART — снаружи
 *      выглядит мёртвой.
 *   2. Любая запись в `CR1` **обрывает идущую последовательность**: там же
 *      лежат `ADON` и `SWSTART`, а подтверждение делается
 *      чтением-модификацией-записью. Измерено: из шести рангов собиралось три
 *      за два входа в обработчик, дальше тишина при чистом `ASTR`.
 *
 * Подтверждать на каждом ранге — останавливать преобразование; не подтверждать
 * — вешать плату. Выход один: **DMA**. Он переносит `ADR` по каждому `EOC` без
 * процессора, а единственное прерывание берётся по завершении пересылки, когда
 * запись в `CR1` уже безопасна. Прерывание от АЦП не разрешается вовсе.
 *
 * ## Ловушка SDK, на которой плата зависает молча
 *
 * `ADC_StartCalibration()` — **не** запуск калибровки, а выбор источника: она
 * выставляет `ASER.CAL_SEL`, а `ADC_IsCalibrationOnGoing()` читает тот же
 * разряд обратно; железо его не снимает. Ожидание «пока калибровка идёт» вечно
 * по построению. По RM единица подменяет вход внутренним нулём, и измеренный
 * так «ноль» равен 1400…1700 отсчётов — это середина шкалы, а не земля.
 * **Вычитать его нельзя:** висящие входы читаются ниже, разность обрезается, и
 * прогон показывает сплошные нули при исправном АЦП. Настоящее смещение,
 * измеренное на закороченном на землю `VIN0`, равно нулю.
 *
 * ## Измерено на этой плате (25 МГц)
 *
 * **18 мкс на канал**, 0 потерь из 500. Одновременной выборки нет: секвенсор
 * идёт по рангам, и последний канал группы отстаёт от первого примерно на
 * 90 мкс при шести каналах.
 */
#include "adc_dma.h"

#include "timebase.h"

#define ADC_CLK_DIV  ADC_CLOCKDIVISION_DIV4
#define ADC_SMP_TIME ADC_SAMPLINGTIME_50CYCLES

static const ADC_Channels_TypeDef g_chan[ADC_N_CH] = {ADC_CH_LIST};

/** Приёмник DMA: пересылка 32-разрядная, поэтому и массив 32-разрядный. */
static volatile uint32_t g_dma[ADC_N_CH];
static uint16_t          g_val[ADC_N_CH];

static volatile bool     g_busy;
static volatile bool     g_have;
static volatile uint32_t g_t_start;
static volatile uint32_t g_conv_us;

static ADC_SeqScanLength_TypeDef
seq_length (uint8_t n) {
    switch (n) {
        case 1U:  return ADC_SEQ_SCAN_LENGTH_1RANK;
        case 2U:  return ADC_SEQ_SCAN_LENGTH_2RANKS;
        case 3U:  return ADC_SEQ_SCAN_LENGTH_3RANKS;
        default:  return ADC_SEQ_SCAN_LENGTH_4RANKS;
    }
}

static ADC_Rank_TypeDef
rank_of (uint8_t i) {
    switch (i) {
        case 0U:  return ADC_RANK0;
        case 1U:  return ADC_RANK1;
        case 2U:  return ADC_RANK2;
        default:  return ADC_RANK3;
    }
}

static void
dma_setup (void) {
    DMA_Enable(ADC_DMA);
    DMA_DisableChannel(ADC_DMA, ADC_DMA_CH);

    DMA_InitStruct_TypeDef d;
    DMA_StructInit(&d);
    /* Источник — регистр данных АЦП, адрес не меняется. */
    d.SrcAddress              = (uint32_t)(uintptr_t)&ADC_UNIT->ADR;
    d.SrcMode                 = DMA_MODE_NORMAL;
    d.SrcIncMode              = DMA_INC_MODE_NO_CHANGE;
    d.SrcTransferWidth        = DMA_TR_WIDTH_32BITS;
    /* По одному значению за раз: запрос приходит на каждый `EOC`, копить не в чем. */
    d.SrcBurstLength          = DMA_BURST_LENGTH_1;
    d.SrcHandshake            = DMA_HS_HARDWARE;
    d.SrcHWHandshakeInterface = ADC_DMA_HS;
    d.SrcHWHandshakePolarity  = DMA_HS_POLARITY_HIGH;

    d.DstAddress             = (uint32_t)(uintptr_t)g_dma;
    d.DstMode                = DMA_MODE_NORMAL;
    d.DstIncMode             = DMA_INC_MODE_INCREMENT;
    d.DstTransferWidth       = DMA_TR_WIDTH_32BITS;
    d.DstBurstLength         = DMA_BURST_LENGTH_1;
    /* Память в подтверждении не нуждается: связь нужна только со стороны
       периферии, и «программная» здесь означает «никакой». */
    d.DstHandshake           = DMA_HS_SOFTWARE;
    d.DstHWHandshakePolarity = DMA_HS_POLARITY_HIGH;

    d.Direction         = DMA_DIR_PERIPH_TO_MEMORY;
    d.Priority          = DMA_PRIORITY_0;
    d.FIFOMode          = DMA_FIFO_MODE_0;
    d.LinkedListPointer = 0U;

    (void)DMA_Init(ADC_DMA, ADC_DMA_CH, &d);
    DMA_SetDataLength(ADC_DMA, ADC_DMA_CH, ADC_N_CH);

    DMA_ClearIT_IntTfr(ADC_DMA, ADC_DMA_CH);
    DMA_EnableIT_IntTfr(ADC_DMA, ADC_DMA_CH);

    CLIC_ConfigIRQ(ADC_DMA_IRQ,
                   CLIC_INTATTR_MODE_MACHINE,
                   ADC_IRQ_LEVEL,
                   ADC_IRQ_PRIO,
                   CLIC_INTATTR_SHV_VECTORED,
                   CLIC_INTATTR_TRIG_TYPE_LEVEL,
                   CLIC_INTATTR_TRIG_POL_P);
    CLIC_EnableIRQ(ADC_DMA_IRQ);
}

bool
adc_init (void) {
    CRU_APB2_EnableClock(ADC_CLK);

    ADC_InitStruct_TypeDef init;
    ADC_StructInit(&init);
    init.ConversionMode       = ADC_CONV_SINGLE; /* группу запускает программа */
    init.SequencerScanMode    = ADC_SEQ_SCAN_ENABLE;
    init.SequencerLength      = seq_length(ADC_N_CH);
    init.SequencerDiscontMode = ADC_SEQ_DISCONT_DISABLE;
    init.TriggerSource        = ADC_TRIG_SOFTWARE;
    init.DMATransfer          = DISABLE;
    if (ADC_Init(ADC_UNIT, &init) != SUCCESS) {
        return false;
    }

    ADC_SetClockDivision(ADC_UNIT, ADC_CLK_DIV);
    ADC_SetInputMode(ADC_UNIT, ADC_INPUT_SINGLE_END);
    ADC_SetOutputMode(ADC_UNIT, ADC_OUTPUT_DATA_UNSIGNED);

    for (uint8_t i = 0U; i < ADC_N_CH; i++) {
        ADC_SetSequencerRanks(ADC_UNIT, rank_of(i), g_chan[i]);
        ADC_SetChannelSamplingTime(ADC_UNIT, g_chan[i], ADC_SMP_TIME);
    }

    ADC_Enable(ADC_UNIT);
    ADC_ClearFlag_All(ADC_UNIT);

    /* Прерывание от самого АЦП не разрешается: за всю последовательность
       процессор не должен трогать `CR1`. */
    ADC_DisableIT_EOC(ADC_UNIT);
    ADC_DMA_Enable(ADC_UNIT);

    dma_setup();

    g_busy = false;
    g_have = false;
    return true;
}

bool
adc_start (void) {
    if (g_busy) {
        /* Запуск поверх идущего преобразования сбил бы счёт рангов, а наружу
           это вышло бы перепутанными каналами — отказ правдоподобный и потому
           опасный. */
        return false;
    }

    /* Канал перезаряжается каждый раз: пересылка блочная и по завершении
       останавливается сама, а адрес приёмника ушёл вперёд на длину блока. */
    DMA_DisableChannel(ADC_DMA, ADC_DMA_CH);
    DMA_SetSrcAddress(ADC_DMA, ADC_DMA_CH, (uint32_t)(uintptr_t)&ADC_UNIT->ADR);
    DMA_SetDstAddress(ADC_DMA, ADC_DMA_CH, (uint32_t)(uintptr_t)g_dma);
    DMA_SetDataLength(ADC_DMA, ADC_DMA_CH, ADC_N_CH);
    DMA_ClearIT_IntTfr(ADC_DMA, ADC_DMA_CH);
    DMA_EnableChannel(ADC_DMA, ADC_DMA_CH);

    g_busy    = true;
    g_t_start = (uint32_t)tb_now();
    ADC_ClearFlag_EOC(ADC_UNIT);
    ADC_StartConversionSWStart(ADC_UNIT);
    return true;
}

bool
adc_ready (void) {
    return g_have && !g_busy;
}

void
adc_get (uint16_t *out, uint32_t *us) {
    for (uint8_t i = 0U; i < ADC_N_CH; i++) {
        g_val[i] = (uint16_t)(g_dma[i] & 0x0FFFU);
        out[i]   = g_val[i];
    }
    if (us != NULL) {
        *us = g_conv_us;
    }
}

/** Завершение пересылки: группа собрана целиком. */
void __attribute__((interrupt))
ADC_DMA_HANDLER (void) {
    /* Аксессора «пришло ли завершение пересылки» у SDK нет — читается сырой
       регистр состояния. Гасится оно `DMA_ClearIT_IntTfr()`. */
    if (READ_BIT(ADC_DMA->INT.RAW_TFR, (1UL << (uint32_t)ADC_DMA_CH)) != 0U) {
        DMA_ClearIT_IntTfr(ADC_DMA, ADC_DMA_CH);
        const uint32_t dt = (uint32_t)tb_now() - g_t_start;
        g_conv_us = (uint32_t)(((uint64_t)dt * 1000000U) / (uint64_t)tb_hz());
        g_busy    = false;
        g_have    = true;
    }
}
