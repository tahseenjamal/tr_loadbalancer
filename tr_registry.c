#include "tr_registry.h"

#include <string.h>

typedef struct {
  int fd;
  int healthy;
} tr_peer_t;

fd_ctx_t g_fd_ctx[MAX_FDS];
static tr_peer_t g_trs[MAX_TRS];
static int g_rr = 0;

void tr_registry_init(void) {
  memset(g_fd_ctx, 0, sizeof(g_fd_ctx));
  memset(g_trs, 0, sizeof(g_trs));
  for (int i = 0; i < MAX_TRS; i++)
    g_trs[i].fd = -1;
}

void tr_registry_set_fd(int tr_id, int fd) {
  if (tr_id <= 0 || tr_id >= MAX_TRS)
    return;
  g_trs[tr_id].fd = fd;
  g_trs[tr_id].healthy = 1;

  if (fd > 0 && fd < MAX_FDS) {
    g_fd_ctx[fd].role = FE_ROLE_TR;
    g_fd_ctx[fd].tr_id = tr_id;
  }
}

void tr_registry_mark_down_fd(int fd) {
  if (fd <= 0 || fd >= MAX_FDS)
    return;

  if (g_fd_ctx[fd].role == FE_ROLE_TR) {
    int tr_id = g_fd_ctx[fd].tr_id;
    if (tr_id > 0 && tr_id < MAX_TRS) {
      g_trs[tr_id].fd = -1;
      g_trs[tr_id].healthy = 0;
    }
  }

  memset(&g_fd_ctx[fd], 0, sizeof(g_fd_ctx[fd]));
}

int tr_registry_get_fd(int tr_id) {
  if (tr_id <= 0 || tr_id >= MAX_TRS)
    return -1;
  if (!g_trs[tr_id].healthy)
    return -1;
  return g_trs[tr_id].fd;
}

int tr_registry_pick_rr(void) {
  for (int i = 1; i < MAX_TRS; i++) {
    int idx = ((g_rr + i - 1) % (MAX_TRS - 1)) + 1;
    if (g_trs[idx].healthy && g_trs[idx].fd > 0) {
      g_rr = idx;
      return idx;
    }
  }
  return -1;
}
