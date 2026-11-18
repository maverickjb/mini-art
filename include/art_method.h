#ifndef MINI_ART_ART_METHOD_H
#define MINI_ART_ART_METHOD_H

#include "runtime.h"

struct ArtMethod {
    DexFile *dex;
    uint32_t method_idx;
    uint32_t access_flags;
    uint32_t code_off;
    const char *name;
    const char *proto;
    int is_native;
    ArtNativeFn fn;
    struct ArtMethod *next;
};

#endif
