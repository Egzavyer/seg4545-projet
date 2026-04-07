#ifndef DRV_LCD_I2C_H
#define DRV_LCD_I2C_H

#include <stdbool.h>

bool lcd_i2c_init(void);
bool lcd_i2c_write_lines(const char *line1, const char *line2);
void lcd_i2c_set_backlight(int on);

#endif
