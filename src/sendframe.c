/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * This file is part of NexMon.                                            *
 *                                                                         *
 * Copyright (c) 2016 NexMon Team                                          *
 *                                                                         *
 * NexMon is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * NexMon is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with NexMon. If not, see <http://www.gnu.org/licenses/>.          *
 *                                                                         *
 **************************************************************************/
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
#include "local_wrapper.h"

char
sendframe(struct wlc_info *wlc, struct sk_buff *p, unsigned int fifo, unsigned int rate)
{
    char ret;

    // this unmutes the currently used channel and allows to send on "quiet/passive" channels
    wlc_bmac_mute(wlc->hw, 0, 0);

    if (wlc->band->bandtype == WLC_BAND_5G && rate < RATES_RATE_6M) {
        rate = RATES_RATE_6M;
    }

    if (wlc->hw->up) {
        ret = wlc_sendctl(wlc, p, wlc->active_queue, wlc->band->hwrs_scb, fifo, rate, 0);
    } else {
        ret = wlc_sendctl(wlc, p, wlc->active_queue, wlc->band->hwrs_scb, fifo, rate, 1);
        printf("ERR: wlc down\n");
    }

    return ret;
}


// check null pointer problem 
// https://github.com/seemoo-lab/nexmon/issues/335
__attribute__((naked))
void
check_scb(void)
{
     asm(
        "cmp r6, #0\n"             // check if pkt->scb is null
        "bne nonnull\n"
        "add lr,lr,0x178\n"        // if null adapt lr to jump out of pkt dequeue loop
        "b return\n"
        "nonnull:\n"
        "ldr.w r3,[r7,#0xe8]\n"    // get scb->cfg->flags (crashed the chip when scb was null)
        "return:\n"
        "push {lr}\n"
        "pop {pc}\n"
    );  
}

__attribute__((at(0x1AF378, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x1AABB0, "", CHIP_VER_BCM43455c0, FW_VER_7_45_206)))
__attribute__((naked))
void
patch_null_pointer_scb(void)
{
    asm(
        "bl check_scb\n"    // branch to null pointer check instead of accessing possibly invalid cfg
    );  
}