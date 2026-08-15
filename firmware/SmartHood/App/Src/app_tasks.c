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

#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

/** 默认任务快速循环周期，用于可靠识别PA0短按。 */
#define APP_MAIN_LOOP_PERIOD_MS       20U

/** 原始按键电平保持不变达到此时间后，才更新稳定状态。 */
#define APP_KEY_DEBOUNCE_MS           40U

/** PA1翻转和心跳日志继续保持原有1秒周期。 */
#define APP_HEARTBEAT_PERIOD_MS     1000U

/** 编码器测速采样周期，与后续PID控制周期保持一致。 */
#define APP_ENCODER_SAMPLE_PERIOD_MS    50U

/** 每10次采样输出一次日志，即约500 ms输出一次。 */
#define APP_ENCODER_LOG_SAMPLE_COUNT    10U

/** M4固定正转挡位，占空比按短按顺序循环。 */
static const uint8_t app_motor_duty_levels[] =
{
    0U,
    30U,
    50U,
    70U
};

/** 根据挡位表自动计算挡位数量，避免手工数量与数组不一致。 */
#define APP_MOTOR_LEVEL_COUNT \
    ((uint8_t)(sizeof(app_motor_duty_levels) / \
               sizeof(app_motor_duty_levels[0])))

/**
 * @brief 应用一个已经确认的M4电机挡位并输出状态日志。
 * @param duty_percent 允许值为0、30、50或70。
 */
static void App_ApplyMotorDuty(uint8_t duty_percent)
{
    if (duty_percent == 0U)
    {
        BSP_Motor_Stop();
        DebugLog_Printf("motor duty=0%%, state=STOP\r\n");
    }
    else
    {
        BSP_Motor_SetDuty(duty_percent);

        DebugLog_Printf("motor duty=%u%%, state=RUN\r\n",
                        (unsigned int)duty_percent);
    }
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
 * @brief 运行显示自检、心跳和M4单键电机挡位控制。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 快速循环每20ms采样PA0并执行40ms消抖；只有稳定状态从低变高时
 * 才切换一次挡位。PA1和心跳使用独立1秒节拍，避免快速循环改变M1行为。
 */
void App_DefaultTask(void *argument)
{
    uint32_t heartbeat = 0U;
    uint32_t heartbeat_tick;
    uint32_t candidate_since_tick;
    uint8_t motor_level_index = 0U;
    GPIO_PinState candidate_key_state;
    GPIO_PinState stable_key_state;
    bool motor_ready;

    (void)argument;

    DebugLog_Printf("\r\nSmartHood M1 start\r\n");

    /*
     * 启动TIM4 PWM，但驱动初始化成功后仍保持0%和STBY低，
     * 因此上电不会让电机自动旋转。
     */
    motor_ready = BSP_Motor_Init();

    if (motor_ready)
    {
        DebugLog_Printf("motor init ok, state=STOP\r\n");
    }
    else
    {
        DebugLog_Printf("motor init failed, state=STOP\r\n");
    }

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
                 * 只在稳定状态从低变高的按下沿切换一次。
                 * 按键保持高电平期间不会再次进入这里。
                 */
                if (motor_ready)
                {
                    motor_level_index++;

                    if (motor_level_index >= APP_MOTOR_LEVEL_COUNT)
                    {
                        motor_level_index = 0U;
                    }

                    App_ApplyMotorDuty(
                        app_motor_duty_levels[motor_level_index]);
                }
                else
                {
                    /*
                     * PWM初始化失败后，不允许按键绕过安全状态。
                     */
                    BSP_Motor_Stop();

                    DebugLog_Printf(
                        "motor unavailable, state=STOP\r\n");
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
 * @brief 每50 ms读取一次编码器，并每约500 ms输出计数、方向和RPM。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * M5只测量编码器，不修改TIM4 PWM、TB6612方向或待机状态。
 */
void App_MotorTask(void *argument)
{
    uint32_t log_sample_count = 0U;
    uint32_t sample_tick;
    int32_t rpm_sum_x10 = 0;
    int32_t delta_sum = 0;

    (void)argument;

    BSP_Encoder_Init();

    if (!BSP_Encoder_Start())
    {
        /*
         * TIM3启动失败时不读取无效数据，也不改变电机状态。
         * 保留任务周期运行，避免异常路径形成无延时死循环。
         */
        DebugLog_Printf("encoder start failed\r\n");

        for (;;)
        {
            osDelay(1000U);
        }
    }

    DebugLog_Printf(
        "encoder start ok, sample=%lu ms, cpr=%lu\r\n",
        (unsigned long)APP_ENCODER_SAMPLE_PERIOD_MS,
        (unsigned long)1400U);

    /*
     * 以启动成功时的内核节拍为基准，后续每次增加50 ms，
     * 避免计算和日志发送耗时累计进采样周期。
     */
    sample_tick = osKernelGetTickCount();

    for (;;)
    {
        int16_t delta;
        int32_t rpm_x10;

        /*
         * 任务先等待一个完整采样周期，再读取增量。
         * 启动时已经同步上次计数，因此首个结果对应实际50 ms窗口。
         */
        sample_tick += APP_ENCODER_SAMPLE_PERIOD_MS;

        /* 使用绝对节拍延时，保持稳定的50 ms采样周期。 */
        (void)osDelayUntil(sample_tick);

        delta = BSP_Encoder_ReadDelta(NULL);

        rpm_x10 = BSP_Encoder_CountToRpmX10(
            delta,
            APP_ENCODER_SAMPLE_PERIOD_MS);

        delta_sum += (int32_t)delta;
        rpm_sum_x10 += rpm_x10;
        log_sample_count++;

        if (log_sample_count >= APP_ENCODER_LOG_SAMPLE_COUNT)
        {
            int32_t average_rpm_x10;
            uint32_t rpm_magnitude;
            const char *direction_text;

            average_rpm_x10 =
                rpm_sum_x10 /
                (int32_t)APP_ENCODER_LOG_SAMPLE_COUNT;

            if (delta_sum > 0)
            {
                direction_text = "forward";
            }
            else if (delta_sum < 0)
            {
                direction_text = "reverse";
            }
            else
            {
                direction_text = "stopped";
            }

            /*
             * 日志格式化不使用浮点数。
             * 先取得绝对值，再分别输出整数位和一位小数。
             */
            if (average_rpm_x10 < 0)
            {
                rpm_magnitude =
                    (uint32_t)(-average_rpm_x10);
            }
            else
            {
                rpm_magnitude =
                    (uint32_t)average_rpm_x10;
            }

            DebugLog_Printf(
                "encoder count=%u delta=%ld "
                "dir=%s rpm=%s%lu.%lu\r\n",
                (unsigned int)BSP_Encoder_ReadCount(),
                (long)delta_sum,
                direction_text,
                (average_rpm_x10 < 0) ? "-" : "",
                (unsigned long)(rpm_magnitude / 10U),
                (unsigned long)(rpm_magnitude % 10U));

            delta_sum = 0;
            rpm_sum_x10 = 0;
            log_sample_count = 0U;
        }
    }
}
