#ifndef M3UA_H
#define M3UA_H

#include <stdint.h>

#define M3UA_VERSION 1

#define M3UA_CLASS_TRANSFER 1
#define M3UA_CLASS_ASPSM    3
#define M3UA_CLASS_ASPTM    4

#define M3UA_TYPE_DATA      1

#define M3UA_TYPE_ASPUP     1
#define M3UA_TYPE_ASPUP_ACK 4

#define M3UA_TYPE_ASPAC     1
#define M3UA_TYPE_ASPAC_ACK 3

typedef struct {
    uint8_t version;
    uint8_t reserved;
    uint8_t msg_class;
    uint8_t msg_type;
    uint32_t msg_len;
} m3ua_hdr_t;

int m3ua_parse_header(const uint8_t *buf, int len, m3ua_hdr_t *h);
int m3ua_is_data(const m3ua_hdr_t *h);
int m3ua_is_aspup(const m3ua_hdr_t *h);
int m3ua_is_aspac(const m3ua_hdr_t *h);

int m3ua_build_simple_hdr(uint8_t *buf, int buflen, uint8_t msg_class, uint8_t msg_type, int payload_len);
int m3ua_build_aspup_ack(uint8_t *buf, int buflen);
int m3ua_build_aspac_ack(uint8_t *buf, int buflen);

#endif
