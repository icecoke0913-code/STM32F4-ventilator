/**
 * @file app_tasks.c
 * @brief SmartHood显示自检、心跳按键、电机挡位和DHT11周期任务实现。
 */

#include "app_tasks.h"

#include <stdint.h>
#include <stdbool.h>

#include "bsp_dht11.h"
#include "bsp_motor.h"
#include "bsp_st7735s.h"
#include "bsp_encoder.h"

#include "control_pi.h"
#include "control_pi_selftest.h"
#include "bsp_key_selftest.h"
#include "mode_manager_selftest.h"

#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

/** 默认任务快速循环周期，用于可靠识别PA0短按。 */
#define APP_MAIN_LOOP_PERIOD_MS       20U

/** 原始按键电平保持不变达到此时间后，才更新稳定状态。 */
#define APP_KEY_DEBOUNCE_MS           40U

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
    APP_MOTOR_STATE_STOP = 0,  /**< 电机停止，等待NEXT命令。 */
    APP_MOTOR_STATE_LOW_START, /**< 低档30%软启动阶段。 */
    APP_MOTOR_STATE_LOW_PI,    /**< 低档PI闭环阶段。 */
    APP_MOTOR_STATE_HIGH_START,/**< 高档30%软启动阶段。 */
    APP_MOTOR_STATE_HIGH_PI,   /**< 高档PI闭环阶段。 */
    APP_MOTOR_STATE_FAULT      /**< 编码器无反馈故障锁存。 */
} App_MotorState_t;

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
#define APP_M7_SELF_TEST_ENABLED 1U

/**
 * @brief 默认任务可以发送给电机任务的控制命令。
 *
 * 当前只有NEXT命令，表示按既定顺序切换到下一个电机状态。
 */
typedef enum
{
    APP_MOTOR_COMMAND_NEXT = 0
} App_MotorCommand_t;

/**
 * @brief 电机命令队列最多保存的命令数量。
 *
 * 正常按键已经经过40ms消抖，长度4足以容纳短时间内的连续操作。
 */
#define APP_MOTOR_COMMAND_QUEUE_LENGTH 4U

/** 电机控制命令队列句柄，创建成功前保持为NULL。 */
static osMessageQueueId_t app_motor_command_queue = NULL;

/**
 * @brief 创建默认任务到电机任务之间的命令队列。
 *
 * @return 创建成功返回true，否则返回false。
 */
bool App_MotorControl_Init(void)
{
    app_motor_command_queue = osMessageQueueNew(
        APP_MOTOR_COMMAND_QUEUE_LENGTH,
        sizeof(App_MotorCommand_t),
        NULL);

    return app_motor_command_queue != NULL;
}

/**
 * @brief 向电机任务发送一次“切换到下一状态”命令。
 *
 * 使用0超时，队列已满时立即返回，不阻塞默认任务的心跳和按键扫描。
 *
 * @return 命令成功进入队列返回true，否则返回false。
 */
