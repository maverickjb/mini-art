#include "dex.h"

#include "dex_internal.h"

const uint16_t *code_item_insns(const CodeItem *code, uint32_t *out_count)
{
    if (code == NULL) {
        if (out_count != NULL) {
            *out_count = 0;
        }
        return NULL;
    }
    if (out_count != NULL) {
        *out_count = code->count;
    }
    return code->insns;
}

uint16_t code_item_registers(const CodeItem *code)
{
    if (code == NULL) {
        return 0;
    }
    return code->registers_size;
}

uint16_t code_item_ins(const CodeItem *code)
{
    if (code == NULL) {
        return 0;
    }
    return code->ins_size;
}
