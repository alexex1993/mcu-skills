// Everything on the shared I2C bus, plus the battery divider.
//
// One bus, four slaves: SHTC3 0x70, ES8311 0x18, ES7210 0x40/0x42,
// PCF85063A 0x51. The two codecs are left alone here — see
// reference/recipes.md for the audio chain.
//
// The SHTC3 and PCF85063A drivers below are written against the datasheets
// rather than pulled in as libraries, so the template has exactly one
// lib_deps entry (U8g2) and nothing to go stale.

#include <Arduino.h>
#include <Wire.h>

#include "app.h"
#include "board_pins.h"

void sensorsBegin() {
  // Always pass the pins. The ESP32-S3 Arduino variant defaults Wire to
  // SDA = 8, SCL = 9, which on this board are the I2S data and bit clock.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

  // GPIO4 is ADC1_CH3. 12 dB attenuation lifts the full-scale reading to
  // ~3.1 V; a full 18650 through the 1/3 divider sits at ~1.4 V, which
  // overflows the default ~0.95 V range and reads as a stuck maximum.
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}

void i2cScan() {
  Serial.println("i2c scan:");
  int found = 0;
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      const char *who = "";
      switch (addr) {
        case I2C_ADDR_ES8311:   who = " ES8311 (speaker codec)"; break;
        case I2C_ADDR_ES7210:   who = " ES7210 (mic ADC)"; break;
        case 0x42:              who = " ES7210 (mic ADC, alt address)"; break;
        case I2C_ADDR_PCF85063: who = " PCF85063A (RTC)"; break;
        case I2C_ADDR_SHTC3:    who = " SHTC3 (temp/humidity)"; break;
        default: break;
      }
      Serial.printf("  0x%02X%s\n", addr, who);
      found++;
    }
  }
  if (found == 0) Serial.println("  nothing answered — wrong pins or bus held low");
}

// --- SHTC3 ----------------------------------------------------------------

static bool shtc3Cmd(uint16_t cmd) {
  Wire.beginTransmission(I2C_ADDR_SHTC3);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  return Wire.endTransmission() == 0;
}

bool shtc3Read(float *tempC, float *rh) {
  if (!shtc3Cmd(0x3517)) return false;  // wake up
  delayMicroseconds(300);               // 240 us minimum

  // Normal-mode measurement, temperature first, clock stretching enabled.
  if (!shtc3Cmd(0x7CA2)) return false;
  delay(13);  // 12.1 ms max for a normal-mode conversion

  if (Wire.requestFrom((int)I2C_ADDR_SHTC3, 6) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();

  shtc3Cmd(0xB098);  // back to sleep; ~0.6 uA

  uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];  // b[2] is the CRC
  uint16_t rawH = ((uint16_t)b[3] << 8) | b[4];  // b[5] is the CRC

  *tempC = -45.0f + 175.0f * (float)rawT / 65535.0f;
  *rh = 100.0f * (float)rawH / 65535.0f;
  return true;
}

// --- PCF85063A ------------------------------------------------------------

static uint8_t bcd2dec(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

bool pcf85063ReadTime(struct tm *out) {
  Wire.beginTransmission(I2C_ADDR_PCF85063);
  Wire.write(0x04);  // Seconds
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)I2C_ADDR_PCF85063, 7) != 7) return false;
  uint8_t sec = Wire.read(), min = Wire.read(), hour = Wire.read();
  uint8_t day = Wire.read(), wday = Wire.read();
  uint8_t mon = Wire.read(), year = Wire.read();

  // Bit 7 of Seconds is OS: set means the oscillator stopped and the time is
  // not trustworthy. That is the normal state on a board with no RTC cell in
  // the PH1.0 holder, which is how these ship.
  bool clockIntegrity = (sec & 0x80) == 0;

  out->tm_sec  = bcd2dec(sec & 0x7F);
  out->tm_min  = bcd2dec(min & 0x7F);
  out->tm_hour = bcd2dec(hour & 0x3F);  // 24-hour mode (Control_1 bit 1 = 0)
  out->tm_mday = bcd2dec(day & 0x3F);
  out->tm_wday = wday & 0x07;
  out->tm_mon  = bcd2dec(mon & 0x1F) - 1;
  out->tm_year = bcd2dec(year) + 100;   // struct tm counts from 1900

  return clockIntegrity;
}

// --- Battery --------------------------------------------------------------

float batteryVolts() {
  // analogReadMilliVolts() applies the chip's eFuse ADC calibration, which is
  // worth ~1-2% against analogRead()'s raw counts. Average a few: the divider
  // is high-impedance and the SAR input is noisy.
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) acc += analogReadMilliVolts(PIN_BAT_ADC);
  return (float)acc / 8.0f / 1000.0f * BAT_DIVIDER;
}
