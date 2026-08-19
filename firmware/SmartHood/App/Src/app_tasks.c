/**
 * @file app_tasks.c
 * @brief SmartHood显示自检、按键事件、电机挡位和DHT11周期任务实现。
 */

#include "app_tasks.h"

#include <stdint.h>
#include <stdbool.h>

#include "bsp_dht11.h"
#include "bsp_key.h"
#include "bsp_motor.h"
#include "bsp_st7735s.h"
#include "bsp_encoder.h"

#include "control_pi.h"
#include "auto_policy.h"
#include "mode_manager.h"
#include "control_pi_selftest.h"
#include "bsp_key_selftest.h"
#include "mode_manager_selftest.h"
#include "auto_policy_selftest.h"

#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

/** 默认任务快速循环周期，用于可靠识别PA0短按。 */
#define APP_MAIN_LOOP_PERIOD_MS       20U

/** PA1翻转和心跳日志继续保持原有1秒周期。 */
#define APP_HEARTBEAT_PERIOD_MS     1000U

/** 电机闭环控制周期，同时也是编码器增量采样周期。 */
#define APP_CONTROL_PERIOD_MS              50U

/** 每10个控制周期输出一次日志，即约500ms。 */
#define APP_CONTROL_LOG_SAMPLE_COUNT       10U

/** 进入低档或高档前使用30% PWM软启动。 */
#define APP_MOTOR_START_DUTY_PERCENT       30U

/** 软启动持续时间，结束后才进入PI闭环。 */
#define APP_MOTOR_START_TIME_MS            300U

/** 低档和高档每50ms的目标编码器计数绝对值。 */
#define APP_MOTOR_LOW_TARGET_COUNT         130L
#define APP_MOTOR_HIGH_TARGET_COUNT        195L

/** 两个挡位的基础前馈PWM百分比。 */
#define APP_MOTOR_LOW_FEEDFORWARD          50L
#define APP_MOTOR_HIGH_FEEDFORWARD         70L

/** PI控制器允许输出的PWM百分比范围。 */
#define APP_MOTOR_OUTPUT_MIN               30L
#define APP_MOTOR_OUTPUT_MAX               90L

/** 实际计数不超过1时，视为编码器没有有效反馈。 */
#define APP_ENCODER_ZERO_THRESHOLD         1L

/** 连续10个周期无反馈，即500ms后锁存故障。 */
#define APP_ENCODER_FAULT_SAMPLE_COUNT     10U

/** Q8定点PI初始参数：Kp=0.25，Ki=0.015625。 */
#define APP_CONTROL_KP_Q8                  64L
#define APP_CONTROL_KI_Q8                  4L

/** 积分累计值的上下限。 */
#define APP_CONTROL_INTEGRAL_MIN           (-2048L)
#define APP_CONTROL_INTEGRAL_MAX           2048L

/**
 * @brief 电机控制任务的内部状态。
 */
typedef enum
{
    APP_MOTOR_STATE_STOP = 0,  /**< 电机停止，等待按键事件。 */
    APP_MOTOR_STATE_LOW_START, /**< 低档30%软启动阶段。 */
    APP_MOTOR_STATE_LOW_PI,    /**< 低档PI闭环阶段。 */
    APP_MOTOR_STATE_HIGH_START,/**< 高档30%软启动阶段。 */
    APP_MOTOR_STATE_HIGH_PI,   /**< 高档PI闭环阶段。 */
    APP_MOTOR_STATE_FAULT      /**< 编码器无反馈故障锁存。 */
} App_MotorState_t;

/**
 * @brief 模式电机请求映射出的纯决策结果。
 */
typedef struct
{
    App_MotorState_t state; /**< 请求对应的电机内部状态。 */
    uint8_t duty_percent;   /**< 请求对应的初始PWM占空比。 */
    bool stop_motor;        /**< true表示必须立即停止电机。 */
} App_MotorRequestAction_t;

static App_MotorRequestAction_t App_MotorRequestToAction(
    ModeMotorRequest_t request);

/**
 * @brief M6 PI 控制器临时板端自检开关。
 *
 * 设置为1时运行自检；完成初次验证后将改为0。
 */
#define APP_CONTROL_PI_SELF_TEST_ENABLED 0U

/**
 * @brief M7按键状态机临时板端自检开关。
 *
 * 设置为1时执行固定时间序列自检；
 * 验收完成后必须恢复为0，避免正式启动时重复运行。
 */
#define APP_M7_SELF_TEST_ENABLED 0U

/** M8A临时板端自检开关，硬件验收完成后必须恢复为0U。 */
#define APP_M8A_SELF_TEST_ENABLED 0U

