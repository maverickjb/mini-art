#ifndef MINI_ART_DEX_INTERNAL_H
#define MINI_ART_DEX_INTERNAL_H

#include <stdint.h>

struct CodeItem {
    const uint16_t *insns;
    uint32_t count;
    uint16_t registers_size;
    uint16_t ins_size;
};

#endif
