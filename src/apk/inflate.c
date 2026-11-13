#include <string.h>

#include "inflate.h"

struct Bits {
    const uint8_t *p;
    const uint8_t *end;
    uint32_t acc;
    int nbits;
};

struct Huff {
    int count[16];
    int first[16];
    int index[16];
    int symbol[320];
};

static int pull(struct Bits *bits, int n, uint32_t *out)
{
    if (n == 0) {
        *out = 0;
        return 0;
    }
    while (bits->nbits < n) {
        if (bits->p >= bits->end) {
            return -1;
        }
        bits->acc |= (uint32_t)(*bits->p++) << bits->nbits;
        bits->nbits += 8;
    }
    *out = bits->acc & ((1u << n) - 1);
    bits->acc >>= n;
    bits->nbits -= n;
    return 0;
}

static int huff_build(struct Huff *huff, const uint8_t *lengths, int nsyms)
{
    int len;
    int sym;
    int code = 0;
    int pos = 0;

    memset(huff, 0, sizeof(*huff));
    for (sym = 0; sym < nsyms; sym++) {
        if (lengths[sym] > 15) {
            return -1;
        }
        huff->count[lengths[sym]]++;
    }
    huff->count[0] = 0;
    for (len = 1; len <= 15; len++) {
        code = (code + huff->count[len - 1]) << 1;
        huff->first[len] = code;
        huff->index[len] = pos;
        pos += huff->count[len];
    }
    memset(huff->count, 0, sizeof(huff->count));
    for (sym = 0; sym < nsyms; sym++) {
        len = lengths[sym];
        if (len != 0) {
            int slot = huff->index[len] + huff->count[len];
            huff->symbol[slot] = sym;
            huff->count[len]++;
        }
    }
    return 0;
}

static int huff_decode(struct Bits *bits, const struct Huff *huff, int *symbol)
{
    int len;
    int code = 0;

    for (len = 1; len <= 15; len++) {
        uint32_t bit;
        int count;
        if (pull(bits, 1, &bit) != 0) {
            return -1;
        }
        code = (code << 1) | (int)bit;
        count = huff->count[len];
        if (code >= huff->first[len] && code < huff->first[len] + count) {
            *symbol = huff->symbol[huff->index[len] + (code - huff->first[len])];
            return 0;
        }
    }
    return -1;
}

static const uint16_t kLenBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static const uint8_t kLenExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static const uint16_t kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

static const uint8_t kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static const uint8_t kClenOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int inflate_codes(struct Bits *bits, uint8_t *dst, size_t dst_len, size_t *out,
                         const struct Huff *lit, const struct Huff *dist)
{
    for (;;) {
        int symbol;
        if (huff_decode(bits, lit, &symbol) != 0) {
            return -1;
        }
        if (symbol < 256) {
            if (*out >= dst_len) {
                return -1;
            }
            dst[(*out)++] = (uint8_t)symbol;
        } else if (symbol == 256) {
            return 0;
        } else if (symbol <= 285) {
            int length_index = symbol - 257;
            uint32_t extra;
            uint32_t distance_extra;
            int distance_symbol;
            size_t length;
            size_t distance;
            size_t i;
            if (length_index >= 29 || pull(bits, kLenExtra[length_index], &extra) != 0 ||
                huff_decode(bits, dist, &distance_symbol) != 0 || distance_symbol >= 30 ||
                pull(bits, kDistExtra[distance_symbol], &distance_extra) != 0) {
                return -1;
            }
            length = (size_t)kLenBase[length_index] + extra;
            distance = (size_t)kDistBase[distance_symbol] + distance_extra;
            if (distance == 0 || distance > *out || *out + length > dst_len) {
                return -1;
            }
            for (i = 0; i < length; i++) {
                dst[*out] = dst[*out - distance];
                (*out)++;
            }
        } else {
            return -1;
        }
    }
}

