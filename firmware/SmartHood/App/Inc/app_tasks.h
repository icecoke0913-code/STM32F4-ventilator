/**
 * @file app_tasks.h
 * @brief SmartHood应用层FreeRTOS任务入口。
 *
 * CubeMX生成的任务入口只负责调用这里声明的应用函数，
 * 业务逻辑因此不会在CubeMX重新生成代码时被覆盖。
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>

/**
 * @brief 在任务创建前建立电机控制命令队列。
 *
 * 队列用于把PA0按键命令从默认任务发送给电机任务，
 * 保证只有电机任务能够直接修改PWM和电机状态。
 *
 * @return 创建成功返回true，创建失败返回false。
 */
bool App_MotorControl_Init(void);

/**
 * @brief 默认任务：执行显示自检、心跳和按键消抖，并发送电机命令。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
void App_DefaultTask(void *argument);

/**
 * @brief 传感器任务：周期读取DHT11并输出采集状态。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
void App_SensorTask(void *argument);

/**
 * @brief 电机测速任务：每50 ms读取编码器并周期输出方向和RPM。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * M5阶段该任务只读取编码器，不修改TIM4 PWM、TB6612方向或待机状态。
 */
void App_MotorTask(void *argument);

#endif
