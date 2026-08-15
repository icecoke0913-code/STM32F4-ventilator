/**
 * @file control_pi.h
 * @brief 相对速度定点 PI 控制器公共接口。
 */

#ifndef CONTROL_PI_H
#define CONTROL_PI_H

#include <stdint.h>

/**
 * @brief Q8 定点数的缩放系数。
 *
 * Q8 使用 256 表示实数 1.0：
 * 64 表示 0.25，4 表示 0.015625。
 */
#define CONTROL_PI_Q8_SCALE 256L

/**
 * @brief PI 控制器的数据结构。
 *
 * 保存控制参数、积分状态以及允许的输出范围。
 */
typedef struct
{
    int32_t kp_q8;          /**< 比例系数放大 256 倍后的整数值。 */
    int32_t ki_q8;          /**< 积分系数放大 256 倍后的整数值。 */
    int32_t integral;       /**< 历次计数误差的累计值。 */
    int32_t integral_min;   /**< 积分累计值的下限。 */
    int32_t integral_max;   /**< 积分累计值的上限。 */
    int32_t output_min;     /**< 控制器输出下限，即最小 PWM 百分比。 */
    int32_t output_max;     /**< 控制器输出上限，即最大 PWM 百分比。 */
} ControlPi_t;

/**
 * @brief 初始化 PI 控制器。
 *
 * @param controller PI 控制器对象。
 * @param kp_q8 放大 256 倍后的比例系数。
 * @param ki_q8 放大 256 倍后的积分系数。
 * @param integral_min 积分下限。
 * @param integral_max 积分上限。
 * @param output_min 输出下限。
 * @param output_max 输出上限。
 */
void ControlPi_Init(ControlPi_t *controller,
                    int32_t kp_q8,
                    int32_t ki_q8,
                    int32_t integral_min,
                    int32_t integral_max,
                    int32_t output_min,
                    int32_t output_max);

/**
 * @brief 清除 PI 控制器的积分状态。
 *
 * @param controller PI 控制器对象。
 */
void ControlPi_Reset(ControlPi_t *controller);

/**
 * @brief 根据目标计数和实际计数计算新的控制输出。
 *
 * @param controller PI 控制器对象。
 * @param target_count 每个控制周期的目标编码器计数。
 * @param actual_count 每个控制周期的实际编码器计数。
 * @param feedforward 前馈 PWM 百分比。
 *
 * @return 经过 PI 修正和上下限限制后的 PWM 百分比。
 */
int32_t ControlPi_Update(ControlPi_t *controller,
                         int32_t target_count,
                         int32_t actual_count,
                         int32_t feedforward);

/**
 * @brief 获取当前积分累计值，供自检和调试日志使用。
 *
 * @param controller PI 控制器对象。
 *
 * @return 当前积分累计值。
 */
int32_t ControlPi_GetIntegral(const ControlPi_t *controller);

#endif /* CONTROL_PI_H */
