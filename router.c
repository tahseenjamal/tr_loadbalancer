#include "router.h"

#include "config.h"
#include "m3ua.h"
#include "net.h"
#include "redis_store.h"
#include "sccp.h"
#include "tcap.h"
#include "tr_registry.h"
#include "upstream.h"

#include <stdio.h>
#include <string.h>

static int build_view(uint8_t *buf, int len, packet_view_t *v) {
  memset(v, 0, sizeof(*v));
  v->data = buf;
  v->len = len;

  m3ua_hdr_t mh;
  if (m3ua_parse_header(buf, len, &mh) < 0)
    return -1;
  if (!m3ua_is_data(&mh))
    return -1;

  v->m3ua_payload = buf + 8;
  v->m3ua_payload_len = len - 8;

  if (sccp_locate_user_data(v->m3ua_payload, v->m3ua_payload_len,
                            &v->sccp_user_data, &v->sccp_user_data_len) < 0) {
    return -1;
  }

  if (tcap_extract_tid(v->sccp_user_data, v->sccp_user_data_len, &v->tid) < 0) {
    return -1;
  }

  return 0;
}

static void make_key(char *key, int sz, uint8_t type, uint64_t tid) {
  snprintf(key, (size_t)sz, "%s:%llx", type == TID_TYPE_DTID ? "dtid" : "otid",
           (unsigned long long)tid);
}

int router_handle_tr_packet(int fd, uint8_t *buf, int len) {
  m3ua_hdr_t mh;
  if (m3ua_parse_header(buf, len, &mh) < 0) {
    printf("[FE] invalid M3UA from TR fd=%d\n", fd);
    return -1;
  }

  if (m3ua_is_aspup(&mh)) {
    uint8_t ack[32];
    int alen = m3ua_build_aspup_ack(ack, sizeof(ack));
    if (alen > 0)
      net_send_all(fd, ack, alen);
    printf("[FE] ASPUP from TR%d -> ASPUP_ACK\n", g_fd_ctx[fd].tr_id);
    return 0;
  }

  if (m3ua_is_aspac(&mh)) {
    uint8_t ack[32];
    int alen = m3ua_build_aspac_ack(ack, sizeof(ack));
    if (alen > 0)
      net_send_all(fd, ack, alen);
    printf("[FE] ASPAC from TR%d -> ASPAC_ACK\n", g_fd_ctx[fd].tr_id);
    return 0;
  }

  if (!m3ua_is_data(&mh)) {
    printf("[FE] ignoring non-DATA from TR%d class=%u type=%u\n",
           g_fd_ctx[fd].tr_id, mh.msg_class, mh.msg_type);
    return 0;
  }

  packet_view_t v;
  if (build_view(buf, len, &v) < 0) {
    printf("[FE] DATA from TR%d but no TID found\n", g_fd_ctx[fd].tr_id);
    int upfd = upstream_registry_first_fd();
    if (upfd > 0)
      net_send_all(upfd, buf, len);
    return 0;
  }

  int tr_id = g_fd_ctx[fd].tr_id;
  char key[128];
  make_key(key, sizeof(key), v.tid.type, v.tid.tid);

  if (v.tid.is_begin && v.tid.type == TID_TYPE_OTID) {
    redis_setnx_tr_id(key, tr_id, g_cfg.redis_ttl_sec);
    printf("[FE] claim %s -> tr:%d\n", key, tr_id);
  }

  /*
   * Forward outbound traffic upstream unchanged.
   * FE is inspecting but mostly relaying original M3UA DATA buffer.
   */
  int upfd = upstream_registry_first_fd();
  if (upfd < 0) {
    printf("[FE] no upstream connected, dropping TR%d packet\n", tr_id);
    return -1;
  }

  if (net_send_all(upfd, buf, len) < 0) {
    printf("[FE] failed sending TR%d packet upstream\n", tr_id);
    return -1;
  }

  return 0;
}

int router_handle_upstream_packet(int fd, uint8_t *buf, int len) {
  m3ua_hdr_t mh;
  if (m3ua_parse_header(buf, len, &mh) < 0) {
    printf("[FE] invalid M3UA from upstream fd=%d\n", fd);
    return -1;
  }

  if (!m3ua_is_data(&mh)) {
    printf("[FE] ignoring non-DATA from upstream class=%u type=%u\n",
           mh.msg_class, mh.msg_type);
    return 0;
  }

  packet_view_t v;
  if (build_view(buf, len, &v) < 0) {
    printf("[FE] upstream DATA but no TID found\n");
    return -1;
  }

  /*
   * Prefer DTID for response routing.
   * Fallback to OTID.
   */
  char key[128];
  make_key(key, sizeof(key), v.tid.type, v.tid.tid);

  int tr_id = redis_get_tr_id(key);

  if (tr_id < 0 && v.tid.type == TID_TYPE_DTID) {
    /*
     * Fallback try OTID only if parser preferred DTID but
     * the payload also contains OTID somewhere.
     * V1 simplification: no secondary scan here.
     */
  }

  if (tr_id < 0) {
    printf("[FE] no TR mapping for %s\n", key);
    return -1;
  }

  /*
   * If upstream packet is non-BEGIN and carries a peer-side TID,
   * store/refresh mapping for cross-FE routing continuity.
   */
  redis_set_tr_id(key, tr_id, g_cfg.redis_ttl_sec);

  int trfd = tr_registry_get_fd(tr_id);
  if (trfd < 0) {
    printf("[FE] mapped TR%d is not connected on this FE\n", tr_id);
    return -1;
  }

  if (net_send_all(trfd, buf, len) < 0) {
    printf("[FE] failed sending upstream packet to TR%d\n", tr_id);
    return -1;
  }

  return 0;
}
