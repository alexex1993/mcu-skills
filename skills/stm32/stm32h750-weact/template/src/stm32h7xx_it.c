/**
 * Cortex-M7 exception handlers. SysTick_Handler is what keeps HAL_GetTick()
 * and HAL_Delay() running.
 */
#include "board.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

void NMI_Handler(void)
{
    for (;;) {
    }
}

void HardFault_Handler(void)
{
    for (;;) {
    }
}

void MemManage_Handler(void)
{
    for (;;) {
    }
}

void BusFault_Handler(void)
{
    for (;;) {
    }
}

void UsageFault_Handler(void)
{
    for (;;) {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}
