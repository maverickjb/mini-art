#ifndef MINI_ART_VERIFIER_H
#define MINI_ART_VERIFIER_H

#include <stdint.h>

#include "dex.h"

typedef struct VerifyResult {
    int ok;
    const char *message;
} VerifyResult;

VerifyResult verifier_check(const DexFile *dex, uint32_t method_idx);

#endif
