/**
 * @file debug_log.c
 * @brief 基于CMSIS-RTOS2互斥量和USART1的线程安全日志实现。
 */

#include "debug_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "usart.h"

/*
 * 两个任务可能同时输出日志，因此使用同一把CMSIS-RTOS2互斥量
 * 串行化“格式化 + UART发送”的完整过程，避免日志交叉或HAL忙状态。
 */
static osMutexId_t debug_log_mutex = NULL;

/**
 * @brief 创建日志互斥量。
 * @return 初始化成功或已经初始化时返回true，创建失败返回false。
 *
 * 该函数在MX_FREERTOS_Init()中、任务创建之前调用，
 * 从而保证任何任务开始输出日志时互斥量已经存在。
 */
bool DebugLog_Init(void)
{
    /* 允许重复调用初始化函数，但不重复分配内核对象。 */
    if (debug_log_mutex != NULL)
    {
        return true;
    }

    debug_log_mutex = osMutexNew(NULL);
    return debug_log_mutex != NULL;
}

/**
 * @brief 格式化并发送一条线程安全的USART1调试日志。
 * @param format printf风格格式字符串，后面可跟可变参数。
 *
 * 日志缓冲区固定为160字节；过长文本会被截断，
 * UART使用100 ms阻塞超时。函数不向调用者返回发送结果。
 */
void DebugLog_Printf(const char *format, ...)
{
    char buffer[160];
    va_list arguments;
    int length;

    /* 未初始化或格式字符串无效时直接忽略本次日志。 */
    if ((format == NULL) || (debug_log_mutex == NULL))
    {
        return;
    }

    /* 获取互斥量后，其他任务必须等待当前整条日志发送完成。 */
    if (osMutexAcquire(debug_log_mutex, osWaitForever) != osOK)
    {
        return;
    }

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length > 0)
    {
        /* 限制发送长度，防止vsnprintf返回的原始长度超过缓冲区。 */
        if (length >= (int)sizeof(buffer))
        {
            length = (int)sizeof(buffer) - 1;
        }

        (void)HAL_UART_Transmit(&huart1,
                                (uint8_t *)buffer,
                                (uint16_t)length,
                                100U);
    }

    /* 无论格式化结果是否有效，都必须释放已经获得的互斥量。 */
    (void)osMutexRelease(debug_log_mutex);
}
