/**
 * @file bsp_encoder.c
 * @brief TIM3 AB相编码器计数、方向识别与RPM换算实现。
 */

#include "bsp_encoder.h"

#include "tim.h"

/** 理论每输出轴一圈的四倍频计数，M5标定后再更新。 */
#define ENCODER_COUNTS_PER_REV 1400UL

/** 保存上一次采样时的16位TIM3计数值。 */
static uint16_t encoder_last_count = 0U;

void BSP_Encoder_Init(void)
{
    /*
     * 初始化阶段同时清零硬件计数器和软件历史值，
     * 确保第一次读取增量时不会包含复位前的旧计数。
     */
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    encoder_last_count = 0U;
}

bool BSP_Encoder_Start(void)
{
    /*
     * TIM_CHANNEL_ALL表示同时启动CH1和CH2。
     * 编码器模式必须同时采集两相信号，才能判断旋转方向。
     */
    if (HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return false;
    }

    /*
     * 启动成功后重新同步软件历史值。
     * 这样停止期间或启动瞬间的计数不会被误认为本次转动量。
     */
    encoder_last_count = BSP_Encoder_ReadCount();

    return true;
}

void BSP_Encoder_Stop(void)
{
    /*
     * 停止两路输入捕获。停止后保留当前计数值，
     * 下次启动时由BSP_Encoder_Start()重新同步历史值。
     */
    (void)HAL_TIM_Encoder_Stop(&htim3, TIM_CHANNEL_ALL);
}

uint16_t BSP_Encoder_ReadCount(void)
{
    /*
     * TIM3配置为16位计数器，只保留低16位。
     * 编码器模式下硬件会根据AB相顺序自动递增或递减。
     */
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
}

int16_t BSP_Encoder_ReadDelta(Encoder_Direction_t *direction)
{
    uint16_t current_count;
    uint16_t delta_unsigned;
    int16_t delta;

    current_count = BSP_Encoder_ReadCount();

    /*
     * 先进行16位无符号减法，再转换为有符号数。
     * 这样可以自动处理计数器跨越0和65535时的回绕。
     */
    delta_unsigned = (uint16_t)(current_count - encoder_last_count);
    delta = (int16_t)delta_unsigned;

    encoder_last_count = current_count;

    /* direction允许传入NULL，调用者可以只获取计数增量。 */
    if (direction != NULL)
    {
        if (delta > 0)
        {
            *direction = ENCODER_DIRECTION_FORWARD;
        }
        else if (delta < 0)
        {
            *direction = ENCODER_DIRECTION_REVERSE;
        }
        else
        {
            *direction = ENCODER_DIRECTION_STOPPED;
        }
    }

    return delta;
}

int32_t BSP_Encoder_CountToRpmX10(int16_t delta, uint32_t sample_ms)
{
    int32_t delta_32;
    uint32_t magnitude;
    uint64_t numerator;
    uint32_t rpm_x10;

    /*
     * sample_ms为0会造成除零，因此无效采样周期直接返回0。
     * 每圈计数常数为编译期非零常量。
     */
    if (sample_ms == 0U)
    {
        return 0;
    }

    /*
     * 先扩展到32位再取绝对值，避免直接对int16_t最小值
     * -32768取负时发生16位有符号溢出。
     */
    delta_32 = (int32_t)delta;

    if (delta_32 < 0)
    {
        magnitude = (uint32_t)(-delta_32);
    }
    else
    {
        magnitude = (uint32_t)delta_32;
    }

    /*
     * RPM乘10计算：
     * counts × 60秒/分钟 × 1000毫秒/秒 × 10
     * ------------------------------------------------
     *       counts/rev × sample_ms
     *
     * 使用64位中间值，避免乘以600000时发生32位溢出。
     */
    numerator = (uint64_t)magnitude * 600000ULL;

    rpm_x10 = (uint32_t)
              (numerator /
               ((uint64_t)ENCODER_COUNTS_PER_REV *
                (uint64_t)sample_ms));

    /* RPM结果保留计数增量的正负方向。 */
    if (delta_32 < 0)
    {
        return -(int32_t)rpm_x10;
    }

    return (int32_t)rpm_x10;
}
