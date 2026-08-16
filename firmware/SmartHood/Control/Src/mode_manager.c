/**
 * @file mode_manager.c
 * @brief SmartHood模式转换和电机请求映射。
 */

#include "mode_manager.h"

#include <stddef.h>

void ModeManager_Init(ModeManager_t *manager)
{
    /* 空上下文不执行任何访问，便于调用方安全地重复初始化。 */
    if (manager == NULL)
    {
        return;
    }

    /* 所有状态统一恢复到上电安全初值，确保电机请求为停止。 */
    manager->run_state = MODE_RUN_STANDBY;
    manager->mode = MODE_AUTO;
    manager->manual_level = MODE_MANUAL_LOW;
    manager->fault = MODE_FAULT_NONE;
}

ModeResult_t ModeManager_HandleEvent(ModeManager_t *manager,
                                     BSP_KeyEvent_t event)
{
    /* 无上下文或无按键事件时，状态机不产生任何变化。 */
    if ((manager == NULL) || (event == BSP_KEY_EVENT_NONE))
    {
        return MODE_RESULT_NONE;
    }

    /* 故障优先级最高：仅允许长按清故障，其余事件全部屏蔽。 */
    if (manager->fault != MODE_FAULT_NONE)
    {
        if (event == BSP_KEY_EVENT_LONG)
        {
            ModeManager_Init(manager);
            return MODE_RESULT_FAULT_CLEARED;
        }

        return MODE_RESULT_IGNORED_FAULT;
    }

    /* 无故障时，长按在待机与运行许可之间切换。 */
    if (event == BSP_KEY_EVENT_LONG)
    {
        manager->run_state =
            (manager->run_state == MODE_RUN_STANDBY) ?
            MODE_RUN_RUNNING : MODE_RUN_STANDBY;
        return MODE_RESULT_CHANGED;
    }

    /* 短按按照AUTO、MANUAL、BACKFLOW的固定顺序循环。 */
    if (event == BSP_KEY_EVENT_SHORT)
    {
        if (manager->mode == MODE_AUTO)
        {
            manager->mode = MODE_MANUAL;
        }
        else if (manager->mode == MODE_MANUAL)
        {
            manager->mode = MODE_BACKFLOW;
        }
        else
        {
            manager->mode = MODE_AUTO;
        }

        return MODE_RESULT_CHANGED;
    }

    /* 双击只在MANUAL模式下切换预选挡位。 */
    if (event == BSP_KEY_EVENT_DOUBLE)
    {
        if (manager->mode != MODE_MANUAL)
        {
            return MODE_RESULT_IGNORED_MODE;
        }

        manager->manual_level =
            (manager->manual_level == MODE_MANUAL_LOW) ?
            MODE_MANUAL_HIGH : MODE_MANUAL_LOW;
        return MODE_RESULT_CHANGED;
    }

    /* 未定义事件不改变状态。 */
    return MODE_RESULT_NONE;
}

void ModeManager_SetFault(ModeManager_t *manager, ModeFault_t fault)
{
    /* 空上下文无需锁存。 */
    if (manager == NULL)
    {
        return;
    }

    /* 清故障时完整安全复位，禁止恢复故障前的运行组合。 */
    if (fault == MODE_FAULT_NONE)
    {
        ModeManager_Init(manager);
        return;
    }

    /* 非NONE故障直接锁存，由电机请求映射为故障停机。 */
    manager->fault = fault;
}

ModeMotorRequest_t ModeManager_GetMotorRequest(const ModeManager_t *manager)
{
    /* 空上下文必须退化为安全停止。 */
    if (manager == NULL)
    {
        return MODE_MOTOR_STOP;
    }

    /* 任何锁存故障都优先映射为故障停机请求。 */
    if (manager->fault != MODE_FAULT_NONE)
    {
        return MODE_MOTOR_FAULT;
    }

    /* 未取得运行许可或不在手动模式时均不得驱动电机。 */
    if ((manager->run_state != MODE_RUN_RUNNING) ||
        (manager->mode != MODE_MANUAL))
    {
        return MODE_MOTOR_STOP;
    }

    /* 仅显式认可LOW和HIGH，非法挡位必须退化为安全停止。 */
    if (manager->manual_level == MODE_MANUAL_LOW)
    {
        return MODE_MOTOR_LOW;
    }
    if (manager->manual_level == MODE_MANUAL_HIGH)
    {
        return MODE_MOTOR_HIGH;
    }

    return MODE_MOTOR_STOP;
}
