#include "drv_dht11.h"

#include <string.h>
#include "main.h"

#define DHT11_PORT              GPIOA
#define DHT11_PIN               GPIO_PIN_8

typedef struct
{
    uint8_t initialized;
} dht11_ctx_t;

static dht11_ctx_t s_ctx = {0};

static void dwt_delay_init(void);
static void delay_us(uint32_t us);
static void dht11_set_output(void);
static void dht11_set_input(void);
static int wait_for_pin_state(GPIO_PinState state, uint32_t timeout_us);

bool dht11_init(void)
{
    dwt_delay_init();
    dht11_set_input();
    s_ctx.initialized = 1U;
    return true;
}

bool dht11_read(dht11_data_t *out)
{
    uint8_t data[5] = {0};
    uint8_t i, j;

    if (out == NULL)
    {
        return false;
    }

    memset(out, 0, sizeof(*out));

    if (s_ctx.initialized == 0U)
    {
        return false;
    }

    dht11_set_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30U);
    dht11_set_input();

    if (wait_for_pin_state(GPIO_PIN_RESET, 100U) < 0) return false;
    if (wait_for_pin_state(GPIO_PIN_SET,   100U) < 0) return false;
    if (wait_for_pin_state(GPIO_PIN_RESET, 100U) < 0) return false;

    for (j = 0U; j < 5U; j++)
    {
        for (i = 0U; i < 8U; i++)
        {
            if (wait_for_pin_state(GPIO_PIN_SET, 70U) < 0) return false;
            delay_us(40U);

            data[j] <<= 1U;
            if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
            {
                data[j] |= 1U;
                if (wait_for_pin_state(GPIO_PIN_RESET, 80U) < 0) return false;
            }
        }
    }

    if (((uint8_t)(data[0] + data[1] + data[2] + data[3])) != data[4])
    {
        return false;
    }

    out->humidity_pct = (float)data[0];
    out->ambient_temp_c = (float)data[2];
    out->valid = true;
    return true;
}

static void dwt_delay_init(void)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0U;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000U);
    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

static void dht11_set_output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void dht11_set_input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static int wait_for_pin_state(GPIO_PinState state, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = timeout_us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != state)
    {
        if ((DWT->CYCCNT - start) > ticks)
        {
            return -1;
        }
    }
    return 0;
}
