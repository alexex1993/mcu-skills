/**
 * @file    adc_dma.h
 * @brief   Сбор аналоговых входов группой: ADC0 + DMA0.
 */
#ifndef ADC_DMA_H
#define ADC_DMA_H

#include <stdbool.h>
#include <stdint.h>

#include "board.h"

/** Открыть группу: ADC_N_CH каналов, запуск программный, выгрузка через DMA. */
bool adc_init(void);

/** Запустить преобразование группы. false — предыдущее ещё идёт. */
bool adc_start(void);

/** Готов ли результат. */
bool adc_ready(void);

/**
 * Забрать результаты последней группы.
 * @param out  массив на ADC_N_CH отсчётов
 * @param us   время преобразования группы, мкс (может быть NULL)
 */
void adc_get(uint16_t *out, uint32_t *us);

#endif /* ADC_DMA_H */
