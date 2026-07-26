#include "debug_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "usart.h"

static osMutexId_t debug_log_mutex = NULL;

bool DebugLog_Init(void)
{
    if (debug_log_mutex != NULL)
    {
        return true;
    }

    debug_log_mutex = osMutexNew(NULL);
    return debug_log_mutex != NULL;
}

void DebugLog_Printf(const char *format, ...)
{
    char buffer[160];
    va_list arguments;
    int length;

    if ((format == NULL) || (debug_log_mutex == NULL))
    {
        return;
    }

    if (osMutexAcquire(debug_log_mutex, osWaitForever) != osOK)
    {
        return;
    }

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length > 0)
    {
        if (length >= (int)sizeof(buffer))
        {
            length = (int)sizeof(buffer) - 1;
        }

        (void)HAL_UART_Transmit(&huart1,
                                (uint8_t *)buffer,
                                (uint16_t)length,
                                100U);
    }

    (void)osMutexRelease(debug_log_mutex);
}
