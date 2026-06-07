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
#include <nexioctls.h>
#include "rtcsi.h"
#include "data_packer.h"

extern void prepend_ethernet_ipv4_udp_header(struct sk_buff *p);

uint16_t rtcsi_version = 0;
uint16_t rtcsi_collect_mode = CSI_COLLECT_RTCSI_TO_ME;

typedef struct{
    uint8 status;
    uint8 mac[MAC_ADDR_LEN];
    uint8 rssi;
    uint32 tx_cnt;
    uint32 rx_cnt;
}rtcsi_responder;

typedef struct{
    uint16 chanspec;
    uint8 mode;  // initiator/responder/...
    uint8 mac[MAC_ADDR_LEN];
    uint32 timer_cnt;
    uint32 tx_cnt;
    uint32 rx_cnt;
    uint16 consecutive_tx_gap;
}rtcsi_dev_common_status;

typedef struct{
    rtcsi_responder responder[MAX_NUM_RESPONDER]; 
    uint8 num_responder;
    uint8 initiator_sounding_period;
}rtcsi_dev_initiator_status;

typedef struct{
    rtcsi_dev_common_status common_status;
    rtcsi_dev_initiator_status initiator_status;
}rtcsi_dev_status;


rtcsi_dev_status dev_status={0};

// uint8 packet_example[] = {
// 0x08,0x02,0x00,0x00,
// 0x11,0x22,0x33,0x44,0x55,0x66,
// 0x22,0x33,0x44,0x55,0x66,0x77,
// 0x33,0x44,0x55,0x66,0x77,0x88,
// 0x40,0x06,
// 0xDE,0xAD,0xBE,0xEF
// };

void delay_us(uint32_t t){
    // according to expereiments, one for iteration with nop is 1/40M sec
    uint32_t n = t*40;
    for(int i=0; i<n; i++){
        asm("nop");
    }
}

void mac_check(uint8* mac, uint8* my_mac, bool *to_me, bool *broadcast){
    *to_me = 1;
    *broadcast = 1;

    for (int i = 0; i < MAC_ADDR_LEN; i++)
    {
       if(*broadcast == 1 && mac[i] != 0xff){
            *broadcast = 0; 
       }
       if(*to_me == 1 && mac[i] != my_mac[i]){
            *to_me = 0; 
       }
    }
}

void sounding_req_v2(struct wlc_info *wlc, uint8 *mac_1, uint8 *mac_2, uint16 seq, uint8 mcs, int *ret){
    rtcsi_frame mac_frame;
    mac_frame.version = rtcsi_version;
    mac_frame.FC = 0x0208;
    mac_frame.ID = 0x0000; // it will change
    memcpy(mac_frame.mac_1, mac_1, MAC_ADDR_LEN);      //RA
    memcpy(mac_frame.mac_2, mac_2, MAC_ADDR_LEN);      //Ta
    memset(mac_frame.mac_3, 0x00, MAC_ADDR_LEN);   
    mac_frame.seq = 0;
    mac_frame.magic_number = FRAME_MAGIC_NUMBER;
    // reset cursor
    mac_frame.dp.pack_idx = 0;
    // pack CMD
    pack((&mac_frame.dp), uint8, RTCSI_CMD_SOUNDING_REQ);
    // pack seq
    pack((&mac_frame.dp), uint16, seq); 

    int valid_len = sizeof(mac_frame) - (DATA_PACK_BUFFER_LEN-mac_frame.dp.pack_idx);
    sk_buff *p = pkt_buf_get_skb(wlc->osh, valid_len + 202);
    if(p==0){
        //printf("NULL skb pointer!\n");
        *ret = -1;
        return;
    }
    // pull header space
    char *packet_skb = (char *) skb_pull(p, 202);
    // copy packet bytes to buffer
    memcpy(packet_skb, &mac_frame, valid_len);
    // send packet with specific rate
    uint32 rate = RATES_SET_HT(mcs);
    sendframe(wlc, p, 1, rate);
    *ret = 0;
}

