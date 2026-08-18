/**
 * @file auto_policy.c
 * @brief DHT11温湿度AUTO档位阈值和迟滞实现。
 */

#include "auto_policy.h"

#include <stddef.h>

/** DHT11有效快照允许保留的最长时间。 */
#define AUTO_POLICY_STALE_TIME_MS          6000U

/** 进入LOW的温度和湿度阈值，数值均扩大10倍。 */
#define AUTO_POLICY_LOW_TEMP_ON_X10         280
#define AUTO_POLICY_LOW_HUMIDITY_ON_X10     700U

/** 进入HIGH的温度和湿度阈值，数值均扩大10倍。 */
#define AUTO_POLICY_HIGH_TEMP_ON_X10        320
#define AUTO_POLICY_HIGH_HUMIDITY_ON_X10    850U

/** HIGH降为LOW的温度和湿度阈值。 */
#define AUTO_POLICY_HIGH_TEMP_OFF_X10       310
#define AUTO_POLICY_HIGH_HUMIDITY_OFF_X10   800U

/** LOW降为STOP的温度和湿度阈值。 */
#define AUTO_POLICY_LOW_TEMP_OFF_X10        270
#define AUTO_POLICY_LOW_HUMIDITY_OFF_X10    650U

ModeMotorRequest_t AutoPolicy_Evaluate(
    const AutoPolicySnapshot_t *snapshot,
    uint32_t now_tick,
    ModeMotorRequest_t previous_auto_request)
{
    bool high_requested;
    bool high_released;
    bool low_requested;
    bool low_released;

    /* 无数据或数据超过三个采样周期时，AUTO必须安全停止。 */
    if ((snapshot == NULL) ||
        !snapshot->valid ||
        ((uint32_t)(now_tick - snapshot->updated_tick) >
         AUTO_POLICY_STALE_TIME_MS))
    {
        return MODE_MOTOR_STOP;
    }

    high_requested =
        (snapshot->temperature_x10 >= AUTO_POLICY_HIGH_TEMP_ON_X10) ||
        (snapshot->humidity_x10 >= AUTO_POLICY_HIGH_HUMIDITY_ON_X10);
    high_released =
        (snapshot->temperature_x10 <= AUTO_POLICY_HIGH_TEMP_OFF_X10) &&
        (snapshot->humidity_x10 <= AUTO_POLICY_HIGH_HUMIDITY_OFF_X10);
    low_requested =
        (snapshot->temperature_x10 >= AUTO_POLICY_LOW_TEMP_ON_X10) ||
        (snapshot->humidity_x10 >= AUTO_POLICY_LOW_HUMIDITY_ON_X10);
    low_released =
        (snapshot->temperature_x10 <= AUTO_POLICY_LOW_TEMP_OFF_X10) &&
        (snapshot->humidity_x10 <= AUTO_POLICY_LOW_HUMIDITY_OFF_X10);

    /* 任一输入达到HIGH进入阈值时立即请求高档。 */
    if (high_requested)
    {
        return MODE_MOTOR_HIGH;
    }

    /* HIGH只有两项都降到关闭阈值后才逐级降为LOW。 */
    if (previous_auto_request == MODE_MOTOR_HIGH)
    {
        return high_released ? MODE_MOTOR_LOW : MODE_MOTOR_HIGH;
    }

    /* LOW只有两项都降到关闭阈值后才停止。 */
    if (previous_auto_request == MODE_MOTOR_LOW)
    {
        return low_released ? MODE_MOTOR_STOP : MODE_MOTOR_LOW;
    }

    /* STOP或非法历史请求按进入阈值重新计算。 */
    return low_requested ? MODE_MOTOR_LOW : MODE_MOTOR_STOP;
}
