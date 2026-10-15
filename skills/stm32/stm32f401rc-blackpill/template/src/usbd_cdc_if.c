/*
 * CDC application layer: the virtual COM port's read/write side, plus the
 * newlib hook that sends printf() to it.
 *
 * Receive runs in the USB interrupt and only fills a ring buffer; all
 * transmitting happens from the main loop through CDC_Write(). Transmitting
 * from both (e.g. echoing inside CDC_Receive_FS while main() is printing)
 * races on the single IN endpoint and its buffer.
 */

#include <stdio.h>
#include <string.h>

#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "stm32f4xx_hal.h"

#define RX_RING_SIZE   256U         /* power of two */
#define TX_TIMEOUT_MS  50U

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

static volatile uint8_t  rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head;   /* written in the USB IRQ */
static volatile uint16_t rx_tail;   /* written in thread context */

/* Baud/parity are fiction over USB CDC: store what the host sets and hand it
   back, or some drivers refuse to open the port. */
static USBD_CDC_LineCodingTypeDef LineCoding = {115200, 0x00, 0x00, 0x08};

volatile uint8_t CDC_PortOpen = 0U;
volatile uint8_t CDC_BootloaderRequest = 0U;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    NULL
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    CDC_PortOpen = 0U;
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;

    switch (cmd)
    {
    case CDC_SET_LINE_CODING:
        LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |
                                           (pbuf[2] << 16) | (pbuf[3] << 24));
        LineCoding.format     = pbuf[4];
        LineCoding.paritytype = pbuf[5];
        LineCoding.datatype   = pbuf[6];
        if (LineCoding.bitrate == 1200U)
        {
            CDC_BootloaderRequest = 1U;     /* see weact_hid_flash.py */
        }
        break;

    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t)(LineCoding.bitrate);
        pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
        pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
        pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
        pbuf[4] = LineCoding.format;
        pbuf[5] = LineCoding.paritytype;
        pbuf[6] = LineCoding.datatype;
        break;

    /* DTR (bit 0 of wValue) goes high when a terminal opens the port. It is
       the only reliable "someone is listening" signal - without it, output
       sent to a closed port fills the endpoint and every write times out. */
    case CDC_SET_CONTROL_LINE_STATE:
        CDC_PortOpen = (((USBD_SetupReqTypedef *)pbuf)->wValue & 0x0001U) ? 1U : 0U;
        break;

    default:
        break;
    }

    return USBD_OK;
}

/* USB interrupt context: copy into the ring, re-arm, nothing else. */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    for (uint32_t i = 0U; i < *Len; i++)
    {
        uint16_t next = (uint16_t)((rx_head + 1U) & (RX_RING_SIZE - 1U));
        if (next == rx_tail)
        {
            break;                  /* ring full: drop the rest */
        }
        rx_ring[rx_head] = Buf[i];
        rx_head = next;
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

int CDC_ReadByte(void)
{
    if (rx_tail == rx_head)
    {
        return -1;
    }
    uint8_t c = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) & (RX_RING_SIZE - 1U));
    return c;
}

/* Wait until the previous packet has left, THEN copy the next chunk into the
   endpoint's buffer: the USB core reads that buffer while it transmits, so
   refilling it any earlier corrupts the packet in flight. */
uint32_t CDC_Write(const uint8_t *buf, uint32_t len)
{
    uint32_t sent = 0U;

    while (sent < len)
    {
        /* pClassDataCmsit[] is the field name in current ST USB Device Library
           releases; older ones (and most tutorials) call it pClassData. */
        USBD_CDC_HandleTypeDef *hcdc =
            (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[0];

        if (!CDC_PortOpen || hcdc == NULL ||
            hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        {
            return sent;
        }

        uint32_t start = HAL_GetTick();
        while (hcdc->TxState != 0U)
        {
            if ((HAL_GetTick() - start) > TX_TIMEOUT_MS)
            {
                return sent;        /* host is not reading - drop */
            }
        }

        uint32_t chunk = len - sent;
        if (chunk > APP_TX_DATA_SIZE)
        {
            chunk = APP_TX_DATA_SIZE;
        }
        memcpy(UserTxBufferFS, buf + sent, chunk);
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, chunk);
        if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK)
        {
            return sent;
        }
        sent += chunk;
    }

    return sent;
}

/* newlib: printf/puts/putchar on stdout and stderr end up here. */
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    CDC_Write((const uint8_t *)ptr, (uint32_t)len);
    return len;                     /* claim success: never block printf */
}
