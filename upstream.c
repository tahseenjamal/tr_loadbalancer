#include "upstream.h"

#include "common.h"
#include "tr_registry.h"

#include <string.h>

typedef struct {
  int fd;
  int healthy;
} upstream_peer_t;

static upstream_peer_t g_upstreams[MAX_UPSTREAMS];

void upstream_registry_set_fd(int upstream_id, int fd) {
  if (upstream_id <= 0 || upstream_id >= MAX_UPSTREAMS)
    return;

  g_upstreams[upstream_id].fd = fd;
  g_upstreams[upstream_id].healthy = 1;

  if (fd > 0 && fd < MAX_FDS) {
    g_fd_ctx[fd].role = FE_ROLE_UPSTREAM;
    g_fd_ctx[fd].upstream_id = upstream_id;
  }
}

void upstream_registry_mark_down_fd(int fd) {
  if (fd <= 0 || fd >= MAX_FDS)
    return;

  if (g_fd_ctx[fd].role == FE_ROLE_UPSTREAM) {
    int id = g_fd_ctx[fd].upstream_id;
    if (id > 0 && id < MAX_UPSTREAMS) {
      g_upstreams[id].fd = -1;
      g_upstreams[id].healthy = 0;
    }
  }

  memset(&g_fd_ctx[fd], 0, sizeof(g_fd_ctx[fd]));
}

int upstream_registry_first_fd(void) {
  for (int i = 1; i < MAX_UPSTREAMS; i++) {
    if (g_upstreams[i].healthy && g_upstreams[i].fd > 0) {
      return g_upstreams[i].fd;
    }
  }
  return -1;
}
