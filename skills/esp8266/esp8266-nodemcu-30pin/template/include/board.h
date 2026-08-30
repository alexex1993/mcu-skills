#ifndef BOARD_H
#define BOARD_H

/*
 * Pin map — NodeMCU 30-pin ESP8266 devkit (ESP-12E / ESP-12F, 4 MB flash).
 * Source: NodeMCU DEVKIT V1.0 schematic sheets 2/5/6/7 and the ESP8266EX
 * datasheet v7.1 Table 2-1. Full detail in ../reference/board-hardware.md.
 *
 * THE SILKSCREEN LIES ABOUT NUMBERS. D1 is GPIO5 and D2 is GPIO4 — the
 * D-numbers are not GPIO numbers and are not even in the same order. Pick one
 * naming scheme per project; mixing them is the most common ESP8266 bug.
 */

/* --- safe general-purpose pins ------------------------------------------ */
#define BOARD_D1        5     /* GPIO5  — also default SCL                   */
#define BOARD_D2        4     /* GPIO4  — also default SDA                   */
#define BOARD_D5       14     /* GPIO14 — HSPI SCLK                          */
#define BOARD_D6       12     /* GPIO12 — HSPI MISO                          */
#define BOARD_D7       13     /* GPIO13 — HSPI MOSI                          */

/* --- boot-strapping pins: usable, with a cost --------------------------- */
#define BOARD_D3        0     /* GPIO0  — FLASH button, 12k pull-up.
                                 LOW at reset = flash download mode, sketch
                                 never runs. Fine as a button, fatal as a
                                 line something else holds low at reset.     */
#define BOARD_D4        2     /* GPIO2  — module LED (active LOW) + UART1 TX,
                                 12k pull-up. MUST be HIGH at reset.         */
#define BOARD_D8       15     /* GPIO15 — HSPI CS, 12k pull-DOWN.
                                 MUST be LOW at reset: one pull-up on this
                                 pin and the board never boots at all — no
                                 serial output, looks dead.                  */

/* --- GPIO16: not a normal GPIO ------------------------------------------ */
#define BOARD_D0       16     /* XPD_DCDC on the RTC domain. digitalRead /
                                 digitalWrite ONLY: no interrupt, no PWM/
                                 analogWrite, no I2C, no OneWire, no Servo.
                                 Its only pull is a pull-DOWN, selected with
                                 pinMode(16, INPUT_PULLDOWN_16) — INPUT_PULLUP
                                 compiles and does nothing.                  */

/* --- LEDs, both active LOW ---------------------------------------------- */
#define BOARD_LED_MODULE   2  /* = LED_BUILTIN. On the ESP-12E/F module.     */
#define BOARD_LED_BOARD   16  /* = LED_BUILTIN_AUX. On the NodeMCU PCB,
                                 anode to 3V3 via 470R (schematic sheet 6).
                                 Absent on bare ESP-12E/F breakouts.         */
#define BOARD_LED_ON     LOW
#define BOARD_LED_OFF    HIGH

/* --- buttons ------------------------------------------------------------ */
#define BOARD_BTN_FLASH   0   /* S2, to GND via 470R. Pressed = LOW.         */
                              /* S1 (RST/USER) acts on nRST — not readable.  */

/* --- buses -------------------------------------------------------------- */
#define BOARD_I2C_SDA     4   /* D2 — Wire's default. Software I2C: there is  */
#define BOARD_I2C_SCL     5   /* D1 — no I2C peripheral in the ESP8266.      */

#define BOARD_HSPI_SCLK  14   /* D5 */
#define BOARD_HSPI_MISO  12   /* D6 */
#define BOARD_HSPI_MOSI  13   /* D7 */
#define BOARD_HSPI_CS    15   /* D8 — see the strapping warning above; an SPI
                                 slave whose CS idles high blocks boot.      */

/* --- UART0: the USB bridge, not free pins ------------------------------- */
#define BOARD_D10         1   /* GPIO1 = U0TXD. Also emits the ROM boot log. */
#define BOARD_D9          3   /* GPIO3 = U0RXD.                              */
                              /* Serial.swap() moves UART0 to GPIO15/GPIO13. */

/* --- ADC ---------------------------------------------------------------- */
#define BOARD_ADC        A0   /* 10-bit, 0..1023. The HEADER pin is 0-3.2 V
                                 through a 220k/100k 1% divider (schematic
                                 sheet 7); the chip's TOUT pad behind it is
                                 0-1.0 V. Source impedance must be well under
                                 the ~320k the divider presents.
                                 ADC_MODE(ADC_VCC) repurposes it to measure
                                 the 3V3 rail and then A0 must be left open. */

/*
 * DO NOT USE — exposed on the header but not free:
 *   SD0..SD3, CLK, CMD  = GPIO6,7,8,9,10,11 — the SPI flash bus. Driving any
 *                         of them crashes or corrupts the running sketch.
 *                         (GPIO9/GPIO10 work only on a DOUT-mode module.)
 *   RSV                 = reserved, not connected (V1.0/Amica).
 *                         On LoLin V3 these two pads are VU (USB 5 V) and GND.
 *
 * Power (schematic sheet 4): NCP1117ST33 LDO, 800 mA, fed from USB 5 V through
 * a 1N5819 Schottky. VIN sits on that same 5 V rail:
 *   - 5 V into VIN is the intended use, and the Schottky keeps it off the host.
 *   - The LDO is rated to 20 V but is an SOT-223 linear part; NodeMCU's own
 *     instruction sheet says never exceed 5 V. At 12 V it thermally shuts down.
 *   - 3V3 is an output. GPIO are 3.3 V and NOT 5 V tolerant, 12 mA max.
 *
 * Deep sleep needs GPIO16 wired to RST. R10 (0R, marked "NC" on the schematic;
 * the instruction sheet calls it the R3 position) is NOT fitted from the
 * factory — see reference/board-hardware.md §8. Verify before you rely on it.
 */

#endif /* BOARD_H */
