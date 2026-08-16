/**
 * @file bsp_key_selftest.h
 * @brief 单按键状态机确定性自检入口。
 */

#ifndef BSP_KEY_SELFTEST_H
#define BSP_KEY_SELFTEST_H

#include <stdbool.h>

/**
 * @brief 运行按键消抖、单击、双击和长按自检。
 * @return 所有测试通过返回true，否则返回false。
 */
bool BSP_Key_RunSelfTests(void);

#endif /* BSP_KEY_SELFTEST_H */
