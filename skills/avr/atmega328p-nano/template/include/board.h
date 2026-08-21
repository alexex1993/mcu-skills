#ifndef BOARD_H
#define BOARD_H

/*
 * Pin map — Arduino Nano (A000005), ATmega328P @ 16 MHz, 5 V logic.
 * Source: the corrected pinout in ../reference/board-hardware.md §2 — the
 * official A000005 manual's own pin tables contain errors (§7 there).
 *
 * A0–A5 are full digital pins too (aliases 14–19). A6/A7 are the exception:
 * analog-input only, no port register behind them.
 */

#define BOARD_PIN_LED      13    /* "L" LED, on = HIGH; also SPI SCK — the LED
                                    loads the line when D13 is an input/SPI  */

/* UART — wired to the FTDI/CH340 USB-serial chip, not free pins */
#define BOARD_PIN_TX       1     /* unusable as GPIO while Serial is open     */
#define BOARD_PIN_RX       0     /* same; keep wiring off it during upload    */

/* The only two external-interrupt pins */
#define BOARD_PIN_INT0     2     /* attachInterrupt(digitalPinToInterrupt())  */
#define BOARD_PIN_INT1     3     /* + PWM ~490 Hz on Timer2                   */

/* PWM, 8-bit — the timer behind each pin is what using that timer costs */
#define BOARD_PIN_PWM_D3   3     /* Timer2, ~490 Hz; lost while tone() plays  */
#define BOARD_PIN_PWM_D5   5     /* Timer0, ~980 Hz; Timer0 = millis()/delay()*/
#define BOARD_PIN_PWM_D6   6     /* Timer0, ~980 Hz; do not reconfigure       */
#define BOARD_PIN_PWM_D9   9     /* Timer1, ~490 Hz; lost while Servo active  */
#define BOARD_PIN_PWM_D10  10    /* Timer1, ~490 Hz; also SPI slave-select    */
#define BOARD_PIN_PWM_D11  11    /* Timer2, ~490 Hz; also SPI MOSI            */

/* I2C (Wire) */
#define BOARD_PIN_SDA      A4    /* = 18 */
#define BOARD_PIN_SCL      A5    /* = 19 */

/* SPI */
#define BOARD_PIN_SS       10
#define BOARD_PIN_MOSI     11
#define BOARD_PIN_MISO     12
#define BOARD_PIN_SCK      13    /* + LED_BUILTIN */

/* ADC: 10-bit, 0–5 V, ~100 µs per analogRead() */
#define BOARD_PIN_ADC_A0   A0    /* A0–A5 double as digital pins 14–19        */
#define BOARD_PIN_ADC_A6   A6    /* analog-input ONLY — pinMode/digitalWrite  */
#define BOARD_PIN_ADC_A7   A7    /*   on A6/A7 compile but silently misbehave */

/*
 * Power facts that burn hardware (see ../reference/board-hardware.md §4):
 *   3V3 pin — output only, 50 mA max, from a dedicated regulator.
 *   +5V pin — feeding it bypasses the regulator: never exceed 5.0 V.
 *   VIN     — 7–12 V recommended (6–20 V absolute) into the onboard LDO.
 *   All GPIO are 5 V push-pull; 3.3 V-only peripherals need level shifting.
 */

#endif /* BOARD_H */
