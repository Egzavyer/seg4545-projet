#ifndef DRV_DS18B20_H
#define DRV_DS18B20_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

/*
 * Selected pin for this project:
 *   DS18B20 DQ -> PB12
 */
#define DS18B20_GPIO_Port   GPIOB
#define DS18B20_Pin         GPIO_PIN_12

bool ds18b20_init(void);
bool ds18b20_start_conversion(void);
bool ds18b20_read_temp_c(float *temp_c);
bool ds18b20_read_rom(uint8_t rom[8]);

#endif
