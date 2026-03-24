#include "redis_store.h"

#include "config.h"
#include "net.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int redis_connect_fd(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)g_cfg.redis_port);

  if (inet_pton(AF_INET, g_cfg.redis_ip, &addr.sin_addr) <= 0) {
    close(fd);
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static int redis_readline(int fd, char *buf, int sz) {
  int off = 0;
  while (off + 1 < sz) {
    char c;
    int n = (int)recv(fd, &c, 1, 0);
    if (n <= 0)
      return -1;
    buf[off++] = c;
    if (off >= 2 && buf[off - 2] == '\r' && buf[off - 1] == '\n') {
      buf[off] = 0;
      return off;
    }
  }
  return -1;
}

static int redis_send_cmd(int fd, const char *cmd) {
  return net_send_all(fd, cmd, (int)strlen(cmd));
}

int redis_get_tr_id(const char *key) {
  int fd = redis_connect_fd();
  if (fd < 0)
    return -1;

  char cmd[512];
  int n = snprintf(cmd, sizeof(cmd),
                   "*2\r\n"
                   "$3\r\nGET\r\n"
                   "$%zu\r\n%s\r\n",
                   strlen(key), key);
  if (n <= 0 || n >= (int)sizeof(cmd)) {
    close(fd);
    return -1;
  }

  if (redis_send_cmd(fd, cmd) < 0) {
    close(fd);
    return -1;
  }

  char line[256];
  if (redis_readline(fd, line, sizeof(line)) < 0) {
    close(fd);
    return -1;
  }

  if (strncmp(line, "$-1", 3) == 0) {
    close(fd);
    return -1;
  }

  if (line[0] != '$') {
    close(fd);
    return -1;
  }

  int blen = atoi(line + 1);
  if (blen <= 0 || blen > 64) {
    close(fd);
    return -1;
  }

  char val[128];
  memset(val, 0, sizeof(val));

  int need = blen + 2;
  int off = 0;
  while (off < need) {
    int r = (int)recv(fd, val + off, (size_t)(need - off), 0);
    if (r <= 0) {
      close(fd);
      return -1;
    }
    off += r;
  }
  val[blen] = 0;
  close(fd);

  if (strncmp(val, "tr:", 3) != 0)
    return -1;
  return atoi(val + 3);
}

int redis_setnx_tr_id(const char *key, int tr_id, int ttl_sec) {
  int fd = redis_connect_fd();
  if (fd < 0)
    return -1;

  char value[64];
  snprintf(value, sizeof(value), "tr:%d", tr_id);

  char ttl[32];
  snprintf(ttl, sizeof(ttl), "%d", ttl_sec);

  char cmd[1024];
  int n = snprintf(cmd, sizeof(cmd),
                   "*6\r\n"
                   "$3\r\nSET\r\n"
                   "$%zu\r\n%s\r\n"
                   "$%zu\r\n%s\r\n"
                   "$2\r\nNX\r\n"
                   "$2\r\nEX\r\n"
                   "$%zu\r\n%s\r\n",
                   strlen(key), key, strlen(value), value, strlen(ttl), ttl);
  if (n <= 0 || n >= (int)sizeof(cmd)) {
    close(fd);
    return -1;
  }

  if (redis_send_cmd(fd, cmd) < 0) {
    close(fd);
    return -1;
  }

  char line[256];
  if (redis_readline(fd, line, sizeof(line)) < 0) {
    close(fd);
    return -1;
  }

  close(fd);

  if (strncmp(line, "+OK", 3) == 0)
    return 1;
  if (strncmp(line, "$-1", 3) == 0)
    return 0;
  return 0;
}

int redis_set_tr_id(const char *key, int tr_id, int ttl_sec) {
  int fd = redis_connect_fd();
  if (fd < 0)
    return -1;

  char value[64];
  snprintf(value, sizeof(value), "tr:%d", tr_id);

  char ttl[32];
  snprintf(ttl, sizeof(ttl), "%d", ttl_sec);

  char cmd[1024];
  int n = snprintf(cmd, sizeof(cmd),
                   "*5\r\n"
                   "$3\r\nSET\r\n"
                   "$%zu\r\n%s\r\n"
                   "$%zu\r\n%s\r\n"
                   "$2\r\nEX\r\n"
                   "$%zu\r\n%s\r\n",
                   strlen(key), key, strlen(value), value, strlen(ttl), ttl);
  if (n <= 0 || n >= (int)sizeof(cmd)) {
    close(fd);
    return -1;
  }

  if (redis_send_cmd(fd, cmd) < 0) {
    close(fd);
    return -1;
  }

  char line[256];
  if (redis_readline(fd, line, sizeof(line)) < 0) {
    close(fd);
    return -1;
  }

  close(fd);
  return (strncmp(line, "+OK", 3) == 0) ? 0 : -1;
}
