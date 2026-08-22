#ifndef BOARD_H
#define BOARD_H

/*
 * Pin map — Raspberry Pi Pico (original, RP2040 @ 133 MHz, 3.3 V logic).
 * Source: the Rev3 pinout in ../reference/board-hardware.md §2 and the
 * arduino-pico variant defaults in §10 there.
 *
 * Physical pins = the 40-pin header, USB at the top: pin 1 top-left, 1-20
 * down the left edge, 21-40 back up the right edge (pin 40 is top-right).
 */

#define BOARD_PIN_LED       25   /* green LED, on = HIGH; NOT on the header   */

/* UARTs — Serial1/Serial2 are the hardware UARTs; plain `Serial` is USB CDC */
#define BOARD_PIN_UART0_TX   0   /* Serial1 default, header pin 1             */
#define BOARD_PIN_UART0_RX   1   /* Serial1 default, header pin 2             */
#define BOARD_PIN_UART1_TX   8   /* Serial2 default, header pin 11            */
#define BOARD_PIN_UART1_RX   9   /* Serial2 default, header pin 12            */

/* I2C — Wire (i2c0) default. Wire1 (i2c1) defaults to 26/27 = A0/A1: remap
 * it with Wire1.setSDA()/setSCL() before Wire1.begin() if you use the ADC */
#define BOARD_PIN_SDA        4
#define BOARD_PIN_SCL        5

/* SPI — the `SPI` object (spi0) defaults */
#define BOARD_PIN_SS        17
#define BOARD_PIN_MOSI      19
#define BOARD_PIN_MISO      16
#define BOARD_PIN_SCK       18

/* ADC: 12-bit 500 ksps, 0 to 3.3 V. analogRead() returns 10-bit values
 * (0-1023) until analogReadResolution(12). Inputs must stay under ~3.6 V. */
#define BOARD_PIN_ADC_A0    26   /* = A0, header pin 31 */
#define BOARD_PIN_ADC_A1    27   /* = A1, header pin 32 */
#define BOARD_PIN_ADC_A2    28   /* = A2, header pin 34 */
/* A3 = GPIO29 = VSYS/3 internally — not on the header, see below */

/* PWM: analogWrite() works on EVERY GPIO 0-29, default 8-bit @ 1 kHz.
 * GPIO 2n and 2n+1 share one PWM slice (one counter): 0&1, 2&3, ... 28&29,
 * so both pins of a pair share the frequency set by analogWriteFreq(). */

/* Internal-use GPIOs — real pins in code, NOT on the 40-pin header */
#define BOARD_PIN_VBUS_SENSE 24  /* input, HIGH while USB VBUS present       */
#define BOARD_PIN_SMPS_PS    23  /* output, HIGH forces SMPS PWM mode: less
                                    ripple on the ADC, worse light-load
                                    efficiency; leave LOW unless measuring   */
#define BOARD_PIN_VSYS_ADC   29  /* A3: reads VSYS/3 through the on-board
                                    divider — multiply the reading by 3      */

/*
 * Power facts that burn hardware (../reference/board-hardware.md §5):
 *   VSYS   — 1.8-5.5 V input (single Li-ion or 3xAA works), header pin 39.
 *   VBUS   — 5 V from USB, pin 40; must be FED 5 V for USB host mode.
 *   3V3    — output only, keep external load under 300 mA, pin 36.
 *            Never drive it from an external supply.
 *   3V3_EN — short low to disable the SMPS; board looks completely dead.
 *   RUN    — short to GND to reset the RP2040, pin 30.
 *   GPIO26-29 are NOT 5 V tolerant and have a reverse diode to the 3.3 V
 *   rail: voltage on them while the board is unpowered back-powers it.
 */

#endif /* BOARD_H */