/** 按键事件队列最多保存的事件数量。 */
#define APP_KEY_EVENT_QUEUE_LENGTH 4U

/** 按键事件队列句柄，创建成功前保持为NULL。 */
static osMessageQueueId_t app_key_event_queue = NULL;

/** 保护DHT11快照读写的互斥量。 */
static osMutexId_t app_sensor_mutex = NULL;

/** 只保存最近一次校验成功的DHT11数据。 */
static AutoPolicySnapshot_t app_sensor_snapshot = {0};

/**
 * @brief 创建默认任务到电机任务之间的按键事件队列。
 *
 * @return 创建成功返回true，否则返回false。
 */
bool App_MotorControl_Init(void)
{
    app_key_event_queue = osMessageQueueNew(
        APP_KEY_EVENT_QUEUE_LENGTH,
        sizeof(BSP_KeyEvent_t),
        NULL);

    return app_key_event_queue != NULL;
}

bool App_SensorState_Init(void)
{
    app_sensor_snapshot.temperature_x10 = 0;
    app_sensor_snapshot.humidity_x10 = 0U;
    app_sensor_snapshot.valid = false;
    app_sensor_snapshot.updated_tick = 0U;
    app_sensor_mutex = osMutexNew(NULL);

    return app_sensor_mutex != NULL;
}

/**
 * @brief 发布一帧已经通过DHT11校验的数据。
 * @param data DHT11有效数据指针。
 * @param now_tick 发布时的RTOS Tick。
 */
static void App_SensorState_Publish(const DHT11_Data_t *data,
                                    uint32_t now_tick)
{
    if ((app_sensor_mutex == NULL) || (data == NULL))
    {
        return;
    }

    if (osMutexAcquire(app_sensor_mutex, 0U) == osOK)
    {
        app_sensor_snapshot.temperature_x10 = data->temperature_x10;
        app_sensor_snapshot.humidity_x10 = data->humidity_x10;
        app_sensor_snapshot.valid = true;
        app_sensor_snapshot.updated_tick = now_tick;
        (void)osMutexRelease(app_sensor_mutex);
    }
}

/**
 * @brief 复制当前DHT11快照，避免任务间读取半帧数据。
 * @param snapshot 输出快照指针。
 * @return 成功复制返回true，互斥量不可用或获取失败返回false。
 */
static bool App_SensorState_Read(AutoPolicySnapshot_t *snapshot)
{
    if ((app_sensor_mutex == NULL) || (snapshot == NULL))
    {
        return false;
    }

    if (osMutexAcquire(app_sensor_mutex, 0U) != osOK)
    {
        return false;
    }

    *snapshot = app_sensor_snapshot;
    (void)osMutexRelease(app_sensor_mutex);

    return true;
}

/**
 * @brief 向电机任务发送一个有效按键事件。
 *
 * 使用0超时，队列已满时立即返回，不阻塞默认任务的心跳和按键扫描。
 *
 * @param event 待发送的按键事件。
 *
 * @return 有效事件成功进入队列返回true，否则返回false。
 */
static bool App_PostKeyEvent(BSP_KeyEvent_t event)
{
    if ((app_key_event_queue == NULL) ||
        (event == BSP_KEY_EVENT_NONE))
    {
        return false;
    }

    return osMessageQueuePut(app_key_event_queue,
                             &event,
                             0U,
                             0U) == osOK;
}

/**
 * @brief 将电机内部状态转换为串口日志文本。
 *
 * @param state 当前电机状态。
 *
 * @return 与状态对应的固定字符串。
 */
static const char *App_MotorStateText(App_MotorState_t state)
{
    switch (state)
    {
        case APP_MOTOR_STATE_LOW_START:
            return "LOW_START";

        case APP_MOTOR_STATE_LOW_PI:
            return "LOW";

        case APP_MOTOR_STATE_HIGH_START:
            return "HIGH_START";

        case APP_MOTOR_STATE_HIGH_PI:
            return "HIGH";

        case APP_MOTOR_STATE_FAULT:
            return "FAULT";

        case APP_MOTOR_STATE_STOP:
        default:
            return "STOP";
    }
}

/**
 * @brief 将模式管理器电机请求转换为串口日志文本。
 * @param request AUTO策略或模式管理器生成的电机请求。
 * @return 与请求对应的固定字符串。
 */
static const char *App_MotorRequestText(ModeMotorRequest_t request)
{
    switch (request)
    {
        case MODE_MOTOR_LOW:
            return "LOW";

        case MODE_MOTOR_HIGH:
            return "HIGH";

        case MODE_MOTOR_FAULT:
            return "FAULT";

        case MODE_MOTOR_STOP:
        default:
            return "STOP";
    }
}

