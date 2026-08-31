/**
 * @file    app.c
 * @brief   Наименьший проект для BE-U1000 на плате EVU-BA-2.1: мигание LD1.
 *
 * Ничего, кроме тактирования порта и вывода: ни прерываний, ни консоли.
 * Смысл — отделить отказ тракта «тулчейн → образ → BootROM → исполнение» от
 * отказа собственного кода. Не мигает это — разбираться в периферии рано.
 *
 * Светодиод LD1 «USER» сидит на `PC0` и **активен высоким** (ТО платы,
 * табл. 3-5). Привязка — в `board.h`, как и у полного варианта.
 *
 * Имя файла не `main.c` намеренно: рядом с `syscalls.c`, который подключает
 * Makefile, в SDK лежит свой `main.c`, и при совпадении имён объектных файлов
 * собирается он.
 */
#include "board.h"

int
main (void) {
    CRU_APB2_EnableClock(LED_CLK);

    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port          = LED_CRU_PORT;
    p.Pin           = LED_CRU_PIN;
    p.Pull          = CRU_PIN_PULL_NO;
    p.InputCtrl     = DISABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = CRU_PIN_AF_0;
    CRU_PIN_Init(&p);

    GPIO_InitStruct_TypeDef g;
    GPIO_StructInit(&g);
    g.PinMask = LED_GPIO_PIN;
    g.Mode    = GPIO_MODE_OUTPUT;
    GPIO_Init(LED_GPIO, &g);

    for (;;) {
        GPIO_ToggleOutputPin(LED_GPIO, LED_GPIO_PIN);
        /* Задержка тактами ядра: временной базы в этом варианте нет. */
        __delay_ms(500UL);
    }

    return 0;
}
