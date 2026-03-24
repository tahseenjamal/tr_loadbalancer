#ifndef SCCP_H
#define SCCP_H

#include <stdint.h>

int sccp_locate_user_data(const uint8_t *buf, int len, const uint8_t **out, int *out_len);

#endif
