#include "tcap.h"

#include <string.h>

int tcap_extract_tid(const uint8_t *buf, int len, tcap_tid_t *tid) {
  if (!buf || len < 2 || !tid)
    return -1;

  memset(tid, 0, sizeof(*tid));

  /*
   * We are often called on an SCCP blob, not directly on TCAP.
   * So buf[0] is frequently not 0x62 even when the payload contains
   * a TCAP BEGIN later in the buffer.
   *
   * Strategy:
   * 1. Look for DTID first (non-BEGIN/ongoing dialog case).
   * 2. Look for OTID.
   * 3. If OTID is found, check whether a 0x62 tag appears before it.
   *    If yes, mark is_begin=1.
   */

  /* Prefer DTID first */
  for (int i = 0; i + 2 < len; i++) {
    if (buf[i] == 0x49) {
      int l = buf[i + 1];
      if (l > 0 && l <= 8 && i + 2 + l <= len) {
        uint64_t v = 0;
        for (int j = 0; j < l; j++)
          v = (v << 8) | buf[i + 2 + j];
        tid->type = TID_TYPE_DTID;
        tid->tid = v;
        tid->is_begin = 0;
        return 0;
      }
    }
  }

  /* Then OTID */
  for (int i = 0; i + 2 < len; i++) {
    if (buf[i] == 0x48) {
      int l = buf[i + 1];
      if (l > 0 && l <= 8 && i + 2 + l <= len) {
        uint64_t v = 0;
        for (int j = 0; j < l; j++)
          v = (v << 8) | buf[i + 2 + j];

        tid->type = TID_TYPE_OTID;
        tid->tid = v;
        tid->is_begin = 0;

        /* Check whether a TCAP BEGIN tag exists before this OTID */
        for (int k = 0; k < i; k++) {
          if (buf[k] == 0x62) {
            tid->is_begin = 1;
            break;
          }
        }

        return 0;
      }
    }
  }

  return -1;
}
