/**
 * Configuration for the ST USB Device Library on this board:
 * a single CDC (virtual COM port) interface on USB_OTG_FS.
 */
#ifndef USBD_CONF_H
#define USBD_CONF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"

#define DEVICE_FS                    0U   /* USBD_Init() device index */

#define USBD_MAX_NUM_INTERFACES      2U   /* CDC = control interface + data interface */
#define USBD_MAX_NUM_CONFIGURATION   1U
#define USBD_MAX_STR_DESC_SIZ        512U
#define USBD_DEBUG_LEVEL             0U
#define USBD_LPM_ENABLED             0U
#define USBD_SELF_POWERED            1U
#define USBD_CDC_INTERVAL            1000U

/* Static allocation only - no heap on this target. */
#define USBD_malloc                  (void *)USBD_static_malloc
#define USBD_free                    USBD_static_free
#define USBD_memset                  memset
#define USBD_memcpy                  memcpy
#define USBD_Delay                   HAL_Delay

#if (USBD_DEBUG_LEVEL > 0U)
#define USBD_UsrLog(...)   do { printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_UsrLog(...)   do {} while (0)
#endif

#if (USBD_DEBUG_LEVEL > 1U)
#define USBD_ErrLog(...)   do { printf("ERROR: "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_ErrLog(...)   do {} while (0)
#endif

#if (USBD_DEBUG_LEVEL > 2U)
#define USBD_DbgLog(...)   do { printf("DEBUG : "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define USBD_DbgLog(...)   do {} while (0)
#endif

void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

#endif /* USBD_CONF_H */
