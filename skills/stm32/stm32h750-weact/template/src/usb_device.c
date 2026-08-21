#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "board.h"

USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS) != USBD_OK) {
        Error_Handler();
    }
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
        Error_Handler();
    }
    if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK) {
        Error_Handler();
    }
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
        Error_Handler();
    }
}
