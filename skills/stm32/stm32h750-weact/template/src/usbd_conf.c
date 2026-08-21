/**
 * Glue between the HAL PCD driver and the USB Device Library.
 * USB_OTG_FS runs off HSI48, exactly as in the vendor SDK
 * (SDK/HAL/STM32H750/11-ExtMem_Boot_USB).
 */
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "board.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* --- MSP ----------------------------------------------------------------- */

void HAL_PCD_MspInit(PCD_HandleTypeDef *pcdHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (pcdHandle->Instance == USB_OTG_FS) {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
        PeriphClkInitStruct.UsbClockSelection    = RCC_USBCLKSOURCE_HSI48;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        HAL_PWREx_EnableUSBVoltageDetector();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* PA11 -> USB_OTG_FS_DM, PA12 -> USB_OTG_FS_DP */
        GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

        HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *pcdHandle)
{
    if (pcdHandle->Instance == USB_OTG_FS) {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

/* --- PCD callbacks -> USB Device Library --------------------------------- */

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                         hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                        hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

    if (hpcd->Init.speed == PCD_SPEED_FULL) {
        speed = USBD_SPEED_FULL;
    } else {
        Error_Handler();
    }

    USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, speed);
    USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/* --- USBD Low Level driver ----------------------------------------------- */

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    hpcd_USB_OTG_FS.Instance                     = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints           = 9;
    hpcd_USB_OTG_FS.Init.speed                   = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable              = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface              = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable              = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable        = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable              = DISABLE;
    hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable     = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1       = DISABLE;

    hpcd_USB_OTG_FS.pData = pdev;
    pdev->pData           = &hpcd_USB_OTG_FS;

    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) {
        Error_Handler();
    }

    /* 1.25 KB of FIFO RAM, in 32-bit words. */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x80);

    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    return (HAL_PCD_DeInit(pdev->pData) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    return (HAL_PCD_Start(pdev->pData) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    return (HAL_PCD_Stop(pdev->pData) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
    return (HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type) == HAL_OK)
               ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_Close(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_Flush(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_SetStall(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_ClrStall(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;

    if ((ep_addr & 0x80) == 0x80) {
        return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
    }
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
    return (HAL_PCD_SetAddress(pdev->pData, dev_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint32_t size)
{
    return (HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size) == HAL_OK)
               ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                          uint8_t *pbuf, uint32_t size)
{
    return (HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size) == HAL_OK)
               ? USBD_OK : USBD_FAIL;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)
{
    HAL_Delay(Delay);
}

/* Single static block, enough for the CDC class handle. */
void *USBD_static_malloc(uint32_t size)
{
    static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) / 4) + 1];
    (void)size;
    return mem;
}

void USBD_static_free(void *p)
{
    (void)p;
}