void sounding_req_v3(struct wlc_info *wlc, uint8 num_responder, uint8 responder_mac[MAX_NUM_RESPONDER][MAC_ADDR_LEN], uint8 index_responder, uint8 *mac_2, uint16 seq, uint8 mcs, int *ret){
    rtcsi_frame mac_frame;
    mac_frame.version = rtcsi_version;
    mac_frame.FC = 0x0208;
    mac_frame.ID = 0x0000; // it will change
    memcpy(mac_frame.mac_1, responder_mac[index_responder], MAC_ADDR_LEN);      //RA
    memcpy(mac_frame.mac_2, mac_2, MAC_ADDR_LEN);      //Ta
    memset(mac_frame.mac_3, 0x00, MAC_ADDR_LEN);   
    mac_frame.seq = 0;
    mac_frame.magic_number = FRAME_MAGIC_NUMBER;
    // reset cursor
    mac_frame.dp.pack_idx = 0;
    // pack CMD
    pack((&mac_frame.dp), uint8, RTCSI_CMD_SOUNDING_REQ);
    // pack seq
    pack((&mac_frame.dp), uint16, seq); 
    // pack number of responder
    pack((&mac_frame.dp), uint8, num_responder);
    // pack all responder mac addr 
    for(int i=0; i<num_responder; i++){
        pack_array((&mac_frame.dp), uint8, (responder_mac[i]), MAC_ADDR_LEN); 
    }
    // pack index of responder
    pack((&mac_frame.dp), uint8, index_responder);
    int valid_len = sizeof(mac_frame) - (DATA_PACK_BUFFER_LEN-mac_frame.dp.pack_idx);
    sk_buff *p = pkt_buf_get_skb(wlc->osh, valid_len + 202);
    if(p==0){
        //printf("NULL skb pointer!\n");
        *ret = -1;
        return;
    }
    // pull header space
    char *packet_skb = (char *) skb_pull(p, 202);
    // copy packet bytes to buffer
    memcpy(packet_skb, &mac_frame, valid_len);
    // send packet with specific rate
    uint32 rate = RATES_SET_HT(mcs);
    sendframe(wlc, p, 1, rate);
    *ret = 0;
}

void sounding_res_v0(struct wlc_info *wlc, uint8 *mac_1, uint8 *mac_2, uint16 seq, int csi_len, uint8 *csi_data, uint16 rx_tsf_time, int8 rssi, uint8 mcs, int *ret){
    rtcsi_frame mac_frame;
    mac_frame.version = rtcsi_version;
    mac_frame.FC = 0x0208;
    mac_frame.ID = 0x0000; // it will change
    memcpy(mac_frame.mac_1, mac_1, MAC_ADDR_LEN);      //RA
    memcpy(mac_frame.mac_2, mac_2, MAC_ADDR_LEN);      //Ta
    memset(mac_frame.mac_3, 0x00, MAC_ADDR_LEN);   
    mac_frame.seq = 0;
    mac_frame.magic_number = FRAME_MAGIC_NUMBER;
    // reset cursor
    mac_frame.dp.pack_idx = 0;
    // pack CMD
    pack((&mac_frame.dp), uint8, RTCSI_CMD_SOUNDING_RES);
    // pack seq
    pack((&mac_frame.dp), uint16, seq); 
    // pack timestamp
    pack((&mac_frame.dp), uint16, rx_tsf_time); 
    // pack rssi
    pack((&mac_frame.dp), int8, rssi); 
    // pack csi data
    pack_array((&mac_frame.dp), uint8, csi_data, csi_len); 

    int valid_len = sizeof(mac_frame) - (DATA_PACK_BUFFER_LEN-mac_frame.dp.pack_idx);
    sk_buff *p = pkt_buf_get_skb(wlc->osh, valid_len + 202);
    if(p==0){
        //printf("NULL skb pointer!\n");
        *ret = -1;
        return;
    }
    // pull header space
    char *packet_skb = (char *) skb_pull(p, 202);
    // copy packet bytes to buffer
    memcpy(packet_skb, &mac_frame, valid_len);
    // send packet with specific rate
    uint32 rate = RATES_SET_HT(mcs);
    sendframe(wlc, p, 1, rate);
    *ret = 0;
}



