/**
 * CDC interface: this board is transmit-only in practice, but the receive
 * path is wired up so anything typed into the monitor is simply echoed back.
 */
#include "usbd_cdc_if.h"
#include "usb_device.h"

#define APP_RX_DATA_SIZE  512
#define APP_TX_DATA_SIZE  512

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
};

/* Line coding the host asked for. The link is virtual, so it is only
   stored and echoed back - the baud rate has no effect on anything. */
static USBD_CDC_LineCodingTypeDef LineCoding = {
    115200, /* bitrate */
    0x00,   /* stop bits: 1 */
    0x00,   /* parity: none */
    0x08,   /* data bits: 8 */
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;

    switch (cmd) {
    case CDC_SET_LINE_CODING:
        LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |
                                           (pbuf[2] << 16) | (pbuf[3] << 24));
        LineCoding.format     = pbuf[4];
        LineCoding.paritytype = pbuf[5];
        LineCoding.datatype   = pbuf[6];
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

    default:
        break;
    }

    return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

    if (hcdc == NULL) {
        return USBD_FAIL;
    }
    if (hcdc->TxState != 0) {
        return USBD_BUSY;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
