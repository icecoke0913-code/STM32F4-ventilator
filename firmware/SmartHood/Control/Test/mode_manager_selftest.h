/**
 * @file mode_manager_selftest.h
 * @brief 模式管理状态转换表自检入口。
 */

#ifndef MODE_MANAGER_SELFTEST_H
#define MODE_MANAGER_SELFTEST_H

#include <stdbool.h>

/**
 * @brief 验证初始状态、模式循环、手动挡位和故障恢复。
 * @return 所有状态转换符合设计时返回true，否则返回false。
 */
bool ModeManager_RunSelfTests(void);

#endif /* MODE_MANAGER_SELFTEST_H */