/**
 * @brief 将运行许可状态转换为串口日志文本。
 * @param state 当前运行许可状态。
 * @return 与状态对应的固定字符串，非法值返回INVALID。
 */
static const char *App_ModeRunText(ModeRunState_t state)
{
    switch (state)
    {
        case MODE_RUN_STANDBY:
            return "STANDBY";

        case MODE_RUN_RUNNING:
            return "RUNNING";

        default:
            return "INVALID";
    }
}

/**
 * @brief 将工作模式转换为串口日志文本。
 * @param mode 当前工作模式。
 * @return 与模式对应的固定字符串，非法值返回INVALID。
 */
static const char *App_ModeTypeText(ModeType_t mode)
{
    switch (mode)
    {
        case MODE_AUTO:
            return "AUTO";

        case MODE_MANUAL:
            return "MANUAL";

        case MODE_BACKFLOW:
            return "BACKFLOW";

        default:
            return "INVALID";
    }
}

/**
 * @brief 将手动挡位预选转换为串口日志文本。
 * @param level 当前手动挡位预选。
 * @return 与挡位对应的固定字符串，非法值返回INVALID。
 */
static const char *App_ModeLevelText(ModeManualLevel_t level)
{
    switch (level)
    {
        case MODE_MANUAL_LOW:
            return "LOW";

        case MODE_MANUAL_HIGH:
            return "HIGH";

        default:
            return "INVALID";
    }
}

/**
 * @brief 将模式故障转换为串口日志文本。
 * @param fault 当前模式故障。
 * @return 与故障对应的固定字符串，非法值返回INVALID。
 */
static const char *App_ModeFaultText(ModeFault_t fault)
{
    switch (fault)
    {
        case MODE_FAULT_NONE:
            return "NONE";

        case MODE_FAULT_ENCODER_TIMEOUT:
            return "ENCODER_TIMEOUT";

        default:
            return "INVALID";
    }
}

/**
 * @brief 将模式事件处理结果转换为串口日志文本。
 * @param result 最近一次事件处理结果。
 * @return 与结果对应的固定字符串，非法值返回INVALID。
 */
static const char *App_ModeResultText(ModeResult_t result)
{
    switch (result)
    {
        case MODE_RESULT_NONE:
            return "NONE";

        case MODE_RESULT_CHANGED:
            return "CHANGED";

        case MODE_RESULT_IGNORED_MODE:
            return "IGNORED_MODE";

        case MODE_RESULT_IGNORED_FAULT:
            return "IGNORED_FAULT";

        case MODE_RESULT_FAULT_CLEARED:
            return "FAULT_CLEARED";

        default:
            return "INVALID";
    }
}

/**
 * @brief 输出模式管理器的完整状态和事件处理结果。
 * @param manager 当前模式管理器上下文。
 * @param result 最近一次事件处理结果。
 */
static void App_LogModeState(const ModeManager_t *manager,
                             ModeResult_t result)
{
    DebugLog_Printf(
        "mode run=%s mode=%s level=%s fault=%s result=%s\r\n",
        App_ModeRunText(manager->run_state),
        App_ModeTypeText(manager->mode),
        App_ModeLevelText(manager->manual_level),
        App_ModeFaultText(manager->fault),
        App_ModeResultText(result));
}

#if APP_M8A_SELF_TEST_ENABLED
/**
 * @brief 组合验证M8A纯策略和模式请求优先级。
 * @return 全部自检通过返回true，否则返回false。
 */
static bool App_M8A_RunSelfTests(void)
{
    ModeManager_t manager;

    if (!AutoPolicy_RunSelfTests() || !ModeManager_RunSelfTests())
    {
        return false;
    }

    ModeManager_Init(&manager);
    manager.run_state = MODE_RUN_RUNNING;

    return ModeManager_GetMotorRequest(&manager,
                                       MODE_MOTOR_LOW) == MODE_MOTOR_LOW;
}
#endif

/**
 * @brief 验证模式电机请求到内部动作的完整安全映射。
 * @return 全部请求及非法请求映射正确时返回true。
 */
