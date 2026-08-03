/**
 * @file bsp_dht11.c
 * @brief 使用PD0单总线和TIM5微秒计时实现DHT11驱动。
 */

#include "bsp_dht11.h"

#include "main.h"
#include "tim.h"

/* DHT11协议时序单位均为微秒，TIM5以1 MHz计数提供时间基准。 */
#define DHT11_START_LOW_US       18000U /* 主机启动信号至少拉低18 ms。 */
#define DHT11_RELEASE_US            30U /* 释放总线后等待30 us再捕获响应。 */
#define DHT11_EDGE_TIMEOUT_US      120U /* 单个响应或数据边沿的超时保护。 */
#define DHT11_ONE_THRESHOLD_US      50U /* 高电平超过50 us判定数据位为1。 */
#define DHT11_DATA_BYTES              5U /* 湿度、温度与校验和共5字节。 */
#define DHT11_DATA_BITS              40U /* 一帧固定传输40个数据位。 */

/**
 * @brief 将数据线配置为开漏输出并主动拉低。
 *
 * 开漏输出只负责产生低电平；后续写SET表示释放总线，
 * 高电平由上拉提供，避免主机与传感器同时驱动造成冲突。
 */
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

/**
 * @brief 将数据线恢复为带内部上拉的输入模式。
 *
 * 内部上拉使传感器断开时总线保持高电平，读取流程会通过超时安全退出。
 */
static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT11_DATA_Pin;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &gpio_init);
}

/**
 * @brief 读取TIM5当前的微秒计数值。
 * @return 32位自由运行计数器的当前值。
 */
static uint32_t DHT11_GetTimeUs(void)
{
    return __HAL_TIM_GET_COUNTER(&htim5);
}

/**
 * @brief 使用TIM5忙等待指定微秒数。
 * @param delay_us 需要等待的时间，单位为微秒。
 *
 * 使用无符号计数差，因此TIM5在等待期间发生32位回绕仍能正确计时。
 */
static void DHT11_DelayUs(uint32_t delay_us)
{
    uint32_t start_time = DHT11_GetTimeUs();

    while ((uint32_t)(DHT11_GetTimeUs() - start_time) < delay_us)
    {
    }
}

/**
 * @brief 等待数据线进入指定电平，并提供超时保护。
 * @param expected_state 期望捕获的GPIO电平。
 * @param timeout_us 最大等待时间，单位为微秒。
 * @return 在超时前到达目标电平返回true，否则返回false。
 */
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

/**
 * @brief 初始化DHT11数据线与TIM5微秒时间基准。
 * @return TIM5成功启动返回true，否则返回false。
 */
bool BSP_DHT11_Init(void)
{
    DHT11_SetInput();
    __HAL_TIM_SET_COUNTER(&htim5, 0U);

    return HAL_TIM_Base_Start(&htim5) == HAL_OK;
}

/**
 * @brief 完成一次DHT11启动、响应捕获、40位读取和校验转换。
 * @param data 有效输出结构指针；读取完全成功后才更新。
 * @return 读取成功、边沿超时或校验和错误状态。
 */
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

    /* 阶段1：主机拉低总线至少18 ms，通知DHT11开始一次传输。 */
    DHT11_SetOutputLow();
    DHT11_DelayUs(DHT11_START_LOW_US);

    /* 阶段2：保存中断状态并释放总线；30 us后开始捕获传感器响应。 */
    saved_primask = __get_PRIMASK();
    __disable_irq();

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port,
                      DHT11_DATA_Pin,
                      GPIO_PIN_SET);
    DHT11_DelayUs(DHT11_RELEASE_US);
    DHT11_SetInput();

    /* 阶段3：确认约80 us低电平和80 us高电平响应序列。 */
    if (!DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US) ||
        !DHT11_WaitForPin(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US) ||
        !DHT11_WaitForPin(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US))
    {
        capture_ok = false;
    }

    /* 阶段4：读取40位数据，高电平宽度大于50 us判定为1。 */
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

    /* 阶段5：先恢复中断和GPIO，再根据采集结果返回错误。 */
    if (saved_primask == 0U)
    {
        __enable_irq();
    }

    DHT11_SetInput();

    if (!capture_ok)
    {
        return DHT11_STATUS_TIMEOUT;
    }

    /* 阶段6：验证前4字节累加校验和，并转换温湿度。 */
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

    /* 只有完整成功后才写入调用者结构，失败时保留旧数据。 */
    data->humidity_x10 = humidity_x10;
    data->temperature_x10 = temperature_x10;

    return DHT11_STATUS_OK;
}
