#include "debug_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "usart.h"

void DebugLog_Printf(const char *format, ...)
{
    char buffer[160];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length <= 0)
    {
        return;
    }

    if (length >= (int)sizeof(buffer))
    {
        length = (int)sizeof(buffer) - 1;
    }

    (void)HAL_UART_Transmit(&huart1,
                            (uint8_t *)buffer,
                            (uint16_t)length,
                            100U);
}
