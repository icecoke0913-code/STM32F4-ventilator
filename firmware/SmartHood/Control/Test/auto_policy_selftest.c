/**
 * @file auto_policy_selftest.c
 * @brief 用固定温湿度和Tick序列验证AUTO策略。
 */

#include "auto_policy_selftest.h"

#include "auto_policy.h"

/**
 * @brief 构造一个有效的DHT11快照。
 */
static AutoPolicySnapshot_t AutoPolicy_TestSnapshot(
    int16_t temperature_x10,
    uint16_t humidity_x10)
{
    AutoPolicySnapshot_t snapshot;

    snapshot.temperature_x10 = temperature_x10;
    snapshot.humidity_x10 = humidity_x10;
    snapshot.valid = true;
    snapshot.updated_tick = 1000U;

    return snapshot;
}

/**
 * @brief 验证无效和过期数据必须安全停止。
 */
static bool AutoPolicy_TestInvalidAndStale(void)
{
    AutoPolicySnapshot_t snapshot = AutoPolicy_TestSnapshot(300, 700);

    snapshot.valid = false;
    if (AutoPolicy_Evaluate(&snapshot,
                            1100U,
                            MODE_MOTOR_LOW) != MODE_MOTOR_STOP)
    {
        return false;
    }

    snapshot.valid = true;
    if (AutoPolicy_Evaluate(&snapshot,
                            7001U,
                            MODE_MOTOR_HIGH) != MODE_MOTOR_STOP)
    {
        return false;
    }

    return true;
}

/**
 * @brief 验证STOP、LOW和HIGH的进入边界。
 */
static bool AutoPolicy_TestEnterThresholds(void)
{
    AutoPolicySnapshot_t snapshot = AutoPolicy_TestSnapshot(260, 600);

    if (AutoPolicy_Evaluate(&snapshot, 1100U, MODE_MOTOR_STOP) !=
        MODE_MOTOR_STOP)
    {
        return false;
    }

    snapshot.temperature_x10 = 280;
    if (AutoPolicy_Evaluate(&snapshot, 1100U, MODE_MOTOR_STOP) !=
        MODE_MOTOR_LOW)
    {
        return false;
    }

    snapshot.temperature_x10 = 320;
    if (AutoPolicy_Evaluate(&snapshot, 1100U, MODE_MOTOR_LOW) !=
        MODE_MOTOR_HIGH)
    {
        return false;
    }

    snapshot.temperature_x10 = 260;
    snapshot.humidity_x10 = 700;
    return AutoPolicy_Evaluate(&snapshot,
                               1100U,
                               MODE_MOTOR_STOP) == MODE_MOTOR_LOW;
}

/**
 * @brief 验证HIGH和LOW的退出迟滞边界。
 */
static bool AutoPolicy_TestExitHysteresis(void)
{
    AutoPolicySnapshot_t snapshot = AutoPolicy_TestSnapshot(310, 800);

    /* HIGH在退出边界仍应降为LOW，而不是直接停止。 */
    if (AutoPolicy_Evaluate(&snapshot, 1100U, MODE_MOTOR_HIGH) !=
        MODE_MOTOR_LOW)
    {
        return false;
    }

    snapshot.temperature_x10 = 270;
    snapshot.humidity_x10 = 650;
    if (AutoPolicy_Evaluate(&snapshot, 1100U, MODE_MOTOR_LOW) !=
        MODE_MOTOR_STOP)
    {
        return false;
    }

    /* 任一输入达到HIGH进入阈值，都不能被另一项低值抵消。 */
    snapshot.temperature_x10 = 320;
    snapshot.humidity_x10 = 300;
    return AutoPolicy_Evaluate(&snapshot,
                               1100U,
                               MODE_MOTOR_LOW) == MODE_MOTOR_HIGH;
}

/**
 * @brief 验证非法上一次请求按STOP基线重新计算。
 */
static bool AutoPolicy_TestIllegalPreviousRequest(void)
{
    AutoPolicySnapshot_t snapshot = AutoPolicy_TestSnapshot(280, 600);

    return AutoPolicy_Evaluate(&snapshot,
                               1100U,
                               MODE_MOTOR_FAULT) == MODE_MOTOR_LOW;
}

bool AutoPolicy_RunSelfTests(void)
{
    return AutoPolicy_TestInvalidAndStale() &&
           AutoPolicy_TestEnterThresholds() &&
           AutoPolicy_TestExitHysteresis() &&
           AutoPolicy_TestIllegalPreviousRequest();
}
