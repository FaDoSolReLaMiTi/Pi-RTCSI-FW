// Modified by Fangzhan for RTCSI
#pragma once
#include <structs.h>            // structures that are used by the code in the firmware

#define TIMER_UNINIT 0
#define TIMER_RUNNING 1
#define TIMER_SUSPENDED 2

extern uint8_t timer_flag;

void 
init_timer(struct wlc_info *wlc);