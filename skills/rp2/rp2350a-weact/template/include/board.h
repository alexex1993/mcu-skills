#ifndef BOARD_H
#define BOARD_H

/*
 * Pin map — WeAct Studio RP2350A Core Board (RP2350A @ 150 MHz, 3.3 V logic).
 * Source: WeAct V10_SCH.pdf / V20_SCH.pdf and the pinout cards, transcribed in
 * ../reference/board-hardware.md §2-§4.
 *
 * Physical pins = the 40-pin header, USB at the BOTTOM in WeAct's own pinout
 * card. Numbering follows the Raspberry Pi Pico convention used by the
 * schematic connectors: H1 = pins 1-20 (GP0 at pin 1, GP15 at pin 20, GNDs at
 * 3/8/13/18), H2 = pins 21-40 back up the other side (GP16 at pin 21,
 * VBUS at pin 40).
 *
 * THE TWO REVISIONS ARE NOT THE SAME BOARD on GP23/GP24/GP29 and on the
 * second power pin. BOARD_REV must be 10 or 20 — set it from platformio.ini.
 */

#if !defined(BOARD_REV)
#error "BOARD_REV is not set. Add -DBOARD_REV=20 (V2.0) or -DBOARD_REV=10 (V1.0) to build_flags — the revisions differ on GP23/24/29 and on the power pin."
#elif (BOARD_REV != 10) && (BOARD_REV != 20)
#error "BOARD_REV must be 10 (RP2350A_V10) or 20 (RP2350A_V20)."
#endif

/* --- LEDs and buttons ---------------------------------------------------
 * Every user LED is anode-to-GPIO through 5.1 kOhm to GND: on = HIGH, and
 * ~0.3 mA, so they are dim — a "dead" LED in daylight is usually alive.
 * RESET and BOOT are hardwired to RUN and QSPI_SS; neither is readable
 * from software.
 */
#if BOARD_REV == 20
#define BOARD_PIN_LED        25  /* green, active-HIGH. The ONLY user LED   */
#else
#define BOARD_PIN_LED        25  /* LED2, green,  active-HIGH               */
#define BOARD_PIN_LED2       24  /* LED1, blue,   active-HIGH (V1.0 only)   */
#define BOARD_PIN_KEY        23  /* 23@KEY button, active-LOW, external
                                    5.1 kOhm pull-up: use pinMode(_, INPUT),
                                    NOT INPUT_PULLDOWN (see errata RP2350-E9
                                    in the skill's rules)                   */
#endif

/* --- UARTs — Serial1/Serial2 are the hardware UARTs; `Serial` is USB CDC */
#define BOARD_PIN_UART0_TX    0  /* Serial1 default, header pin 1           */
#define BOARD_PIN_UART0_RX    1  /* Serial1 default, header pin 2           */
#define BOARD_PIN_UART1_TX    8  /* Serial2 default, header pin 11          */
#define BOARD_PIN_UART1_RX    9  /* Serial2 default, header pin 12          */

/* --- I2C — Wire (i2c0) defaults. Wire1 (i2c1) defaults to 26/27 = A0/A1:
 * remap it with Wire1.setSDA()/setSCL() before begin() if you use the ADC */
#define BOARD_PIN_SDA         4
#define BOARD_PIN_SCL         5

/* --- SPI — the `SPI` object (spi0) defaults */
#define BOARD_PIN_SS         17
#define BOARD_PIN_MOSI       19
#define BOARD_PIN_MISO       16
#define BOARD_PIN_SCK        18

/* --- ADC: 12-bit, 0 to ADC_VREF. analogRead() returns 10-bit values
 * (0-1023) until analogReadResolution(12). Inputs must stay under ~3.6 V.
 * ADC_VREF is +3V3 through R18 100R (and R19 1R to ADC_AVDD, 2.2 uF to GND),
 * so it sags by ~15 mV under the ADC's own supply current. */
#define BOARD_PIN_ADC_A0     26  /* = A0, header pin 31                     */
#define BOARD_PIN_ADC_A1     27  /* = A1, header pin 32                     */
#define BOARD_PIN_ADC_A2     28  /* = A2, header pin 34                     */

#if BOARD_REV == 10
/* V1.0 puts GP29/ADC3 on header pin 35 — the slot the Pico 2 and V2.0 use
 * for ADC_VREF. A fourth free analog input, and no VSYS sense anywhere. */
#define BOARD_PIN_ADC_A3     29  /* = A3, header pin 35, silkscreen "29"    */
#else
/* V2.0 matches the Pico 2: pin 35 is ADC_VREF, and GP29 is the VSYS sense
 * (not on the header — round test pad marked 29 next to the BOOT button).
 * The network is R23 100K / R24 100K to GND with R25 100K after a FET whose
 * gate sits at +3V3: nominally VSYS/3 at the pin, but the FET's on-resistance
 * varies with VSYS, so CALIBRATE against a meter before trusting absolutes. */
#define BOARD_PIN_VSYS_ADC   29  /* A3, ~VSYS/3 — see the skill's rule 8    */
#define BOARD_PIN_VBUS_SENSE 24  /* input, HIGH while USB VBUS present
                                    (VBUS/2 through 100K/100K)             */
#define BOARD_PIN_SMPS_MODE  23  /* output. LOW (default, 100K pulldown) =
                                    PFM: best efficiency, more ripple.
                                    HIGH = forced PWM: quieter rail for ADC
                                    work, much worse light-load efficiency */
#endif

/* --- PWM: analogWrite() works on every GPIO 0-29; default 8-bit @ 1 kHz.
 * GPIO 2n and 2n+1 share one PWM slice (one counter): 0&1, 2&3 ... 28&29,
 * so both pins of a pair share whatever analogWriteFreq() last set. */

/*
 * Power facts that burn hardware (../reference/board-hardware.md §4).
 * The pin next to the USB connector is silkscreened VIN on BOTH revisions
 * and is USB VBUS on both — it is not the input you were looking for. The
 * input is the pin above it, and it is not the same thing on the two boards:
 *
 *   V1.0  silk "5V"    LDO input, 3.6-6.5 V -> 3.3 V @ 800 mA.
 *                      Below ~3.6 V it drops out: a 1S Li-ion browns out.
 *   V2.0  silk "VSYS"  buck-boost input, 1.8-5.5 V (1S Li-ion, 3xAA, USB).
 *                      6.5 V here is over the absolute maximum.
 *
 *   VIN/VBUS  5 V from USB, header pin 40, ahead of the Schottky.
 *   3V3       output only — never back-feed it.
 *   EN        3V3_EN: pull low and the whole 3.3 V rail dies, board looks
 *             bricked (V1.0: LDO CE via 5.1K; V2.0: buck-boost EN via 100K).
 *   RUN       short to GND to reset the chip, header pin 30.
 *   GPIO26-29 are NOT 5 V tolerant and have a reverse diode to the 3.3 V
 *             rail: voltage on them while the board is unpowered back-powers
 *             the board through the pin.
 */

#endif /* BOARD_H */
