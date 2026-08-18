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
 * @brief 在任务创建前建立按键事件队列。
 *
 * 队列用于把PA0按键事件从默认任务发送给电机任务，
 * 保证只有电机任务能够直接修改PWM和电机状态。
 *
 * @return 创建成功返回true，创建失败返回false。
 */
bool App_MotorControl_Init(void);

/**
 * @brief 创建DHT11快照互斥量并恢复无效初始状态。
 * @return 创建成功返回true，否则返回false。
 */
bool App_SensorState_Init(void);

/**
 * @brief 默认任务：执行显示自检、心跳和按键事件识别与发送。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
void App_DefaultTask(void *argument);

/**
 * @brief 传感器任务：周期读取DHT11并输出采集状态。
 * @param argument FreeRTOS任务参数，本项目未使用。
 */
void App_SensorTask(void *argument);

/**
 * @brief 电机控制任务：每50ms读取编码器并执行软启动、PI和故障停机。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 该任务是唯一允许初始化TB6612、修改PWM和改变电机状态的任务。
 */
void App_MotorTask(void *argument);

#endif
