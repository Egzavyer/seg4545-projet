#include "drv_ds18b20.h"

#include <string.h>

/*
 * DS18B20 commands
 */
#define DS18B20_CMD_SKIP_ROM        0xCCU
#define DS18B20_CMD_READ_ROM        0x33U
#define DS18B20_CMD_CONVERT_T       0x44U
#define DS18B20_CMD_READ_SCRATCHPAD 0xBEU

static void dwt_delay_init(void);
static void delay_us(uint32_t us);

static inline void bus_drive_low(void);
static inline void bus_release_high(void);
static inline uint8_t bus_read_level(void);

static bool onewire_reset_pulse(void);
static void onewire_write_bit(uint8_t bit);
static uint8_t onewire_read_bit(void);
static void onewire_write_byte(uint8_t value);
static uint8_t onewire_read_byte(void);
static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len);

bool ds18b20_init(void)
{
    dwt_delay_init();
    bus_release_high();
    HAL_Delay(5U);
    return onewire_reset_pulse();
}

bool ds18b20_start_conversion(void)
{
    if (!onewire_reset_pulse())
    {
        return false;
    }

    onewire_write_byte(DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(DS18B20_CMD_CONVERT_T);
    return true;
}

bool ds18b20_read_rom(uint8_t rom[8])
{
    if (rom == NULL)
    {
        return false;
    }

    if (!onewire_reset_pulse())
    {
        return false;
    }

    onewire_write_byte(DS18B20_CMD_READ_ROM);

    for (uint8_t i = 0U; i < 8U; i++)
    {
        rom[i] = onewire_read_byte();
    }

    return (ds18b20_crc8(rom, 7U) == rom[7]);
}

bool ds18b20_read_temp_c(float *temp_c)
{
    uint8_t scratch[9];
    int16_t raw;

    if (temp_c == NULL)
    {
        return false;
    }

    if (!onewire_reset_pulse())
    {
        return false;
    }

    onewire_write_byte(DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(DS18B20_CMD_READ_SCRATCHPAD);

    for (uint8_t i = 0U; i < 9U; i++)
    {
        scratch[i] = onewire_read_byte();
    }

    if (ds18b20_crc8(scratch, 8U) != scratch[8])
    {
        return false;
    }

    raw = (int16_t)(((uint16_t)scratch[1] << 8) | scratch[0]);
    *temp_c = (float)raw / 16.0f;
    return true;
}

static void dwt_delay_init(void)
{
    static uint8_t inited = 0U;

    if (inited)
    {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0U;
    inited = 1U;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
        /* busy wait */
    }
}

static inline void bus_drive_low(void)
{
    HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
}

static inline void bus_release_high(void)
{
    HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);
}

static inline uint8_t bus_read_level(void)
{
    return (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static bool onewire_reset_pulse(void)
{
    uint8_t presence;

    __disable_irq();

    bus_drive_low();
    delay_us(500U);
    bus_release_high();
    delay_us(70U);
    presence = (uint8_t)(!bus_read_level());
    delay_us(410U);

    __enable_irq();

    return (presence != 0U);
}

static void onewire_write_bit(uint8_t bit)
{
    __disable_irq();

    bus_drive_low();
    if (bit)
    {
        delay_us(6U);
        bus_release_high();
        delay_us(64U);
    }
    else
    {
        delay_us(60U);
        bus_release_high();
        delay_us(10U);
    }

    __enable_irq();
}

static uint8_t onewire_read_bit(void)
{
    uint8_t bit;

    __disable_irq();

    bus_drive_low();
    delay_us(6U);
    bus_release_high();
    delay_us(9U);
    bit = bus_read_level();
    delay_us(55U);

    __enable_irq();

    return bit;
}

static void onewire_write_byte(uint8_t value)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        onewire_write_bit((uint8_t)(value & 0x01U));
        value >>= 1;
    }
}

static uint8_t onewire_read_byte(void)
{
    uint8_t value = 0U;

    for (uint8_t i = 0U; i < 8U; i++)
    {
        value >>= 1;
        if (onewire_read_bit())
        {
            value |= 0x80U;
        }
    }

    return value;
}

static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0U;

    for (uint8_t i = 0U; i < len; i++)
    {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0U; j < 8U; j++)
        {
            uint8_t mix = (uint8_t)((crc ^ inbyte) & 0x01U);
            crc >>= 1;
            if (mix)
            {
                crc ^= 0x8CU;
            }
            inbyte >>= 1;
        }
    }

    return crc;
}
