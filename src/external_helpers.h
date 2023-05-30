#pragma once

#include <string>
inline int base64_encode(const unsigned char *src, size_t src_len, char *dst,
                         size_t *dst_len) {
  static const char *b64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i, j;
  int a, b, c;

  if (dst_len != NULL) {
    /* Expected length including 0 termination: */
    /* IN 1 -> OUT 5, IN 2 -> OUT 5, IN 3 -> OUT 5, IN 4 -> OUT 9,
     * IN 5 -> OUT 9, IN 6 -> OUT 9, IN 7 -> OUT 13, etc. */
    size_t expected_len = ((src_len + 2) / 3) * 4 + 1;
    if (*dst_len < expected_len) {
      if (*dst_len > 0) {
        dst[0] = '\0';
      }
      *dst_len = expected_len;
      return 0;
    }
  }

  for (i = j = 0; i < src_len; i += 3) {
    a = src[i];
    b = ((i + 1) >= src_len) ? 0 : src[i + 1];
    c = ((i + 2) >= src_len) ? 0 : src[i + 2];

    dst[j++] = b64[a >> 2];
    dst[j++] = b64[((a & 3) << 4) | (b >> 4)];
    if (i + 1 < src_len) {
      dst[j++] = b64[(b & 15) << 2 | (c >> 6)];
    }
    if (i + 2 < src_len) {
      dst[j++] = b64[c & 63];
    }
  }
  while (j % 4 != 0) {
    dst[j++] = '=';
  }
  dst[j++] = '\0';

  if (dst_len != NULL) {
    *dst_len = (size_t)j;
  }

  /* Return -1 for "OK" */
  return -1;
}