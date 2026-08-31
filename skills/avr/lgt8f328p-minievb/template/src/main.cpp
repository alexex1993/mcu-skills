/*
 * LGT8F328P-LQFP32 MiniEVB ("LGTBF32BP") — full bring-up.
 *
 * Exercises the five things this chip does that an ATmega328P does not, and
 * that a ported Nano sketch therefore gets wrong:
 *
 *   - 32 MHz from the internal RC, no crystal   (report F_CPU / F_OSC)
 *   - a 12-bit ADC that pretends to be 10-bit   (analogReadResolution)
 *   - a real supply that is not 5 V             (VCC read back via V5D1)
 *   - an 8-bit DAC sitting on D4                (analogWrite(4, v))
 *   - an EEPROM emulated in program flash       (wiped by every upload)
 *
 * Timing is the non-blocking millis() pattern; delay() is never called in
 * loop(). Every string literal is in F() — SRAM is 2 KB and the heap and
 * stack share it with no MPU between them.
 */

#include <Arduino.h>
#include <EEPROM.h>

#include "board.h"

#define HEARTBEAT_MS   500u    /* toggling every 500 ms = 1 Hz blink */
#define REPORT_MS      5000u   /* serial report cadence              */

/* The internal 1.024 V reference, in millivolts, and the divider in front of
 * the V5D1 channel. VCC = adc / full_scale * 1024 mV * 5. */
#define IVREF_MV       1024UL
#define ADC_FULL_SCALE 4096UL  /* after analogReadResolution(12) */

struct boot_log_t {
    uint32_t boots;            /* survives reset, NOT a re-upload */
    uint32_t last_uptime_s;
};

static uint32_t last_beat_ms   = 0;
static uint32_t last_report_ms = 0;
static bool     led_state      = false;
static uint8_t  dac_level      = 0;

/* Free bytes between the top of the heap and the stack pointer. On a 2 KB
 * part this is the number to watch: below ~200 B, restructure. The collision,
 * when it comes, looks like random variable corruption, not an allocation
 * failure. */
static int free_ram(void)
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

/* Real supply voltage in millivolts, measured against the internal 1.024 V
 * reference through the chip's own VCC/5 channel. On USB power this reads
 * near 4600, not 5000: a protection diode sits in the board's 5 V line.
 * Anything that calls analogRead() with the DEFAULT (AVCC) reference is
 * scaled by this number, which is why it is worth printing. */
static uint16_t vcc_mv(void)
{
    analogReference(INTERNAL1V024);
    (void)analogRead(BOARD_ADC_VCC_DIV5);   /* discard: the mux just moved */
    const uint32_t raw = analogRead(BOARD_ADC_VCC_DIV5);
    return (uint16_t)((raw * IVREF_MV * 5UL) / ADC_FULL_SCALE);
}

void setup()
{
    pinMode(BOARD_PIN_LED, OUTPUT);

    /* The hardware is 12-bit; the core throws the bottom 2 bits away for
     * Arduino compatibility until this is called. */
    analogReadResolution(12);

    Serial.begin(115200);

    /* 1 KB of emulated EEPROM, which costs 2 KB of program flash. Calling
     * this is what selects the size; the default is the same 1 KB. */
    lgt_eeprom_init(1);

    boot_log_t log;
    EEPROM.get(0, log);
    if (log.boots == 0xFFFFFFFFul) {   /* blank, or freshly re-uploaded */
        log.boots = 0;
        log.last_uptime_s = 0;
    }
    log.boots++;
    EEPROM.put(0, log);

    Serial.print(F("# LGT8F328P up, F_CPU "));
    Serial.print(F_CPU / 1000000UL);
    Serial.print(F(" MHz from "));
    Serial.print(F_OSC / 1000000UL);
    Serial.print(F(" MHz "));
    Serial.println(CLOCK_SOURCE == 1 ? F("internal RC") : F("external xtal"));

    Serial.print(F("# boot "));
    Serial.print(log.boots);
    Serial.print(F(", previous run "));
    Serial.print(log.last_uptime_s);
    Serial.print(F(" s, VCC "));
    Serial.print(vcc_mv());
    Serial.println(F(" mV"));
}

void loop()
{
    const uint32_t now = millis();

    if (now - last_beat_ms >= HEARTBEAT_MS) {
        last_beat_ms = now;
        led_state = !led_state;
        digitalWrite(BOARD_PIN_LED, led_state);

        /* D4 is the DAC, not a PWM pin: this is a staircase of real analog
         * voltages between 0 and VCC, no filter needed. */
        dac_level += 16;
        analogWrite(BOARD_PIN_DAC, dac_level);
    }

    if (now - last_report_ms >= REPORT_MS) {
        last_report_ms = now;

        boot_log_t log;
        EEPROM.get(0, log);
        log.last_uptime_s = now / 1000;
        EEPROM.put(0, log);

        /* Back to AVCC for the header pin. Note the scale: full range is
         * VCC (~4.6 V), not 5.000 V. */
        analogReference(DEFAULT);
        (void)analogRead(BOARD_PIN_ADC_A0);
        const int a0 = analogRead(BOARD_PIN_ADC_A0);

        Serial.print(F("up "));
        Serial.print(now / 1000);
        Serial.print(F(" s, free RAM "));
        Serial.print(free_ram());
        Serial.print(F(" B, A0 "));
        Serial.print(a0);
        Serial.print(F("/4095, VCC "));
        Serial.print(vcc_mv());
        Serial.println(F(" mV"));
    }
}
