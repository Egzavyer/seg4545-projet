#include "drv_mq2.h"

#include "main.h"

extern ADC_HandleTypeDef hadc1;

static uint32_t s_baseline_acc = 0U;
static uint32_t s_baseline = 0U;
static uint16_t s_baseline_count = 0U;
static uint16_t read_adc_once(void);

bool mq2_init(void)
{
    s_baseline_acc = 0U;
    s_baseline = 0U;
    s_baseline_count = 0U;
    return true;
}

bool mq2_read(mq2_data_t *out)
{
    uint16_t raw;

    if (out == NULL)
    {
        return false;
    }

    raw = read_adc_once();
    out->adc_raw = raw;

    if (raw == 0U)
    {
        out->normalized_level = 0.0f;
        out->valid = false;
        return false;
    }

    if (s_baseline_count < 20U)
    {
        s_baseline_acc += raw;
        s_baseline_count++;
        s_baseline = s_baseline_acc / s_baseline_count;
    }
    else
    {
        s_baseline = (15U * s_baseline + raw) / 16U;
    }

    if (s_baseline == 0U)
    {
        out->normalized_level = 0.0f;
        out->valid = false;
        return false;
    }

    out->normalized_level = ((float)raw) / ((float)s_baseline);
    out->valid = true;
    return true;
}

static uint16_t read_adc_once(void)
{
    uint32_t value = 0U;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
    {
        value = HAL_ADC_GetValue(&hadc1);
    }

    (void)HAL_ADC_Stop(&hadc1);
    return (uint16_t)value;
}
