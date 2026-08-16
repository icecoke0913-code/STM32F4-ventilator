/**
 * @file mode_manager_selftest.c
 * @brief 使用固定按键事件验证模式管理器的完整状态转换表。
 */

#include "mode_manager_selftest.h"

#include <stddef.h>

#include "mode_manager.h"

bool ModeManager_RunSelfTests(void)
{
    ModeManager_t manager;
    ModeManager_t snapshot;

    /* 所有公共接口都必须安全处理空上下文。 */
    ModeManager_Init(NULL);
    if (ModeManager_HandleEvent(NULL, BSP_KEY_EVENT_SHORT) !=
        MODE_RESULT_NONE)
    {
        return false;
    }
    ModeManager_SetFault(NULL, MODE_FAULT_ENCODER_TIMEOUT);
    if (ModeManager_GetMotorRequest(NULL) != MODE_MOTOR_STOP)
    {
        return false;
    }

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

    /* NONE事件不得改变有效上下文中的任何状态。 */
    snapshot = manager;
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_NONE) !=
         MODE_RESULT_NONE) ||
        (manager.run_state != snapshot.run_state) ||
        (manager.mode != snapshot.mode) ||
        (manager.manual_level != snapshot.manual_level) ||
        (manager.fault != snapshot.fault))
    {
        return false;
    }

    /* 独立验证STANDBY和MANUAL组合下短按进入BACKFLOW。 */
    ModeManager_Init(&manager);
    manager.mode = MODE_MANUAL;
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
         MODE_RESULT_CHANGED) ||
        (manager.run_state != MODE_RUN_STANDBY) ||
        (manager.mode != MODE_BACKFLOW))
    {
        return false;
    }

    /* 独立验证STANDBY和BACKFLOW组合下短按返回AUTO。 */
    ModeManager_Init(&manager);
    manager.mode = MODE_BACKFLOW;
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
         MODE_RESULT_CHANGED) ||
        (manager.run_state != MODE_RUN_STANDBY) ||
        (manager.mode != MODE_AUTO))
    {
        return false;
    }

    /* 独立验证RUNNING和AUTO组合下短按进入可输出低档的MANUAL。 */
    ModeManager_Init(&manager);
    manager.run_state = MODE_RUN_RUNNING;
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
         MODE_RESULT_CHANGED) ||
        (manager.run_state != MODE_RUN_RUNNING) ||
        (manager.mode != MODE_MANUAL) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_LOW))
    {
        return false;
    }

    /* 非法运行状态、模式和手动挡位都必须退化为安全请求。 */
    ModeManager_Init(&manager);
    manager.run_state = (ModeRunState_t)2;
    manager.mode = MODE_MANUAL;
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP)
    {
        return false;
    }
    manager.run_state = MODE_RUN_RUNNING;
    manager.mode = (ModeType_t)3;
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP)
    {
        return false;
    }
    manager.mode = MODE_MANUAL;
    manager.manual_level = (ModeManualLevel_t)2;
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP)
    {
        return false;
    }

    /* API清除故障也必须完整安全复位，禁止恢复故障前运行组合。 */
    manager.run_state = MODE_RUN_RUNNING;
    manager.mode = MODE_MANUAL;
    manager.manual_level = MODE_MANUAL_HIGH;
    ModeManager_SetFault(&manager, MODE_FAULT_ENCODER_TIMEOUT);
    if (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_FAULT)
    {
        return false;
    }
    ModeManager_SetFault(&manager, MODE_FAULT_NONE);
    if ((manager.run_state != MODE_RUN_STANDBY) ||
        (manager.mode != MODE_AUTO) ||
        (manager.manual_level != MODE_MANUAL_LOW) ||
        (manager.fault != MODE_FAULT_NONE) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* 后续按完整事件序列复核各状态间的连续转换。 */
    ModeManager_Init(&manager);

    /* AUTO不接受双击，模式和低档预选都必须保持不变。 */
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
         MODE_RESULT_IGNORED_MODE) ||
        (manager.mode != MODE_AUTO) ||
        (manager.manual_level != MODE_MANUAL_LOW))
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

    /* MANUAL连续双击必须先切到HIGH，再切回LOW。 */
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
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.manual_level != MODE_MANUAL_LOW) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* 再切到HIGH后取得运行许可，保留高档电机请求映射检查。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.run_state != MODE_RUN_RUNNING) ||
        (manager.manual_level != MODE_MANUAL_HIGH) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_HIGH))
    {
        return false;
    }

    /* RUNNING和MANUAL组合下双击回LOW，电机请求应同步降为低档。 */
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
         MODE_RESULT_CHANGED) ||
        (manager.manual_level != MODE_MANUAL_LOW) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_LOW))
    {
        return false;
    }

    /* RUNNING再次长按必须回到STANDBY并立即停止电机。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.run_state != MODE_RUN_STANDBY) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP))
    {
        return false;
    }

    /* 再次长按恢复运行许可，为运行中的模式循环测试建立条件。 */
    if (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_LONG) !=
        MODE_RESULT_CHANGED)
    {
        return false;
    }
    if ((manager.run_state != MODE_RUN_RUNNING) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_LOW))
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

    /* BACKFLOW不接受双击，且短按必须继续循环回AUTO。 */
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
         MODE_RESULT_IGNORED_MODE) ||
        (manager.mode != MODE_BACKFLOW) ||
        (manager.manual_level != MODE_MANUAL_LOW))
    {
        return false;
    }
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
         MODE_RESULT_CHANGED) ||
        (manager.mode != MODE_AUTO))
    {
        return false;
    }

    /* 即使已有运行许可，AUTO仍必须映射为STOP并拒绝双击。 */
    if ((manager.run_state != MODE_RUN_RUNNING) ||
        (ModeManager_GetMotorRequest(&manager) != MODE_MOTOR_STOP) ||
        (ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
         MODE_RESULT_IGNORED_MODE) ||
        (manager.mode != MODE_AUTO))
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

    /* 故障锁存时SHORT必须返回忽略，并保持完整状态快照不变。 */
    snapshot = manager;
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_SHORT) !=
         MODE_RESULT_IGNORED_FAULT) ||
        (manager.run_state != snapshot.run_state) ||
        (manager.mode != snapshot.mode) ||
        (manager.manual_level != snapshot.manual_level) ||
        (manager.fault != snapshot.fault))
    {
        return false;
    }

    /* 故障锁存时DOUBLE同样不得改变任何状态字段。 */
    snapshot = manager;
    if ((ModeManager_HandleEvent(&manager, BSP_KEY_EVENT_DOUBLE) !=
         MODE_RESULT_IGNORED_FAULT) ||
        (manager.run_state != snapshot.run_state) ||
        (manager.mode != snapshot.mode) ||
        (manager.manual_level != snapshot.manual_level) ||
        (manager.fault != snapshot.fault))
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
