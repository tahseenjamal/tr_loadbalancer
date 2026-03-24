#include "tcap.h"

#include <string.h>

int tcap_extract_tid(const uint8_t *buf, int len, tcap_tid_t *tid) {
  if (!buf || len < 2 || !tid)
    return -1;

  memset(tid, 0, sizeof(*tid));

  uint8_t tag = buf[0];
  tid->is_begin = (tag == 0x62); /* TCAP BEGIN */

  /*
   * Minimal heuristic:
   * - Non-BEGIN: prefer DTID (0x49)
   * - Fallback: OTID (0x48)
   */
  if (!tid->is_begin) {
    for (int i = 0; i + 2 < len; i++) {
      if (buf[i] == 0x49) {
        int l = buf[i + 1];
        if (l > 0 && l <= 8 && i + 2 + l <= len) {
          uint64_t v = 0;
          for (int j = 0; j < l; j++)
            v = (v << 8) | buf[i + 2 + j];
          tid->type = TID_TYPE_DTID;
          tid->tid = v;
          return 0;
        }
      }
    }
  }

  for (int i = 0; i + 2 < len; i++) {
    if (buf[i] == 0x48) {
      int l = buf[i + 1];
      if (l > 0 && l <= 8 && i + 2 + l <= len) {
        uint64_t v = 0;
        for (int j = 0; j < l; j++)
          v = (v << 8) | buf[i + 2 + j];
        tid->type = TID_TYPE_OTID;
        tid->tid = v;
        return 0;
      }
    }
  }

  return -1;
}
