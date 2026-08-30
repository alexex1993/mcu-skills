#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "board.h"
#include "app.h"

static bool s_mounted = false;

bool sdBegin(void) {
  /* setPins() MUST be called before begin(). Called after, it returns true and
   * changes nothing, and begin() then tries the ESP32-S3 default SDMMC pins —
   * which on an R8 module include the octal-PSRAM pins and reboot the board. */
  if (!SD_MMC.setPins(BOARD_SD_CLK, BOARD_SD_CMD, BOARD_SD_D0)) {
    Serial.println("sd: setPins failed");
    return false;
  }

  /* mode1bit = true: D1/D2/D3 are not routed on this board.
   * format_if_empty = false: never reformat a user's card behind their back. */
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5)) {
    Serial.println("sd: mount failed — card absent, not FAT32, or contacts dirty");
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("sd: no card detected");
    return false;
  }
  s_mounted = true;
  return true;
}

void sdReport(void) {
  if (!s_mounted) { Serial.println("microSD     : not mounted"); return; }
  const char *type = "UNKNOWN";
  switch (SD_MMC.cardType()) {
    case CARD_MMC:  type = "MMC";  break;
    case CARD_SD:   type = "SDSC"; break;
    case CARD_SDHC: type = "SDHC"; break;
    default: break;
  }
  Serial.printf("microSD     : %s, %llu MB, %llu MB used (1-bit SDMMC)\n",
                type, SD_MMC.cardSize() / (1024ULL * 1024ULL),
                SD_MMC.usedBytes() / (1024ULL * 1024ULL));
}

bool sdWriteBlob(const char *path, const uint8_t *buf, size_t len) {
  if (!s_mounted) return false;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) { Serial.printf("sd: cannot open %s for writing\n", path); return false; }
  size_t n = f.write(buf, len);
  f.close();
  if (n != len) { Serial.printf("sd: short write %u/%u\n", (unsigned)n, (unsigned)len); return false; }
  return true;
}

int sdNextIndex(const char *dir) {
  if (!s_mounted) return 0;
  if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
  File d = SD_MMC.open(dir);
  int max_i = 0;
  if (d && d.isDirectory()) {
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
      int i = atoi(strrchr(e.name(), '/') ? strrchr(e.name(), '/') + 1 : e.name());
      if (i > max_i) max_i = i;
      e.close();
    }
  }
  if (d) d.close();
  return max_i + 1;
}
