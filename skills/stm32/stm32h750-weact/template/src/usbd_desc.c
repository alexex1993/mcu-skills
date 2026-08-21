/**
 * USB descriptors for the virtual COM port.
 * VID/PID are ST's well-known STM32 Virtual COM Port pair, so the stock
 * CDC-ACM drivers pick the device up without anything to install.
 */
#include "usbd_desc.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "board.h"

#define USBD_VID            0x0483
#define USBD_PID            0x5740
#define USBD_LANGID_STRING  0x409
#define USBD_MANUFACTURER   "WeAct Studio"
#define USBD_PRODUCT_STRING "MiniSTM32H750 Virtual COM Port"
#define USBD_CONFIG_STRING  "CDC Config"
#define USBD_INTERFACE_STR  "CDC Interface"

static uint8_t *USBD_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef CDC_Desc = {
    USBD_DeviceDescriptor,
    USBD_LangIDStrDescriptor,
    USBD_ManufacturerStrDescriptor,
    USBD_ProductStrDescriptor,
    USBD_SerialStrDescriptor,
    USBD_ConfigStrDescriptor,
    USBD_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    USB_LEN_DEV_DESC,           /* bLength */
    USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
    0x00, 0x02,                 /* bcdUSB 2.00 */
    0x02,                       /* bDeviceClass: CDC */
    0x02,                       /* bDeviceSubClass */
    0x00,                       /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),
    0x00, 0x02,                 /* bcdDevice 2.00 */
    USBD_IDX_MFC_STR,           /* iManufacturer */
    USBD_IDX_PRODUCT_STR,       /* iProduct */
    USBD_IDX_SERIAL_STR,        /* iSerialNumber */
    USBD_MAX_NUM_CONFIGURATION, /* bNumConfigurations */
};

__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

__ALIGN_BEGIN static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN static uint8_t USBD_StringSerial[26] __ALIGN_END = {
    26, USB_DESC_TYPE_STRING,
};

/* Build a unique serial number out of the MCU's 96-bit unique ID. */
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    uint8_t idx;

    for (idx = 0; idx < len; idx++) {
        uint8_t nibble = (value >> 28) & 0x0F;
        pbuf[2 * idx]     = (nibble < 0xA) ? (nibble + '0') : (nibble + 'A' - 10);
        pbuf[2 * idx + 1] = 0;
        value <<= 4;
    }
}

static void Get_SerialNum(void)
{
    uint32_t deviceserial0 = HAL_GetUIDw0();
    uint32_t deviceserial1 = HAL_GetUIDw1();
    uint32_t deviceserial2 = HAL_GetUIDw2();

    deviceserial0 += deviceserial2;
    if (deviceserial0 != 0) {
        IntToUnicode(deviceserial0, &USBD_StringSerial[2], 8);
        IntToUnicode(deviceserial1, &USBD_StringSerial[18], 4);
    }
}

static uint8_t *USBD_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

static uint8_t *USBD_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *USBD_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = USBD_StringSerial[0];
    Get_SerialNum();
    return USBD_StringSerial;
}

static uint8_t *USBD_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_CONFIG_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_INTERFACE_STR, USBD_StrDesc, length);
    return USBD_StrDesc;
}
