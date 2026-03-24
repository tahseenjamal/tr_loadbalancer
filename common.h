#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MAX_FDS           65536
#define MAX_EVENTS        128
#define MAX_PKT           8192
#define MAX_TRS           32
#define MAX_UPSTREAMS     8
#define MAX_IP_STR        64

#define FE_ROLE_NONE      0
#define FE_ROLE_TR        1
#define FE_ROLE_UPSTREAM  2
#define FE_ROLE_TR_LISTEN 3
#define FE_ROLE_NW_LISTEN 4

#define TID_TYPE_NONE 0
#define TID_TYPE_OTID 1
#define TID_TYPE_DTID 2

typedef struct {
    int role;
    int tr_id;
    int upstream_id;
} fd_ctx_t;

typedef struct {
    uint8_t type;
    uint64_t tid;
    int is_begin;
} tcap_tid_t;

typedef struct {
    uint8_t *data;
    int len;
    const uint8_t *m3ua_payload;
    int m3ua_payload_len;
    const uint8_t *sccp_user_data;
    int sccp_user_data_len;
    tcap_tid_t tid;
} packet_view_t;

#endif
