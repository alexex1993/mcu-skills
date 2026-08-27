/*
 * Variant for the ProMicro nRF52840 (V1940) — a nice!nano v2 compatible clone
 * in the Pro Micro form factor.
 *
 * Arduino pin numbers follow the Pro Micro silkscreen, NOT the nRF52840 port
 * numbering. D0..D16 are the header pins; D11..D13 and D17 are not broken out
 * (D11 = P0.15 is the on-board blue LED).
 *
 * The physical port number of every Arduino pin lives in g_ADigitalPinMap
 * (variant.cpp): index = Arduino pin, value = 32 * port + pin.
 */

#ifndef _VARIANT_PROMICRO_NRF52840_
#define _VARIANT_PROMICRO_NRF52840_

/** Core clock. Fixed at 64 MHz on the nRF52840; there is no PLL to configure. */
#define VARIANT_MCK       (64000000ul)

/*
 * Low-frequency clock source.
 *
 * USE_LFRC = internal 32 kHz RC oscillator (~250 ppm, recalibrated periodically).
 * USE_LFXO = external 32.768 kHz crystal (~20 ppm, lower current).
 *
 * Many ProMicro/SuperMini clones ship WITHOUT the LF crystal populated. With
 * USE_LFXO the SoftDevice waits forever for a clock that never starts: the
 * board enumerates as a USB device and then does nothing at all. USE_LFRC
 * always works. Switch to USE_LFXO only after confirming the crystal is on
 * your board (the two pads at P0.00/P0.01 = XL1/XL2).
 */
#define USE_LFRC
// #define USE_LFXO

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "WVariant.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// Must match the number of entries in g_ADigitalPinMap.
#define PINS_COUNT           (22)
#define NUM_DIGITAL_PINS     (22)
#define NUM_ANALOG_INPUTS    (4)
#define NUM_ANALOG_OUTPUTS   (0)

/*
 * LEDs
 */
#define PIN_LED1             (11)   // P0.15 - on-board blue LED, not on the header
#define LED_BUILTIN          PIN_LED1
#define LED_BLUE             PIN_LED1
#define LED_STATE_ON         1      // active HIGH; ledOn()/ledOff() honour this

// Fallback candidates: a few clone revisions put the LED elsewhere. Exposed as
// D12/D13 so a bring-up sketch can blink all three and see which one lights.
#define PIN_LED_ALT1         (12)   // P0.26
#define PIN_LED_ALT2         (13)   // P0.30

/*
 * Analog inputs (A0..A3 on the silkscreen)
 *
 * WARNING: A0 = P1.15 is DIGITAL ONLY. The nRF52840 SAADC is wired to P0.02-
 * P0.05 and P0.28-P0.31 only; nothing on port 1 can be sampled. analogRead(A0)
 * compiles and returns a meaningless value.
 */
#define PIN_A0               (18)   // P1.15 - digital only, no ADC channel
#define PIN_A1               (19)   // P0.02 / AIN0
#define PIN_A2               (20)   // P0.29 / AIN5
#define PIN_A3               (21)   // P0.31 / AIN7

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
#define ADC_RESOLUTION       14     // SAADC hardware maximum; core default is 10-bit

#define PIN_AREF             PIN_A3
static const uint8_t AREF = PIN_AREF;

// NFC antenna pins, usable as plain GPIO because the board definition builds
// with -DCONFIG_NFCT_PINS_AS_GPIOS -- plural. The core (system_nrf52840.c)
// tests only that spelling; the singular form most clone board JSONs carry is
// a silent no-op. It clears UICR->NFCPINS on first boot and resets once.
#define PIN_NFC1             (10)   // P0.09
#define PIN_NFC2             (16)   // P0.10

/*
 * Serial1 - hardware UARTE on the header pins labelled 0 and 1.
 * `Serial` is USB CDC and is a different object entirely.
 */
#define PIN_SERIAL1_RX       (0)    // P0.08
#define PIN_SERIAL1_TX       (1)    // P0.06

/*
 * SPI - Pro Micro layout: 14/15/16
 */
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_MISO         (14)   // P1.11
#define PIN_SPI_SCK          (15)   // P1.13
#define PIN_SPI_MOSI         (16)   // P0.10 (NFC2)

static const uint8_t SS   = (10);   // P0.09 (NFC1)
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;

/*
 * I2C - Pro Micro layout: D2/D3
 */
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (2)    // P0.17
#define PIN_WIRE_SCL         (3)    // P0.20

#ifdef __cplusplus
}
#endif

#endif
