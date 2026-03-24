#include "config.h"
#include "net.h"
#include "router.h"
#include "tr_registry.h"
#include "upstream.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>

static int g_epfd = -1;

static void close_fd_full(int fd) {
  if (fd <= 0)
    return;

  net_epoll_del(g_epfd, fd);

  if (fd < MAX_FDS) {
    if (g_fd_ctx[fd].role == FE_ROLE_TR) {
      printf("[FE] TR%d disconnected fd=%d\n", g_fd_ctx[fd].tr_id, fd);
      tr_registry_mark_down_fd(fd);
    } else if (g_fd_ctx[fd].role == FE_ROLE_UPSTREAM) {
      printf("[FE] Upstream%d disconnected fd=%d\n", g_fd_ctx[fd].upstream_id,
             fd);
      upstream_registry_mark_down_fd(fd);
    } else {
      memset(&g_fd_ctx[fd], 0, sizeof(g_fd_ctx[fd]));
    }
  }

  close(fd);
}

static void accept_tr(int listen_fd) {
  char ip[MAX_IP_STR];
  int fd = net_accept_peer_ip(listen_fd, ip, sizeof(ip));
  if (fd < 0)
    return;

  int tr_id = config_tr_id_by_ip(ip);
  if (tr_id < 0) {
    printf("[FE] Reject TR connection from unknown IP %s\n", ip);
    close(fd);
    return;
  }

  tr_registry_set_fd(tr_id, fd);
  net_epoll_add(g_epfd, fd, EPOLLIN);

  printf("[FE] TR%d connected from %s fd=%d\n", tr_id, ip, fd);
}

static void accept_upstream(int listen_fd) {
  char ip[MAX_IP_STR];
  int fd = net_accept_peer_ip(listen_fd, ip, sizeof(ip));
  if (fd < 0)
    return;

  int upstream_id = config_upstream_id_by_ip(ip);
  if (upstream_id < 0) {
    printf("[FE] Reject upstream connection from unknown IP %s\n", ip);
    close(fd);
    return;
  }

  upstream_registry_set_fd(upstream_id, fd);
  net_epoll_add(g_epfd, fd, EPOLLIN);

  printf("[FE] Upstream%d connected from %s fd=%d\n", upstream_id, ip, fd);
}

int main(void) {
  config_init_defaults();
  tr_registry_init();

  printf("Starting FE v1\n");
  printf("TR listen: %s:%d\n", g_cfg.tr_listen_ip, g_cfg.tr_listen_port);
  printf("NW listen: %s:%d\n", g_cfg.nw_listen_ip, g_cfg.nw_listen_port);
  printf("Redis: %s:%d\n", g_cfg.redis_ip, g_cfg.redis_port);

  g_epfd = epoll_create1(0);
  if (g_epfd < 0) {
    perror("epoll_create1");
    return 1;
  }

  int tr_listen_fd =
      net_create_sctp_listener(g_cfg.tr_listen_ip, g_cfg.tr_listen_port);
  if (tr_listen_fd < 0)
    return 1;

  int nw_listen_fd =
      net_create_sctp_listener(g_cfg.nw_listen_ip, g_cfg.nw_listen_port);
  if (nw_listen_fd < 0)
    return 1;

  g_fd_ctx[tr_listen_fd].role = FE_ROLE_TR_LISTEN;
  g_fd_ctx[nw_listen_fd].role = FE_ROLE_NW_LISTEN;

  net_epoll_add(g_epfd, tr_listen_fd, EPOLLIN);
  net_epoll_add(g_epfd, nw_listen_fd, EPOLLIN);

  struct epoll_event events[MAX_EVENTS];

  while (1) {
    int n = epoll_wait(g_epfd, events, MAX_EVENTS, -1);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait");
      break;
    }

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if (fd == tr_listen_fd) {
        accept_tr(tr_listen_fd);
        continue;
      }

      if (fd == nw_listen_fd) {
        accept_upstream(nw_listen_fd);
        continue;
      }

      uint8_t buf[MAX_PKT];
      int r = (int)recv(fd, buf, sizeof(buf), 0);
      if (r <= 0) {
        close_fd_full(fd);
        continue;
      }

      if (fd <= 0 || fd >= MAX_FDS) {
        close_fd_full(fd);
        continue;
      }

      if (g_fd_ctx[fd].role == FE_ROLE_TR) {
        if (router_handle_tr_packet(fd, buf, r) < 0) {
          printf("[FE] TR packet handling failed fd=%d\n", fd);
        }
        continue;
      }

      if (g_fd_ctx[fd].role == FE_ROLE_UPSTREAM) {
        if (router_handle_upstream_packet(fd, buf, r) < 0) {
          printf("[FE] upstream packet handling failed fd=%d\n", fd);
        }
        continue;
      }
    }
  }

  close_fd_full(tr_listen_fd);
  close_fd_full(nw_listen_fd);
  close(g_epfd);
  return 0;
}
