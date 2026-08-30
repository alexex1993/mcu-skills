#ifndef BOARD_H
#define BOARD_H

/*
 * Pin map — CJMCU / DFRobot Beetle (Mini Arduino Leonardo), ATmega32U4
 * @ 16 MHz, 5 V logic, 21 x 28 mm.
 *
 * Source: DFRobot's own "IO Port Mapping in correspondence with Arduino Port"
 * table (Beetle SKU:DFR0282), crossed with the Arduino AVR core's leonardo
 * variant (pins_arduino.h) — see ../reference/board-hardware.md §2. The
 * Beetle brings out 10 of the
 * ATmega32U4's 26 I/O lines; everything else in the Leonardo pin numbering
 * exists in software but goes nowhere on this board.
 *
 * EXPOSED (castellated edge pads):
 *   RX TX SDA SCL 9 10 11 A0 A1 A2  + two power pads marked "+" and "-"
 *
 * NOT EXPOSED (do not reference them; they compile and do nothing):
 *   D4 D5 D6 D7 D8 D12 D13 A3 A4 A5, and the TX/RX LED pins D30/D17.
 *   SPI (D14/D15/D16) is on unlabelled test pads — see the note below.
 */

/* ---- the ten pads ------------------------------------------------------ */

#define BOARD_PIN_RX        0    /* PD2  Serial1 RXD1, INT2                  */
#define BOARD_PIN_TX        1    /* PD3  Serial1 TXD1, INT3                  */
#define BOARD_PIN_SDA       2    /* PD1  Wire SDA, INT1                      */
#define BOARD_PIN_SCL       3    /* PD0  Wire SCL, INT0, PWM (Timer0 OC0B)   */

#define BOARD_PIN_D9        9    /* PB5  PWM (Timer1 OC1A), ADC12 = A9       */
#define BOARD_PIN_D10      10    /* PB6  PWM (Timer1 OC1B), ADC13 = A10      */
#define BOARD_PIN_D11      11    /* PB7  PWM (Timer0 OC0A), PCINT7           */

/* The same four pads under their plain Arduino numbers, for when D0-D3 are
   used as GPIO or interrupt inputs rather than as UART/I2C. */
#define BOARD_PIN_D0       BOARD_PIN_RX
#define BOARD_PIN_D1       BOARD_PIN_TX
#define BOARD_PIN_D2       BOARD_PIN_SDA
#define BOARD_PIN_D3       BOARD_PIN_SCL

#define BOARD_PIN_A0       A0    /* = D18, PF7, ADC7  — also JTAG TDI        */
#define BOARD_PIN_A1       A1    /* = D19, PF6, ADC6  — also JTAG TDO        */
#define BOARD_PIN_A2       A2    /* = D20, PF5, ADC5  — also JTAG TMS        */

/*
 * ADC: 10-bit, 0-5 V. FIVE channels reach the pads — A0, A1, A2 and the
 * dual-role D9/D10. Read the latter through the A9/A10 *constants*:
 * analogRead() on this core interprets a bare small integer as an ANALOG
 * channel index, not a digital pin, so analogRead(2) samples A2 (PF5), not
 * the SDA pad.
 */
#define BOARD_PIN_A9       A9    /* = D9,  PB5, ADC12                        */
#define BOARD_PIN_A10      A10   /* = D10, PB6, ADC13                        */

/*
 * PWM: exactly four pads, and which timer each one rents matters more here
 * than on any other Arduino board, because two of the four are on Timer0 —
 * the timer that IS millis()/micros()/delay().
 *
 *   D3   Timer0 OC0B  ~977 Hz   } Timer0 — reconfiguring it breaks delay().
 *   D11  Timer0 OC0A  ~977 Hz   }   Fast PWM, /64 -> 16e6/(64*256)
 *   D9   Timer1 OC1A  ~490 Hz   } Timer1 — the Servo library takes both.
 *   D10  Timer1 OC1B  ~490 Hz   }   Phase-correct, /64 -> 16e6/(64*510)
 *
 * The two frequencies are not a typo: the core runs Timer0 in fast-PWM and
 * Timer1 in phase-correct mode (cores/arduino/wiring.c init()), so the two
 * halves of your PWM budget hum at different pitches. Audible if you are
 * driving anything mechanical.
 *
 * tone() takes Timer3, whose only output (OC3A = D5) is not on a Beetle pad:
 * tone() therefore costs no PWM here, unlike on an Uno/Nano.
 */
