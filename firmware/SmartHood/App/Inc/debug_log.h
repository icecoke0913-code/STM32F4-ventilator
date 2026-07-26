#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdbool.h>

bool DebugLog_Init(void);
void DebugLog_Printf(const char *format, ...);

#endif
