// Procedural video-effect carousel for the ESP32-C6-LCD-1.47.
//
// Each effect is a self-contained {init, frame, done} triplet. init() is
// called once on entry and may allocate working buffers; frame(ms) renders a
// single image into g_fb (ms = milliseconds since the effect started); done()
// frees whatever init() allocated. Because buffers are per-effect, only one
// effect's working set is ever alive at a time, keeping the heap footprint
// small.
//
// The per-pixel inner loops are strictly integer math -- the ESP32-C6's
// RISC-V core has no FPU -- so float is confined to the one-time init paths.

#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    void (*init)(void);
    void (*frame)(uint32_t ms);
    void (*done)(void);
    uint32_t duration_ms;
} effect_t;

extern const effect_t *const g_effects[];
extern const int g_effect_count;
