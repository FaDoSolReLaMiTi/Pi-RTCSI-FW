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
 * Copyright (c) 2019 Matthias Schulz                                      *
 *                                                                         *
 * Permission is hereby granted, free of charge, to any person obtaining a *
 * copy of this software and associated documentation files (the           *
 * "Software"), to deal in the Software without restriction, including     *
 * without limitation the rights to use, copy, modify, merge, publish,     *
 * distribute, sublicense, and/or sell copies of the Software, and to      *
 * permit persons to whom the Software is furnished to do so, subject to   *
 * the following conditions:                                               *
 *                                                                         *
 * 1. The above copyright notice and this permission notice shall be       *
 *    include in all copies or substantial portions of the Software.       *
 *                                                                         *
 * 2. Any use of the Software which results in an academic publication or  *
 *    other publication which includes a bibliography must include         *
 *    citations to the nexmon project a) and the paper cited under b):     *
 *                                                                         *
 *    a) "Matthias Schulz, Daniel Wegemer and Matthias Hollick. Nexmon:    *
 *        The C-based Firmware Patching Framework. https://nexmon.org"     *
 *                                                                         *
 *    b) "Francesco Gringoli, Matthias Schulz, Jakob Link, and Matthias    *
 *        Hollick. Free Your CSI: A Channel State Information Extraction   *
 *        Platform For Modern Wi-Fi Chipsets. Accepted to appear in        *
 *        Proceedings of the 13th Workshop on Wireless Network Testbeds,   *
 *        Experimental evaluation & CHaracterization (WiNTECH 2019),       *
 *        October 2019."                                                   *
 *                                                                         *
 * 3. The Software is not used by, in cooperation with, or on behalf of    *
 *    any armed forces, intelligence agencies, reconnaissance agencies,    *
 *    defense agencies, offense agencies or any supplier, contractor, or   *
 *    research associated.                                                 *
 *                                                                         *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  *
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY    *
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,    *
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE       *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                  *
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
#include <version.h>            // version information
#include <argprintf.h>          // allows to execute argprintf to print into the arg buffer
#include <objmem.h>
#include "customized_timer.h"
#include "rtcsi.h"

#if NEXMON_CHIP == CHIP_VER_BCM4366c0
#define SHM_CSI_COLLECT         0xB80
#define NSSMASK                 0xB81
#define COREMASK                0xB82
#define APPLY_PKT_FILTER        0xB83
#define PKT_FILTER_BYTE         0xB84
#define N_CMP_SRC_MAC           0xB85
#define CMP_SRC_MAC_0_0         0xB86
#define CMP_SRC_MAC_0_1         0xB87
#define CMP_SRC_MAC_0_2         0xB88
#define CMP_SRC_MAC_1_0         0xB89
#define CMP_SRC_MAC_1_1         0xB8A
#define CMP_SRC_MAC_1_2         0xB8B
#define CMP_SRC_MAC_2_0         0xB8C
#define CMP_SRC_MAC_2_1         0xB8D
#define CMP_SRC_MAC_2_2         0xB8E
#define CMP_SRC_MAC_3_0         0xB8F
#define CMP_SRC_MAC_3_1         0xB90
#define CMP_SRC_MAC_3_2         0xB91
#define FORCEDEAF               0xB92
#define CLEANDEAF               0xB93
#define FIFODELAY               0xB94
#else
#define N_CMP_SRC_MAC           0x888
#define CMP_SRC_MAC_0_0         0x889
#define CMP_SRC_MAC_0_1         0x88A
#define CMP_SRC_MAC_0_2         0x88B
#define CMP_SRC_MAC_1_0         0x88C
#define CMP_SRC_MAC_1_1         0x88D
#define CMP_SRC_MAC_1_2         0x88E
#define CMP_SRC_MAC_2_0         0x88F
#define CMP_SRC_MAC_2_1         0x890
#define CMP_SRC_MAC_2_2         0x891
#define CMP_SRC_MAC_3_0         0x892
#define CMP_SRC_MAC_3_1         0x893
#define CMP_SRC_MAC_3_2         0x894
#define APPLY_PKT_FILTER        0x898
#define PKT_FILTER_BYTE         0x899
#define FIFODELAY               0x89e
#define SHM_CSI_COLLECT         0x8b0
#define	CLEANDEAF               0x8a3
#define	FORCEDEAF               0x8a4
#define NSSMASK                 0x8a6
#define COREMASK                0x8a7
#endif

