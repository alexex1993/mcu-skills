/*
 * NodeMCU 30-pin ESP8266 — board self-test.
 *
 * Prints everything you need to tell a board problem from a code problem:
 * reset cause, chip and flash identity, the real flash layout, heap, the
 * boot-strap levels, the ADC, whether the GPIO16<->RST deep-sleep link is
 * fitted, a LittleFS boot counter and a Wi-Fi scan. Then it blinks both LEDs
 * and re-runs the report when you press FLASH.
 *
 * Nothing here blocks for more than a few ms: loop() must return often enough
 * for the SDK to service Wi-Fi and feed the watchdog.
 */
#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include "board.h"

/* ---------- GPIO16 <-> RST link self-test -------------------------------- */
/* RTC RAM survives reset but not power-off, which is exactly what this needs. */
struct RtcProbe { uint32_t magic; uint32_t stage; };
static const uint32_t PROBE_MAGIC = 0x8266C0D7;

static const char *gpio16_rst_link()
{
  RtcProbe p;
  ESP.rtcUserMemoryRead(0, (uint32_t *)&p, sizeof(p));

  if (p.magic == PROBE_MAGIC && p.stage == 1) {
    /* We armed the test, then rebooted: the pin really is tied to RST. */
    p.stage = 2;
    ESP.rtcUserMemoryWrite(0, (uint32_t *)&p, sizeof(p));
    return "FITTED (deep sleep will wake)";
  }
  if (p.magic == PROBE_MAGIC && p.stage == 2)
    return "FITTED (latched; power-cycle to retest)";

  p.magic = PROBE_MAGIC;
  p.stage = 1;
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&p, sizeof(p));
  Serial.flush();
  pinMode(BOARD_D0, OUTPUT);
  digitalWrite(BOARD_D0, LOW);      /* if R10 is fitted this resets the chip */
  delay(5);
  digitalWrite(BOARD_D0, HIGH);
  delay(20);
  p.stage = 0;                       /* survived — re-arm for the next boot  */
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&p, sizeof(p));
  return "NOT fitted (ESP.deepSleep never wakes)";
}

/* ---------- report ------------------------------------------------------- */
static void rule() { Serial.println(F("------------------------------------------------")); }

static void report()
{
  rule();
  Serial.println(F("NodeMCU 30-pin ESP8266 self-test"));
  rule();

  Serial.printf("reset reason  : %s\n", ESP.getResetReason().c_str());
  Serial.printf("boot mode     : %u (1 = normal flash boot)\n", ESP.getBootMode());
  Serial.printf("chip id       : 0x%06X   mac %s\n", ESP.getChipId(), WiFi.macAddress().c_str());
  Serial.printf("core / sdk    : %s / %s\n", ESP.getCoreVersion().c_str(), ESP.getSdkVersion());
  Serial.printf("cpu           : %u MHz\n", ESP.getCpuFreqMHz());
  rule();

  Serial.printf("flash id      : 0x%08X\n", ESP.getFlashChipId());
  Serial.printf("flash size    : %u B real / %u B configured%s\n",
                ESP.getFlashChipRealSize(), ESP.getFlashChipSize(),
                ESP.getFlashChipRealSize() == ESP.getFlashChipSize()
                  ? "" : "   <-- MISMATCH: wrong board/ldscript");
  Serial.printf("flash speed   : %u Hz, mode %d (0=QIO 1=QOUT 2=DIO 3=DOUT)\n",
                ESP.getFlashChipSpeed(), (int)ESP.getFlashChipMode());
  Serial.printf("sketch        : %u B used, %u B free for OTA\n",
                ESP.getSketchSize(), ESP.getFreeSketchSpace());
  rule();

  Serial.printf("heap free     : %u B  (frag %u%%, largest block %u B)\n",
                ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
  Serial.printf("deepSleepMax  : %.2f h\n", ESP.deepSleepMax() / 3600e6);
  rule();

  /* Boot straps, read back after boot. Required: 0 = H, 2 = H, 15 = L. */
  pinMode(BOARD_D3, INPUT);
  pinMode(BOARD_D4, INPUT);
  pinMode(BOARD_D8, INPUT);
  Serial.printf("straps        : GPIO0=%d (want 1)  GPIO2=%d (want 1)  GPIO15=%d (want 0)\n",
                digitalRead(BOARD_D3), digitalRead(BOARD_D4), digitalRead(BOARD_D8));
  Serial.printf("GPIO16<->RST  : %s\n", gpio16_rst_link());
  rule();

  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) { acc += analogRead(BOARD_ADC); delay(1); }
  Serial.printf("A0            : %u counts of 1023  (~%.2f V at the header pin)\n",
                acc / 16, (acc / 16) * 3.2f / 1023.0f);
  rule();

  /* LittleFS boot counter — also proves the filesystem partition is real. */
  if (LittleFS.begin()) {
    uint32_t boots = 0;
    File f = LittleFS.open("/boots.bin", "r");
    if (f) { f.read((uint8_t *)&boots, sizeof(boots)); f.close(); }
    boots++;
    f = LittleFS.open("/boots.bin", "w");
    if (f) { f.write((const uint8_t *)&boots, sizeof(boots)); f.close(); }

    FSInfo info;
    LittleFS.info(info);
    Serial.printf("littlefs      : boot #%u, %u/%u B used\n",
                  boots, info.usedBytes, info.totalBytes);
  } else {
    Serial.println(F("littlefs      : mount FAILED — run `pio run -t uploadfs` once,"));
    Serial.println(F("                or check board_build.filesystem = littlefs"));
  }
  rule();

  /* Wi-Fi scan: proves the radio and the power supply. A reset here is a
     brownout, not a bug — the TX burst wants ~500 mA peak. */
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.printf("wifi scan     : %d networks\n", n);
  for (int i = 0; i < n && i < 5; i++)
    Serial.printf("                %-24s %4d dBm  ch %2d  %s\n",
                  WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "enc");
  WiFi.scanDelete();
  Serial.printf("heap after wifi: %u B\n", ESP.getFreeHeap());
  rule();
  Serial.println(F("press FLASH (D3/GPIO0) to re-run"));
  rule();
}

/* ---------- runtime ------------------------------------------------------ */
void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println();

  pinMode(BOARD_LED_MODULE, OUTPUT);
  digitalWrite(BOARD_LED_MODULE, BOARD_LED_OFF);
#if BOARD_HAS_LED_ON_GPIO16
  pinMode(BOARD_LED_BOARD, OUTPUT);
  digitalWrite(BOARD_LED_BOARD, BOARD_LED_OFF);
#endif
  pinMode(BOARD_BTN_FLASH, INPUT_PULLUP);

  report();
}

void loop()
{
  /* Non-blocking heartbeat: 100 ms on, 900 ms off, on the module LED. */
  static uint32_t last = 0;
  static bool lit = false;
  uint32_t now = millis();
  if (now - last >= (uint32_t)(lit ? 100 : 900)) {
    last = now;
    lit = !lit;
    digitalWrite(BOARD_LED_MODULE, lit ? BOARD_LED_ON : BOARD_LED_OFF);
#if BOARD_HAS_LED_ON_GPIO16
    digitalWrite(BOARD_LED_BOARD, lit ? BOARD_LED_OFF : BOARD_LED_ON);
#endif
  }

  /* FLASH button, debounced. Held at RESET it means download mode instead. */
  static uint32_t edge = 0;
  static int prev = HIGH;
  int cur = digitalRead(BOARD_BTN_FLASH);
  if (cur != prev && now - edge > 40) {
    edge = now;
    prev = cur;
    if (cur == LOW) report();
  }
}
