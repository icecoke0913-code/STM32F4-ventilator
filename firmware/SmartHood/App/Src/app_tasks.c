#include "app_tasks.h"

#include <stdint.h>

#include "bsp_st7735s.h"
#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

static bool App_RunDisplayTest(void)
{
    if (!BSP_ST7735S_Init())
    {
        return false;
    }

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
        BSP_ST7735S_SetBacklight(false);
        DebugLog_Printf("ST7735S init or draw failed\r\n");
    }

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
