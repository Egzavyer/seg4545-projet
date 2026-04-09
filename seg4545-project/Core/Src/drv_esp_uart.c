#include "drv_esp_uart.h"

#include <string.h>
#include "main.h"
#include "debug.h"

extern UART_HandleTypeDef huart1;

bool esp_uart_init(void)
{
    return true;
}

bool esp_uart_send_line(const char *line)
{
    HAL_StatusTypeDef st;
    uint16_t len;

    if (line == NULL)
    {
        return false;
    }

    len = (uint16_t)strlen(line);

    for (uint8_t attempt = 0U; attempt < 3U; attempt++)
    {
        st = HAL_UART_Transmit(&huart1, (uint8_t *)line, len, 100U);

        if (st == HAL_OK)
        {
            return true;
        }

        if (st == HAL_BUSY)
        {
            HAL_Delay(2U);
            continue;
        }

        if (st == HAL_ERROR || st == HAL_TIMEOUT)
        {
            debug_log("[ESPDRV] TX fail st=%d err=0x%08lX attempt=%u\r\n",
                      (int)st,
                      (unsigned long)huart1.ErrorCode,
                      (unsigned)(attempt + 1U));

            __HAL_UART_CLEAR_OREFLAG(&huart1);
            __HAL_UART_CLEAR_NEFLAG(&huart1);
            __HAL_UART_CLEAR_FEFLAG(&huart1);
            __HAL_UART_CLEAR_PEFLAG(&huart1);

            HAL_Delay(2U);
        }
    }

    return false;
}
