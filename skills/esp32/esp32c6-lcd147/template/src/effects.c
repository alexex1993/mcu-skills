// Four classic demoscene effects, each writing directly into g_fb.

#include <math.h>
#include <stdlib.h>

#include "esp_random.h"

#include "effects.h"
#include "gfx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// HSV (0..1) -> RGB 0..255. Used only in one-time init code.
static void hsv_to_rgb(float h, float s, float v,
                       int *r, int *g, int *b)
{
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float rr, gg, bb;
    switch ((int)i % 6) {
        case 0: rr = v; gg = t; bb = p; break;
        case 1: rr = q; gg = v; bb = p; break;
        case 2: rr = p; gg = v; bb = t; break;
        case 3: rr = p; gg = q; bb = v; break;
        case 4: rr = t; gg = p; bb = v; break;
        default: rr = v; gg = p; bb = q; break;
    }
    *r = (int)(rr * 255.0f);
    *g = (int)(gg * 255.0f);
    *b = (int)(bb * 255.0f);
}

// =============================================================================
// Plasma -- four integer sine waves summed per pixel, looked up in a 256-entry
// rainbow palette. The classic flowing-colour look.
// =============================================================================

static int8_t  pl_sin[256];       // sin(x) scaled to -127..127
static uint16_t pl_palette[256];  // cyclic HSV rainbow, wire-order RGB565

static void plasma_init(void)
{
    for (int i = 0; i < 256; i++) {
        float s = sinf((float)i * 2.0f * (float)M_PI / 256.0f);
        pl_sin[i] = (int8_t)lroundf(s * 127.0f);
    }
    for (int i = 0; i < 256; i++) {
        int r, g, b;
        hsv_to_rgb(i / 255.0f, 1.0f, 1.0f, &r, &g, &b);
        pl_palette[i] = GFX_RGB(r, g, b);
    }
}

static void plasma_frame(uint32_t ms)
{
    const int ph = (int)(ms >> 3);            // slow global phase
    const int ph2 = (int)(ms >> 4);
    const int ph3 = (int)(ms >> 5);
    uint16_t *p = g_fb;

    for (int y = 0; y < LCD_V_RES; y++) {
        int sy_y = pl_sin[(y * 2 - ph2) & 255];
        for (int x = 0; x < LCD_H_RES; x++) {
            int v  = pl_sin[(x * 2 + ph)  & 255];
            v += sy_y;
            v += pl_sin[((x + y) + ph3) & 255];
            v += pl_sin[((x - y) + ph2) & 255];
            // v spans roughly -508..508; fold into 0..255.
            int idx = ((v + 512) >> 2) & 255;
            *p++ = pl_palette[idx];
        }
    }
}

static void plasma_done(void) {}

// =============================================================================
// Starfield -- stars scattered in 3D, flying toward the camera (warp drive).
// Cheapest of the lot: only a few hundred pixels are touched per frame.
// =============================================================================

#define STARS_N 220

typedef struct { int16_t x, y, z; } star_t;

static star_t *st_stars;
static uint16_t st_palette[32];   // 32 cool blue->white brightness steps

static int16_t star_rand16(int range)
{
    return (int16_t)(esp_random() % (uint32_t)(2 * range + 1)) - range;
}

static void star_spawn(star_t *s)
{
    s->x = star_rand16(110);
    s->y = star_rand16(200);
    s->z = 220 + (int16_t)(esp_random() % 64);
}

static void star_init(void)
{
    st_stars = malloc(STARS_N * sizeof(star_t));
    for (int i = 0; i < STARS_N; i++) {
        star_spawn(&st_stars[i]);
        st_stars[i].z = (int16_t)(esp_random() % 256) + 1;  // spread depth
    }
    for (int i = 0; i < 32; i++) {
        float t = i / 31.0f;
        int r = (int)(40.0f + t * 215.0f);
        int g = (int)(60.0f + t * 195.0f);
        int b = (int)(110.0f + t * 145.0f);
        st_palette[i] = GFX_RGB(r, g, b);
    }
}

static void star_frame(uint32_t ms)
{
    (void)ms;
    const int cx = LCD_H_RES / 2;
    const int cy = LCD_V_RES / 2;
    const int speed = 7;

    gfx_clear(GFX_RGB(3, 3, 12));

    for (int i = 0; i < STARS_N; i++) {
        star_t *s = &st_stars[i];
        s->z -= speed;
        if (s->z <= 0) {
            star_spawn(s);
            continue;
        }
        int px = cx + ((int)s->x * 220) / s->z;
        int py = cy + ((int)s->y * 220) / s->z;
        if ((unsigned)px >= LCD_H_RES || (unsigned)py >= LCD_V_RES) {
            star_spawn(s);
            continue;
        }
        // Brightness grows as the star approaches the camera.
        int b = 256 - s->z;                 // 0..255ish
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        g_fb[py * LCD_H_RES + px] = st_palette[b >> 3];
    }
}

static void star_done(void)
{
    free(st_stars);
    st_stars = NULL;
}

// =============================================================================
// Tunnel -- a texture mapped onto polar coordinates around the screen centre.
// Per-pixel (u,v) is baked into a table at init; the frame loop just scrolls
// the texture offset. Looks like flying down a checkered shaft.
// =============================================================================

#define TUN_TEX_W 64
#define TUN_TEX_H 64

static uint16_t *tun_tex;                          // 64x64 procedural texture
static uint16_t *tun_uv;                           // packed (v<<6)|u per pixel

