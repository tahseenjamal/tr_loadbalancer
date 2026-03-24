#include "sccp.h"

int sccp_locate_user_data(const uint8_t *buf, int len, const uint8_t **out,
                          int *out_len) {
  if (!buf || len < 5 || !out || !out_len)
    return -1;

  /*
   * Minimal heuristic:
   * - Many simple SCCP payload forms can be approximated by using pointer at
   * byte 3
   * - Otherwise pass whole buffer through
   */
  int ptr = buf[3];
  if (ptr > 0 && ptr < len) {
    *out = buf + ptr;
    *out_len = len - ptr;
    return 0;
  }

  *out = buf;
  *out_len = len;
  return 0;
}
