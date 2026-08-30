/* app.h — interfaces of the self-test modules. Each .cpp is independent;
 * delete one and its call in setup()/loop() and the rest still builds. */
#pragma once
#include <Arduino.h>

/* board_report.cpp */
void reportBoard(void);

/* camera.cpp */
bool cameraBegin(void);                       /* false = init failed, reason printed */
bool cameraCaptureToSd(const char *dir);      /* one JPEG into <dir> on the SD card */
void cameraReport(void);

/* sdcard.cpp */
bool sdBegin(void);                           /* SDMMC 1-bit mount at /sdcard */
void sdReport(void);
bool sdWriteBlob(const char *path, const uint8_t *buf, size_t len);
int  sdNextIndex(const char *dir);
