/**
 * @file    evout.h
 * @brief   Аппаратно таймированный импульс на PWMA2 канал 0 (PC5, XP8 конт. 6).
 */
#ifndef EVOUT_H
#define EVOUT_H

#include <stdbool.h>
#include <stdint.h>

/** Открыть канал. `active_high` — уровень активного состояния на выводе. */
bool evo_init(bool active_high);

/**
 * Выдать один импульс: включение через `lead_us`, длительность `width_us`.
 *
 * Оба фронта формирует компаратор, программа лишь переписывает число между
 * ними. Возвращает false, если канал занят либо `lead_us + width_us` не
 * умещается в окно счётчика (см. `evo_window_us()`).
 */
bool evo_pulse_us(uint32_t lead_us, uint32_t width_us);

/** Окно планирования в микросекундах: один оборот 16-разрядного счётчика. */
uint32_t evo_window_us(void);

/** Идёт ли импульс либо ждёт ли своего фронта. */
bool evo_busy(void);

/** Сколько импульсов доведено до конца. */
uint32_t evo_done_count(void);

#endif /* EVOUT_H */
