// Modified by Fangzhan for RTCSI
#pragma once

// this header file is used for master processing, this we need some special treatment
#if __has_include (<types.h>)
#include <types.h>
#else 
#include <stdint.h>
#endif

#if __has_include ("structs.h")
#include "structs.h"
#else
#warning "structs.h" not exist. This warnning can be ignored when compiling master program.
#endif

#include "data_packer.h"

#define MAC_ADDR_LEN 6
#define MAX_NUM_RESPONDER 8
#define MAX_MAC_WHITE_LIST_LEN 4
#define MAX_CSI_LEN 1024
#define FRAME_MAGIC_NUMBER 0x0ff0
// #define REPLY_TEST
// #define READ_GAIN

extern uint16_t rtcsi_version;
extern uint8_t block_escan;

enum NIC_FRAME_TYPE{
    NIC_FRAME_TYPE_MAC,
    NIC_FRAME_TYPE_CSI
};

enum RTCSI_MODE{
    RTCSI_MODE_INITIATOR,
    RTCSI_MODE_RESPONDER,
    RTCSI_MODE_LISTENER
};

enum RTCSI_CMD{
    DUMMY_1,
    DUMMY_2,
    RTCSI_CMD_SOUNDING_REQ,
    RTCSI_CMD_SOUNDING_RES
};

enum CSI_COLLECT_MODE{
    CSI_COLLECT_RTCSI_TO_ME,
    CSI_COLLECT_RTCSI,
    CSI_COLLECT_ALL
};

// RTCSI Device Config
typedef struct{
    uint16_t version;
    uint16_t chanspec;
    uint8_t tx_gain_idx;
    uint8_t mode;  // initiator/responder/...
    uint8_t mac[MAC_ADDR_LEN];
    uint8_t responder_mac[MAX_NUM_RESPONDER][MAC_ADDR_LEN]; 
    uint8_t num_responder;
    uint8_t initiator_sounding_period;
    uint8_t mac_white_list[MAX_MAC_WHITE_LIST_LEN][MAC_ADDR_LEN]; 
    uint8_t mac_white_list_len;
    uint16_t consecutive_tx_gap;
    uint8_t csi_collect_mode;
}__attribute__((packed)) rtcsi_dev_cfg;

typedef struct{
    uint8_t cfo_reg;
    uint16_t PhyStatsGainInfo0;
    uint16_t Auxphystats0;
    uint8_t tr_loss_reg;
    uint8_t agc_gain_reg;
    uint8_t pad_1;
    uint8_t pad_2;
}__attribute__((packed)) rx_phy_info_v2;

typedef struct{
    uint16_t version;
    uint64_t master_timestamp; //ms
    uint16_t rx_tsf_time; //us
    uint32_t tsf_l; //us
    int8_t rssi;
    rx_phy_info_v2 rx_phy;
    uint16_t csiconf;
    uint16_t chanspec; 
    uint16_t chip;
    uint32_t mac_data_len;
    uint32_t csi_data_len;
}__attribute__((packed)) rtcsi_frame_meta_data_v2;

typedef rx_phy_info_v2 rx_phy_info_v3;
typedef rtcsi_frame_meta_data_v2 rtcsi_frame_meta_data_v3;

// RTCSI UDP streaming frame (to linux userspace)
typedef struct{
    #ifdef STRUCTS_H
    struct ethernet_ip_udp_header hdrs;
    #endif
    uint16_t magic_number;
    uint16_t version;
    uint8_t payload[];
} __attribute__((packed)) rtcsi_udp_frame;


// 802.11 frame
typedef struct{
    uint16_t FC;
    uint16_t ID;
    uint8_t mac_1[MAC_ADDR_LEN];
    uint8_t mac_2[MAC_ADDR_LEN];
    uint8_t mac_3[MAC_ADDR_LEN];
    uint16_t seq;
    uint16_t magic_number;
    uint16_t version;
    struct data_pack dp;
}__attribute__((packed)) rtcsi_frame;

void init_rtcsi_dev(rtcsi_dev_cfg *dev_cfg_in);
void on_rtcsi_dev_timer_event(struct wlc_info *wlc);
void on_rtcsi_dev_frame_receive(struct wlc_info *wlc, void *meta_data, uint8_t *mac_data, uint8_t *csi_data);
void delay_us(uint32_t t);