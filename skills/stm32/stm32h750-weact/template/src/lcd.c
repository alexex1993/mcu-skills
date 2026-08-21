#include "lcd.h"
#include "font.h"

/* --- register select / chip select helpers ------------------------------- */
#define LCD_DC_DATA()    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)
#define LCD_DC_CMD()     HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define LCD_CS_IDLE()    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_CS_ACTIVE()  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)

#define LCD_SPI          (&hspi4)

static int32_t lcd_io_init(void);
static int32_t lcd_io_gettick(void);
static int32_t lcd_io_writereg(uint8_t reg, uint8_t *pdata, uint32_t length);
static int32_t lcd_io_readreg(uint8_t reg, uint8_t *pdata);
static int32_t lcd_io_senddata(uint8_t *pdata, uint32_t length);
static int32_t lcd_io_recvdata(uint8_t *pdata, uint32_t length);

static ST7735_IO_t st7735_pIO = {
    lcd_io_init,
    NULL,
    0,
    lcd_io_writereg,
    lcd_io_readreg,
    lcd_io_senddata,
    lcd_io_recvdata,
    lcd_io_gettick,
};

ST7735_Object_t st7735_pObj;

uint16_t POINT_COLOR = WHITE;
uint16_t BACK_COLOR  = BLACK;

void LCD_Init(void)
{
    ST7735_Ctx_t ctx;

    /* 0.96" 160x80 HannStar panel, mounted rotated on the core board */
    ctx.Orientation = ST7735_ORIENTATION_LANDSCAPE_ROT180;
    ctx.Panel       = HannStar_Panel;
    ctx.Type        = ST7735_0_9_inch_screen;

    ST7735_RegisterBusIO(&st7735_pObj, &st7735_pIO);
    if (ST7735_LCD_Driver.Init(&st7735_pObj, ST7735_FORMAT_RBG565, &ctx) != ST7735_OK) {
        Error_Handler();
    }
}

void LCD_Clear(uint16_t color)
{
    ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, ST7735Ctx.Width, ST7735Ctx.Height, color);
}

void LCD_SetBrightness(uint32_t brightness)
{
    __HAL_TIM_SET_COMPARE(&htim1, LCD_BL_TIM_CHANNEL, brightness);
}

uint32_t LCD_GetBrightness(void)
{
    return __HAL_TIM_GET_COMPARE(&htim1, LCD_BL_TIM_CHANNEL);
}

/* Ramp the backlight from its current level to brightness_dis over `time` ms. */
void LCD_Light(uint32_t brightness_dis, uint32_t time)
{
    uint32_t brightness_now = LCD_GetBrightness();
    uint32_t tick;
    float k;

    if (brightness_now == brightness_dis || time == 0) {
        LCD_SetBrightness(brightness_dis);
        return;
    }

    k = ((float)brightness_now - (float)brightness_dis) / -(float)time;

    tick = HAL_GetTick();
    for (;;) {
        uint32_t time_now;

        float level;

        HAL_Delay(1);
        time_now = HAL_GetTick() - tick;
        level = (float)time_now * k + (float)brightness_now;
        if (level < 0.0f) {
            level = 0.0f;
        }
        LCD_SetBrightness((uint32_t)level);

        if (time_now >= time) {
            break;
        }
    }
    LCD_SetBrightness(brightness_dis);
}

