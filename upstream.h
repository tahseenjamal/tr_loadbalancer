#ifndef UPSTREAM_H
#define UPSTREAM_H

void upstream_registry_set_fd(int upstream_id, int fd);
void upstream_registry_mark_down_fd(int fd);
int  upstream_registry_first_fd(void);

#endif