void sounding_res_v2(struct wlc_info *wlc, uint8 *mac_1, uint8 *mac_2, uint16 seq, int csi_len, uint8 *csi_data, uint16 rx_tsf_time, uint32 tsf_l, int8 rssi, rx_phy_info_v2 *rx_phy, uint8 mcs, int *ret){
    rtcsi_frame mac_frame;
    mac_frame.version = rtcsi_version;
    mac_frame.FC = 0x0208;
    mac_frame.ID = 0x0000; // it will change
    memcpy(mac_frame.mac_1, mac_1, MAC_ADDR_LEN);      //RA
    memcpy(mac_frame.mac_2, mac_2, MAC_ADDR_LEN);      //Ta
    memset(mac_frame.mac_3, 0x00, MAC_ADDR_LEN);   
    mac_frame.seq = 0;
    mac_frame.magic_number = FRAME_MAGIC_NUMBER;
    // reset cursor
    mac_frame.dp.pack_idx = 0;
    // pack CMD
    pack((&mac_frame.dp), uint8, RTCSI_CMD_SOUNDING_RES);
    // pack seq
    pack((&mac_frame.dp), uint16, seq); 
    // pack timestamp
    pack((&mac_frame.dp), uint16, rx_tsf_time); 
    // pack tsf_l
    pack((&mac_frame.dp), uint32, tsf_l); 
    // pack rssi
    pack((&mac_frame.dp), int8, rssi); 
    // pack rx_phy
    pack((&mac_frame.dp), rx_phy_info_v2, *rx_phy); 
    // pack csi len
    pack((&mac_frame.dp), uint16, csi_len); 
    // pack csi data
    pack_array((&mac_frame.dp), uint8, csi_data, csi_len); 

    int valid_len = sizeof(mac_frame) - (DATA_PACK_BUFFER_LEN-mac_frame.dp.pack_idx);
    sk_buff *p = pkt_buf_get_skb(wlc->osh, valid_len + 202);
    if(p==0){
        //printf("NULL skb pointer!\n");
        *ret = -1;
        return;
    }
    // pull header space
    char *packet_skb = (char *) skb_pull(p, 202);
    // copy packet bytes to buffer
    memcpy(packet_skb, &mac_frame, valid_len);
    // send packet with specific rate
    uint32 rate = RATES_SET_HT(mcs);
    sendframe(wlc, p, 1, rate);
    *ret = 0;
}

void sounding_res_v3(struct wlc_info *wlc, uint8 *mac_1, uint8 *mac_2, uint16 seq, int csi_len, uint8 *csi_data, uint16 rx_tsf_time, uint32 tsf_l, int8 rssi, rx_phy_info_v3 *rx_phy, uint8 mcs, int *ret){
    sounding_res_v2(wlc, mac_1,  mac_2,  seq, csi_len, csi_data, rx_tsf_time, tsf_l, rssi, rx_phy, mcs, ret);
}