#if APP_M7_SELF_TEST_ENABLED
static bool App_MotorModeIntegration_RunSelfTests(void)
{
    App_MotorRequestAction_t action;

    action = App_MotorRequestToAction(MODE_MOTOR_STOP);
    if ((action.state != APP_MOTOR_STATE_STOP) ||
        (action.duty_percent != 0U) ||
        !action.stop_motor)
    {
        return false;
    }

    action = App_MotorRequestToAction(MODE_MOTOR_LOW);
    if ((action.state != APP_MOTOR_STATE_LOW_START) ||
        (action.duty_percent != 30U) ||
        action.stop_motor)
    {
        return false;
    }

    action = App_MotorRequestToAction(MODE_MOTOR_HIGH);
    if ((action.state != APP_MOTOR_STATE_HIGH_START) ||
        (action.duty_percent != 30U) ||
        action.stop_motor)
    {
        return false;
    }

    action = App_MotorRequestToAction(MODE_MOTOR_FAULT);
    if ((action.state != APP_MOTOR_STATE_FAULT) ||
        (action.duty_percent != 0U) ||
        !action.stop_motor)
    {
        return false;
    }

    action = App_MotorRequestToAction((ModeMotorRequest_t)99);
    if ((action.state != APP_MOTOR_STATE_STOP) ||
        (action.duty_percent != 0U) ||
        !action.stop_motor)
    {
        return false;
    }

    return true;
}
#endif

/**
 * @brief 将模式电机请求转换为不访问硬件的纯决策结果。
 * @param request ModeManager提出的电机请求。
 * @return 请求对应的状态、初始占空比和立即停机标志。
 *
 * 非法请求与STOP使用相同的安全动作，禁止产生电机输出。
 */
static App_MotorRequestAction_t App_MotorRequestToAction(
    ModeMotorRequest_t request)
{
    App_MotorRequestAction_t action;

    action.state = APP_MOTOR_STATE_STOP;
    action.duty_percent = 0U;
    action.stop_motor = true;

    switch (request)
    {
        case MODE_MOTOR_LOW:
            action.state = APP_MOTOR_STATE_LOW_START;
            action.duty_percent = APP_MOTOR_START_DUTY_PERCENT;
            action.stop_motor = false;
            break;

        case MODE_MOTOR_HIGH:
            action.state = APP_MOTOR_STATE_HIGH_START;
            action.duty_percent = APP_MOTOR_START_DUTY_PERCENT;
            action.stop_motor = false;
            break;

        case MODE_MOTOR_FAULT:
            action.state = APP_MOTOR_STATE_FAULT;
            break;

        case MODE_MOTOR_STOP:
        default:
            break;
    }

    return action;
}

/**
 * @brief 根据当前状态选择每50ms的目标编码器计数。
 *
 * 软启动阶段也返回对应挡位目标，便于日志提前显示最终目标；
 * STOP和FAULT不允许控制输出，因此目标返回0。
 *
 * @param state 当前电机状态。
 *
 * @return 当前状态对应的目标编码器计数绝对值。
 */
static int32_t App_MotorTargetCount(App_MotorState_t state)
{
    if ((state == APP_MOTOR_STATE_LOW_START) ||
        (state == APP_MOTOR_STATE_LOW_PI))
    {
        return APP_MOTOR_LOW_TARGET_COUNT;
    }

    if ((state == APP_MOTOR_STATE_HIGH_START) ||
        (state == APP_MOTOR_STATE_HIGH_PI))
    {
        return APP_MOTOR_HIGH_TARGET_COUNT;
    }

    return 0L;
}

/**
 * @brief 执行一次ST7735S启动自检。
 * @return 初始化和全部绘图操作成功时返回true，任一步失败返回false。
 *
 * 自检依次显示纯色画面，再绘制边框、四色角标和文本，
 * 用于同时检查SPI通信、方向、RGB顺序、坐标范围和字体绘制。
 */
