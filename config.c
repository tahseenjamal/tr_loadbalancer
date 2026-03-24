#include "config.h"

#include <stdio.h>
#include <string.h>

fe_config_t g_cfg;

void config_init_defaults(void) {
  memset(&g_cfg, 0, sizeof(g_cfg));

  g_cfg.fe_id = 1;

  snprintf(g_cfg.tr_listen_ip, sizeof(g_cfg.tr_listen_ip), "0.0.0.0");
  g_cfg.tr_listen_port = 2905;

  snprintf(g_cfg.nw_listen_ip, sizeof(g_cfg.nw_listen_ip), "0.0.0.0");
  g_cfg.nw_listen_port = 2915;

  snprintf(g_cfg.redis_ip, sizeof(g_cfg.redis_ip), "127.0.0.1");
  g_cfg.redis_port = 6379;
  g_cfg.redis_ttl_sec = 120;

  /*
   * Example static IP -> TR mapping.
   * Adjust these to your TR source IPs.
   */
  g_cfg.tr_count = 2;
  g_cfg.trs[0].tr_id = 1;
  snprintf(g_cfg.trs[0].ip, sizeof(g_cfg.trs[0].ip), "127.0.0.11");
  g_cfg.trs[1].tr_id = 2;
  snprintf(g_cfg.trs[1].ip, sizeof(g_cfg.trs[1].ip), "127.0.0.12");

  /*
   * Example upstream peer IP mapping.
   * Adjust to your upstream telecom/STP source IPs.
   */
  g_cfg.upstream_count = 1;
  g_cfg.upstreams[0].upstream_id = 1;
  snprintf(g_cfg.upstreams[0].ip, sizeof(g_cfg.upstreams[0].ip), "127.0.0.21");
}

int config_tr_id_by_ip(const char *ip) {
  for (int i = 0; i < g_cfg.tr_count; i++) {
    if (strcmp(g_cfg.trs[i].ip, ip) == 0)
      return g_cfg.trs[i].tr_id;
  }
  return -1;
}

int config_upstream_id_by_ip(const char *ip) {
  for (int i = 0; i < g_cfg.upstream_count; i++) {
    if (strcmp(g_cfg.upstreams[i].ip, ip) == 0)
      return g_cfg.upstreams[i].upstream_id;
  }
  return -1;
}
