#ifndef USBD_CDC_IF_H
#define USBD_CDC_IF_H

#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

#endif /* USBD_CDC_IF_H */