static bool App_RunDisplayTest(void)
{
    if (!BSP_ST7735S_Init())
    {
        return false;
    }

    /* 纯色切换用于检查RGB565颜色通道和全屏写入。 */
    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_RED))
    {
        return false;
    }
    osDelay(500U);

    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_GREEN))
    {
        return false;
    }
    osDelay(500U);

    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_BLUE))
    {
        return false;
    }
    osDelay(500U);

    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_WHITE))
    {
        return false;
    }
    osDelay(500U);

    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_BLACK))
    {
        return false;
    }

    /* 单像素白框用于确认四条边界和坐标偏移。 */
    if (!BSP_ST7735S_FillRect(
            0U, 0U, 128U, 1U,
            ST7735S_COLOR_WHITE) ||
        !BSP_ST7735S_FillRect(
            0U, 159U, 128U, 1U,
            ST7735S_COLOR_WHITE) ||
        !BSP_ST7735S_FillRect(
            0U, 0U, 1U, 160U,
            ST7735S_COLOR_WHITE) ||
        !BSP_ST7735S_FillRect(
            127U, 0U, 1U, 160U,
            ST7735S_COLOR_WHITE))
    {
        return false;
    }

    /* 四色角标用于同时核对方向、角点位置和颜色顺序。 */
    if (!BSP_ST7735S_FillRect(
            2U, 2U, 8U, 8U,
            ST7735S_COLOR_RED) ||
        !BSP_ST7735S_FillRect(
            118U, 2U, 8U, 8U,
            ST7735S_COLOR_GREEN) ||
        !BSP_ST7735S_FillRect(
            2U, 150U, 8U, 8U,
            ST7735S_COLOR_BLUE) ||
        !BSP_ST7735S_FillRect(
            118U, 150U, 8U, 8U,
            ST7735S_COLOR_YELLOW))
    {
        return false;
    }

    /* 英文与数字文本用于检查5×7字模和字符串绘制。 */
    if (!BSP_ST7735S_DrawString(
            12U,
            50U,
            "SmartHood",
            ST7735S_COLOR_WHITE,
            ST7735S_COLOR_BLACK) ||
        !BSP_ST7735S_DrawString(
            12U,
            70U,
            "ST7735S OK",
            ST7735S_COLOR_CYAN,
            ST7735S_COLOR_BLACK) ||
        !BSP_ST7735S_DrawString(
            12U,
            90U,
            "128x160",
            ST7735S_COLOR_YELLOW,
            ST7735S_COLOR_BLACK))
    {
        return false;
    }

    return true;
}

/**
 * @brief 运行显示自检、心跳，并把PA0按键事件发送给电机任务。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 快速循环每20ms采样PA0并驱动按键状态机，识别出的有效事件通过
 * 消息队列发送。PA1和心跳继续使用独立的1秒节拍。
 */
void App_DefaultTask(void *argument)
{
    BSP_Key_t key;
    uint32_t heartbeat = 0U;
    uint32_t heartbeat_tick;
    uint32_t now_tick;
    bool initial_pressed;

    (void)argument;

    DebugLog_Printf("\r\nSmartHood M1 start\r\n");

    if (App_RunDisplayTest())
    {
        DebugLog_Printf("ST7735S init and test OK\r\n");
    }
    else
    {
        /* 关闭背光，使显示初始化或绘图失败具有明确的可见状态。 */
        BSP_ST7735S_SetBacklight(false);
        DebugLog_Printf("ST7735S init or draw failed\r\n");
    }

    /* 读取实际初始电平，上电按住时由按键状态机等待稳定释放。 */
    now_tick = HAL_GetTick();
    initial_pressed =
        HAL_GPIO_ReadPin(USER_KEY_GPIO_Port,
                         USER_KEY_Pin) == GPIO_PIN_SET;
    BSP_Key_Init(&key, initial_pressed, now_tick);
    heartbeat_tick = now_tick;

    for (;;)
    {
        BSP_KeyEvent_t event;
        bool raw_pressed;

        now_tick = HAL_GetTick();
        raw_pressed =
            HAL_GPIO_ReadPin(USER_KEY_GPIO_Port,
                             USER_KEY_Pin) == GPIO_PIN_SET;
        event = BSP_Key_Process(&key, raw_pressed, now_tick);

        /* 队列发送失败时记录日志，但不阻塞心跳和其他任务。 */
        if ((event != BSP_KEY_EVENT_NONE) &&
            !App_PostKeyEvent(event))
        {
            DebugLog_Printf("key event queue full\r\n");
        }

        if ((uint32_t)(now_tick - heartbeat_tick) >=
            APP_HEARTBEAT_PERIOD_MS)
        {
            /*
             * 默认任务虽然每20ms循环一次，但心跳和PA1仍只在
             * 独立的1秒节拍到达时执行。
             */
            heartbeat_tick = now_tick;

            HAL_GPIO_TogglePin(BOARD_LED_GPIO_Port,
                               BOARD_LED_Pin);

            DebugLog_Printf(
                "heartbeat=%lu key=%u tick=%lu\r\n",
                (unsigned long)heartbeat,
                (unsigned int)BSP_Key_IsPressed(&key),
                (unsigned long)now_tick);

            heartbeat++;
        }

        osDelay(APP_MAIN_LOOP_PERIOD_MS);
    }
}

/**
 * @brief 每约2秒读取一次DHT11并输出温湿度或错误状态。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 首次读取前等待2秒满足DHT11上电稳定时间；读取失败不会终止任务，
 * 下一个周期会自动重试，因此传感器重新接入后可以自行恢复。
 */
