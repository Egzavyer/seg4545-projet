#include "drv_lcd_i2c.h"

#include <string.h>

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

#ifndef APP_UI_LCD_I2C_ADDR
#define APP_UI_LCD_I2C_ADDR   (0x27U << 1)
#endif

#define LCD_BACKLIGHT 0x08U
#define LCD_ENABLE    0x04U
#define LCD_RS        0x01U

static uint8_t s_backlight = LCD_BACKLIGHT;
static void lcd_expander_write(uint8_t data);
static void lcd_pulse(uint8_t data);
static void lcd_write4(uint8_t nibble, uint8_t rs);
static void lcd_write_cmd(uint8_t cmd);
static void lcd_write_data(uint8_t data);
static void lcd_set_cursor(uint8_t row, uint8_t col);
static void lcd_print_padded(const char *s);

bool lcd_i2c_init(void)
{
    HAL_Delay(50U);

    /* standard HD44780 4-bit init over PCF8574 */
    lcd_write4(0x03U, 0U);
    HAL_Delay(5U);
    lcd_write4(0x03U, 0U);
    HAL_Delay(5U);
    lcd_write4(0x03U, 0U);
    HAL_Delay(1U);
    lcd_write4(0x02U, 0U);

    lcd_write_cmd(0x28U); /* 4-bit, 2-line, 5x8 */
    lcd_write_cmd(0x0CU); /* display on, cursor off */
    lcd_write_cmd(0x06U); /* entry mode */
    lcd_write_cmd(0x01U); /* clear */
    HAL_Delay(2U);

    return true;
}

bool lcd_i2c_write_lines(const char *line1, const char *line2)
{
    if (line1 == NULL || line2 == NULL)
    {
        return false;
    }

    lcd_set_cursor(0U, 0U);
    lcd_print_padded(line1);
    lcd_set_cursor(1U, 0U);
    lcd_print_padded(line2);
    return true;
}

void lcd_i2c_set_backlight(int on)
{
    s_backlight = on ? LCD_BACKLIGHT : 0U;
}

static void lcd_expander_write(uint8_t data)
{
    (void)HAL_I2C_Master_Transmit(&hi2c1, APP_UI_LCD_I2C_ADDR, &data, 1U, 100U);
}

static void lcd_pulse(uint8_t data)
{
    lcd_expander_write(data | LCD_ENABLE | s_backlight);
    HAL_Delay(1U);
    lcd_expander_write((uint8_t)((data & (uint8_t)~LCD_ENABLE) | s_backlight));
    HAL_Delay(1U);
}

static void lcd_write4(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (uint8_t)((nibble << 4) | (rs ? LCD_RS : 0U) | s_backlight);
    lcd_pulse(data);
}

static void lcd_write_cmd(uint8_t cmd)
{
    lcd_write4((uint8_t)((cmd >> 4) & 0x0FU), 0U);
    lcd_write4((uint8_t)(cmd & 0x0FU), 0U);
    HAL_Delay(1U);
}

static void lcd_write_data(uint8_t data)
{
    lcd_write4((uint8_t)((data >> 4) & 0x0FU), 1U);
    lcd_write4((uint8_t)(data & 0x0FU), 1U);
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    static const uint8_t row_offsets[2] = {0x00U, 0x40U};
    lcd_write_cmd((uint8_t)(0x80U | (row_offsets[row & 0x01U] + col)));
}

static void lcd_print_padded(const char *s)
{
    char buf[17];
    size_t i = 0U;

    memset(buf, ' ', sizeof(buf));
    buf[16] = '\0';

    while ((i < 16U) && (s[i] != '\0'))
    {
        buf[i] = s[i];
        i++;
    }

    for (i = 0U; i < 16U; i++)
    {
        lcd_write_data((uint8_t)buf[i]);
    }
}
