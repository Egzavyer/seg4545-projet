#include "event_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "app_config.h"

typedef struct
{
    uint32_t tick_ms;
    char line[EVENT_LOG_LINE_LEN];
} event_log_entry_t;

static event_log_entry_t s_log[EVENT_LOG_DEPTH];
static uint32_t s_head = 0U;
static uint32_t s_count = 0U;

void event_log_init(void)
{
    memset(s_log, 0, sizeof(s_log));
    s_head = 0U;
    s_count = 0U;
}

void event_log_push(const char *fmt, ...)
{
    va_list args;

    if (fmt == NULL)
    {
        return;
    }

    s_log[s_head].tick_ms = HAL_GetTick();

    va_start(args, fmt);
    (void)vsnprintf(s_log[s_head].line, sizeof(s_log[s_head].line), fmt, args);
    va_end(args);

    s_head = (s_head + 1U) % EVENT_LOG_DEPTH;
    if (s_count < EVENT_LOG_DEPTH)
    {
        s_count++;
    }
}

uint32_t event_log_count(void)
{
    return s_count;
}