// void set_tx_pwr(struct wlc_info *wlc, int pwr_dbm){
//     char val_name[] = "qtxpower";
//     int qpwr = pwr_dbm*4;
//     int name_len = strlen(val_name)+1;
//     int val_len = sizeof(qpwr);
//     char *cmd = malloc(name_len+val_len, 0);
//     memcpy(cmd, val_name, name_len);
//     memcpy(cmd + name_len, &qpwr, val_len);
//     wlc_ioctl(wlc, WLC_SET_VAR, cmd, name_len+val_len, 0);
//     free(cmd);
// }
uint8_t block_escan = 1;

static void
exp_set_gains(struct phy_info *pi, uint8 ipa, uint8 txlpf, uint8 pga, uint8 pad, uint8 txgm, uint8 bbmult)
{
    wlc_phy_txpwrctrl_enable_acphy(pi, 0);
    ac_txgain_setting_t gains = { 0 };
    gains.ipa = ipa;  // default: 255 (650 mW)
    gains.txlpf = txlpf;  // default: 0
    gains.pga = pga;
    gains.pad = pad;  // default: 255 (100 mW)
    gains.txgm = txgm; // default: 255
    gains.bbmult = bbmult; // maximum value: 255, for iq transmissions set to 64, normally below
    wlc_phy_txcal_txgain_cleanup_acphy(pi, &gains);
}

static void
exp_set_gains_by_index(struct phy_info *pi, int8 index)
{
    ac_txgain_setting_t gains = { 0 };
    wlc_phy_txpwrctrl_enable_acphy(pi, 0);
    wlc_phy_get_txgain_settings_by_index_acphy(pi, &gains, index);
    wlc_phy_txcal_txgain_cleanup_acphy(pi, &gains);
}

void print_cfg(rtcsi_dev_cfg *cfg){
    printf("\n");
    printf("version: %d\n", cfg->version);
    printf("chanspec: 0x%04x\n", cfg->chanspec);
    printf("tx_gain_idx: %d\n", cfg->tx_gain_idx);
    printf("mode: %d\n", cfg->mode);

    printf("mac: ");
    for(int i = 0; i < MAC_ADDR_LEN; i++){
        printf("%02x ",  cfg->mac[i]);
    }
    printf("\n");

    printf("responder_mac: ");
    for(int i = 0; i < MAX_NUM_RESPONDER; i++){
        printf("\n");
        for(int j= 0; j < MAC_ADDR_LEN; j++){
            printf("%02x ",  cfg->responder_mac[i][j]);
        }
    }
    printf("\n");

    printf("num_responder: %d\n", cfg->num_responder);
    printf("initiator_sounding_period: %d\n", cfg->initiator_sounding_period);
    printf("\n");
    printf("mac_white_list: ");
    for(int i = 0; i < MAX_MAC_WHITE_LIST_LEN; i++){
        printf("\n");
        for(int j= 0; j < MAC_ADDR_LEN; j++){
            printf("%02x ",  cfg->mac_white_list[i][j]);
        }
    }
    printf("\n");
    printf("mac_white_list_len: %d\n", cfg->mac_white_list_len);    
    printf("\n");
}

