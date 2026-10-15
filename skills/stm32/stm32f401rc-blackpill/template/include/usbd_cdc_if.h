#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#include <stdint.h>
#include "usbd_cdc.h"

#define APP_RX_DATA_SIZE  64U       /* one full-speed bulk packet */
#define APP_TX_DATA_SIZE  512U

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/* 1 while the host holds DTR, i.e. while a terminal has the port open */
extern volatile uint8_t CDC_PortOpen;

/* Set when the host opens the port at 1200 baud - the Arduino "touch"
   convention for "reboot into the bootloader". Acted on from main(). */
extern volatile uint8_t CDC_BootloaderRequest;

/* Blocking write from thread context (never from an interrupt). Drops the
   data when no terminal is open or the host stops reading for 50 ms, so a
   closed port never stalls the firmware. Returns bytes accepted. */
uint32_t CDC_Write(const uint8_t *buf, uint32_t len);

/* Next received byte, or -1. Bytes arrive in the USB interrupt and wait in a
   256-byte ring; anything beyond that while the main loop is busy is dropped. */
int CDC_ReadByte(void);

#endif /* __USBD_CDC_IF_H */
