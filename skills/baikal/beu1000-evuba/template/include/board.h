/**
 * @file    board.h
 * @brief   Привязка к плате EVU-BA-2.1: что делает какая периферия и на каком
 *          выводе она сидит.
 *
 * Один файл на всю плату. Модули (`timebase.c`, `capture.c`, …) знают только
 * эти имена, поэтому переезд на другой вывод или другой блок — правка здесь,
 * а не поиск по проекту.
 *
 * Номера контактов разъёмов — по ТО платы, рис. 3-6…3-8. Альтернативные
 * функции — по таблицам функций выводов RM (регистры IOAFCR*).
 */
#ifndef BOARD_H
#define BOARD_H

#include "bmcu_adc.h"
#include "bmcu_common.h"
#include "bmcu_cru.h"
#include "bmcu_dma.h"
#include "bmcu_gpio.h"
#include "bmcu_pwma.h"
#include "bmcu_tim.h"
#include "bmcu_uart.h"

/* --------------------------------------------------------------------------
 * Светодиод и кнопка (BSP платы, табл. 3-4 и 3-5 ТО)
 *
 * LD1 «USER» — PC0, **активный высокий**.
 * SB1 «USER» — PC13, **нажата = ноль**, вывод настраивается с подтяжкой вверх.
 * SB2 — аппаратный сброс, программе недоступна.
 * -------------------------------------------------------------------------- */
#define LED_GPIO      GPIO2
#define LED_GPIO_PIN  GPIO_PIN_0
#define LED_CRU_PORT  CRU_PORT_C
#define LED_CRU_PIN   CRU_PIN_0
#define LED_CLK       CRU_APB2_PERIPH_GPIO2

#define BTN_GPIO      GPIO2
#define BTN_GPIO_PIN  GPIO_PIN_13
#define BTN_CRU_PORT  CRU_PORT_C
#define BTN_CRU_PIN   CRU_PIN_13
#define BTN_CLK       CRU_APB2_PERIPH_GPIO2

/* --------------------------------------------------------------------------
 * Консоль: UART0, PA6/PA7, альтернативная функция #1
 *
 * На плате к преобразователю USB подключён **только** нулевой UART (разъём
 * XS2 «JTAG UART0»). По нему же идут прошивка и BootROM CLI, поэтому двоичный
 * протокол и текст консоли на этой линии несовместимы: либо одно, либо другое.
 * -------------------------------------------------------------------------- */
#define CON_UART      UART0
#define CON_UART_CLK  CRU_APB0_PERIPH_UART0
#define CON_GPIO_CLK  CRU_APB0_PERIPH_GPIO0
#define CON_PORT      CRU_PORT_A
#define CON_PIN_TX    CRU_PIN_6
#define CON_PIN_RX    CRU_PIN_7
#define CON_AF        CRU_PIN_AF_1
#define CON_BAUD      115200U

/* --------------------------------------------------------------------------
 * Временная база: TIM0, канал 0
 *
 * TIM 32-разрядный, тактируется от PCLK0 и не зависит от того, что делает
 * процессор. Счётчик ядра (`mcycle`) поплыл бы при смене частоты ядра.
 * -------------------------------------------------------------------------- */
#define TB_TIM          TIM0
#define TB_CH           TIM_CH0
#define TB_CLK          CRU_APB0_PERIPH_TIM0
#define TB_IRQ          CLIC_TIM0_Channel0_IRQn
#define TB_IRQ_HANDLER  TIM0_Channel0_IRQHandler
#define TB_IRQ_LEVEL    1U
#define TB_IRQ_PRIO     1U

void __attribute__((interrupt)) TB_IRQ_HANDLER(void);

/* --------------------------------------------------------------------------
 * Захват фронта: PWMA1, канал 0 → PB8, функция #4, разъём XP10 контакт 13
 *
 * На порту B (разъём XP10) из блоков PWMA есть только PWMA1 — выбор задан
 * выводами, а не вкусом.
 * -------------------------------------------------------------------------- */
