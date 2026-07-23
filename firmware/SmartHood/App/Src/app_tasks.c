#include "app_tasks.h"

#include <stdint.h>

#include "cmsis_os2.h"
#include "debug_log.h"
#include "gpio.h"

void App_DefaultTask(void *argument)
{
    uint32_t heartbeat = 0U;

    (void)argument;

    DebugLog_Printf("\r\nSmartHood M1 start\r\n");

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