int 
wlc_ioctl_hook(struct wlc_info *wlc, int cmd, char *arg, int len, void *wlc_if)
{
    int ret = IOCTL_ERROR;
    argprintf_init(arg, len);
    switch(cmd) {
        case 500:   // set csi_collect
        {
            struct params {
                uint16 chanspec;            // chanspec to tune to
                uint8  csi_collect;         // trigger csi collect (1: on, 0: off)
                uint8  core_nss_mask;       // coremask and spatialstream mask
                uint8  use_pkt_filter;      // trigger first packet byte filter (1: on, 0: off)
                uint8  first_pkt_byte;      // first packet byte to filter for
                uint16 n_mac_addr;          // number of mac addresses to filter for (0: off, 1-4: on,use src_mac_0-3)
                uint16 cmp_src_mac_0_0;     // filter src mac 0
                uint16 cmp_src_mac_0_1;
                uint16 cmp_src_mac_0_2;
                uint16 cmp_src_mac_1_0;     // filter src mac 1
                uint16 cmp_src_mac_1_1;
                uint16 cmp_src_mac_1_2;
                uint16 cmp_src_mac_2_0;     // filter src mac 2
                uint16 cmp_src_mac_2_1;
                uint16 cmp_src_mac_2_2;
                uint16 cmp_src_mac_3_0;     // filter src mac 3
                uint16 cmp_src_mac_3_1;
                uint16 cmp_src_mac_3_2;
                uint16 delay;               // delay between extractions in us
            };
            struct params *params = (struct params *) arg;
            // deactivate scanning
            set_scansuppress(wlc, 1);
            // deactivate minimum power consumption
            set_mpc(wlc, 0);
            // set the channel
            set_chanspec(wlc, params->chanspec);
            // disbale retransmission
            set_intioctl(wlc, WLC_SET_LRL, 1);
            set_intioctl(wlc, WLC_SET_SRL, 1);
            // set tx power
            set_intioctl(wlc, WLC_SET_TXPWR, 30);
            // write shared memory
            if (wlc->hw->up && len > 1) {
                wlc_bmac_write_shm(wlc->hw, SHM_CSI_COLLECT * 2, params->csi_collect);
                wlc_bmac_write_shm(wlc->hw, NSSMASK * 2, ((params->core_nss_mask)&0xf0)>>4);
                wlc_bmac_write_shm(wlc->hw, COREMASK * 2, (params->core_nss_mask)&0x0f);
                wlc_bmac_write_shm(wlc->hw, N_CMP_SRC_MAC * 2, params->n_mac_addr);
                wlc_bmac_write_shm(wlc->hw, APPLY_PKT_FILTER * 2, params->use_pkt_filter);
                wlc_bmac_write_shm(wlc->hw, PKT_FILTER_BYTE * 2, params->first_pkt_byte);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_0_0 * 2, params->cmp_src_mac_0_0);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_0_1 * 2, params->cmp_src_mac_0_1);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_0_2 * 2, params->cmp_src_mac_0_2);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_1_0 * 2, params->cmp_src_mac_1_0);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_1_1 * 2, params->cmp_src_mac_1_1);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_1_2 * 2, params->cmp_src_mac_1_2);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_2_0 * 2, params->cmp_src_mac_2_0);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_2_1 * 2, params->cmp_src_mac_2_1);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_2_2 * 2, params->cmp_src_mac_2_2);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_3_0 * 2, params->cmp_src_mac_3_0);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_3_1 * 2, params->cmp_src_mac_3_1);
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_3_2 * 2, params->cmp_src_mac_3_2);
                wlc_bmac_write_shm(wlc->hw, FIFODELAY * 2, params->delay);
                ret = IOCTL_SUCCESS;
            }
            break;
        }
        case 501:   // get csi collect
        {
            if (wlc->hw->up && len > 1) {
                *(uint16 *) arg = wlc_bmac_read_shm(wlc->hw, SHM_CSI_COLLECT * 2);
                ret = IOCTL_SUCCESS;
            }
            break;
        }
        
        case 600:   // load rtcsi parameters and run
        {
            uint8  csi_collect = 1;                         // trigger csi collect (1: on, 0: off)
            uint8  core_nss_mask = 0x11;                    // coremask and spatialstream mask
            uint8  use_pkt_filter = 0;                      // trigger first packet byte filter (1: on, 0: off)
            uint8  first_pkt_byte = 0x08;         // first packet byte to filter for
            uint16 n_mac_addr = 0;                          // number of mac addresses to filter for (0: off, 1-4: on,use src_mac_0-3)
            uint16 delay = 0;                               // delay between extractions in us       

            rtcsi_dev_cfg *cfg = (rtcsi_dev_cfg *) arg;
            cfg->csi_collect_mode = CSI_COLLECT_RTCSI;
            print_cfg(cfg);

            // print_agc_gain_table(wlc->hw->band->pi);
            if (wlc->hw->up && len > 1) {
                wlc_bmac_write_shm(wlc->hw, SHM_CSI_COLLECT * 2, csi_collect);
                wlc_bmac_write_shm(wlc->hw, NSSMASK * 2, ((core_nss_mask)&0xf0)>>4);
                wlc_bmac_write_shm(wlc->hw, COREMASK * 2, (core_nss_mask)&0x0f);
                wlc_bmac_write_shm(wlc->hw, N_CMP_SRC_MAC * 2, cfg->mac_white_list_len <= MAX_MAC_WHITE_LIST_LEN?cfg->mac_white_list_len:MAX_MAC_WHITE_LIST_LEN);

                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_0_0 * 2, *(uint16_t *)(&cfg->mac_white_list[0][0]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_0_1 * 2, *(uint16_t *)(&cfg->mac_white_list[0][2]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_0_2 * 2, *(uint16_t *)(&cfg->mac_white_list[0][4]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_1_0 * 2, *(uint16_t *)(&cfg->mac_white_list[1][0]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_1_1 * 2, *(uint16_t *)(&cfg->mac_white_list[1][2]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_1_2 * 2, *(uint16_t *)(&cfg->mac_white_list[1][4]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_2_0 * 2, *(uint16_t *)(&cfg->mac_white_list[2][0]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_2_1 * 2, *(uint16_t *)(&cfg->mac_white_list[2][2]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_2_2 * 2, *(uint16_t *)(&cfg->mac_white_list[2][4]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_3_0 * 2, *(uint16_t *)(&cfg->mac_white_list[3][0]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_3_1 * 2, *(uint16_t *)(&cfg->mac_white_list[3][2]));
                wlc_bmac_write_shm(wlc->hw, CMP_SRC_MAC_3_2 * 2, *(uint16_t *)(&cfg->mac_white_list[3][4]));

                wlc_bmac_write_shm(wlc->hw, APPLY_PKT_FILTER * 2, use_pkt_filter);
                wlc_bmac_write_shm(wlc->hw, PKT_FILTER_BYTE * 2, first_pkt_byte);
                wlc_bmac_write_shm(wlc->hw, FIFODELAY * 2, delay);
            }

            // deactivate scanning
            set_scansuppress(wlc, 1);
            // deactivate minimum power consumption
            set_mpc(wlc, 0);
            // set the channel
            set_chanspec(wlc, cfg->chanspec);
            // disbale retransmission
            set_intioctl(wlc, WLC_SET_LRL, 1);
            set_intioctl(wlc, WLC_SET_SRL, 1);      
            // Minimun Contention Window
            set_intioctl(wlc, WLC_SET_CWMIN , 1);
            set_intioctl(wlc, WLC_SET_CWMAX, 255);        

            struct phy_info *pi = wlc->hw->band->pi;
            if (cfg->tx_gain_idx==0){
                set_intioctl(wlc, WLC_SET_TXPWR, 30);
            }else{
                exp_set_gains_by_index(pi, cfg->tx_gain_idx);
            }
            
            // print_agc_gain_table(wlc->hw->band->pi);

            if(cfg->mode==RTCSI_MODE_INITIATOR){         //initiator mode
                init_timer(wlc);
            }
            init_rtcsi_dev(cfg);
            timer_flag = TIMER_RUNNING;
            printf("wlc_ioctl 600 called\n");
            ret = IOCTL_SUCCESS;
            break;
        }

        case NEX_READ_OBJMEM:
        {
            set_mpc(wlc, 0);
            if (wlc->hw->up && len >= 4) {
                int addr = ((int *) arg)[0];
                int i = 0;
                for (i = 0; i < len / 4; i++) {
                    wlc_bmac_read_objmem32_objaddr(wlc->hw, addr + i, &((unsigned int *) arg)[i]);
                }
                ret = IOCTL_SUCCESS;
            }
            break;
        }

        default:
            // printf("default\n");
            // printf("cmd: %d\n", cmd);
            // printf("arg: %s\n", arg);

            if(block_escan && cmd==WLC_SET_VAR	&& strncmp(arg, "escan", 5) == 0){
                ret = -26;
            }else{
                ret = wlc_ioctl(wlc, cmd, arg, len, wlc_if);
            }
    }

    return ret;
}

__attribute__((at(0x1F3488, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
__attribute__((at(0x20CD80, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x1F3230, "", CHIP_VER_BCM4358, FW_VER_7_112_300_14)))
__attribute__((at(0x2F0CF8, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
GenericPatch4(wlc_ioctl_hook, wlc_ioctl_hook + 1);
