#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

int net_set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int net_create_sctp_listener(const char *ip, int port) {
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  if (fd < 0) {
    perror("socket");
    return -1;
  }

  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);

  if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
    perror("inet_pton");
    close(fd);
    return -1;
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(fd);
    return -1;
  }

  if (listen(fd, 128) < 0) {
    perror("listen");
    close(fd);
    return -1;
  }

  if (net_set_nonblock(fd) < 0) {
    perror("set_nonblock");
    close(fd);
    return -1;
  }

  return fd;
}

int net_epoll_add(int epfd, int fd, uint32_t events) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = events;
  ev.data.fd = fd;
  return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int net_epoll_del(int epfd, int fd) {
  return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

int net_send_all(int fd, const void *buf, int len) {
  const uint8_t *p = (const uint8_t *)buf;
  int off = 0;

  while (off < len) {
    int n = (int)send(fd, p + off, (size_t)(len - off), 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return -2;
      return -1;
    }
    if (n == 0)
      return -1;
    off += n;
  }
  return 0;
}

int net_accept_peer_ip(int listen_fd, char *ipbuf, int ipbuf_sz) {
  struct sockaddr_in addr;
  socklen_t alen = sizeof(addr);

  int fd = accept(listen_fd, (struct sockaddr *)&addr, &alen);
  if (fd < 0)
    return -1;

  if (inet_ntop(AF_INET, &addr.sin_addr, ipbuf, (socklen_t)ipbuf_sz) == NULL) {
    close(fd);
    return -1;
  }

  if (net_set_nonblock(fd) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}