#define BOARD_PIN_PWM_D3    3
#define BOARD_PIN_PWM_D9    9
#define BOARD_PIN_PWM_D10  10
#define BOARD_PIN_PWM_D11  11

/*
 * External interrupts: FOUR of them reach the pads (an Uno/Nano has two).
 * Always go through digitalPinToInterrupt() — the pin-to-INTn mapping on the
 * 32U4 is deliberately scrambled relative to the pin numbers.
 *
 *   D3 -> INT0    D2 -> INT1    D0 -> INT2    D1 -> INT3
 *
 * Pin-change interrupts are available on D9, D10, D11 (PCINT5/6/7, all in
 * the single PCINT0 vector).
 */
#define BOARD_PIN_INT_D0    0
#define BOARD_PIN_INT_D1    1
#define BOARD_PIN_INT_D2    2
#define BOARD_PIN_INT_D3    3

/*
 * The onboard LED is on D13 (PC7) and the pad is NOT brought out — you can
 * blink it, you cannot wire to it. Active HIGH.
 *
 * Clone caveat: a handful of Beetle clones populate no D13 LED. If the
 * minimal blink stays dark, the board is alive but has no LED there; fall
 * back to the TX LED (D30, PD5) or RX LED (D17, PB0), both ACTIVE-LOW, or
 * to a real LED on D9/D10/D11.
 */
#define BOARD_PIN_LED      LED_BUILTIN   /* = 13, PC7, on = HIGH */

/*
 * SPI lives on the SIX DOTS ON THE BACK, which DFRobot document as a standard
 * 6-pin ICSP interface. That same array is the board's only reset:
 *
 *   ICSP 1 MISO  = D14 = PB3 (QFN pin 11)   ICSP 2 VCC
 *   ICSP 3 SCK   = D15 = PB1 (QFN pin 9)    ICSP 4 MOSI = D16 = PB2 (pin 10)
 *   ICSP 5 RESET       (QFN pin 13)         ICSP 6 GND
 *
 * Note SS (D17 = PB0, QFN pin 7) is NOT on that header — use any edge pad for
 * chip-select instead. Shorting ICSP 5 to ICSP 6 resets the board into the
 * bootloader for 8 s; that is the recovery procedure for everything.
 */
#define BOARD_PIN_MISO     14   /* PB3, ICSP 1 */
#define BOARD_PIN_SCK      15   /* PB1, ICSP 3 */
#define BOARD_PIN_MOSI     16   /* PB2, ICSP 4 */
#define BOARD_PIN_SS       17   /* PB0 — NOT on the ICSP header; also RX LED */

/*
 * Serial ports — two of them, and they are NOT the same object:
 *   Serial   = USB CDC over the micro-USB connector. No baud rate; disappears
 *              from the host when the sketch resets. `while (!Serial);` blocks
 *              forever on a battery-powered board.
 *   Serial1  = the hardware UART on the TX/RX pads (D1/D0). A genuinely free
 *              UART — unlike an Uno/Nano, nothing else is wired to it.
 *
 * Power — DFRobot's own numbers (see ../reference/board-hardware.md §4):
 *   4.5-5 V reliable · 3-4.5 V "may work, reliability not guaranteed" ·
 *   6 V DAMAGES THE BOARD. No regulator, no VIN, no 3V3 output.
 *   Feeding "+" while USB is plugged in shorts your supply to the host VBUS.
 *   Large loads (motors, relays): feed them from the supply DIRECTLY, not
 *   through the board, with >= 10 uF in parallel — otherwise the transient
 *   resets the MCU and it looks exactly like a firmware bug.
 *   All I/O are 5 V push-pull.
 */

#endif /* BOARD_H */
