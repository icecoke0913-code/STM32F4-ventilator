/**
 * @file auto_policy_selftest.h
 * @brief AUTO温湿度策略的固定输入自检接口。
 */

#ifndef AUTO_POLICY_SELFTEST_H
#define AUTO_POLICY_SELFTEST_H

#include <stdbool.h>

/**
 * @brief 执行AUTO策略边界、迟滞和过期保护自检。
 * @return 全部断言通过返回true，否则返回false。
 */
bool AutoPolicy_RunSelfTests(void);

#endif /* AUTO_POLICY_SELFTEST_H */
