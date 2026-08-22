#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#include "usbd_cdc.h"

#define APP_RX_DATA_SIZE  512U
#define APP_TX_DATA_SIZE  512U

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/* 1 while the host holds DTR, i.e. while a monitor has the port open */
extern volatile uint8_t CDC_PortOpen;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

#endif /* __USBD_CDC_IF_H */