void send_simple_frame(struct wlc_info *wlc, uint8 addr){
    rtcsi_frame mac_frame;
    mac_frame.version = rtcsi_version;
    mac_frame.FC = 0x0208;
    mac_frame.ID = 0x0000; // it will change
    memset(mac_frame.mac_1, addr, MAC_ADDR_LEN);      //RA
    memcpy(mac_frame.mac_2, dev_status.common_status.mac, MAC_ADDR_LEN);      //Ta
    memset(mac_frame.mac_3, 0x00, MAC_ADDR_LEN);   
    mac_frame.seq = 0;
    mac_frame.magic_number = FRAME_MAGIC_NUMBER;
    // reset cursor
    mac_frame.dp.pack_idx = 0;

    int valid_len = sizeof(mac_frame) - (DATA_PACK_BUFFER_LEN-mac_frame.dp.pack_idx);
    valid_len = 256;
    sk_buff *p = pkt_buf_get_skb(wlc->osh, valid_len + 202);
    if(p==0){
        //printf("NULL skb pointer!\n");
        return;
    }
    // pull header space
    char *packet_skb = (char *) skb_pull(p, 202);
    // copy packet bytes to buffer
    memcpy(packet_skb, &mac_frame, valid_len);
    // send packet with specific rate
    uint32 rate = RATES_SET_HT(1);
    sendframe(wlc, p, 1, rate);
}

void on_rtcsi_dev_timer_event(struct wlc_info *wlc){
#ifdef REPLY_TEST
    send_simple_frame(wlc, 0xff);
#else
    dev_status.common_status.timer_cnt++;
    if(dev_status.common_status.mode==RTCSI_MODE_INITIATOR){         //initiator mode
        // according to test, we cannot send two packet in one short window
        if(dev_status.initiator_status.initiator_sounding_period!=0
            && dev_status.common_status.timer_cnt%dev_status.initiator_status.initiator_sounding_period==0){   //send sounding req every Xms
            int ret;
            switch (rtcsi_version){
                case 0:
                {
                    break;  
                }
                case 1:
                {
                    break;  
                }
                case 2:{
                    uint8 responder_idx = dev_status.common_status.timer_cnt/dev_status.initiator_status.initiator_sounding_period%dev_status.initiator_status.num_responder;
                    uint8 *temp_responder_mac = dev_status.initiator_status.responder[responder_idx].mac;
                    // printf("dev_status.common_status.tx_cnt: %d\n", dev_status.common_status.tx_cnt);
                    // printf("responder_idx: %d\n", responder_idx);
                    sounding_req_v2(wlc, temp_responder_mac, dev_status.common_status.mac, dev_status.common_status.tx_cnt, 4, &ret);
                    break;  
                }
                case 3:
                {
                    uint8 responder_idx = 0;
                    uint8 responder_mac[MAX_NUM_RESPONDER][MAC_ADDR_LEN];
                    for(int i = 0; i < dev_status.initiator_status.num_responder; i++){
                        memcpy(responder_mac[i], dev_status.initiator_status.responder[i].mac, MAC_ADDR_LEN);
                    }
                    sounding_req_v3(wlc, dev_status.initiator_status.num_responder, responder_mac, responder_idx, dev_status.common_status.mac, dev_status.common_status.tx_cnt, 4, &ret);
                    break;   
                }
                default:
                    printf("unknown version!\n");
            } 
            if(ret == 0){
                dev_status.common_status.tx_cnt++;
            }
        }       
    }
#endif
}

void report_via_udp_v2(struct wlc_info *wlc, rtcsi_frame_meta_data_v2 *meta_data, uint8 *mac_data, uint8 *csi_data){
    struct wl_info *wl = wlc->wl;
    int payload_len = sizeof(*meta_data) + meta_data->mac_data_len + meta_data->csi_data_len;
    struct sk_buff *p = pkt_buf_get_skb(wlc->osh, sizeof(rtcsi_udp_frame) + payload_len);
    if (p == 0) {
        return;
    }
    rtcsi_udp_frame *udp_frame = (rtcsi_udp_frame *) p->data;
    udp_frame->magic_number = FRAME_MAGIC_NUMBER;
    udp_frame->version = rtcsi_version;

    
    // a hack here, we set the mac_addr_3 in mac as the local mac addr,
    // it is a VERY BAD implementation since it modifies the raw data and even breaks the FCS!
    rtcsi_frame *mac_frame = (rtcsi_frame *) mac_data;
    memcpy(mac_frame->mac_3, dev_status.common_status.mac, MAC_ADDR_LEN); 

    int idx = 0;
    memcpy(udp_frame->payload+idx, meta_data, sizeof(*meta_data));
    idx += sizeof(*meta_data);
    memcpy(udp_frame->payload+idx, csi_data, meta_data->csi_data_len);
    idx += meta_data->csi_data_len;
    memcpy(udp_frame->payload+idx, mac_data, meta_data->mac_data_len);
    idx += meta_data->mac_data_len;

    skb_pull(p, sizeof(struct ethernet_ip_udp_header));
    prepend_ethernet_ipv4_udp_header(p);

    wl->dev->chained->funcs->xmit(wl->dev, wl->dev->chained, p);
}