void App_SensorTask(void *argument)
{
    DHT11_Data_t data = {0};

    (void)argument;

    if (!BSP_DHT11_Init())
    {
        /* TIM5无法启动属于不可恢复的基础外设故障，保留任务但停止采集。 */
        DebugLog_Printf("DHT11 timer start failed\r\n");

        for (;;)
        {
            osDelay(2000U);
        }
    }

    /* 等待DHT11上电稳定，避免启动后立即读取造成无效响应。 */
    osDelay(2000U);

    for (;;)
    {
        DHT11_Status_t status = BSP_DHT11_Read(&data);

        if (status == DHT11_STATUS_OK)
        {
            /* 使用扩大10倍的整数拆分整数位和小数位，避免浮点格式化。 */
            int32_t temperature_x10 = data.temperature_x10;
            uint32_t temperature_magnitude;

            if (temperature_x10 < 0)
            {
                temperature_magnitude =
                    (uint32_t)(-temperature_x10);
            }
            else
            {
                temperature_magnitude =
                    (uint32_t)temperature_x10;
            }

            DebugLog_Printf(
                "DHT11 temp=%s%lu.%luC "
                "humidity=%lu.%lu%% status=OK\r\n",
                (temperature_x10 < 0) ? "-" : "",
                (unsigned long)(temperature_magnitude / 10U),
                (unsigned long)(temperature_magnitude % 10U),
                (unsigned long)(data.humidity_x10 / 10U),
                (unsigned long)(data.humidity_x10 % 10U));

            /* 只有校验成功的数据才能成为AUTO控制输入。 */
            App_SensorState_Publish(&data,
                                    osKernelGetTickCount());
        }
        else if (status == DHT11_STATUS_CHECKSUM_ERROR)
        {
            /* TIMEOUT和CHECKSUM_ERROR只记录状态，下个周期继续尝试。 */
            DebugLog_Printf(
                "DHT11 status=CHECKSUM_ERROR\r\n");
        }
        else
        {
            DebugLog_Printf("DHT11 status=TIMEOUT\r\n");
        }

        osDelay(2000U);
    }
}

/**
 * @brief 电机控制任务：每50ms读取编码器并执行软启动、PI和故障停机。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * MotorTask是唯一允许初始化TB6612、修改PWM和改变电机状态的任务。
 * PA0所在的DefaultTask只通过消息队列发送按键事件。
 */
