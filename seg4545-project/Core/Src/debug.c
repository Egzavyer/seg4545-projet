#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "cmsis_os2.h"

extern UART_HandleTypeDef huart2;

static osMutexId_t s_debugMutex = NULL;

void debug_init(void)
{
    if (s_debugMutex == NULL)
    {
        const osMutexAttr_t attr = { .name = "debugMutex" };
        s_debugMutex = osMutexNew(&attr);
    }
}

void debug_log(const char *fmt, ...)
{
    char buffer[192];
    va_list args;
    int len;
    uint8_t locked = 0U;

    if (fmt == NULL)
    {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0)
    {
        return;
    }

    if ((osKernelGetState() == osKernelRunning) && (s_debugMutex != NULL))
    {
        if (osMutexAcquire(s_debugMutex, 50U) == osOK)
        {
            locked = 1U;
        }
    }

    (void)HAL_UART_Transmit(&huart2, (uint8_t *)buffer, (uint16_t)strnlen(buffer, sizeof(buffer)), 100U);

    if (locked != 0U)
    {
        (void)osMutexRelease(s_debugMutex);
    }
}
