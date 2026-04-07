#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SENSOR_ID_DHT11 = 0,
    SENSOR_ID_MAX30102,
    SENSOR_ID_MPU6050,
    SENSOR_ID_MQ2,
    SENSOR_ID_SYSTEM
} sensor_id_t;

typedef enum
{
    SYS_BOOT = 0,
    SYS_SELF_TEST,
    SYS_WARMUP,
    SYS_MONITORING,
    SYS_WARNING,
    SYS_ALARM_LOCAL,
    SYS_ALARM_REMOTE,
    SYS_DEGRADED,
    SYS_FAULT
} system_state_t;

typedef enum
{
    SENSOR_STATUS_OK = 0,
    SENSOR_STATUS_NO_DATA,
    SENSOR_STATUS_COMM_ERROR,
    SENSOR_STATUS_RANGE_ERROR,
    SENSOR_STATUS_NOT_PRESENT
} sensor_status_t;

typedef struct
{
    float ambient_temp_c;
    float humidity_pct;
    bool  valid;
} dht11_data_t;

typedef struct
{
    float heart_rate_bpm;
    float spo2_pct;
    bool  finger_present;
    bool  signal_ok;
    bool  valid;

    float red_dc;
    float ir_dc;
    float red_ac;
    float ir_ac;
    uint8_t sample_count;
    uint8_t quality_score;
    uint8_t peak_count;
} max30102_data_t;

typedef struct
{
    float accel_g_x;
    float accel_g_y;
    float accel_g_z;
    float motion_score;
    bool  moving;
    bool  valid;
} mpu6050_data_t;

typedef struct
{
    uint16_t adc_raw;
    float    normalized_level;
    bool     valid;
} mq2_data_t;

typedef struct
{
    sensor_id_t      id;
    uint32_t         tick_ms;
    sensor_status_t  status;
    union
    {
        dht11_data_t    dht11;
        max30102_data_t max30102;
        mpu6050_data_t  mpu6050;
        mq2_data_t      mq2;
    } data;
} sensor_msg_t;

typedef struct
{
    dht11_data_t      dht11;
    max30102_data_t   max30102;
    mpu6050_data_t    mpu6050;
    mq2_data_t        mq2;
    system_state_t    state;
    bool              wifi_link_ok;
    bool              sensor_fault_active;
    bool              warning_active;
    bool              alarm_active;
} system_snapshot_t;

#endif
