#ifndef ROUTER_H
#define ROUTER_H

int router_handle_tr_packet(int fd, uint8_t *buf, int len);
int router_handle_upstream_packet(int fd, uint8_t *buf, int len);

#endif
