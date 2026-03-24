#include "m3ua.h"

#include <arpa/inet.h>
#include <string.h>

int m3ua_parse_header(const uint8_t *buf, int len, m3ua_hdr_t *h) {
  if (len < 8 || !h)
    return -1;

  h->version = buf[0];
  h->reserved = buf[1];
  h->msg_class = buf[2];
  h->msg_type = buf[3];

  uint32_t netlen;
  memcpy(&netlen, buf + 4, 4);
  h->msg_len = ntohl(netlen);

  if (h->version != M3UA_VERSION)
    return -1;
  if ((int)h->msg_len > len)
    return -1;
  if (h->msg_len < 8)
    return -1;

  return 0;
}

int m3ua_is_data(const m3ua_hdr_t *h) {
  return h && h->msg_class == M3UA_CLASS_TRANSFER &&
         h->msg_type == M3UA_TYPE_DATA;
}

int m3ua_is_aspup(const m3ua_hdr_t *h) {
  return h && h->msg_class == M3UA_CLASS_ASPSM &&
         h->msg_type == M3UA_TYPE_ASPUP;
}

int m3ua_is_aspac(const m3ua_hdr_t *h) {
  return h && h->msg_class == M3UA_CLASS_ASPTM &&
         h->msg_type == M3UA_TYPE_ASPAC;
}

int m3ua_build_simple_hdr(uint8_t *buf, int buflen, uint8_t msg_class,
                          uint8_t msg_type, int payload_len) {
  if (!buf || buflen < 8 + payload_len)
    return -1;

  buf[0] = M3UA_VERSION;
  buf[1] = 0;
  buf[2] = msg_class;
  buf[3] = msg_type;

  uint32_t netlen = htonl((uint32_t)(8 + payload_len));
  memcpy(buf + 4, &netlen, 4);

  return 8;
}

int m3ua_build_aspup_ack(uint8_t *buf, int buflen) {
  return m3ua_build_simple_hdr(buf, buflen, M3UA_CLASS_ASPSM,
                               M3UA_TYPE_ASPUP_ACK, 0);
}

int m3ua_build_aspac_ack(uint8_t *buf, int buflen) {
  return m3ua_build_simple_hdr(buf, buflen, M3UA_CLASS_ASPTM,
                               M3UA_TYPE_ASPAC_ACK, 0);
}
