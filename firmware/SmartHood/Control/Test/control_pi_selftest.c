/**
 * @file control_pi_selftest.c
 * @brief 使用确定输入验证 PI 输出、限幅和积分抗饱和。
 */

#include "control_pi_selftest.h"

#include "control_pi.h"

/**
 * @brief 运行 PI 控制器自检。
 *
 * 测试使用固定参数和固定输入，因此每次运行都应得到相同结果。
 *
 * @return 全部测试通过返回 true，否则返回 false。
 */
bool ControlPi_RunSelfTests(void)
{
    ControlPi_t controller;
    int32_t output;
    uint32_t index;

    /*
     * 初始化测试控制器：
     * Kp = 64 / 256 = 0.25；
     * Ki = 4 / 256 = 0.015625；
     * PWM 输出限制为 30% 至 90%。
     */
    ControlPi_Init(&controller,
                   64L,
                   4L,
                   -2048L,
                   2048L,
                   30L,
                   90L);

    /*
     * 测试一：目标值等于反馈值。
     * 误差为零，不产生 PI 修正，只输出 50% 前馈。
     */
    output = ControlPi_Update(&controller, 130L, 130L, 50L);
    if ((output != 50L) ||
        (ControlPi_GetIntegral(&controller) != 0L))
    {
        return false;
    }

    /*
     * 测试二：实际计数比目标少 10。
     * 正误差应提高 PWM，预期输出为 52%。
     */
    ControlPi_Reset(&controller);
    output = ControlPi_Update(&controller, 130L, 120L, 50L);
    if (output != 52L)
    {
        return false;
    }

    /*
     * 测试三：实际计数比目标多 70。
     * 负误差应降低 PWM，预期输出为 32%。
     */
    ControlPi_Reset(&controller);
    output = ControlPi_Update(&controller, 130L, 200L, 50L);
    if (output != 32L)
    {
        return false;
    }

    /*
     * 测试四：连续输入较大的正误差。
     * 输出达到 90% 上限后，不应继续累积正积分，
     * 因此积分最终冻结在 520。
     */
    ControlPi_Reset(&controller);

    for (index = 0U; index < 5U; index++)
    {
        output = ControlPi_Update(&controller, 130L, 0L, 50L);
    }

    if ((output != 90L) ||
        (ControlPi_GetIntegral(&controller) != 520L))
    {
        return false;
    }

    /*
     * 测试五：输入较大的负误差。
     * 输出触及 30% 下限时，不应继续累积负积分。
     */
    ControlPi_Reset(&controller);
    output = ControlPi_Update(&controller, 0L, 300L, 50L);

    if ((output != 30L) ||
        (ControlPi_GetIntegral(&controller) != 0L))
    {
        return false;
    }

    return true;
}
