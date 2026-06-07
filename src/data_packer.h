// Modified by Fangzhan for RTCSI
#pragma once

#define DATA_PACK_BUFFER_LEN 1024

struct data_pack{
    unsigned int pack_idx;
    unsigned int unpack_idx;
    char data[DATA_PACK_BUFFER_LEN];
}__attribute__((packed));


#define pack(dp, t1, v1)                                                \
({                                                                      \
    if((dp)->pack_idx + sizeof(t1) <= DATA_PACK_BUFFER_LEN ){           \
        *(t1 *)((dp)->data + (dp)->pack_idx) = v1;                      \
        (dp)->pack_idx += sizeof(t1);                                   \
    }                                                                   \
})

#define pack_array(dp, t1, vp1, n)                                      \
({                                                                      \
    if((dp)->pack_idx + sizeof(t1)*n <= DATA_PACK_BUFFER_LEN ){         \
        for(int dp_i=0; dp_i<n; dp_i++){                                \
            *(t1 *)((dp)->data + (dp)->pack_idx) = (t1)vp1[dp_i];       \
            (dp)->pack_idx += sizeof(t1);                               \
        }                                                               \
    }                                                                   \
})

#define unpack(dp, t1, vp1)                                             \
({                                                                      \
    if((dp)->unpack_idx + sizeof(t1) <= DATA_PACK_BUFFER_LEN ){         \
        *vp1 = *(t1 *)((dp)->data + (dp)->unpack_idx);                  \
        (dp)->unpack_idx += sizeof(t1);                                 \
    }                                                                   \
})

#define unpack_array(dp, t1, vp1, n)                                    \
({                                                                      \
    if((dp)->unpack_idx + sizeof(t1)*n <= DATA_PACK_BUFFER_LEN ){       \
        for(int dp_i=0; dp_i<n; dp_i++){                                \
            vp1[dp_i] = *(t1 *)((dp)->data + (dp)->unpack_idx);         \
            (dp)->unpack_idx += sizeof(t1);                             \
        }                                                               \
    }                                                                   \
})
