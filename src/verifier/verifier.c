#include "verifier.h"

VerifyResult verifier_check(const DexFile *dex, uint32_t method_idx)
{
    VerifyResult result;

    (void)dex;
    (void)method_idx;
    result.ok = 0;
    result.message = "verifier is not implemented";
    return result;
}
