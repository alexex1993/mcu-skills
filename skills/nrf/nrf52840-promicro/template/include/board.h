/*
 * ProMicro nRF52840 (V1940) — board facts in one place.
 *
 * Arduino pin numbers here are the Pro Micro silkscreen numbers defined by
 * boards/variants/promicro_nrf52840/variant.h. The P0.xx / P1.xx comments are
 * the physical nRF52840 port pins — the number you need when reading the
 * Nordic datasheet, writing Zephyr devicetree, or talking to nrfjprog.
 */

#ifndef BOARD_H
#define BOARD_H

/* --- LED -------------------------------------------------------------- */
/* On-board blue LED, active HIGH, NOT on the header. Use ledOn()/ledOff()
 * from the core rather than digitalWrite: they honour LED_STATE_ON. */
#define BOARD_PIN_LED            11   /* P0.15                            */
#define BOARD_PIN_LED_ALT1       12   /* P0.26 - other clone revisions    */
#define BOARD_PIN_LED_ALT2       13   /* P0.30 - other clone revisions    */

/* --- Header pins ------------------------------------------------------- */
#define BOARD_PIN_D0              0   /* P0.08  Serial1 RX                */
#define BOARD_PIN_D1              1   /* P0.06  Serial1 TX                */
#define BOARD_PIN_D2              2   /* P0.17  Wire SDA                  */
#define BOARD_PIN_D3              3   /* P0.20  Wire SCL                  */
#define BOARD_PIN_D4              4   /* P0.22                            */
#define BOARD_PIN_D5              5   /* P0.24                            */
#define BOARD_PIN_D6              6   /* P1.00                            */
#define BOARD_PIN_D7              7   /* P0.11                            */
#define BOARD_PIN_D8              8   /* P1.04                            */
#define BOARD_PIN_D9              9   /* P1.06                            */
#define BOARD_PIN_D10            10   /* P0.09  SPI SS,  NFC1             */
#define BOARD_PIN_D14            14   /* P1.11  SPI MISO                  */
#define BOARD_PIN_D15            15   /* P1.13  SPI SCK                   */
#define BOARD_PIN_D16            16   /* P0.10  SPI MOSI, NFC2            */

/* --- Analog ------------------------------------------------------------ */
/* A0 is a trap: P1.15 has no SAADC channel. Only P0.02-P0.05 and
 * P0.28-P0.31 can be sampled on this chip. */
#define BOARD_PIN_A0_DIGITAL_ONLY 18  /* P1.15  NO ADC                    */
#define BOARD_PIN_A1             19   /* P0.02  AIN0                      */
#define BOARD_PIN_A2             20   /* P0.29  AIN5                      */
#define BOARD_PIN_A3             21   /* P0.31  AIN7                      */

/* SAADC defaults in the Adafruit core (wiring_analog_nRF52.c): 10-bit result,
 * internal 0.6 V reference, gain 1/6 -> full scale 3.6 V, NOT ratiometric to
 * VDD. analogReadResolution(12) widens the result; analogReference() picks a
 * different full scale (AR_INTERNAL_3_0 -> 3.0 V, AR_VDD4 -> VDD). */
#define BOARD_ADC_FULLSCALE_MV  3600

/* --- Clocks ------------------------------------------------------------ */
/* Core is 64 MHz, fixed. The low-frequency clock is the configurable one and
 * it is set in variant.h (USE_LFRC / USE_LFXO), not here. */
#define BOARD_F_CPU_HZ      64000000UL

/* --- Flash map (SoftDevice S140 6.1.1 + Adafruit UF2 bootloader) -------- */
#define BOARD_FLASH_APP_START   0x26000UL  /* after MBR + SoftDevice       */
#define BOARD_FLASH_APP_END     0xED000UL  /* linker limit, 815104 B usable */
#define BOARD_FLASH_BOOTLOADER  0xF4000UL
#define BOARD_FLASH_BL_SETTINGS 0xFF000UL
/* RAM available to the application: 0x20006000..0x20040000 = 237568 B.
 * Everything below 0x20006000 belongs to the SoftDevice. */
#define BOARD_RAM_APP_START 0x20006000UL

#endif /* BOARD_H */