static bool App_MotorPostNextCommand(void)
{
    App_MotorCommand_t command = APP_MOTOR_COMMAND_NEXT;

    if (app_motor_command_queue == NULL)
    {
        return false;
    }

    return osMessageQueuePut(app_motor_command_queue,
                             &command,
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
 * @brief 运行显示自检、心跳，并把PA0有效按下发送给电机任务。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 快速循环每20ms采样PA0并执行40ms消抖；只有稳定状态从低变高时
 * 才发送一次NEXT命令。PA1和心跳继续使用独立的1秒节拍。
 */
void App_DefaultTask(void *argument)
{
    uint32_t heartbeat = 0U;
    uint32_t heartbeat_tick;
    uint32_t candidate_since_tick;
    GPIO_PinState candidate_key_state;
    GPIO_PinState stable_key_state;

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

    /*
     * 以任务开始处理按键时的实际电平作为初始稳定状态。
     * 如果上电时PA0已经被按住，不会因此触发电机；
     * 必须先释放按键，再重新按下。
     */
    stable_key_state = HAL_GPIO_ReadPin(USER_KEY_GPIO_Port,
                                        USER_KEY_Pin);

    candidate_key_state = stable_key_state;
    candidate_since_tick = HAL_GetTick();
    heartbeat_tick = candidate_since_tick;

    for (;;)
    {
        GPIO_PinState raw_key_state;
        uint32_t now_tick;

        now_tick = HAL_GetTick();

        raw_key_state = HAL_GPIO_ReadPin(USER_KEY_GPIO_Port,
                                        USER_KEY_Pin);

        if (raw_key_state != candidate_key_state)
        {
            /*
             * 原始电平发生变化时，只记录新的候选状态，
             * 并重新开始40ms消抖计时。
             */
            candidate_key_state = raw_key_state;
            candidate_since_tick = now_tick;
        }
        else if ((candidate_key_state != stable_key_state) &&
                 ((uint32_t)(now_tick - candidate_since_tick) >=
                  APP_KEY_DEBOUNCE_MS))
        {
            /*
             * 候选电平连续保持40ms，确认它是稳定状态，
             * 而不是机械按键触点抖动。
             */
            stable_key_state = candidate_key_state;

            if (stable_key_state == GPIO_PIN_SET)
            {
                /*
                 * 默认任务不再直接修改PWM，只向电机任务发送NEXT命令。
                 * 队列发送失败时记录日志，但不阻塞心跳和其他任务。
                 */
                if (!App_MotorPostNextCommand())
                {
                    DebugLog_Printf("motor command queue full\r\n");
                }
            }
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
                (unsigned int)stable_key_state,
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
 * PA0所在的DefaultTask只通过消息队列发送NEXT命令。
 */
void App_MotorTask(void *argument)
{
    ControlPi_t controller;
    App_MotorState_t state = APP_MOTOR_STATE_STOP;
    uint32_t sample_tick;
    uint32_t state_enter_tick;
    uint32_t log_sample_count = 0U;
    uint32_t zero_sample_count = 0U;
    int32_t actual_sum = 0L;
    int32_t signed_delta_sum = 0L;
    uint8_t duty_percent = 0U;

    (void)argument;

#if APP_M7_SELF_TEST_ENABLED
    /*
     * 在初始化电机之前验证按键识别和模式转换状态机。
     * 任一自检失败都保持电机停止，并阻止控制任务继续启动。
     */
    if (!BSP_Key_RunSelfTests() ||
        !ModeManager_RunSelfTests())
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

    /* 所有初始化完成后仍强制保持STOP，等待PA0命令。 */
    BSP_Motor_Stop();
    DebugLog_Printf("motor control ready, state=STOP\r\n");

    sample_tick = osKernelGetTickCount();
    state_enter_tick = sample_tick;

    for (;;)
    {
        App_MotorCommand_t command;
        Encoder_Direction_t direction;
        int16_t delta;
        int32_t delta_32;
        int32_t actual_count;
        int32_t target_count;
        int32_t feedforward;
        uint32_t now_tick;

        /* 使用绝对节拍保持稳定的50ms控制周期。 */
        sample_tick += APP_CONTROL_PERIOD_MS;
        (void)osDelayUntil(sample_tick);
        now_tick = osKernelGetTickCount();

        /* 每个周期最多处理一个NEXT命令，且不阻塞闭环计算。 */
        if (osMessageQueueGet(app_motor_command_queue,
                              &command,
                              NULL,
                              0U) == osOK)
        {
            if (state == APP_MOTOR_STATE_FAULT)
            {
                /* FAULT第一次按键只清除故障，不自动重新启动。 */
                state = APP_MOTOR_STATE_STOP;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
                duty_percent = 0U;
                DebugLog_Printf(
                    "motor fault cleared, state=STOP\r\n");
            }
            else if (state == APP_MOTOR_STATE_STOP)
            {
                /* STOP按键后先进入低档30%软启动。 */
                state = APP_MOTOR_STATE_LOW_START;
                state_enter_tick = now_tick;
                ControlPi_Reset(&controller);
                BSP_Motor_SetDuty(APP_MOTOR_START_DUTY_PERCENT);
                duty_percent = APP_MOTOR_START_DUTY_PERCENT;
                DebugLog_Printf(
                    "motor state=LOW_START duty=30%%\r\n");
            }
            else if ((state == APP_MOTOR_STATE_LOW_START) ||
                     (state == APP_MOTOR_STATE_LOW_PI))
            {
                /* 低档按键后重新以30%软启动进入高档。 */
                state = APP_MOTOR_STATE_HIGH_START;
                state_enter_tick = now_tick;
                ControlPi_Reset(&controller);
                BSP_Motor_SetDuty(APP_MOTOR_START_DUTY_PERCENT);
                duty_percent = APP_MOTOR_START_DUTY_PERCENT;
                DebugLog_Printf(
                    "motor state=HIGH_START duty=30%%\r\n");
            }
            else
            {
                /* 高档再次按键进入STOP。 */
                state = APP_MOTOR_STATE_STOP;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                zero_sample_count = 0U;
                duty_percent = 0U;
                DebugLog_Printf(
                    "motor state=STOP duty=0%%\r\n");
            }
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
                /* 连续500ms无反馈时立即停止并锁存FAULT。 */
                state = APP_MOTOR_STATE_FAULT;
                BSP_Motor_Stop();
                ControlPi_Reset(&controller);
                duty_percent = 0U;
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

        /* 累计10个控制周期，每约500ms输出一次平均状态。 */
        actual_sum += actual_count;
        signed_delta_sum += delta_32;
        log_sample_count++;

        if (log_sample_count >= APP_CONTROL_LOG_SAMPLE_COUNT)
        {
            int32_t actual_average;
            int32_t error_average;
            const char *direction_text;

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

            DebugLog_Printf(
                "control state=%s target=%ld actual=%ld "
                "error=%ld duty=%u integral=%ld "
                "fault=%u dir=%s\r\n",
                App_MotorStateText(state),
                (long)target_count,
                (long)actual_average,
                (long)error_average,
                (unsigned int)duty_percent,
                (long)ControlPi_GetIntegral(&controller),
                (unsigned int)(state == APP_MOTOR_STATE_FAULT),
                direction_text);

            actual_sum = 0L;
            signed_delta_sum = 0L;
            log_sample_count = 0U;
        }
    }
}
