/*
 * Reset_Handler that survives being jumped to by the WeAct HID bootloader.
 *
 * The bootloader does not reset the chip before starting the app. It sets MSP
 * from 0x08004000 and branches to the reset vector at 0x08004004, leaving
 * SCB->VTOR at 0x08000000 - ITS vector table - and interrupts unmasked.
 * V1.2 (this board) jumps before it has started any clock or SysTick; V1.1,
 * still found on boards, jumps with HSE + PLL as SYSCLK and SysTick running.
 *
 * With ST's stock startup, HAL_Init() enables SysTick but the vector still
 * points into the bootloader, whose handler increments the bootloader's
 * uwTick - some random word of OUR RAM - so HAL_GetTick() never moves and the
 * first HAL_Delay() hangs forever. Under V1.1 the first tick can even land
 * while .data/.bss are being initialised.
 *
 * ST's Reset_Handler is weak, so this one replaces it: mask interrupts, stop
 * SysTick, point VTOR at our own table, THEN copy .data and zero .bss.
 * A V1.1 clock tree is taken back in SystemClock_Config() (HAL_RCC_DeInit first).
 *
 * Harmless when the chip boots straight into this image from 0x08000000
 * ([env:dfu] / [env:stlink]): VTOR then just gets the value it already had.
 */

#include "stm32f4xx.h"

extern uint32_t g_pfnVectors[];             /* startup_stm32f401xc.s */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
extern void __libc_init_array(void);
extern int main(void);

void Reset_Handler(void)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->VAL  = 0U;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;
    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    /* SystemInit() BEFORE the VTOR write: built with -DUSER_VECT_TAB_ADDRESS
       it writes VTOR = 0x08000000 itself (the framework's system file
       hard-codes VECT_TAB_OFFSET to 0; a -D cannot override it). */
    SystemInit();

    SCB->VTOR = (uint32_t)g_pfnVectors;     /* 0x08004000 behind the bootloader */
    __DSB();
    __ISB();

    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;)
    {
        *dst++ = *src++;
    }
    for (uint32_t *dst = &_sbss; dst < &_ebss;)
    {
        *dst++ = 0U;
    }

    __libc_init_array();

    __enable_irq();
    main();

    while (1)
    {
    }
}
