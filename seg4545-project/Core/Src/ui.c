#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_tasks.h"
#include "debug.h"
#include "drv_lcd_i2c.h"

static uint8_t s_use_lcd = APP_UI_USE_LCD_I2C;
static uint8_t s_page = 0U;
static uint32_t s_last_page_ms = 0U;
static uint32_t s_last_refresh_ms = 0U;
static uint32_t s_ack_banner_until_ms = 0U;
static char s_last_l1[17];
static char s_last_l2[17];

static const char *state_name(system_state_t state);
static void format_lines(const system_snapshot_t *snapshot, char *l1, char *l2);
static void write_lines_if_changed(const char *l1, const char *l2);

void ui_init(void)
{
    memset(s_last_l1, 0, sizeof(s_last_l1));
    memset(s_last_l2, 0, sizeof(s_last_l2));
    s_page = 0U;
    s_last_page_ms = HAL_GetTick();
    s_last_refresh_ms = 0U;
    s_ack_banner_until_ms = 0U;

    if (s_use_lcd)
    {
        if (g_i2cMutex != NULL)
        {
            osMutexAcquire(g_i2cMutex, osWaitForever);
            (void)lcd_i2c_init();
            osMutexRelease(g_i2cMutex);
        }
        debug_log("[LCD] I2C mode enabled addr=0x%02X\r\n", (unsigned)(APP_UI_LCD_I2C_ADDR >> 1));
    }
    else
    {
        debug_log("[LCD] disabled\r\n");
    }
}

void ui_acknowledge(void)
{
    s_ack_banner_until_ms = HAL_GetTick() + 2000U;
}

void ui_update(const system_snapshot_t *snapshot)
{
    char l1[17];
    char l2[17];
    uint32_t now = HAL_GetTick();

    if (snapshot == NULL)
    {
        return;
    }

    if ((now - s_last_refresh_ms) < APP_UI_REFRESH_MS)
    {
        return;
    }
    s_last_refresh_ms = now;

    if ((now - s_last_page_ms) >= APP_UI_PAGE_PERIOD_MS)
    {
        s_last_page_ms = now;
        s_page = (uint8_t)((s_page + 1U) % 3U);
    }

    format_lines(snapshot, l1, l2);
    write_lines_if_changed(l1, l2);
}

static const char *state_name(system_state_t state)
{
    switch (state)
    {
        case SYS_BOOT: return "BOOT";
        case SYS_SELF_TEST: return "SELFTEST";
        case SYS_WARMUP: return "WARMUP";
        case SYS_MONITORING: return "MONITOR";
        case SYS_WARNING: return "WARNING";
        case SYS_ALARM_LOCAL: return "ALARM";
        case SYS_ALARM_REMOTE: return "REMOTE";
        case SYS_DEGRADED: return "DEGRADED";
        case SYS_FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

static void format_lines(const system_snapshot_t *snapshot, char *l1, char *l2)
{
    uint32_t now = HAL_GetTick();

    memset(l1, 0, 17U);
    memset(l2, 0, 17U);

    if ((s_ack_banner_until_ms != 0U) && ((int32_t)(s_ack_banner_until_ms - now) > 0))
    {
        snprintf(l1, 17U, "ALARM SILENCED");
        snprintf(l2, 17U, "LOCAL BUZZ OFF");
        return;
    }

    switch (s_page)
    {
        case 0U:
            snprintf(l1, 17U, "%-16s", state_name(snapshot->state));
            snprintf(l2, 17U, "T%2.0f H%2.0f MQ%1.1f",
                     snapshot->dht11.ambient_temp_c,
                     snapshot->dht11.humidity_pct,
                     snapshot->mq2.normalized_level);
            break;

        case 1U:
            if (snapshot->max30102.valid)
            {
                snprintf(l1, 17U, "HR:%3.0f SP:%3.0f",
                         snapshot->max30102.heart_rate_bpm,
                         snapshot->max30102.spo2_pct);
            }
            else
            {
                snprintf(l1, 17U, "HR:--- SP:---");
            }
            snprintf(l2, 17U, "FP:%u SIG:%u QS:%u",
                     snapshot->max30102.finger_present ? 1U : 0U,
                     snapshot->max30102.signal_ok ? 1U : 0U,
                     snapshot->max30102.quality_score);
            break;

        default:
            snprintf(l1, 17U, "MOVE:%1.3f", snapshot->mpu6050.motion_score);
            snprintf(l2, 17U, "W:%u A:%u F:%u",
                     snapshot->warning_active ? 1U : 0U,
                     snapshot->alarm_active ? 1U : 0U,
                     snapshot->sensor_fault_active ? 1U : 0U);
            break;
    }
}

static void write_lines_if_changed(const char *l1, const char *l2)
{
    if ((strncmp(l1, s_last_l1, 16U) == 0) && (strncmp(l2, s_last_l2, 16U) == 0))
    {
        return;
    }

    strncpy(s_last_l1, l1, 16U);
    strncpy(s_last_l2, l2, 16U);
    s_last_l1[16] = '\0';
    s_last_l2[16] = '\0';

    if (s_use_lcd)
    {
        if (g_i2cMutex != NULL)
        {
            osMutexAcquire(g_i2cMutex, osWaitForever);
            (void)lcd_i2c_write_lines(s_last_l1, s_last_l2);
            osMutexRelease(g_i2cMutex);
        }
    }

#if APP_LOG_LCD_TO_UART
    debug_log("[LCD] %s | %s\r\n", s_last_l1, s_last_l2);
#endif
}
