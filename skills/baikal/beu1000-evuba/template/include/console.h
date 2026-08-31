/**
 * @file    console.h
 * @brief   Консоль UART0: вывод через printf, ввод опросом.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

/** Поднять UART0 на 115200 8N1. Зовётся до первого printf. */
void con_init(void);

/** Байт из приёмной очереди либо -1, если её нет. Не блокирует. */
int con_getc(void);

#endif /* CONSOLE_H */
