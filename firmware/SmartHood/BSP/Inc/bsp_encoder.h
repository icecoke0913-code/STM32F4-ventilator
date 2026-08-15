/**
 * @file bsp_encoder.h
 * @brief TIM3 AB 相编码器计数与 RPM 换算接口。
 */

#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 编码器旋转方向。
 */
typedef enum
{
    ENCODER_DIRECTION_STOPPED = 0, /**< 本次采样没有计数变化。 */
    ENCODER_DIRECTION_FORWARD,     /**< 计数增量为正。 */
    ENCODER_DIRECTION_REVERSE      /**< 计数增量为负。 */
} Encoder_Direction_t;

/**
 * @brief 初始化编码器软件状态。
 *
 * 该函数不会启动定时器，只清零软件保存的上次计数值。
 */
void BSP_Encoder_Init(void);

/**
 * @brief 启动TIM3硬件编码器计数。
 *
 * 启动成功后，TIM3根据PC6和PC7上的AB相信号自动计数。
 *
 * @return 启动成功返回true，失败返回false。
 */
bool BSP_Encoder_Start(void);

/**
 * @brief 停止 TIM3 编码器计数。
 */
void BSP_Encoder_Stop(void);

/**
 * @brief 获取当前TIM3计数器值。
 *
 * @return 16位无符号计数器当前值。
 */
uint16_t BSP_Encoder_ReadCount(void);

/**
 * @brief 读取相邻采样之间的有符号计数增量。
 *
 * 函数内部使用无符号减法处理65535到0或0到65535的回绕，
 * 然后转换为有符号值。调用间隔为50 ms时，单次增量必须小于32768。
 *
 * @param direction 输出旋转方向，可传入NULL表示不需要方向。
 * @return 本次采样的有符号计数增量。
 */
int16_t BSP_Encoder_ReadDelta(Encoder_Direction_t *direction);

/**
 * @brief 将计数增量换算为放大10倍的RPM整数。
 *
 * 例如返回1234表示123.4 RPM。当前使用理论值1400 counts/rev，
 * 仅用于转速趋势显示；需要精确闭环控制前应重新标定。
 *
 * @param delta 采样窗口内的有符号计数增量。
 * @param sample_ms 采样窗口，单位为毫秒。
 * @return 带方向的RPM乘10结果。
 */
int32_t BSP_Encoder_CountToRpmX10(int16_t delta, uint32_t sample_ms);

#endif /* BSP_ENCODER_H */
