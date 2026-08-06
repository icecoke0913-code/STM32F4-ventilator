/**
 * @file bsp_motor.h
 * @brief TB6612FNG A通道开环电机控制公共接口。
 */

#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化TIM4 PWM，并使TB6612保持安全停止状态。
 * @return PWM成功启动时返回true，失败时返回false且STBY保持低电平。
 */
bool BSP_Motor_Init(void);

/**
 * @brief 设置A通道固定正转占空比。
 * @param duty_percent 百分比占空比；大于100时在内部限制为100。
 *
 * 传入0会执行完整安全停机。驱动尚未成功初始化时，
 * 任何非零请求也只会保持停止，不会使能TB6612。
 */
void BSP_Motor_SetDuty(uint8_t duty_percent);

/**
 * @brief 立即关闭TB6612输出、清零PWM并拉低方向引脚。
 */
void BSP_Motor_Stop(void);

#endif
