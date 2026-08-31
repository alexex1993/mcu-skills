/**
 * @file    timebase.h
 * @brief   Единая временная база на TIM0: 64 разряда, тики PCLK0.
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdbool.h>
#include <stdint.h>

/** Запустить базу. false — не удалось настроить TIM. */
bool tb_init(void);

/** Текущее время в тиках. Монотонно, не переполняется за время жизни платы. */
uint64_t tb_now(void);

/** Частота базы, Гц (равна PCLK0). */
uint32_t tb_hz(void);

/** Задержка опросом. Только для инициализации: занимает процессор целиком. */
void tb_delay_us(uint32_t us);

#endif /* TIMEBASE_H */
