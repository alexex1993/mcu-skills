/**
 * @file    console.c
 * @brief   Консоль UART0 на плате EVU-BA-2.1.
 *
 * ## Почему опросом, а не по прерыванию
 *
 * Это шаблон, и здесь важнее, чтобы вывод работал раньше всего остального:
 * консоль опросом печатает даже из отладочной вставки в обработчике. Цена —
 * `printf` блокирует ядро на время передачи (при 115200 это 87 мкс на байт).
 * **В угловом домене так печатать нельзя**: строка в 60 байт займёт 5 мс.
 *
 * Боевой вариант — программная кольцевая очередь плюс прерывание. У него своя
 * ловушка, стоившая полдня: неблокирующая запись берёт столько, сколько влезло
 * в очередь, и возвращает это число. Наивная обёртка теряет вывод **молча**, и
 * выглядит это в точности как зависание платы на той строке, где очередь
 * кончилась. Обёртка обязана дожидаться места.
 *
 * ## Почему printf вообще работает
 *
 * `syscalls.c` из шаблона SDK направляет `_write` в слабый `__io_putchar`,
 * который определён ниже, и возвращает из `_isatty` единицу — поток строчно
 * буферизован и уходит по `\n`. (На К1921ВГ1Т с тем же newlib `_isatty`
 * возвращает ноль, поток буферизуется полностью и `printf` молчит навсегда —
 * то есть на этот механизм полагаться вслепую нельзя.)
 *
 * ## Чего от BootROM ждать не надо
 *
 * В режиме загрузки EFLASH BootROM **не поднимает UART0 и не печатает**: ни
 * тактирования, ни альтернативной функции выводов он не включает. Его `printf`
 * по `0x4000b082` заводится только в режимах UART и USB CDC, где он сам ведёт
 * консоль. Приложение обязано поднимать порт само — этим и занят `con_init()`.
 */
#include "console.h"

#include "board.h"

int
__io_putchar (int ch) {
    while ((UART_GetLineStatus(CON_UART) & UART_LINE_STATUS_THRE) == 0UL) {
        /* ждём места в передающем регистре */
    }
    UART_TransmitData8b(CON_UART, (uint8_t)ch);
    return ch;
}

void
con_init (void) {
    CRU_APB0_EnableClock(CON_UART_CLK);
    CRU_APB0_EnableClock(CON_GPIO_CLK);

    /* Оба вывода настраиваются целиком, а не одной альтернативной функцией:
       приёмнику нужен разрешённый входной буфер (`InputCtrl`), иначе линия
       «молчит» при исправном передатчике на той стороне. */
    CRU_PIN_InitStruct_TypeDef p;
    CRU_PIN_StructInit(&p);
    p.Port          = CON_PORT;
    p.Pin           = CON_PIN_TX;
    p.Pull          = CRU_PIN_PULL_NO;
    p.InputCtrl     = DISABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = CON_AF;
    CRU_PIN_Init(&p);

    CRU_PIN_StructInit(&p);
    p.Port          = CON_PORT;
    p.Pin           = CON_PIN_RX;
    /* Подтяжка вверх: при оборванном кабеле линия покоя высокая, и приёмник
       видит тишину, а не поток нулевых байт с ошибкой кадра. */
    p.Pull          = CRU_PIN_PULL_UP;
    p.InputCtrl     = ENABLE;
    p.DriveStrength = CRU_PIN_DRIVE_STRENGTH_0;
    p.Alternate     = CON_AF;
    CRU_PIN_Init(&p);

    (void)UART_DeInit(CON_UART);

    UART_InitStruct_TypeDef init;
    UART_StructInit(&init);
    init.BaudRate         = CON_BAUD;
    init.DataWidth        = UART_DATAWIDTH_8B;
    init.Transfer9b       = UART_TRANSFER_9B_DISABLE;
    init.Parity           = UART_PARITY_NONE;
    init.StopBits         = UART_STOP_1BIT;
    init.CtrlFIFO         = ENABLE;
    init.TxFIFOThreshold  = UART_TX_FIFO_EMPTY;
    init.RxFIFOThreshold  = UART_RX_FIFO_CHAR_1;
    init.CtrlIrdaMode     = DISABLE;
    init.CtrlLoopbackMode = DISABLE;
    (void)UART_Init(CON_UART, &init);
}

int
con_getc (void) {
    if (UART_IsActiveFlag(CON_UART, UART_FLAG_RFNE) == 0UL) {
        return -1;
    }
    return (int)UART_ReceiveData8b(CON_UART);
}
