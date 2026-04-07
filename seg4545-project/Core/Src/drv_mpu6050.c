#include "drv_mpu6050.h"

#include <string.h>
#include "main.h"

#define MPU6050_ADDR_7BIT        0x68U
#define MPU6050_ADDR             (MPU6050_ADDR_7BIT << 1)

#define MPU6050_REG_SMPLRT_DIV   0x19U
#define MPU6050_REG_CONFIG       0x1AU
#define MPU6050_REG_GYRO_CONFIG  0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_INT_ENABLE   0x38U
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_PWR_MGMT_1   0x6BU
#define MPU6050_REG_WHO_AM_I     0x75U

extern I2C_HandleTypeDef hi2c1;

typedef struct
{
    uint8_t initialized;
    float prev_ax;
    float prev_ay;
    float prev_az;
} mpu6050_ctx_t;

static mpu6050_ctx_t s_ctx = {0};

static bool reg_write(uint8_t reg, uint8_t value);
static bool reg_read(uint8_t reg, uint8_t *value);
static bool burst_read(uint8_t reg, uint8_t *buf, uint16_t len);
static float absf_local(float x);

bool mpu6050_init(void)
{
    uint8_t who = 0U;

    if (!reg_read(MPU6050_REG_WHO_AM_I, &who))
    {
        return false;
    }

    if ((who & 0x7EU) != 0x68U)
    {
        return false;
    }

    if (!reg_write(MPU6050_REG_PWR_MGMT_1, 0x80U)) return false;
    HAL_Delay(100U);
    if (!reg_write(MPU6050_REG_PWR_MGMT_1, 0x01U)) return false;
    HAL_Delay(10U);
    if (!reg_write(MPU6050_REG_SMPLRT_DIV, 0x07U)) return false;
    if (!reg_write(MPU6050_REG_CONFIG, 0x03U)) return false;
    if (!reg_write(MPU6050_REG_GYRO_CONFIG, 0x00U)) return false;
    if (!reg_write(MPU6050_REG_ACCEL_CONFIG, 0x00U)) return false;
    if (!reg_write(MPU6050_REG_INT_ENABLE, 0x00U)) return false;

    s_ctx.initialized = 1U;
    s_ctx.prev_ax = 0.0f;
    s_ctx.prev_ay = 0.0f;
    s_ctx.prev_az = 1.0f;
    return true;
}

bool mpu6050_read(mpu6050_data_t *out)
{
    uint8_t raw[14];
    int16_t ax_raw, ay_raw, az_raw;
    float ax, ay, az;
    float motion;

    if ((out == NULL) || (s_ctx.initialized == 0U))
    {
        return false;
    }

    if (!burst_read(MPU6050_REG_ACCEL_XOUT_H, raw, sizeof(raw)))
    {
        memset(out, 0, sizeof(*out));
        return false;
    }

    ax_raw = (int16_t)((raw[0] << 8) | raw[1]);
    ay_raw = (int16_t)((raw[2] << 8) | raw[3]);
    az_raw = (int16_t)((raw[4] << 8) | raw[5]);

    ax = ((float)ax_raw) / 16384.0f;
    ay = ((float)ay_raw) / 16384.0f;
    az = ((float)az_raw) / 16384.0f;

    motion = absf_local(ax - s_ctx.prev_ax) + absf_local(ay - s_ctx.prev_ay) + absf_local(az - s_ctx.prev_az);

    out->accel_g_x = ax;
    out->accel_g_y = ay;
    out->accel_g_z = az;
    out->motion_score = motion;
    out->moving = (motion > 0.08f) ? true : false;
    out->valid = true;

    s_ctx.prev_ax = ax;
    s_ctx.prev_ay = ay;
    s_ctx.prev_az = az;
    return true;
}

static bool reg_write(uint8_t reg, uint8_t value)
{
    return (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U, 100U) == HAL_OK);
}

static bool reg_read(uint8_t reg, uint8_t *value)
{
    return (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 1U, 100U) == HAL_OK);
}

static bool burst_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100U) == HAL_OK);
}

static float absf_local(float x)
{
    return (x < 0.0f) ? -x : x;
}
