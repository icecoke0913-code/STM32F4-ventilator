/**
 * @file bsp_motor.c
 * @brief TB6612FNG A通道安全初始化、固定正转和PWM调速实现。
 */

#include "bsp_motor.h"

#include "main.h"
#include "tim.h"

/** 记录PWM是否已成功启动，防止初始化失败后误使能驱动器。 */
static bool motor_initialized = false;

/**
 * @brief 将百分比换算为TIM4_CH1比较值。
 * @param duty_percent 已限制在1～100范围内的占空比。
 * @return 对应一个PWM周期的高电平计数值。
 */
static uint32_t Motor_DutyToPulse(uint8_t duty_percent)
{
    uint32_t period_counts;

    /*
     * ARR=4199表示计数器从0计数到4199，
     * 因此一个完整PWM周期共有4200个计数。
     */
    period_counts = __HAL_TIM_GET_AUTORELOAD(&htim4) + 1U;

    return (period_counts * (uint32_t)duty_percent) / 100U;
}

bool BSP_Motor_Init(void)
{
    /*
     * 初始化入口先进入安全停止状态，
     * 避免上一次调试状态影响本次启动。
     */
    motor_initialized = false;
    BSP_Motor_Stop();

    if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK)
    {
        BSP_Motor_Stop();
        return false;
    }

    /*
     * PWM虽然已经启动，但比较值仍为0且STBY为低，
     * 因此初始化成功不会让电机自动旋转。
     */
    motor_initialized = true;
    return true;
}

void BSP_Motor_SetDuty(uint8_t duty_percent)
{
    uint32_t pulse;

    /*
     * PWM未成功初始化或请求0%时，都执行完整安全停机。
     */
    if ((!motor_initialized) || (duty_percent == 0U))
    {
        BSP_Motor_Stop();
        return;
    }

    /* 防止百分比超过合法范围并导致比较值计算异常。 */
    if (duty_percent > 100U)
    {
        duty_percent = 100U;
    }

    pulse = Motor_DutyToPulse(duty_percent);

    /*
     * 先保持STBY为低，再准备方向和PWM，
     * 最后才使能输出，避免切换过程中短暂错误驱动。
     *
     * AIN1高、AIN2低固定为M4的软件正转方向。
     */
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port,
                      MOTOR_STBY_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port,
                      MOTOR_AIN1_Pin,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port,
                      MOTOR_AIN2_Pin,
                      GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim4,
                          TIM_CHANNEL_1,
                          pulse);

    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port,
                      MOTOR_STBY_Pin,
                      GPIO_PIN_SET);
}

void BSP_Motor_Stop(void)
{
    /*
     * 先拉低STBY关闭驱动输出，再清零PWM和方向信号，
     * 使TB6612进入确定的高阻停止状态。
     */
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port,
                      MOTOR_STBY_Pin,
                      GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim4,
                          TIM_CHANNEL_1,
                          0U);

    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port,
                      MOTOR_AIN1_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port,
                      MOTOR_AIN2_Pin,
                      GPIO_PIN_RESET);
}
