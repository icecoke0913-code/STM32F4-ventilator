/**
 * @file control_pi_selftest.h
 * @brief PI 纯算法板端自检接口。
 */

#ifndef CONTROL_PI_SELFTEST_H
#define CONTROL_PI_SELFTEST_H

#include <stdbool.h>

/**
 * @brief 运行 PI 控制器的全部板端自检项目。
 *
 * @return 全部测试通过返回 true，任意测试失败返回 false。
 */
bool ControlPi_RunSelfTests(void);

#endif /* CONTROL_PI_SELFTEST_H */