void App_MotorTask(void *argument)
{
    ControlPi_t controller;
    ModeManager_t mode_manager;
    ModeMotorRequest_t previous_request = MODE_MOTOR_STOP;
    ModeMotorRequest_t previous_auto_request = MODE_MOTOR_STOP;
    App_MotorState_t state = APP_MOTOR_STATE_STOP;
    uint32_t sample_tick;
    uint32_t state_enter_tick;
    uint32_t log_sample_count = 0U;
    uint32_t zero_sample_count = 0U;
    int32_t actual_sum = 0L;
    int32_t signed_delta_sum = 0L;
    uint8_t duty_percent = 0U;

    (void)argument;

#if APP_M8A_SELF_TEST_ENABLED
    /* M8A自检只验证纯算法和请求映射，不初始化电机输出。 */
    if (!App_M8A_RunSelfTests())
    {
        DebugLog_Printf("M8A auto policy self-test FAILED\r\n");
        BSP_Motor_Stop();

        for (;;)
        {
            osDelay(1000U);
        }
    }

    DebugLog_Printf("M8A auto policy self-test PASSED\r\n");
#endif

#if APP_M7_SELF_TEST_ENABLED
    /*
     * 在初始化电机之前验证按键识别和模式转换状态机。
     * 任一自检失败都保持电机停止，并阻止控制任务继续启动。
     */
    if (!BSP_Key_RunSelfTests() ||
        !ModeManager_RunSelfTests() ||
        !App_MotorModeIntegration_RunSelfTests())
    {
        DebugLog_Printf("M7 self-test FAILED\r\n");
        BSP_Motor_Stop();

        for (;;)
        {
            osDelay(1000U);
        }
    }

    DebugLog_Printf("M7 self-test PASSED\r\n");
#endif

#if APP_CONTROL_PI_SELF_TEST_ENABLED
    /* 临时自检开关为1时，电机初始化前先验证PI纯算法。 */
    if (!ControlPi_RunSelfTests())
    {
        DebugLog_Printf("control PI self-test FAILED\r\n");
        BSP_Motor_Stop();

        for (;;)
        {
            osDelay(1000U);
        }
    }

    DebugLog_Printf("control PI self-test PASSED\r\n");
#endif

    /* 模式管理器先恢复安全初值，再初始化PI和电机外设。 */
    ModeManager_Init(&mode_manager);
    DebugLog_Printf(
        "mode run=STANDBY mode=AUTO level=LOW fault=NONE\r\n");

    /* 初始化PI参数、积分范围和PWM输出范围。 */
    ControlPi_Init(&controller,
                   APP_CONTROL_KP_Q8,
                   APP_CONTROL_KI_Q8,
                   APP_CONTROL_INTEGRAL_MIN,
                   APP_CONTROL_INTEGRAL_MAX,
                   APP_MOTOR_OUTPUT_MIN,
                   APP_MOTOR_OUTPUT_MAX);

    /* 电机初始化失败时持续执行安全停止，不进入控制循环。 */
    if (!BSP_Motor_Init())
    {
        DebugLog_Printf("motor init failed, state=STOP\r\n");

        for (;;)
        {
            BSP_Motor_Stop();
            osDelay(1000U);
        }
    }

    BSP_Encoder_Init();

    /* 编码器定时器启动失败时禁止电机运行。 */
    if (!BSP_Encoder_Start())
    {
        DebugLog_Printf("encoder start failed, state=STOP\r\n");

        for (;;)
        {
            BSP_Motor_Stop();
            osDelay(1000U);
        }
    }

    /* 所有初始化完成后仍强制保持STOP，等待PA0按键事件。 */
    BSP_Motor_Stop();
    DebugLog_Printf("motor control ready, state=STOP\r\n");

    sample_tick = osKernelGetTickCount();
    state_enter_tick = sample_tick;

    for (;;)
    {
        BSP_KeyEvent_t event;
        Encoder_Direction_t direction;
        int16_t delta;
        int32_t delta_32;
        int32_t actual_count;
        int32_t target_count;
        int32_t feedforward;
        App_MotorRequestAction_t action;
        AutoPolicySnapshot_t sensor_snapshot = {0};
        ModeMotorRequest_t auto_request;
        ModeMotorRequest_t request;
        uint32_t now_tick;
        bool skip_log_sample = false;

        /* 使用绝对节拍保持稳定的50ms控制周期。 */
        sample_tick += APP_CONTROL_PERIOD_MS;
        (void)osDelayUntil(sample_tick);
        now_tick = osKernelGetTickCount();

        /* 每个周期清空已有事件，并逐个更新和记录模式状态。 */
        while (osMessageQueueGet(app_key_event_queue,
                                 &event,
                                 NULL,
                                 0U) == osOK)
        {
            ModeResult_t result;

            result = ModeManager_HandleEvent(&mode_manager, event);
            App_LogModeState(&mode_manager, result);
        }

        /* 读取最新DHT11快照；互斥量失败时使用无效快照并安全停止AUTO。 */
        if (!App_SensorState_Read(&sensor_snapshot))
        {
            sensor_snapshot.valid = false;
        }

        /* 先计算AUTO候选，再由ModeManager统一处理模式和故障优先级。 */
        auto_request = AutoPolicy_Evaluate(&sensor_snapshot,
                                           now_tick,
                                           previous_auto_request);
        request = ModeManager_GetMotorRequest(&mode_manager,
                                              auto_request);

        if (mode_manager.mode == MODE_AUTO)
        {
            previous_auto_request = auto_request;
        }
        else
        {
            /* 离开AUTO后清除历史候选，重新进入时从STOP基线判断。 */
            previous_auto_request = MODE_MOTOR_STOP;
        }

        if (request != previous_request)
        {
            ControlPi_Reset(&controller);
            zero_sample_count = 0U;
            action = App_MotorRequestToAction(request);
            state = action.state;
            duty_percent = action.duty_percent;

            if (action.stop_motor)
            {
                BSP_Motor_Stop();
            }
            else
            {
                state_enter_tick = now_tick;
                BSP_Motor_SetDuty(action.duty_percent);
            }

            /* 本周期仍执行控制，仅跳过属于前一状态的统计样本。 */
            skip_log_sample = true;
            previous_request = request;
        }

        /* 固定正转可能得到负计数，因此PI使用计数绝对值。 */
        delta = BSP_Encoder_ReadDelta(&direction);
        delta_32 = (int32_t)delta;
        actual_count = (delta_32 < 0L) ? -delta_32 : delta_32;
        target_count = App_MotorTargetCount(state);
        feedforward = 0L;

        /* 两个挡位均先保持30%软启动300ms。 */
        if ((state == APP_MOTOR_STATE_LOW_START) ||
            (state == APP_MOTOR_STATE_HIGH_START))
        {
            BSP_Motor_SetDuty(APP_MOTOR_START_DUTY_PERCENT);
            duty_percent = APP_MOTOR_START_DUTY_PERCENT;

            if ((uint32_t)(now_tick - state_enter_tick) >=
                APP_MOTOR_START_TIME_MS)
            {
                state = (state == APP_MOTOR_STATE_LOW_START) ?
                    APP_MOTOR_STATE_LOW_PI :
                    APP_MOTOR_STATE_HIGH_PI;
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
            }
        }

        /* 只有PI状态才检测编码器无反馈并计算控制输出。 */
        if ((state == APP_MOTOR_STATE_LOW_PI) ||
            (state == APP_MOTOR_STATE_HIGH_PI))
        {
            if (actual_count <= APP_ENCODER_ZERO_THRESHOLD)
            {
                zero_sample_count++;
            }
            else
            {
                zero_sample_count = 0U;
            }

            if (zero_sample_count >= APP_ENCODER_FAULT_SAMPLE_COUNT)
            {
                /* 连续500ms无反馈时立即停止、锁存FAULT并清零内部计数。 */
                ModeManager_SetFault(&mode_manager,
                                     MODE_FAULT_ENCODER_TIMEOUT);
                previous_request = MODE_MOTOR_FAULT;
                state = APP_MOTOR_STATE_FAULT;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
                duty_percent = 0U;
                target_count = 0L;
                /* 本周期仍完成故障处理，仅跳过切入FAULT前的统计样本。 */
                skip_log_sample = true;
                DebugLog_Printf(
                    "motor state=FAULT "
                    "reason=ENCODER_TIMEOUT duty=0%%\r\n");
            }
            else
            {
                feedforward =
                    (state == APP_MOTOR_STATE_LOW_PI) ?
                    APP_MOTOR_LOW_FEEDFORWARD :
                    APP_MOTOR_HIGH_FEEDFORWARD;

                duty_percent = (uint8_t)ControlPi_Update(
                    &controller,
                    target_count,
                    actual_count,
                    feedforward);
                BSP_Motor_SetDuty(duty_percent);
            }
        }

        if (skip_log_sample)
        {
            /* 丢弃切换周期delta，新窗口从下一完整50ms周期开始。 */
            actual_sum = 0L;
            signed_delta_sum = 0L;
            log_sample_count = 0U;
        }
        else
        {
            /* 累计10个完整控制周期，每约500ms输出一次平均状态。 */
            actual_sum += actual_count;
            signed_delta_sum += delta_32;
            log_sample_count++;

            if (log_sample_count >= APP_CONTROL_LOG_SAMPLE_COUNT)
            {
                int32_t actual_average;
                int32_t error_average;
                const char *direction_text;
                uint32_t sensor_age;

                actual_average =
                    actual_sum /
                    (int32_t)APP_CONTROL_LOG_SAMPLE_COUNT;
                error_average = target_count - actual_average;

                /* 使用10个周期的计数总和判断方向，降低抖动。 */
                if (signed_delta_sum > 0L)
                {
                    direction_text = "forward";
                }
                else if (signed_delta_sum < 0L)
                {
                    direction_text = "reverse";
                }
                else
                {
                    direction_text = "stopped";
                }

                if (sensor_snapshot.valid)
                {
                    sensor_age =
                        (uint32_t)(now_tick -
                                   sensor_snapshot.updated_tick);
                    DebugLog_Printf(
                        "control state=%s target=%ld actual=%ld "
                        "error=%ld duty=%u integral=%ld fault=%u "
                        "dir=%s auto=%s temp_x10=%ld humidity_x10=%u "
                        "sensor_age=%lu\r\n",
                        App_MotorStateText(state),
                        (long)target_count,
                        (long)actual_average,
                        (long)error_average,
                        (unsigned int)duty_percent,
                        (long)ControlPi_GetIntegral(&controller),
                        (unsigned int)(state == APP_MOTOR_STATE_FAULT),
                        direction_text,
                        App_MotorRequestText(auto_request),
                        (long)sensor_snapshot.temperature_x10,
                        (unsigned int)sensor_snapshot.humidity_x10,
                        (unsigned long)sensor_age);
                }
                else
                {
                    DebugLog_Printf(
                        "control state=%s target=%ld actual=%ld "
                        "error=%ld duty=%u integral=%ld fault=%u "
                        "dir=%s auto=%s temp=NA humidity=NA "
                        "sensor_age=STALE\r\n",
                        App_MotorStateText(state),
                        (long)target_count,
                        (long)actual_average,
                        (long)error_average,
                        (unsigned int)duty_percent,
                        (long)ControlPi_GetIntegral(&controller),
                        (unsigned int)(state == APP_MOTOR_STATE_FAULT),
                        direction_text,
                        App_MotorRequestText(auto_request));
                }

                actual_sum = 0L;
                signed_delta_sum = 0L;
                log_sample_count = 0U;
            }
        }
    }
}
