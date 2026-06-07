// Modified by Fangzhan for RTCSI
#pragma NEXMON targetregion "patch"

#include <firmware_version.h>   // definition of firmware version macros
#include <debug.h>              // contains macros to access the debug hardware
#include <wrapper.h>            // wrapper definitions for functions that already exist in the firmware
#include <structs.h>            // structures that are used by the code in the firmware
#include <helper.h>             // useful helper functions
#include <patcher.h>            // macros used to craete patches such as BLPatch, BPatch, ...
#include <rates.h>              // rates used to build the ratespec for frame injection
#include <nexioctls.h>          // ioctls added in the nexmon patch
#include <capabilities.h>       // capabilities included in a nexmon patch
#include <sendframe.h>
#include "customized_timer.h"
#include "rtcsi.h"

struct timer_private_data {
    struct wlc_info *wlc;
    uint16_t count;
};

uint8_t timer_flag=TIMER_UNINIT;

static void
timer_handler(struct hndrte_timer *t)
{
    // timer cannot be shutdown on RaspberryPi 4, we can set it to suspend
    if(timer_flag!=TIMER_RUNNING){
    	return;
    }
    struct timer_private_data *data = (struct timer_private_data *) t->data;
    data->count++;
    on_rtcsi_dev_timer_event(data->wlc);  
}

void 
init_timer(struct wlc_info *wlc){
	if(timer_flag != TIMER_UNINIT){
		printf("Timer Has Been Initilied\n");
		return;
	}
    printf("Init Timer\n");

    struct timer_private_data *data = (struct timer_private_data *) malloc(sizeof(struct timer_private_data), 0);
    memset(data, 0, sizeof(struct timer_private_data)); // will be freed after finishing the task
    data->wlc = wlc;
    data->count = 0;

	struct hndrte_timer *t;
	t = hndrte_init_timer(init_timer, data, timer_handler, 0);
    if (!t) {
    	printf("ERR: could not init timer");
    	free(data);
        return;
    }

    if (!hndrte_add_timer(t, 1, 1)) {
        hndrte_free_timer(t);
        free(data);
        printf("ERR: could not add timer");
        return;
    }

    printf("Init Timer OK\n");
    timer_flag = TIMER_SUSPENDED;
    return;
}