/**
 * @file mode_manager_selftest.c
 * @brief 使用固定按键事件验证模式管理器的完整状态转换表。
 */

#include "mode_manager_selftest.h"

#include "mode_manager.h"

bool ModeManager_RunSelfTests(void)
{
    ModeManager_t manager;

    /* 上电必须处于安全待机状态，并预选自动模式和手动低档。 */
    ModeManager_Init(&manager);
    if ((manager.run_state != MODE_RUN_STANDBY) ||
        (manager.mode != MODE_AUTO) ||
        (manager.manual_level != MODE_MANUAL_LOW) ||
        (manager.fault != MODE_FAULT_NONE) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* 待机短按切换AUTO到MANUAL，但未取得运行许可时仍禁止电机。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.mode != MODE_MANUAL) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* MANUAL双击切换到HIGH预选，待机状态仍不产生电机输出请求。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.manual_level != MODE_MANUAL_HIGH) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* 长按取得运行许可，MANUAL和HIGH组合应产生高档电机请求。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.run_state != MODE_RUN_RUNNING) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_HIGH))
    {
        return false;
    }

    /* 运行中短按进入BACKFLOW；M7阶段该模式必须保持电机停止。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.mode != MODE_BACKFLOW) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* BACKFLOW不接受挡位切换，双击不得改变模式管理器状态。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
        MODE_RESULT_IGNORED_MODE)
    {
        return false;
    }

    /* 编码器超时故障优先级最高，任何运行组合都映射为FAULT请求。 */
    ModeManager_SetFault(&manager, MODE_FAULT_ENCODER_TIMEOUT);
    if ((manager.fault != MODE_FAULT_ENCODER_TIMEOUT) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_FAULT))
    {
        return false;
    }

    /* 故障锁存时忽略短按和双击，避免改变模式或手动挡位。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
        MODE_RESULT_IGNORED_FAULT)
    {
        return false;
    }
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
        MODE_RESULT_IGNORED_FAULT)
    {
        return false;
    }

    /* 故障中长按执行人工清除，并恢复全部安全初始状态。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) !=
        MODE_RESULT_FAULT_CLEARED)
    {
        return false;
    }

    return (manager.run_state == MODE_RUN_STANDBY) &&
           (manager.mode == MODE_AUTO) &&
           (manager.manual_level == MODE_MANUAL_LOW) &&
           (manager.fault == MODE_FAULT_NONE) &&
           (ModeManager_GetMotorRequest(&manager) == MODE_MOTOR_STOP);
}
