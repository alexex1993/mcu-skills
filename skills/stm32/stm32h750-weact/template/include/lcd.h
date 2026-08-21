/**
 * Thin glue between the ST7735 component driver and this board:
 * SPI4 bus IO, backlight PWM and a small ASCII text renderer.
 */
#ifndef LCD_H
#define LCD_H

#include "board.h"
#include "st7735.h"

#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define MAGENTA     0xF81F
#define GRAY        0x8430

extern ST7735_Object_t st7735_pObj;
extern ST7735_Ctx_t    ST7735Ctx;   /* filled in by the driver on Init */

extern uint16_t POINT_COLOR;        /* foreground colour, RGB565 */
extern uint16_t BACK_COLOR;         /* background colour, RGB565 */

void     LCD_Init(void);
void     LCD_Clear(uint16_t color);
void     LCD_SetBrightness(uint32_t brightness);   /* 0..100 */
uint32_t LCD_GetBrightness(void);
void     LCD_Light(uint32_t brightness_dis, uint32_t time);
void     LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode);
void     LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                        uint8_t size, const char *p);

/**
 * Panel refresh rate (FRMCTR1, normal mode):
 *     f_frame = f_osc / ((rtna * 2 + 40) * (LINE + fpa + bpa))
 * with f_osc ~= 850 kHz and LINE = 160 gate lines on this panel.
 * The driver's stock (1, 0x2C, 0x2D) gives 42 * 249 -> ~80 Hz.
 */
#define LCD_FRAMERATE_DEFAULT   0x01U, 0x2CU, 0x2DU   /* ~80 Hz  */
#define LCD_FRAMERATE_FAST      0x00U, 0x02U, 0x02U   /* ~130 Hz */

void     LCD_SetFrameRate(uint8_t rtna, uint8_t fpa, uint8_t bpa);

/* --- strip renderer ------------------------------------------------------
 * A full-width scratch band of up to LCD_STRIP_MAX_H rows. Compose into it
 * with the LCD_Strip* calls, then push it in ONE SPI burst with
 * LCD_StripFlush(). Per-character FillRGBRect costs ~200 SPI transactions
 * per text line; a strip costs one cursor set plus one 5 KB transfer.
 */
#define LCD_STRIP_MAX_W  160
#define LCD_STRIP_MAX_H  16

void     LCD_StripClear(uint16_t color);
void     LCD_StripRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void     LCD_StripText(uint16_t x, uint16_t y, uint8_t size, uint16_t color, const char *p);
void     LCD_StripFlush(uint16_t y, uint16_t height);

#endif /* LCD_H */
