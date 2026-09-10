// The 4.2" reflective panel. Sitronix ST7305 command set, 300 x 400 native,
// 1 bit per pixel. A full frame is 300 * 400 / 8 = 15000 bytes, so U8g2's
// full-frame ("_F_") buffer fits in internal SRAM with room to spare and there
// is no need for the firstPage()/nextPage() dance.

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#include "app.h"
#include "board_pins.h"

// The controller's own orientation is 300 wide x 400 tall. U8G2_R1 rotates that
// into the 400 x 300 landscape the board is meant to be read in. If text comes
// out upside down for your enclosure, use U8G2_R3.
#define LCD_ROTATION U8G2_R1

// 4-wire hardware SPI, no MISO — the ST7305 is write-only.
static U8G2_ST7305_300X400_F_4W_HW_SPI lcd(LCD_ROTATION, PIN_LCD_CS, PIN_LCD_DC,
                                           PIN_LCD_RST);

void displayBegin() {
  // U8g2 talks to the panel through the global SPI object but never assigns its
  // pins, and the ESP32-S3 defaults are not the ones this board is wired with.
  // Claiming the bus here first makes U8g2's own SPI.begin() a no-op. Without
  // this line the panel gets no data and stays blank — with no error anywhere.
  SPI.begin(PIN_LCD_SCLK, -1 /* no MISO */, PIN_LCD_MOSI, -1 /* CS is U8g2's */);

  lcd.begin();

  // SPI2 reaches these pins through the GPIO matrix rather than the IO MUX,
  // which caps the bus at ~40 MHz; the ST7305 itself wants a write period of
  // at least 30 ns (~33 MHz). 24 MHz clears both with margin.
  lcd.setBusClock(24000000);
}

static void drawCentered(const char *s, int y) {
  int x = (lcd.getDisplayWidth() - lcd.getStrWidth(s)) / 2;
  lcd.drawStr(x, y, s);
}

void displayDrawDashboard(float tempC, float rh, float battV, uint32_t wakes) {
  const int w = lcd.getDisplayWidth();   // 400
  const int h = lcd.getDisplayHeight();  // 300

  lcd.clearBuffer();

  // Title bar, drawn inverted.
  lcd.drawBox(0, 0, w, 40);
  lcd.setDrawColor(0);
  lcd.setFont(u8g2_font_helvB14_tr);
  drawCentered("ESP32-S3-RLCD-4.2", 27);
  lcd.setDrawColor(1);

  lcd.drawFrame(0, 0, w, h);

  char buf[48];

  lcd.setFont(u8g2_font_helvR14_tr);
  lcd.drawStr(20, 75, "TEMP");
  lcd.drawStr(220, 75, "HUMIDITY");

  lcd.setFont(u8g2_font_logisoso32_tn);
  if (isnan(tempC)) {
    lcd.setFont(u8g2_font_helvR14_tr);
    lcd.drawStr(20, 115, "n/a");
  } else {
    snprintf(buf, sizeof(buf), "%.1f", tempC);
    lcd.drawStr(20, 118, buf);
  }

  lcd.setFont(u8g2_font_logisoso32_tn);
  if (isnan(rh)) {
    lcd.setFont(u8g2_font_helvR14_tr);
    lcd.drawStr(220, 115, "n/a");
  } else {
    snprintf(buf, sizeof(buf), "%.0f", rh);
    lcd.drawStr(220, 118, buf);
  }

  lcd.drawHLine(0, 150, w);

  lcd.setFont(u8g2_font_helvR14_tr);
  snprintf(buf, sizeof(buf), "Battery  %.2f V", battV);
  lcd.drawStr(20, 180, buf);

  // Battery bar: outline, then a fill proportional to 2.5 V .. 4.2 V.
  const int bx = 20, by = 195, bw = w - 40, bh = 28;
  lcd.drawFrame(bx, by, bw, bh);
  float pct = (battV - 2.5f) / (4.2f - 2.5f);
  pct = pct < 0.0f ? 0.0f : (pct > 1.0f ? 1.0f : pct);
  int fill = (int)((bw - 4) * pct);
  if (fill > 0) lcd.drawBox(bx + 2, by + 2, fill, bh - 4);

  lcd.setFont(u8g2_font_6x13_tr);
  snprintf(buf, sizeof(buf), "refresh #%lu   KEY redraws", (unsigned long)wakes);
  lcd.drawStr(20, 270, buf);

  lcd.sendBuffer();
}

// U8g2 has no wrapper for the ST7305 power modes, so the command bytes go out
// directly. setPowerSave(1) is NOT this: it sends 0x28 (display off) and the
// image disappears.
void displayEnterLowPower() {
  lcd.sendF("c", 0x39);  // LPM — panel self-refresh drops to ~1 Hz
  delay(100);            // the datasheet requires a settling delay
}

void displayLeaveLowPower() {
  lcd.sendF("c", 0x38);  // HPM — ~32 Hz, required before a fast redraw
  delay(100);
}
