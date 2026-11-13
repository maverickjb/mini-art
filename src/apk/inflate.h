#ifndef MINI_ART_INFLATE_H
#define MINI_ART_INFLATE_H

#include <stddef.h>
#include <stdint.h>

/* Raw DEFLATE. Returns 0 when the output length is exactly dst_len. */
int raw_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);

#endif
