#ifndef TCAP_H
#define TCAP_H

#include "common.h"

int tcap_extract_tid(const uint8_t *buf, int len, tcap_tid_t *tid);

#endif