static void tunnel_init(void)
{
    tun_tex = malloc(TUN_TEX_W * TUN_TEX_H * sizeof(uint16_t));
    tun_uv  = malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t));

    // Cyan/magenta checkered texture with a vertical brightness gradient.
    for (int ty = 0; ty < TUN_TEX_H; ty++) {
        for (int tx = 0; tx < TUN_TEX_W; tx++) {
            int block = ((tx >> 3) ^ (ty >> 3)) & 1;
            int grad = (ty < 32) ? ty : (63 - ty);          // 0..31 ramp
            int shade = block ? (90 + grad * 5) : (10 + grad * 2);
            if (shade > 255) shade = 255;
            int r = shade >> 2;
            int g = shade;
            int b = shade - (shade >> 3);
            tun_tex[ty * TUN_TEX_W + tx] = GFX_RGB(r, g, b);
        }
    }

    const int cx = LCD_H_RES / 2;
    const int cy = LCD_V_RES / 2;
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            float dist = sqrtf(dx * dx + dy * dy) + 0.5f;
            float ang = atan2f(dy, dx);                      // -pi..pi
            int u = (int)(ang * (TUN_TEX_H / (2.0f * (float)M_PI)) * 2.0f);
            int v = (int)((TUN_TEX_W * 2.0f) / dist);        // rings tighten
            u &= (TUN_TEX_W - 1);
            v &= (TUN_TEX_H - 1);
            tun_uv[y * LCD_H_RES + x] = (uint16_t)((v << 6) | u);
        }
    }
}

static void tunnel_frame(uint32_t ms)
{
    const int off_u = (int)(ms / 90);   // rotation
    const int off_v = -(int)(ms / 14);  // fly into the shaft
    uint16_t *p = g_fb;

    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        uint16_t uv = tun_uv[i];
        int u = (uv & 63) + off_u;
        int v = ((uv >> 6) & 63) + off_v;
        u &= 63;
        v &= 63;
        *p++ = tun_tex[v * TUN_TEX_W + u];
    }
}

static void tunnel_done(void)
{
    free(tun_tex);
    free(tun_uv);
    tun_tex = NULL;
    tun_uv = NULL;
}

// =============================================================================
// Fire -- a per-pixel heat buffer propagated upward one row per frame, mapped
// through a black->red->orange->yellow->white palette.
// =============================================================================

static uint8_t  *fire_heat;
static uint16_t fire_palette[256];

static void fire_init(void)
{
    fire_heat = calloc(LCD_H_RES * LCD_V_RES, 1);
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        float r = t * 1.6f;
        float g = t * 1.6f - 0.55f;
        float b = t * 1.7f - 1.25f;
        if (r > 1.0f) r = 1.0f;
        if (g > 1.0f) g = 1.0f;
        if (g < 0.0f) g = 0.0f;
        if (b > 1.0f) b = 1.0f;
        if (b < 0.0f) b = 0.0f;
        fire_palette[i] = GFX_RGB((int)(r * 255), (int)(g * 255), (int)(b * 255));
    }
}

static void fire_frame(uint32_t ms)
{
    (void)ms;
    const int W = LCD_H_RES;
    const int H = LCD_V_RES;

    // Re-ignite the bottom two rows with hot spots (some gaps for flicker).
    uint8_t *row_last = &fire_heat[(H - 1) * W];
    uint8_t *row_prev = &fire_heat[(H - 2) * W];
    for (int x = 0; x < W; x++) {
        uint32_t r = esp_random();
        row_last[x] = (r & 1) ? 255 : 90;
        row_prev[x] = (uint8_t)(140 + (r >> 1) % 116);
    }

    // Diffuse upward: each cell averages the three below it, minus cooling.
    for (int y = 0; y < H - 2; y++) {
        uint8_t *row  = &fire_heat[y * W];
        uint8_t *b1 = &fire_heat[(y + 1) * W];
        uint8_t *b2 = &fire_heat[(y + 2) * W];
        for (int x = 0; x < W; x++) {
            int xl = x > 0 ? x - 1 : 0;          // clamp left edge
            int xr = x < W - 1 ? x + 1 : W - 1;  // clamp right edge
            int v = (b1[xl] + b1[x] + b1[xr] + b2[x]) >> 2;
            v -= 2;                              // cooling
            if (v < 0) v = 0;
            row[x] = (uint8_t)v;
        }
    }

    // Map heat -> framebuffer.
    uint16_t *p = g_fb;
    for (int i = 0; i < W * H; i++) {
        *p++ = fire_palette[fire_heat[i]];
    }
}

static void fire_done(void)
{
    free(fire_heat);
    fire_heat = NULL;
}

// =============================================================================
// Registry -- the order the carousel cycles through.
// =============================================================================

static const effect_t s_plasma  = { "ПЛАЗМА",   plasma_init,  plasma_frame,  plasma_done,  9000 };
static const effect_t s_stars   = { "ЗВЁЗДЫ",   star_init,    star_frame,    star_done,    8000 };
static const effect_t s_tunnel  = { "ТУННЕЛЬ",  tunnel_init,  tunnel_frame,  tunnel_done,  9000 };
static const effect_t s_fire    = { "ОГОНЬ",    fire_init,    fire_frame,    fire_done,    8000 };

const effect_t *const g_effects[] = {
    &s_plasma, &s_stars, &s_tunnel, &s_fire,
};
const int g_effect_count = sizeof(g_effects) / sizeof(g_effects[0]);
