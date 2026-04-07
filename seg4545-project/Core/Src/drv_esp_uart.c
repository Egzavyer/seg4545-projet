#include "drv_esp_uart.h"

#include <string.h>
#include "main.h"

extern UART_HandleTypeDef huart1;

bool esp_uart_init(void)
{
    return true;
}

bool esp_uart_send_line(const char *line)
{
    if (line == NULL)
    {
        return false;
    }

    return (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)strlen(line), 100U) == HAL_OK);
}
