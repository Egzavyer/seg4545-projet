#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* Queue sizes */
#define SENSOR_QUEUE_LENGTH              128U

/* Task periods (ms) */
#define DHT_TASK_PERIOD_MS               2500U
#define MAX_TASK_PERIOD_MS               20U
#define MPU_TASK_PERIOD_MS               200U
#define MQ2_TASK_PERIOD_MS               200U
#define UI_TASK_PERIOD_MS                100U
#define ESP_TASK_PERIOD_MS               500U
#define SUPERVISOR_TASK_PERIOD_MS        500U

/* Startup and supervision */
#define STARTUP_GRACE_MS                 6000U

/* Per-task supervision deadlines (ms) */
#define DHT_HEARTBEAT_TIMEOUT_MS         6000U
#define MAX_HEARTBEAT_TIMEOUT_MS         500U
#define MPU_HEARTBEAT_TIMEOUT_MS         1000U
#define MQ2_HEARTBEAT_TIMEOUT_MS         1000U
#define DECISION_HEARTBEAT_TIMEOUT_MS    1000U
#define UI_HEARTBEAT_TIMEOUT_MS          1000U
#define ESP_HEARTBEAT_TIMEOUT_MS         2000U

/* Thresholds - project/demo values */
#define HR_LOW_BPM                       120.0f
#define HR_HIGH_BPM                      180.0f

#define SPO2_WARN_PCT                    94.0f
#define SPO2_ALARM_PCT                   92.0f
#define SPO2_CRITICAL_PCT                90.0f

#define TEMP_LOW_C                       20.0f
#define TEMP_HIGH_C                      23.0f

#define HUMIDITY_LOW_PCT                 30.0f
#define HUMIDITY_HIGH_PCT                55.0f

#define MQ2_WARN_RATIO                   1.30f
#define MQ2_ALARM_RATIO                  1.60f

/* Persistence counters expressed in task updates */
#define HR_WARN_COUNT                    8U
#define HR_ALARM_COUNT                   20U

#define SPO2_WARN_COUNT                  10U
#define SPO2_ALARM_COUNT                 5U

#define TEMP_WARN_COUNT                  12U
#define HUMIDITY_WARN_COUNT              60U
#define MQ2_WARN_COUNT                   25U
#define MQ2_ALARM_COUNT                  10U

/* Event flags */
#define EVT_WARN_ACTIVE                  (1UL << 0)
#define EVT_ALARM_ACTIVE                 (1UL << 1)
#define EVT_SENSOR_FAULT                 (1UL << 2)
#define EVT_COMMS_FAULT                  (1UL << 3)
#define EVT_BUTTON_ACK                   (1UL << 4)
#define EVT_NEW_STATUS                   (1UL << 5)

/* MAX30102 tuning */
#define MAX30102_FINGER_IR_MIN           20000.0f
#define MAX30102_FINGER_RED_MIN          12000.0f
#define MAX30102_AC_MIN_RATIO            0.0006f
#define MAX30102_AC_MAX_RATIO            0.0600f
#define MAX30102_SIGNAL_SETTLE_SAMPLES   8U
#define MAX30102_MIN_IBI_MS              333U
#define MAX30102_MAX_IBI_MS              1500U
#define MAX30102_MIN_PEAK_THRESHOLD      120.0f
#define MAX30102_DYNAMIC_PEAK_RATIO      0.0012f
#define MAX30102_DATA_STALE_MS           300U
#define MAX30102_VALID_HOLD_MS           2000U
#define MAX30102_QUALITY_UP_STEP         2U
#define MAX30102_QUALITY_DOWN_STEP       1U
#define MAX30102_QUALITY_GOOD_SCORE      8U
#define MAX30102_QUALITY_MAX_SCORE       20U

/* Local alarm / UI */
#define BUZZER_SILENCE_MS                60000U
#define APP_OUTPUT_SELFTEST_MS           500U
#define APP_RED_LED_ACTIVE_LOW           0
#define APP_BUZZER_ACTIVE_LOW            0

/* LCD/UI */
#define APP_UI_USE_LCD_I2C               1
#define APP_UI_LCD_I2C_ADDR              (0x3FU << 1)
#define APP_UI_PAGE_PERIOD_MS            3000U
#define APP_UI_REFRESH_MS                1000U
#define APP_LOG_LCD_TO_UART              0

/* Logging */
#define APP_LOG_SUMMARY_PERIOD_MS        15000U
#define APP_LOG_MAX30102_VERBOSE         0

/* Event log */
#define EVENT_LOG_DEPTH                  24U
#define EVENT_LOG_LINE_LEN               64U

/* Inactivity demo thresholds */
#define APP_INACTIVITY_ACTIVE_SCORE      0.030f
#define APP_INACTIVITY_WARN_MS           20000UL
#define APP_INACTIVITY_ALARM_MS          40000UL

/* Demo mode controls */
#define APP_DEMO_MODE_ENABLE             1
#define APP_BUTTON_LONG_PRESS_MS         1500UL

#endif
