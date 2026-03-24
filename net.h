#ifndef NET_H
#define NET_H

#include <stdint.h>

int net_set_nonblock(int fd);
int net_create_sctp_listener(const char *ip, int port);
int net_epoll_add(int epfd, int fd, uint32_t events);
int net_epoll_del(int epfd, int fd);
int net_send_all(int fd, const void *buf, int len);
int net_accept_peer_ip(int listen_fd, char *ipbuf, int ipbuf_sz);

#endif
