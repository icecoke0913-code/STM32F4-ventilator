/**
 * @file debug_log.h
 * @brief 线程安全调试日志模块的公共接口。
 *
 * 本模块由应用任务调用，通过USART1输出格式化文本。
 * 互斥量的创建与具体发送过程由debug_log.c负责。
 */

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdbool.h>

/**
 * @brief 创建保护调试串口的互斥量。
 * @return 创建成功或已经初始化时返回true，否则返回false。
 */
bool DebugLog_Init(void);

/**
 * @brief 按printf格式生成日志并通过USART1阻塞发送。
 * @param format printf风格的格式字符串，后面可跟可变参数。
 */
void DebugLog_Printf(const char *format, ...);

#endif
