/**
 * @file    capture.h
 * @brief   Захват фронта на PWMA1 канал 0 (PB8, XP10 контакт 13).
 */
#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

/** Статистика захвата. Читать под запретом прерываний либо мириться с гонкой. */
typedef struct {
    uint32_t count;      /**< сколько фронтов пришло */
    uint32_t lost;       /**< сколько событий не было разобрано вовремя */
    uint64_t last;       /**< метка последнего фронта, тики базы */
    uint32_t period;     /**< интервал между двумя последними, тики базы */
    uint32_t period_min; /**< минимальный интервал за время работы */
    uint32_t period_max; /**< максимальный */
} cap_stats_t;

/** Открыть захват. `rising` — какой фронт ловим. */
bool cap_init(bool rising);

/** Забрать статистику. */
void cap_get(cap_stats_t *out);

/** Обнулить статистику (разбросы считаются заново). */
void cap_reset(void);

#endif /* CAPTURE_H */
