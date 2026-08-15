/**
 * @file control_pi.c
 * @brief 相对速度 Q8 定点 PI、积分抗饱和与输出限幅。
 */

#include "control_pi.h"

#include <stddef.h>

/**
 * @brief 将数值限制在指定范围内。
 *
 * @param value 待限制的数值。
 * @param minimum 允许的最小值。
 * @param maximum 允许的最大值。
 *
 * @return 限制后的数值。
 */
static int32_t ControlPi_Clamp(int32_t value,
                               int32_t minimum,
                               int32_t maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

void ControlPi_Init(ControlPi_t *controller,
                    int32_t kp_q8,
                    int32_t ki_q8,
                    int32_t integral_min,
                    int32_t integral_max,
                    int32_t output_min,
                    int32_t output_max)
{
    /* 空指针不能访问，直接返回以避免非法内存操作。 */
    if (controller == NULL)
    {
        return;
    }

    controller->kp_q8 = kp_q8;
    controller->ki_q8 = ki_q8;
    controller->integral = 0L;
    controller->integral_min = integral_min;
    controller->integral_max = integral_max;
    controller->output_min = output_min;
    controller->output_max = output_max;
}

void ControlPi_Reset(ControlPi_t *controller)
{
    /* 切换挡位或停止电机时，只需清除历史积分。 */
    if (controller != NULL)
    {
        controller->integral = 0L;
    }
}

int32_t ControlPi_Update(ControlPi_t *controller,
                         int32_t target_count,
                         int32_t actual_count,
                         int32_t feedforward)
{
    int32_t error;
    int32_t candidate_integral;
    int32_t correction;
    int32_t unclamped_output;
    int32_t output;

    /* 控制器无效时返回安全输出，不访问非法内存。 */
    if (controller == NULL)
    {
        return 0L;
    }

    /* 当前误差等于目标计数减去实际计数。 */
    error = target_count - actual_count;

    /*
     * 先计算候选积分，并限制其累计范围。
     * 此时暂不写回控制器，等待后面判断输出是否饱和。
     */
    candidate_integral = ControlPi_Clamp(
        controller->integral + error,
        controller->integral_min,
        controller->integral_max);

    /*
     * Q8 定点 PI 修正量：
     * correction = Kp × error + Ki × integral。
     * Kp和Ki放大了256倍，因此最终除以256恢复实际比例。
     */
    correction =
        ((controller->kp_q8 * error) +
         (controller->ki_q8 * candidate_integral)) /
        CONTROL_PI_Q8_SCALE;

    unclamped_output = feedforward + correction;

    /* 将最终 PWM 限制在控制器允许的范围内。 */
    output = ControlPi_Clamp(unclamped_output,
                             controller->output_min,
                             controller->output_max);

    /*
     * 如果输出已经越过上限，且正误差还会推动输出继续增加；
     * 或输出已经越过下限，且负误差还会推动输出继续降低，
     * 则拒绝本周期的新积分，防止积分饱和。
     */
    if (((unclamped_output > controller->output_max) &&
         (error > 0L)) ||
        ((unclamped_output < controller->output_min) &&
         (error < 0L)))
    {
        /*
         * 使用旧积分重新计算输出。
         * 当误差方向反转时，控制器可以立即退出饱和状态。
         */
        correction =
            ((controller->kp_q8 * error) +
             (controller->ki_q8 * controller->integral)) /
            CONTROL_PI_Q8_SCALE;

        output = ControlPi_Clamp(feedforward + correction,
                                 controller->output_min,
                                 controller->output_max);
    }
    else
    {
        /* 输出未被误差继续推向饱和区，接受本周期积分。 */
        controller->integral = candidate_integral;
    }

    return output;
}

int32_t ControlPi_GetIntegral(const ControlPi_t *controller)
{
    /* 空指针情况下返回零，便于日志和自检安全调用。 */
    if (controller == NULL)
    {
        return 0L;
    }

    return controller->integral;
}
