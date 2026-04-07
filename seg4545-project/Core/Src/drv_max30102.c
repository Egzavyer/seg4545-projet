#include "drv_max30102.h"

#include <math.h>
#include <string.h>

#include "main.h"
#include "app_config.h"

extern I2C_HandleTypeDef hi2c1;

#define MAX30102_I2C_ADDR            (0x57U << 1)

#define REG_INTR_STATUS_1            0x00U
#define REG_INTR_STATUS_2            0x01U
#define REG_INTR_ENABLE_1            0x02U
#define REG_INTR_ENABLE_2            0x03U
#define REG_FIFO_WR_PTR              0x04U
#define REG_OVF_COUNTER              0x05U
#define REG_FIFO_RD_PTR              0x06U
#define REG_FIFO_DATA                0x07U
#define REG_FIFO_CONFIG              0x08U
#define REG_MODE_CONFIG              0x09U
#define REG_SPO2_CONFIG              0x0AU
#define REG_LED1_PA                  0x0CU
#define REG_LED2_PA                  0x0DU
#define REG_MULTI_LED_CTRL1          0x11U
#define REG_MULTI_LED_CTRL2          0x12U
#define REG_PART_ID                  0xFFU

#define MAX30102_PART_ID             0x15U

typedef struct
{
    float red_dc;
    float ir_dc;
    float red_ac;
    float ir_ac;

    float bpm;
    float spo2;

    float last_good_bpm;
    float last_good_spo2;

    float prev2_hp;
    float prev1_hp;

    uint32_t last_peak_ms;
    uint32_t last_sample_ms;
    uint32_t last_good_ms;

    uint8_t quality_score;
    uint8_t peak_count;
} max30102_state_t;

static max30102_state_t s_state;

static HAL_StatusTypeDef reg_write(uint8_t reg, uint8_t value);
static HAL_StatusTypeDef reg_read(uint8_t reg, uint8_t *value);
static HAL_StatusTypeDef burst_read(uint8_t reg, uint8_t *buf, uint16_t len);
static void reset_state(void);
static void process_single_sample(uint32_t red_raw, uint32_t ir_raw, uint32_t sample_time_ms);

bool max30102_init(void)
{
    uint8_t part_id = 0U;

    reset_state();

    if (reg_read(REG_PART_ID, &part_id) != HAL_OK)
    {
        return false;
    }

    if (part_id != MAX30102_PART_ID)
    {
        return false;
    }

    if (reg_write(REG_MODE_CONFIG, 0x40U) != HAL_OK) return false;
    HAL_Delay(10U);

    (void)reg_read(REG_INTR_STATUS_1, &part_id);
    (void)reg_read(REG_INTR_STATUS_2, &part_id);

    if (reg_write(REG_FIFO_WR_PTR, 0x00U) != HAL_OK) return false;
    if (reg_write(REG_OVF_COUNTER, 0x00U) != HAL_OK) return false;
    if (reg_write(REG_FIFO_RD_PTR, 0x00U) != HAL_OK) return false;
    if (reg_write(REG_FIFO_CONFIG, 0x6FU) != HAL_OK) return false;

    if (reg_write(REG_MODE_CONFIG, 0x03U) != HAL_OK) return false;
    if (reg_write(REG_SPO2_CONFIG, 0x27U) != HAL_OK) return false;

    if (reg_write(REG_LED1_PA, 0x3CU) != HAL_OK) return false;
    if (reg_write(REG_LED2_PA, 0x3CU) != HAL_OK) return false;

    if (reg_write(REG_MULTI_LED_CTRL1, 0x00U) != HAL_OK) return false;
    if (reg_write(REG_MULTI_LED_CTRL2, 0x00U) != HAL_OK) return false;
    if (reg_write(REG_INTR_ENABLE_1, 0x40U) != HAL_OK) return false;
    if (reg_write(REG_INTR_ENABLE_2, 0x00U) != HAL_OK) return false;

    HAL_Delay(10U);
    return true;
}

