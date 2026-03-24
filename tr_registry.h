#ifndef TR_REGISTRY_H
#define TR_REGISTRY_H

#include "common.h"

void tr_registry_init(void);
void tr_registry_set_fd(int tr_id, int fd);
void tr_registry_mark_down_fd(int fd);
int  tr_registry_get_fd(int tr_id);
int  tr_registry_pick_rr(void);

extern fd_ctx_t g_fd_ctx[MAX_FDS];

#endif