void report_via_udp_v3(struct wlc_info *wlc, rtcsi_frame_meta_data_v3 *meta_data, uint8 *mac_data, uint8 *csi_data){
    report_via_udp_v2(wlc, meta_data, mac_data, csi_data);
}



void on_rtcsi_dev_frame_receive(struct wlc_info *wlc, void *meta_data, uint8 *mac_data, uint8 *csi_data){
#ifdef REPLY_TEST
    rtcsi_frame *temp_frame = (rtcsi_frame *)mac_data;
    if (temp_frame->mac_1[0] == 0xff){
        send_simple_frame(wlc, 0xf0);
    } 
#else
    rtcsi_frame *temp_frame = (rtcsi_frame *)mac_data;
    if (FRAME_MAGIC_NUMBER!=temp_frame->magic_number){
        return;
    }
    if(rtcsi_version!=temp_frame->version){
        printf("version mismatch! rtcsi_version: %d, recv_version: %d\n", rtcsi_version, temp_frame->version);
        return;
    }

    bool to_me, broadcast;
    mac_check(temp_frame->mac_1, dev_status.common_status.mac, &to_me, &broadcast);

    int temp_frame_len = temp_frame->dp.pack_idx;
    // reset unpack cursor
    temp_frame->dp.unpack_idx = 0;
    // unpack CMD
    uint8 cmd;
    unpack((&temp_frame->dp), uint8, &cmd);
    uint16 seq;
    unpack((&temp_frame->dp), uint16, &seq);
    switch (rtcsi_version){
        case 0:
        {
            break;
        }
        case 1:
        {   
            break;
        }
        case 2:
        {
            if(dev_status.common_status.mode==RTCSI_MODE_RESPONDER){    
                if(to_me && cmd==RTCSI_CMD_SOUNDING_REQ){
                    // sounding req, send res
                    int ret;
                    sounding_res_v2(wlc, temp_frame->mac_2, dev_status.common_status.mac, seq, 
                        ((rtcsi_frame_meta_data_v2 *)meta_data)->csi_data_len, csi_data, ((rtcsi_frame_meta_data_v2 *)meta_data)->rx_tsf_time, ((rtcsi_frame_meta_data_v2 *)meta_data)->tsf_l,
                        ((rtcsi_frame_meta_data_v2 *)meta_data)->rssi, &((rtcsi_frame_meta_data_v2 *)meta_data)->rx_phy, 4, &ret);
                    if(ret == 0){
                        dev_status.common_status.tx_cnt++;
                    }
                }      
            }
            break;
        }
        case 3:
        { 
            if(to_me && cmd==RTCSI_CMD_SOUNDING_REQ){
                // 1 send response
                int ret;
                sounding_res_v3(wlc, temp_frame->mac_2, dev_status.common_status.mac, seq, 
                        ((rtcsi_frame_meta_data_v3 *)meta_data)->csi_data_len, csi_data, ((rtcsi_frame_meta_data_v3 *)meta_data)->rx_tsf_time, ((rtcsi_frame_meta_data_v3 *)meta_data)->tsf_l,
                        ((rtcsi_frame_meta_data_v3 *)meta_data)->rssi, &((rtcsi_frame_meta_data_v3 *)meta_data)->rx_phy, 4, &ret);

                // a delay is required, the next responder would recevie the res and process it, it will be busy and cannot process the req to it in time
                delay_us(dev_status.common_status.consecutive_tx_gap);

                // 2 send req to next responder (if required)
                if(dev_status.common_status.mode==RTCSI_MODE_RESPONDER){
                    uint8 num_responder=0;
                    unpack((&temp_frame->dp), uint8, &num_responder);
                    uint8 responder_mac[MAX_NUM_RESPONDER][MAC_ADDR_LEN];
                    for(int i=0; i<num_responder; i++){
                        unpack_array((&temp_frame->dp), uint8, responder_mac[i], MAC_ADDR_LEN); 
                    }
                    uint8 index_responder=0;
                    unpack((&temp_frame->dp), uint8, &index_responder);
                    index_responder++;
                    if(index_responder < num_responder){
                        sounding_req_v3(wlc, num_responder, responder_mac, index_responder, dev_status.common_status.mac, seq, 4, &ret);
                    }
                }
            }
            break;
        }
        default:
            printf("unknown version!");
    }

    // Send response
    // Upload report_via_udp
    switch (rtcsi_version){
        case 0:
        {
            break;
        }
        case 1:
        {
            break;
        }
        case 2:
        {
            if((rtcsi_collect_mode == CSI_COLLECT_RTCSI_TO_ME) && (FRAME_MAGIC_NUMBER==temp_frame->magic_number) && (to_me || broadcast) && cmd==RTCSI_CMD_SOUNDING_RES){
                report_via_udp_v2(wlc, meta_data, mac_data, csi_data);
            }else if((rtcsi_collect_mode == CSI_COLLECT_RTCSI) && (FRAME_MAGIC_NUMBER==temp_frame->magic_number)){
                report_via_udp_v2(wlc, meta_data, mac_data, csi_data);
            }else if((rtcsi_collect_mode == CSI_COLLECT_ALL)){
                report_via_udp_v2(wlc, meta_data, mac_data, csi_data);
            }else{
                // Do nothing
            }
            break;
        }
        case 3:
        {
            if((rtcsi_collect_mode == CSI_COLLECT_RTCSI_TO_ME) && (FRAME_MAGIC_NUMBER==temp_frame->magic_number) && (to_me || broadcast) && cmd==RTCSI_CMD_SOUNDING_RES){
                report_via_udp_v3(wlc, meta_data, mac_data, csi_data);
            }else if((rtcsi_collect_mode == CSI_COLLECT_RTCSI) && (FRAME_MAGIC_NUMBER==temp_frame->magic_number)){
                report_via_udp_v3(wlc, meta_data, mac_data, csi_data);
            }else if((rtcsi_collect_mode == CSI_COLLECT_ALL)){
                report_via_udp_v3(wlc, meta_data, mac_data, csi_data);
            }else{
                // Do nothing
            }
            break;
        }   
    }
    return;
#endif
}

void init_rtcsi_dev(rtcsi_dev_cfg *dev_cfg_in){
    dev_status.common_status.chanspec = dev_cfg_in->chanspec;
    dev_status.common_status.mode = dev_cfg_in->mode;
    dev_status.common_status.consecutive_tx_gap = dev_cfg_in->consecutive_tx_gap;
    memcpy(dev_status.common_status.mac, dev_cfg_in->mac, MAC_ADDR_LEN);
    for(int i=0; i<MAX_NUM_RESPONDER; i++){
        memcpy(dev_status.initiator_status.responder[i].mac, dev_cfg_in->responder_mac[i], MAC_ADDR_LEN);
    }
    dev_status.initiator_status.initiator_sounding_period = dev_cfg_in->initiator_sounding_period;
    dev_status.initiator_status.num_responder = dev_cfg_in->num_responder;
    rtcsi_version = dev_cfg_in->version;
    rtcsi_collect_mode = dev_cfg_in->csi_collect_mode;
}
