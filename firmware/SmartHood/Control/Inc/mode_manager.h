/**
 * @file mode_manager.h
 * @brief SmartHood运行许可、工作模式、手动挡位和故障状态机接口。
 */

#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdbool.h>

#include "bsp_key.h"

/**
 * @brief 系统运行许可状态。
 */
typedef enum
{
    MODE_RUN_STANDBY = 0, /**< 待机，禁止电机输出。 */
    MODE_RUN_RUNNING      /**< 取得系统运行许可。 */
} ModeRunState_t;

/**
 * @brief SmartHood工作模式。
 */
typedef enum
{
    MODE_AUTO = 0, /**< 自动模式，M7阶段保持停止。 */
    MODE_MANUAL,   /**< 手动模式，允许低档或高档闭环。 */
    MODE_BACKFLOW  /**< 防回流模式，M7阶段保持停止。 */
} ModeType_t;

/**
 * @brief 手动模式预选挡位。
 */
typedef enum
{
    MODE_MANUAL_LOW = 0, /**< 手动低档预选。 */
    MODE_MANUAL_HIGH     /**< 手动高档预选。 */
} ModeManualLevel_t;

/**
 * @brief 模式管理器能够锁存的故障。
 */
typedef enum
{
    MODE_FAULT_NONE = 0,       /**< 当前没有故障。 */
    MODE_FAULT_ENCODER_TIMEOUT /**< 编码器连续无有效反馈。 */
} ModeFault_t;

/**
 * @brief 处理一个按键事件后的结果。
 */
typedef enum
{
    MODE_RESULT_NONE = 0,    /**< 没有事件或状态没有变化。 */
    MODE_RESULT_CHANGED,     /**< 运行状态、模式或挡位已改变。 */
    MODE_RESULT_IGNORED_MODE, /**< 当前模式不接受该事件。 */
    MODE_RESULT_IGNORED_FAULT, /**< 故障锁存期间忽略该事件。 */
    MODE_RESULT_FAULT_CLEARED /**< 长按已清除故障并恢复安全初始状态。 */
} ModeResult_t;

/**
 * @brief ModeManager向MotorTask提出的电机请求。
 */
typedef enum
{
    MODE_MOTOR_STOP = 0, /**< 保持安全停止。 */
    MODE_MOTOR_LOW,      /**< 运行手动低档闭环。 */
    MODE_MOTOR_HIGH,     /**< 运行手动高档闭环。 */
    MODE_MOTOR_FAULT     /**< 故障锁存并停止电机。 */
} ModeMotorRequest_t;

/**
 * @brief 模式管理器的完整状态快照。
 */
typedef struct
{
    ModeRunState_t run_state;       /**< 当前运行许可。 */
    ModeType_t mode;                /**< 当前工作模式。 */
    ModeManualLevel_t manual_level; /**< 手动挡位预选。 */
    ModeFault_t fault;              /**< 当前锁存故障。 */
} ModeManager_t;

/**
 * @brief 初始化为STANDBY、AUTO、LOW预选和无故障。
 * @param manager 模式管理器上下文。
 */
void ModeManager_Init(ModeManager_t *manager);

/**
 * @brief 根据一个按键事件更新模式状态。
 * @param manager 模式管理器上下文。
 * @param event 已完成消抖和单双击仲裁的按键事件。
 * @return 状态变化、忽略原因或故障清除结果。
 */
ModeResult_t ModeManager_HandleEvent(ModeManager_t *manager,
                                     BSP_KeyEvent_t event);

/**
 * @brief 设置或清除模式管理器故障。
 * @param manager 模式管理器上下文。
 * @param fault 新的故障状态；传入NONE会完整恢复为
 *              STANDBY、AUTO、LOW预选和无故障。
 */
void ModeManager_SetFault(ModeManager_t *manager,
                          ModeFault_t fault);

/**
 * @brief 把运行许可、模式、挡位和故障映射为电机请求。
 * @param manager 模式管理器上下文。
 * @return STOP、LOW、HIGH或FAULT请求；非法状态安全返回STOP。
 */
ModeMotorRequest_t ModeManager_GetMotorRequest(
    const ModeManager_t *manager);

#endif /* MODE_MANAGER_H */
