/*
 * Variant for the ProMicro nRF52840 (V1940 / nice!nano v2 compatible).
 *
 * g_ADigitalPinMap[arduino_pin] = 32 * port + pin. Everything in the Arduino
 * core — pinMode, digitalWrite, analogRead, Wire, SPI — goes through this
 * table, so a wrong entry here misroutes a peripheral with no compile error.
 */

#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] =
{
  // index = Pro Micro silkscreen number
   8,  // D0  = P0.08  RX  (Serial1)
   6,  // D1  = P0.06  TX  (Serial1)
  17,  // D2  = P0.17  SDA (Wire)
  20,  // D3  = P0.20  SCL (Wire)
  22,  // D4  = P0.22
  24,  // D5  = P0.24
  32,  // D6  = P1.00
  11,  // D7  = P0.11
  36,  // D8  = P1.04
  38,  // D9  = P1.06
   9,  // D10 = P0.09  SS, NFC1

  // D11..D13 are not on the header
  15,  // D11 = P0.15  on-board blue LED
  26,  // D12 = P0.26  LED fallback candidate
  30,  // D13 = P0.30  LED fallback candidate

  43,  // D14 = P1.11  MISO
  45,  // D15 = P1.13  SCK
  10,  // D16 = P0.10  MOSI, NFC2

  28,  // D17 = P0.28  not on the header (AIN4)

  47,  // D18 = P1.15  A0 — digital only, no ADC channel
   2,  // D19 = P0.02  A1 / AIN0
  29,  // D20 = P0.29  A2 / AIN5
  31,  // D21 = P0.31  A3 / AIN7

  // Deliberately absent:
  //  P0.00 / P0.01 — XL1 / XL2 (32.768 kHz crystal pads)
  //  P0.18         — RESET
};

void initVariant()
{
  pinMode(PIN_LED1, OUTPUT);
  ledOff(PIN_LED1);
}
