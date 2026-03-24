#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

typedef struct {
    int fe_id;

    char tr_listen_ip[MAX_IP_STR];
    int  tr_listen_port;

    char nw_listen_ip[MAX_IP_STR];
    int  nw_listen_port;

    char redis_ip[MAX_IP_STR];
    int  redis_port;

    int  redis_ttl_sec;

    int tr_count;
    struct {
        int  tr_id;
        char ip[MAX_IP_STR];
    } trs[MAX_TRS];

    int upstream_count;
    struct {
        int  upstream_id;
        char ip[MAX_IP_STR];
    } upstreams[MAX_UPSTREAMS];
} fe_config_t;

extern fe_config_t g_cfg;

void config_init_defaults(void);
int config_tr_id_by_ip(const char *ip);
int config_upstream_id_by_ip(const char *ip);

#endif