/**
 * Draw one ASCII character (' '..'~').
 * size: 12 or 16 (font height in pixels, the width is size/2)
 * mode: 0 = opaque (paint BACK_COLOR behind), 1 = transparent
 */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode)
{
    uint8_t  temp, t1, t;
    uint16_t y0 = y;
    uint16_t x0 = x;
    uint16_t colortemp = POINT_COLOR;
    uint32_t h, w;
    uint16_t write[size][size == 12 ? 6 : 8];
    uint16_t count;

    ST7735_GetXSize(&st7735_pObj, &w);
    ST7735_GetYSize(&st7735_pObj, &h);

    if (num < ' ' || num > '~') {
        return;
    }
    num = num - ' ';
    count = 0;

    if (!mode) {
        for (t = 0; t < size; t++) {
            temp = (size == 12) ? asc2_1206[num][t] : asc2_1608[num][t];

            for (t1 = 0; t1 < 8; t1++) {
                if (temp & 0x80) {
                    POINT_COLOR = (colortemp & 0xFF) << 8 | colortemp >> 8;
                } else {
                    POINT_COLOR = (BACK_COLOR & 0xFF) << 8 | BACK_COLOR >> 8;
                }

                write[count][t / 2] = POINT_COLOR;
                count++;
                if (count >= size) {
                    count = 0;
                }

                temp <<= 1;
                y++;
                if (y >= h) { POINT_COLOR = colortemp; return; }
                if ((y - y0) == size) {
                    y = y0;
                    x++;
                    if (x >= w) { POINT_COLOR = colortemp; return; }
                    break;
                }
            }
        }
    } else {
        for (t = 0; t < size; t++) {
            temp = (size == 12) ? asc2_1206[num][t] : asc2_1608[num][t];

            for (t1 = 0; t1 < 8; t1++) {
                if (temp & 0x80) {
                    write[count][t / 2] = (POINT_COLOR & 0xFF) << 8 | POINT_COLOR >> 8;
                }
                count++;
                if (count >= size) {
                    count = 0;
                }

                temp <<= 1;
                y++;
                if (y >= h) { POINT_COLOR = colortemp; return; }
                if ((y - y0) == size) {
                    y = y0;
                    x++;
                    if (x >= w) { POINT_COLOR = colortemp; return; }
                    break;
                }
            }
        }
    }

    ST7735_FillRGBRect(&st7735_pObj, x0, y0, (uint8_t *)&write,
                       size == 12 ? 6 : 8, size);
    POINT_COLOR = colortemp;
}

/* --- strip renderer ------------------------------------------------------ */

/* Pixels are stored byte-swapped so the buffer can go straight out on the
   wire: the panel wants RGB565 big-endian, the M7 is little-endian. */
#define LCD_SWAP16(c)  ((uint16_t)(((uint16_t)(c) << 8) | ((uint16_t)(c) >> 8)))

static uint16_t lcd_strip[LCD_STRIP_MAX_W * LCD_STRIP_MAX_H];

void LCD_StripClear(uint16_t color)
{
    uint16_t raw = LCD_SWAP16(color);
    uint32_t i;

    for (i = 0; i < (uint32_t)(LCD_STRIP_MAX_W * LCD_STRIP_MAX_H); i++) {
        lcd_strip[i] = raw;
    }
}

void LCD_StripRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t raw = LCD_SWAP16(color);
    uint16_t row, col;

    for (row = y; row < y + h && row < LCD_STRIP_MAX_H; row++) {
        for (col = x; col < x + w && col < LCD_STRIP_MAX_W; col++) {
            lcd_strip[row * LCD_STRIP_MAX_W + col] = raw;
        }
    }
}

/**
 * Draw a string into the strip at (x,y), foreground pixels only - whatever
 * LCD_StripClear() painted shows through as the background.
 * size: 12 or 16 (glyph height; width is size/2)
 */
void LCD_StripText(uint16_t x, uint16_t y, uint8_t size, uint16_t color, const char *p)
{
    uint16_t raw = LCD_SWAP16(color);
    uint8_t  cw  = (uint8_t)(size / 2);

    for (; *p >= ' ' && *p <= '~'; p++, x = (uint16_t)(x + cw)) {
        uint8_t num = (uint8_t)(*p - ' ');
        uint8_t col, row;

        if (x + cw > LCD_STRIP_MAX_W) {
            break;
        }

        for (col = 0; col < cw; col++) {
            for (row = 0; row < size; row++) {
                /* font byte layout: column-major, 8 rows per byte */
                uint8_t idx  = (uint8_t)(col * 2U + (row >> 3));
                uint8_t bits = (size == 12) ? asc2_1206[num][idx] : asc2_1608[num][idx];

                if ((bits & (0x80U >> (row & 7U))) != 0U &&
                    (y + row) < LCD_STRIP_MAX_H) {
                    lcd_strip[(y + row) * LCD_STRIP_MAX_W + (x + col)] = raw;
                }
            }
        }
    }
}

