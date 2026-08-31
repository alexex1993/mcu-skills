#ifndef BOARD_H
#define BOARD_H

/*
 * Pin map — LGT8F328P-LQFP32 MiniEVB, Nano-style 30-pin board.
 * Silkscreen "LGTBF32BP" = LGT8F328P (Logic Green / Prodesign).
 *
 * Source: LGT8FX8P databook v1.0.5 §"Pinout Configuration" (QFP48 -> QFP32
 * bundling table) + the LGT328P-LQFP32-Nano schematic + the lgt8fx core's
 * variants/lgt8fx8p pin tables. See ../reference/board-hardware.md §2.
 *
 * D0-D13 / A0-A5 match an Arduino Nano exactly. Everything below D14 does
 * not, and that is where ported ATmega328P sketches break.
 */

/* -------------------------------------------------------------------------
 * Digital pins that behave like a Nano
 * ---------------------------------------------------------------------- */
#define BOARD_PIN_LED      13    /* PB5, "L" LED, on = HIGH; also SPI SCK    */

#define BOARD_PIN_RX       0     /* PD0 — wired to the USB-serial bridge     */
#define BOARD_PIN_TX       1     /* PD1 — bridge; ALSO Timer3 OC3A (see §2)  */

#define BOARD_PIN_INT0     2     /* PD2 — the only external interrupts are   */
#define BOARD_PIN_INT1     3     /* PD3   INT0/D2 and INT1/D3, as on a Nano  */

/* -------------------------------------------------------------------------
 * PWM / DAC. The timer behind a pin is what using that timer costs.
 * digitalPinHasPWM() is inherited from the AVR core and is WRONG here: it
 * reports only 3/5/6/9/10/11 and misses D1, D2 and D4.
 * ---------------------------------------------------------------------- */
#define BOARD_PIN_PWM_D1   1     /* Timer3 OC3A — steals the UART TX pin     */
#define BOARD_PIN_PWM_D2   2     /* Timer3 OC3B — steals INT0                */
#define BOARD_PIN_PWM_D3   3     /* Timer2 OC2B                              */
#define BOARD_PIN_DAC      4     /* PD4 = DAC0. analogWrite(4,v) is the 8-bit
                                    DAC, NOT PWM — a real analog voltage     */
#define BOARD_PIN_PWM_D5   5     /* Timer0 OC0B — Timer0 is millis()/delay() */
#define BOARD_PIN_PWM_D6   6     /* Timer0 OC0A — never reconfigure Timer0   */
#define BOARD_PIN_PWM_D9   9     /* Timer1 OC1A                              */
#define BOARD_PIN_PWM_D10  10    /* Timer1 OC1B; also SPI slave-select       */
#define BOARD_PIN_PWM_D11  11    /* Timer2 OC2A; also SPI MOSI               */

/* -------------------------------------------------------------------------
 * Buses
 * ---------------------------------------------------------------------- */
#define BOARD_PIN_SDA      A4    /* PC4 = 18 */
#define BOARD_PIN_SCL      A5    /* PC5 = 19 */
#define BOARD_PIN_SS       10    /* PB2 */
#define BOARD_PIN_MOSI     11    /* PB3 */
#define BOARD_PIN_MISO     12    /* PB4 */
#define BOARD_PIN_SCK      13    /* PB5 + LED_BUILTIN */

/* -------------------------------------------------------------------------
 * ADC. 12-bit hardware; analogRead() returns 10 bits until you call
 * analogReadResolution(12). A0-A5 are full digital pins (14-19); A6/A7 are
 * analog-input only, as on a Nano.
 * ---------------------------------------------------------------------- */
#define BOARD_PIN_ADC_A0   A0    /* PC0 = 14 */
#define BOARD_PIN_ADC_A6   A6    /* PE1 = 20 — analog input only             */
#define BOARD_PIN_ADC_A7   A7    /* PE3 = 21 — analog input only             */

/* Internal ADC channels (core macros, LQFP32 variant). analogRead(V5D1)
 * returns VCC/5 — the way to find the real supply, which sits near 4.6 V on
 * USB power because of the protection diode in the 5 V line. */
#define BOARD_ADC_VCC_DIV5 V5D1  /* = 22, VCC x 1/5 */
#define BOARD_ADC_VCC_DIV4 V5D4  /* = 28, VCC x 4/5 */
#define BOARD_ADC_IVREF    IVREF /* = 27, the internal reference itself */

/* -------------------------------------------------------------------------
 * Extra pins this board has and a Nano does not. Use the E0/E2/E4/E5/E6
 * names: the Dnn aliases for PE5/PE6 do NOT match their own numbers
 * (D25 == 26 == PE5, D26 == 25 == PE6).
 * ---------------------------------------------------------------------- */
#define BOARD_PIN_SWC      E0    /* = 22, PE0 — SWD clock. GPIO use kills    */
#define BOARD_PIN_SWD      E2    /* = 23, PE2 — SWD data.  the debug/recovery
                                    port; see rule 12 before touching them   */
#define BOARD_PIN_PE4      E4    /* = 24, pad is PE4 || PF4 (OC0A / OC1B)    */
#define BOARD_PIN_PE5      E5    /* = 26, pad is PE5 || PF5 (AC1O / OC1A)    */
#define BOARD_PIN_AREF     E6    /* = 25, PE6 — AVREF by default; needs
                                    PMX2.E6EN before it is a GPIO            */
#define BOARD_PIN_RESET    C6    /* = 27, PC6 — reset by default; needs
                                    PMX2.C6EN, and then you lose reset       */

/*
 * Pads that carry two internal ports at once on this package (databook
 * §"Pinout Configuration"). Driving the hidden half against the visible one
 * is a short inside the die:
 *   D1  = PD1 || PF1      D2  = PD2 || PF2
 *   E4  = PE4 || PF4      E5  = PE5 || PF5
 *
 * 80 mA drive (HDR register, default 12 mA). Header pins only:
 *   HDR[0] PD5=D5   HDR[1] PD6=D6   HDR[2] PD1=D1(TX)   HDR[3] PD2=D2
 *   HDR[4] PE4=E4   HDR[5] PE5=E5
 * QFP32 has one VCC/GND pair — the databook says do not drive four of these
 * at high current at once.
 *
 * Power (../reference/board-hardware.md §4):
 *   A protection diode in the 5 V line means VCC ~ 4.6 V on USB power.
 *   analogRead() against DEFAULT (= AVCC) is therefore scaled by ~0.92.
 *   All GPIO are VCC-level push-pull; the part runs 1.8-5.5 V.
 */

#endif /* BOARD_H */
