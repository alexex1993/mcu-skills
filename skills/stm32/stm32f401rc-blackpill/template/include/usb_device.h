#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

#include "usbd_def.h"

#define DEVICE_FS 0     /* USB controller index: the Full Speed core */

extern USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void);

#endif /* __USB_DEVICE_H */
