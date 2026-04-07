#include "esp_uart_test.h"

#include "main.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

static void dbg2(const char *s)
{
    if (s == NULL) return;
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100U);
}

void esp_uart_blocking_test(void)
{
    char line[64];

    dbg2("\r\n[ESPTEST] starting USART1->ESP32 blocking UART test\r\n");
    dbg2("[ESPTEST] expected path: STM32 PA9 -> ESP32 GPIO16 (RX2)\r\n");
    dbg2("[ESPTEST] common ground required\r\n");

    for (uint32_t i = 0; i < 10; i++)
    {
        int n = snprintf(line, sizeof(line), "ESPTEST %lu\r\n", (unsigned long)i);
        if (n > 0)
        {
            (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)n, 200U);
            dbg2("[ESPTEST] sent on USART1: ");
            dbg2(line);
        }

        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(1000U);
    }

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    dbg2("[ESPTEST] done\r\n");
}
