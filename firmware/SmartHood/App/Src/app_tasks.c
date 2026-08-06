/**
 * @file app_tasks.c
 * @brief SmartHood显示自检、心跳按键和DHT11周期任务实现。
 */

#include "app_tasks.h"

#include <stdint.h>

#include "bsp_dht11.h"
#include "bsp_motor.h"
#include "bsp_st7735s.h"

#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

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
 * @brief 运行M1基础功能和M2显示自检的默认任务。
 * @param argument FreeRTOS任务参数，本项目未使用。
 *
 * 任务启动时只执行一次TFT自检；随后每秒翻转PA1、读取PA0，
 * 并输出心跳序号、按键状态和HAL毫秒Tick。
 */
void App_DefaultTask(void *argument)
{
    uint32_t heartbeat = 0U;

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

    /* 默认任务以1秒为周期执行心跳、LED翻转和按键采样。 */
    for (;;)
    {
        GPIO_PinState key_state;

        HAL_GPIO_TogglePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin);
        key_state = HAL_GPIO_ReadPin(USER_KEY_GPIO_Port, USER_KEY_Pin);

        DebugLog_Printf("heartbeat=%lu key=%u tick=%lu\r\n",
                        (unsigned long)heartbeat,
                        (unsigned int)key_state,
                        (unsigned long)HAL_GetTick());

        heartbeat++;
        osDelay(1000U);
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