#define CAP_PWMA        PWMA1
#define CAP_CH          PWMA_CH0
#define CAP_CLK         CRU_APB2_PERIPH_PWMA1
/**
 * Тактирование порта B. ⚠ Без него вывод к периферии **не подключается**, и
 * захват молча не видит ни одного фронта: настройка `CRU_PIN_Init()` проходит,
 * регистры таймера верны, провод исправен — а счётчик стоит на нуле.
 * Найдено на плате 2026-08-31: до этого порт C тактировался попутно, ради
 * светодиода, и отказ был только у порта B.
 */
#define CAP_GPIO_CLK    CRU_APB1_PERIPH_GPIO1
#define CAP_IRQ         CLIC_PWMA1_IRQn
#define CAP_IRQ_HANDLER PWMA1_IRQHandler
#define CAP_PORT        CRU_PORT_B
#define CAP_PIN         CRU_PIN_8
#define CAP_AF          CRU_PIN_AF_4
#define CAP_IRQ_LEVEL   1U
#define CAP_IRQ_PRIO    1U
/** Восемь совпадающих выборок: гасит наводку, отсекает импульсы короче ~320 нс. */
#define CAP_FILTER      PWMA_IC_FILTER_FDIV1_N8

void __attribute__((interrupt)) CAP_IRQ_HANDLER(void);

/* --------------------------------------------------------------------------
 * Аппаратно таймированный импульс: PWMA2, канал 0 → PC5, функция #3,
 * разъём XP8 контакт 6
 *
 * Вывод и захват **намеренно на разных блоках**: у PWMA один вектор прерывания
 * на все каналы блока, и на одном блоке перезарядка компаратора вывода
 * исполнялась бы внутри обработчика захвата.
 *
 * Проверка без генератора: перемычка PC5 (XP8.6) → PB8 (XP10.13). Плата меряет
 * собственный выход собственным захватом, и меряет честно — блоки разные.
 * -------------------------------------------------------------------------- */
#define EVO_PWMA        PWMA2
#define EVO_CH          PWMA_CH0
#define EVO_CLK         CRU_APB2_PERIPH_PWMA2
/** Тактирование порта C — см. предупреждение у `CAP_GPIO_CLK`. */
#define EVO_GPIO_CLK    CRU_APB2_PERIPH_GPIO2
#define EVO_IRQ         CLIC_PWMA2_IRQn
#define EVO_IRQ_HANDLER PWMA2_IRQHandler
#define EVO_PORT        CRU_PORT_C
#define EVO_PIN         CRU_PIN_5
#define EVO_AF          CRU_PIN_AF_3
#define EVO_IRQ_LEVEL   1U
#define EVO_IRQ_PRIO    1U

void __attribute__((interrupt)) EVO_IRQ_HANDLER(void);

/* --------------------------------------------------------------------------
 * Аналоговые входы: ADC0, четыре канала, сбор через DMA0
 *
 * Входов у кристалла восемь (`VIN0…VIN7`, выделенные аналоговые выводы), и это
 * предел кристалла, а не платы: альтернативной функцией портов АЦП не бывает.
 * На XP8 они выведены на контакты 28, 30, 32, 34, 36, 38, 35, 37; «AREF» — 7.
 *
 * Линия связи DMA — RM табл. 8-1: у DMA_0 линия 13 это `ADC_0 RX`,
 * линия 15 — `ADC_1 RX`. Обе выделенные.
 * -------------------------------------------------------------------------- */
#define ADC_UNIT        ADC0
#define ADC_CLK         CRU_APB2_PERIPH_ADC0
#define ADC_N_CH        4U
#define ADC_CH_LIST     ADC_CH0, ADC_CH1, ADC_CH2, ADC_CH3
#define ADC_DMA         DMA0
#define ADC_DMA_CH      DMA_CH0
#define ADC_DMA_HS      13U /**< RM табл. 8-1: ADC_0 RX */
#define ADC_DMA_IRQ     CLIC_DMA0_IRQn
#define ADC_DMA_HANDLER DMA0_IRQHandler
#define ADC_IRQ_LEVEL   1U
#define ADC_IRQ_PRIO    1U

void __attribute__((interrupt)) ADC_DMA_HANDLER(void);

#endif /* BOARD_H */