bool max30102_read(max30102_data_t *out)
{
    uint8_t wr = 0U, rd = 0U;
    uint8_t sample_count = 0U;
    uint8_t buf[24];
    uint32_t now_ms = HAL_GetTick();

    if (out == NULL)
    {
        return false;
    }

    memset(out, 0, sizeof(*out));

    if (reg_read(REG_FIFO_WR_PTR, &wr) != HAL_OK) return false;
    if (reg_read(REG_FIFO_RD_PTR, &rd) != HAL_OK) return false;

    sample_count = (uint8_t)((wr - rd) & 0x1FU);

    if (sample_count == 0U)
    {
        out->red_dc = s_state.red_dc;
        out->ir_dc  = s_state.ir_dc;
        out->red_ac = s_state.red_ac;
        out->ir_ac  = s_state.ir_ac;
        out->finger_present = (s_state.ir_dc > MAX30102_FINGER_IR_MIN) &&
                              (s_state.red_dc > MAX30102_FINGER_RED_MIN);
        out->quality_score = s_state.quality_score;
        out->peak_count = s_state.peak_count;
        out->signal_ok = (s_state.quality_score >= MAX30102_QUALITY_GOOD_SCORE);

        if (out->finger_present &&
            (s_state.last_good_ms != 0U) &&
            ((now_ms - s_state.last_good_ms) <= MAX30102_VALID_HOLD_MS))
        {
            out->heart_rate_bpm = s_state.last_good_bpm;
            out->spo2_pct = s_state.last_good_spo2;
            out->valid = true;
        }
        else
        {
            out->heart_rate_bpm = 0.0f;
            out->spo2_pct = 0.0f;
            out->valid = false;
        }

        return false;
    }

    if (sample_count > 4U)
    {
        sample_count = 4U;
    }

    if (burst_read(REG_FIFO_DATA, buf, (uint16_t)(sample_count * 6U)) != HAL_OK)
    {
        return false;
    }

    out->sample_count = sample_count;

    for (uint8_t i = 0U; i < sample_count; i++)
    {
        uint32_t red_raw = (((uint32_t)buf[i * 6U + 0U]) << 16) |
                           (((uint32_t)buf[i * 6U + 1U]) << 8)  |
                           ((uint32_t)buf[i * 6U + 2U]);
        uint32_t ir_raw  = (((uint32_t)buf[i * 6U + 3U]) << 16) |
                           (((uint32_t)buf[i * 6U + 4U]) << 8)  |
                           ((uint32_t)buf[i * 6U + 5U]);

        red_raw &= 0x3FFFFU;
        ir_raw  &= 0x3FFFFU;

        process_single_sample(red_raw, ir_raw, now_ms - ((sample_count - 1U - i) * 10U));
    }

    out->red_dc = s_state.red_dc;
    out->ir_dc  = s_state.ir_dc;
    out->red_ac = s_state.red_ac;
    out->ir_ac  = s_state.ir_ac;
    out->finger_present = (s_state.ir_dc > MAX30102_FINGER_IR_MIN) &&
                          (s_state.red_dc > MAX30102_FINGER_RED_MIN);
    out->quality_score = s_state.quality_score;
    out->peak_count = s_state.peak_count;
    out->signal_ok = (s_state.quality_score >= MAX30102_QUALITY_GOOD_SCORE);

    if (out->finger_present &&
        (s_state.last_good_ms != 0U) &&
        ((now_ms - s_state.last_good_ms) <= MAX30102_VALID_HOLD_MS))
    {
        out->heart_rate_bpm = s_state.last_good_bpm;
        out->spo2_pct = s_state.last_good_spo2;
        out->valid = true;
    }
    else
    {
        out->heart_rate_bpm = 0.0f;
        out->spo2_pct = 0.0f;
        out->valid = false;
    }

    return true;
}

