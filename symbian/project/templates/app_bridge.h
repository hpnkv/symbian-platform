#ifndef APP_BRIDGE_H_
#define APP_BRIDGE_H_

// A narrow boundary keeps original SDK descriptors and placement-new
// declarations out of the modern C++ model's translation unit.
#include "clock_time.h"

extern "C" {
void* AppCreate();
void AppDestroy(void* app);
int AppLogTime(void* app, ClockTime now);
void AppClear(void* app);
int AppLineCount(const void* app);
const char* AppLine(const void* app, int line);
void AppHomeTime(ClockTime* result);
#ifdef SYMBIAN_ENABLE_TIMER_TASKS
int AppTasksOpen(void* app);
void AppTasksSchedule(void* app);
void AppTasksCancel(void* app);
int AppTasksDispatch(void* app);
void AppTasksPark(void* app);
#endif
}

#endif  // APP_BRIDGE_H_
