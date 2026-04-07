#ifndef DRV_ESP_UART_H
#define DRV_ESP_UART_H

#include <stdbool.h>

bool esp_uart_init(void);
bool esp_uart_send_line(const char *line);

#endif
