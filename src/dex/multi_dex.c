#include "apk.h"
#include "dex.h"

size_t multi_dex_count(const Apk *apk)
{
    return apk_dex_count(apk);
}

int multi_dex_read(const Apk *apk, size_t index, uint8_t **out_bytes, size_t *out_size)
{
    return apk_read_dex(apk, index, out_bytes, out_size);
}