static void process_single_sample(uint32_t red_raw, uint32_t ir_raw, uint32_t sample_time_ms)
{
    float red = (float)red_raw;
    float ir  = (float)ir_raw;
    float red_hp, ir_hp;
    float red_ratio, ir_ratio;

    if (s_state.red_dc <= 1.0f) s_state.red_dc = red;
    if (s_state.ir_dc  <= 1.0f) s_state.ir_dc  = ir;

    s_state.red_dc = (0.95f * s_state.red_dc) + (0.05f * red);
    s_state.ir_dc  = (0.95f * s_state.ir_dc)  + (0.05f * ir);

    red_hp = red - s_state.red_dc;
    ir_hp  = ir  - s_state.ir_dc;

    s_state.red_ac = fabsf(red_hp);
    s_state.ir_ac  = fabsf(ir_hp);

    red_ratio = (s_state.red_dc > 1.0f) ? (s_state.red_ac / s_state.red_dc) : 0.0f;
    ir_ratio  = (s_state.ir_dc  > 1.0f) ? (s_state.ir_ac  / s_state.ir_dc)  : 0.0f;

    if ((s_state.ir_dc > MAX30102_FINGER_IR_MIN) &&
        (s_state.red_dc > MAX30102_FINGER_RED_MIN) &&
        (red_ratio >= MAX30102_AC_MIN_RATIO) && (red_ratio <= MAX30102_AC_MAX_RATIO) &&
        (ir_ratio  >= MAX30102_AC_MIN_RATIO) && (ir_ratio  <= MAX30102_AC_MAX_RATIO))
    {
        if (s_state.quality_score + MAX30102_QUALITY_UP_STEP >= MAX30102_QUALITY_MAX_SCORE)
        {
            s_state.quality_score = MAX30102_QUALITY_MAX_SCORE;
        }
        else
        {
            s_state.quality_score += MAX30102_QUALITY_UP_STEP;
        }
    }
    else
    {
        if (s_state.quality_score > MAX30102_QUALITY_DOWN_STEP)
        {
            s_state.quality_score -= MAX30102_QUALITY_DOWN_STEP;
        }
        else
        {
            s_state.quality_score = 0U;
        }
    }

    if (s_state.quality_score >= MAX30102_QUALITY_GOOD_SCORE)
    {
        float threshold = fmaxf(MAX30102_MIN_PEAK_THRESHOLD,
                                s_state.ir_dc * MAX30102_DYNAMIC_PEAK_RATIO);

        if ((s_state.prev1_hp > s_state.prev2_hp) &&
            (s_state.prev1_hp > ir_hp) &&
            (s_state.prev1_hp > threshold))
        {
            if ((s_state.last_peak_ms == 0U) || ((sample_time_ms - s_state.last_peak_ms) >= MAX30102_MIN_IBI_MS))
            {
                if (s_state.last_peak_ms != 0U)
                {
                    uint32_t ibi = sample_time_ms - s_state.last_peak_ms;
                    if ((ibi >= MAX30102_MIN_IBI_MS) && (ibi <= MAX30102_MAX_IBI_MS))
                    {
                        float inst_bpm = 60000.0f / (float)ibi;
                        if (s_state.bpm <= 1.0f)
                        {
                            s_state.bpm = inst_bpm;
                        }
                        else
                        {
                            s_state.bpm = (0.85f * s_state.bpm) + (0.15f * inst_bpm);
                        }

                        if ((s_state.red_dc > 1.0f) && (s_state.ir_dc > 1.0f) &&
                            (s_state.red_ac > 1.0f) && (s_state.ir_ac > 1.0f))
                        {
                            float R = (s_state.red_ac / s_state.red_dc) / (s_state.ir_ac / s_state.ir_dc);
                            if ((R > 0.2f) && (R < 3.4f))
                            {
                                float est_spo2 = 104.0f - (17.0f * R);
                                if (est_spo2 < 70.0f) est_spo2 = 70.0f;
                                if (est_spo2 > 100.0f) est_spo2 = 100.0f;
                                s_state.spo2 = est_spo2;
                            }
                        }

                        if ((s_state.bpm >= 45.0f) && (s_state.bpm <= 220.0f) &&
                            (s_state.spo2 >= 70.0f) && (s_state.spo2 <= 100.0f))
                        {
                            s_state.last_good_bpm = s_state.bpm;
                            s_state.last_good_spo2 = s_state.spo2;
                            s_state.last_good_ms = sample_time_ms;
                        }

                        if (s_state.peak_count < 255U)
                        {
                            s_state.peak_count++;
                        }
                    }
                }
                s_state.last_peak_ms = sample_time_ms;
            }
        }
    }

    s_state.prev2_hp = s_state.prev1_hp;
    s_state.prev1_hp = ir_hp;
    s_state.last_sample_ms = sample_time_ms;
}

static HAL_StatusTypeDef reg_write(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c1, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U, 100U);
}

static HAL_StatusTypeDef reg_read(uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(&hi2c1, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 1U, 100U);
}

static HAL_StatusTypeDef burst_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100U);
}

static void reset_state(void)
{
    memset(&s_state, 0, sizeof(s_state));
}
