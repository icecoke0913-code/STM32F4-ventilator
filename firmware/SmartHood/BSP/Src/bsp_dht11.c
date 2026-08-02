#include "bsp_dht11.h"

#include "main.h"
#include "tim.h"

#define DHT11_START_LOW_US       18000U
#define DHT11_RELEASE_US            30U
#define DHT11_EDGE_TIMEOUT_US      120U
#define DHT11_ONE_THRESHOLD_US      50U
#define DHT11_DATA_BYTES              5U
#define DHT11_DATA_BITS              40U

static void DHT11_SetOutputLow(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_RESET);

    gpio_init.Pin = DHT11_DATA_Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &gpio_init);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT11_DATA_Pin;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &gpio_init);
}

static uint32_t DHT11_GetTimeUs(void)
{
    return __HAL_TIM_GET_COUNTER(&htim5);
}

static void DHT11_DelayUs(uint32_t delay_us)
{
    uint32_t start_time = DHT11_GetTimeUs();

    while ((uint32_t)(DHT11_GetTimeUs() - start_time) < delay_us)
    {
    }
}

static bool DHT11_WaitForPin(GPIO_PinState expected_state,
                             uint32_t timeout_us)
{
    uint32_t start_time = DHT11_GetTimeUs();

    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port,
                           DHT11_DATA_Pin) != expected_state)
    {
        if ((uint32_t)(DHT11_GetTimeUs() - start_time) >= timeout_us)
        {
            return false;
        }
    }

    return true;
}

bool BSP_DHT11_Init(void)
{
    DHT11_SetInput();
    __HAL_TIM_SET_COUNTER(&htim5, 0U);

    return HAL_TIM_Base_Start(&htim5) == HAL_OK;
}

DHT11_Status_t BSP_DHT11_Read(DHT11_Data_t *data)
{
    uint8_t raw_data[DHT11_DATA_BYTES] = {0U};
    uint32_t saved_primask;
    uint32_t high_start;
    uint32_t high_width;
    uint32_t bit_index;
    uint32_t byte_index;
    bool capture_ok = true;
    uint16_t humidity_x10;
    int16_t temperature_x10;

    DHT11_SetOutputLow();
    DHT11_DelayUs(DHT11_START_LOW_US);

    saved_primask = __get_PRIMASK();
    __disable_irq();

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_SET);
    DHT11_DelayUs(DHT11_RELEASE_US);
    DHT11_SetInput();

    if (!DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US) ||
        !DHT11_WaitForPin(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US) ||
        !DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US))
    {
        capture_ok = false;
    }

    for (bit_index = 0U;
         capture_ok && (bit_index < DHT11_DATA_BITS);
         bit_index++)
    {
        if (!DHT11_WaitForPin(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US))
        {
            capture_ok = false;
            break;
        }

        high_start = DHT11_GetTimeUs();

        if (!DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US))
        {
            capture_ok = false;
            break;
        }

        high_width = (uint32_t)(DHT11_GetTimeUs() - high_start);
        byte_index = bit_index / 8U;
        raw_data[byte_index] <<= 1U;

        if (high_width > DHT11_ONE_THRESHOLD_US)
        {
            raw_data[byte_index] |= 1U;
        }
    }

    if (saved_primask == 0U)
    {
        __enable_irq();
    }

    DHT11_SetInput();

    if (!capture_ok)
    {
        return DHT11_STATUS_TIMEOUT;
    }

    if ((uint8_t)(raw_data[0] + raw_data[1] +
                  raw_data[2] + raw_data[3]) != raw_data[4])
    {
        return DHT11_STATUS_CHECKSUM_ERROR;
    }

    humidity_x10 = (uint16_t)raw_data[0] * 10U + raw_data[1];
    temperature_x10 = (int16_t)
        (((uint16_t)(raw_data[2] & 0x7FU) * 10U) + raw_data[3]);

    if ((raw_data[2] & 0x80U) != 0U)
    {
        temperature_x10 = (int16_t)-temperature_x10;
    }

    data->humidity_x10 = humidity_x10;
    data->temperature_x10 = temperature_x10;

    return DHT11_STATUS_OK;
}
