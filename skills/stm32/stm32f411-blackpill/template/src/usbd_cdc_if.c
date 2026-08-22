/* CDC application layer: the read/write side of the virtual COM port */

#include "usbd_cdc_if.h"
#include "usb_device.h"

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* The host sets baud/parity, but over USB CDC they are fiction: store what it
   asks for and hand it back, or some drivers refuse to open the port. */
static USBD_CDC_LineCodingTypeDef LineCoding = {
    115200, /* bitrate  */
    0x00,   /* stopbits: 1 */
    0x00,   /* parity:   none */
    0x08    /* databits: 8 */
};

volatile uint8_t CDC_PortOpen = 0U;

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

    /* A no-data request: DTR/RTS live in the setup packet's wValue. DTR goes
       high when the host opens the port, which is how the firmware knows a
       monitor is attached and it is worth printing a banner. */
    case CDC_SET_CONTROL_LINE_STATE:
        CDC_PortOpen = (((USBD_SetupReqTypedef *)pbuf)->wValue & 0x0001U) ? 1U : 0U;
        break;

    default:
        break;
    }

    return USBD_OK;
}

/* Receive path: everything typed into the monitor lands here. Echo it back.
   Keep this fast - it runs in USB interrupt context. */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    CDC_Transmit_FS(Buf, (uint16_t)*Len);

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    /* pClassDataCmsit[] is the field name in current ST USB Device Library
       releases; older ones (and most tutorials) call it pClassData. Copying an
       old snippet in here fails to compile - it is the same pointer. */
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[0];

    if (hcdc == NULL)
    {
        return USBD_FAIL;
    }
    if (hcdc->TxState != 0)
    {
        return USBD_BUSY;   /* the previous packet has not gone out yet */
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