/**
 * Push the top `height` rows of the strip to panel rows y .. y+height-1.
 *
 * The panel's address window is left at full screen by Init/FillRect, so a
 * single SetCursor plus one continuous burst lets the controller's address
 * counter walk the rows for us - no per-row command overhead.
 */
void LCD_StripFlush(uint16_t y, uint16_t height)
{
    if (height > LCD_STRIP_MAX_H || (y + height) > ST7735Ctx.Height) {
        return;
    }

    ST7735_SetCursor(&st7735_pObj, 0, y);
    lcd_io_senddata((uint8_t *)lcd_strip, (uint32_t)ST7735Ctx.Width * height * 2U);
}

/* Panel refresh rate, FRMCTR1 (normal mode). See lcd.h for the formula. */
void LCD_SetFrameRate(uint8_t rtna, uint8_t fpa, uint8_t bpa)
{
    uint8_t args[3] = { rtna, fpa, bpa };

    lcd_io_writereg(ST7735_FRAME_RATE_CTRL1, args, sizeof(args));
}

/**
 * Draw a string, wrapping inside the (width x height) box anchored at x,y.
 */
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    uint8_t size, const char *p)
{
    uint16_t x0 = x;

    width  += x;
    height += y;

    while ((*p <= '~') && (*p >= ' ')) {
        if (x >= width) {
            x = x0;
            y += size;
        }
        if (y >= height) {
            break;
        }
        LCD_ShowChar(x, y, (uint8_t)*p, size, 0);
        x += size / 2;
        p++;
    }
}

/* --- ST7735 bus IO ------------------------------------------------------- */

static int32_t lcd_io_init(void)
{
    /* Backlight PWM. CH2N is a complementary output, hence PWMN_Start. */
    HAL_TIMEx_PWMN_Start(&htim1, LCD_BL_TIM_CHANNEL);
    LCD_SetBrightness(0);
    return ST7735_OK;
}

static int32_t lcd_io_gettick(void)
{
    return (int32_t)HAL_GetTick();
}

static int32_t lcd_io_writereg(uint8_t reg, uint8_t *pdata, uint32_t length)
{
    int32_t result;

    LCD_CS_ACTIVE();
    LCD_DC_CMD();
    result = HAL_SPI_Transmit(LCD_SPI, &reg, 1, 100);
    LCD_DC_DATA();
    if (length > 0) {
        result += HAL_SPI_Transmit(LCD_SPI, pdata, (uint16_t)length, 500);
    }
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}

static int32_t lcd_io_readreg(uint8_t reg, uint8_t *pdata)
{
    int32_t result;

    LCD_CS_ACTIVE();
    LCD_DC_CMD();
    result = HAL_SPI_Transmit(LCD_SPI, &reg, 1, 100);
    LCD_DC_DATA();
    result += HAL_SPI_Receive(LCD_SPI, pdata, 1, 500);
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}

static int32_t lcd_io_senddata(uint8_t *pdata, uint32_t length)
{
    int32_t result;

    LCD_CS_ACTIVE();
    result = HAL_SPI_Transmit(LCD_SPI, pdata, (uint16_t)length, 100);
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}

static int32_t lcd_io_recvdata(uint8_t *pdata, uint32_t length)
{
    int32_t result;

    LCD_CS_ACTIVE();
    result = HAL_SPI_Receive(LCD_SPI, pdata, (uint16_t)length, 500);
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}