int raw_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len)
{
    struct Bits bits;
    size_t out = 0;

    bits.p = src;
    bits.end = src + src_len;
    bits.acc = 0;
    bits.nbits = 0;

    for (;;) {
        uint32_t bfinal;
        uint32_t btype;
        if (pull(&bits, 1, &bfinal) != 0 || pull(&bits, 2, &btype) != 0) {
            return -1;
        }
        if (btype == 0) {
            uint32_t len;
            uint32_t nlen;
            bits.acc = 0;
            bits.nbits = 0;
            if (bits.p + 4 > bits.end) {
                return -1;
            }
            len = (uint32_t)bits.p[0] | ((uint32_t)bits.p[1] << 8);
            nlen = (uint32_t)bits.p[2] | ((uint32_t)bits.p[3] << 8);
            bits.p += 4;
            if ((len ^ 0xffffu) != nlen || out + len > dst_len || bits.p + len > bits.end) {
                return -1;
            }
            memcpy(dst + out, bits.p, len);
            bits.p += len;
            out += len;
        } else if (btype == 1 || btype == 2) {
            struct Huff lit;
            struct Huff dist;
            uint8_t lit_len[288];
            uint8_t dist_len[32];
            if (btype == 1) {
                int i;
                for (i = 0; i <= 143; i++) {
                    lit_len[i] = 8;
                }
                for (i = 144; i <= 255; i++) {
                    lit_len[i] = 9;
                }
                for (i = 256; i <= 279; i++) {
                    lit_len[i] = 7;
                }
                for (i = 280; i <= 287; i++) {
                    lit_len[i] = 8;
                }
                for (i = 0; i < 30; i++) {
                    dist_len[i] = 5;
                }
            } else {
                uint32_t hlit;
                uint32_t hdist;
                uint32_t hclen;
                uint8_t clen[19];
                struct Huff clen_huff;
                uint32_t total;
                uint32_t filled = 0;
                int i;
                if (pull(&bits, 5, &hlit) != 0 || pull(&bits, 5, &hdist) != 0 ||
                    pull(&bits, 4, &hclen) != 0) {
                    return -1;
                }
                hlit += 257;
                hdist += 1;
                hclen += 4;
                if (hlit > 286 || hdist > 30 || hclen > 19) {
                    return -1;
                }
                memset(clen, 0, sizeof(clen));
                for (i = 0; i < (int)hclen; i++) {
                    uint32_t length;
                    if (pull(&bits, 3, &length) != 0) {
                        return -1;
                    }
                    clen[kClenOrder[i]] = (uint8_t)length;
                }
                if (huff_build(&clen_huff, clen, 19) != 0) {
                    return -1;
                }
                total = hlit + hdist;
                memset(lit_len, 0, sizeof(lit_len));
                memset(dist_len, 0, sizeof(dist_len));
                while (filled < total) {
                    int symbol;
                    uint32_t repeat;
                    uint8_t value;
                    if (huff_decode(&bits, &clen_huff, &symbol) != 0) {
                        return -1;
                    }
                    if (symbol < 16) {
                        value = (uint8_t)symbol;
                        repeat = 1;
                    } else if (symbol == 16) {
                        if (filled == 0 || pull(&bits, 2, &repeat) != 0) {
                            return -1;
                        }
                        value = filled <= hlit ? lit_len[filled - 1] : dist_len[filled - hlit - 1];
                        repeat += 3;
                    } else if (symbol == 17) {
                        if (pull(&bits, 3, &repeat) != 0) {
                            return -1;
                        }
                        value = 0;
                        repeat += 3;
                    } else if (symbol == 18) {
                        if (pull(&bits, 7, &repeat) != 0) {
                            return -1;
                        }
                        value = 0;
                        repeat += 11;
                    } else {
                        return -1;
                    }
                    if (filled + repeat > total) {
                        return -1;
                    }
                    while (repeat-- > 0) {
                        if (filled < hlit) {
                            lit_len[filled] = value;
                        } else {
                            dist_len[filled - hlit] = value;
                        }
                        filled++;
                    }
                }
            }
            if (huff_build(&lit, lit_len, 288) != 0 || huff_build(&dist, dist_len, 32) != 0 ||
                inflate_codes(&bits, dst, dst_len, &out, &lit, &dist) != 0) {
                return -1;
            }
        } else {
            return -1;
        }
        if (bfinal) {
            return out == dst_len ? 0 : -1;
        }
    }
}
