#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_insns(const CodeItem *code, uint32_t pc, int count, uint16_t *out)
{
    uint32_t total = 0;
    const uint16_t *insns = code_item_insns(code, &total);
    int i;

    if (insns == NULL || pc + (uint32_t)count > total) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        out[i] = insns[pc + (uint32_t)i];
    }
    return 0;
}

static int set_register(JValue *regs, uint16_t count, unsigned reg, JValue value)
{
    if (reg >= count) {
        return -1;
    }
    regs[reg] = value;
    return 0;
}

static int invoke(Runtime *rt, ArtMethod *caller, const uint16_t *words, JValue *regs,
                  uint16_t reg_count, JValue *result)
{
    unsigned argc = words[0] >> 12;
    unsigned fifth = (words[0] >> 8) & 0xf;
    uint32_t method_idx = words[1];
    unsigned sources[5];
    JValue args[5];
    ArtMethod *callee;
    unsigned i;
    JValue ret;

    if (argc > 5) {
        return -1;
    }
    sources[0] = words[2] & 0xf;
    sources[1] = (words[2] >> 4) & 0xf;
    sources[2] = (words[2] >> 8) & 0xf;
    sources[3] = (words[2] >> 12) & 0xf;
    sources[4] = fifth;
    memset(args, 0, sizeof(args));
    for (i = 0; i < argc; i++) {
        if (sources[i] >= reg_count) {
            return -1;
        }
        args[i] = regs[sources[i]];
    }

    callee = runtime_resolve_method(rt, art_method_dex(caller), method_idx);
    if (callee == NULL) {
        return -1;
    }
    memset(&ret, 0, sizeof(ret));
    if (art_method_is_native(callee)) {
        ArtNativeFn fn = art_method_native(callee);
        if (fn == NULL || fn(rt, args, argc, &ret) != 0) {
            return -1;
        }
    } else if (interpreter_invoke(rt, callee, args, &ret) != 0) {
        return -1;
    }
    *result = ret;
    return 0;
}

int interpreter_invoke(Runtime *rt, ArtMethod *method, JValue *args, JValue *result)
{
    const CodeItem *code;
    uint16_t reg_count;
    uint16_t ins;
    JValue *regs = NULL;
    uint32_t pc = 0;
    JValue last_result;
    int rc = -1;
    uint16_t i;

    if (rt == NULL || method == NULL || art_method_is_native(method)) {
        return -1;
    }
    code = dex_file_code_item(art_method_dex(method), art_method_id(method));
    if (code == NULL) {
        return -1;
    }
    reg_count = code_item_registers(code);
    ins = code_item_ins(code);
    if (ins > reg_count || (ins > 0 && args == NULL)) {
        return -1;
    }
    regs = calloc(reg_count ? reg_count : 1, sizeof(*regs));
    if (regs == NULL) {
        return -1;
    }
    for (i = 0; i < ins; i++) {
        regs[reg_count - ins + i] = args[i];
    }
    memset(&last_result, 0, sizeof(last_result));

    for (;;) {
        uint16_t words[3];
        uint8_t op;
        if (read_insns(code, pc, 1, words) != 0) {
            goto out;
        }
        op = words[0] & 0xff;
        if (op == 0x0e) {
            rc = 0;
            goto out;
        }
        if (op == 0x0c || op == 0x0a) {
            JValue value = last_result;
            if (op == 0x0a) {
                memset(&value, 0, sizeof(value));
                value.i = last_result.i;
            }
            if (set_register(regs, reg_count, words[0] >> 8, value) != 0) {
                goto out;
            }
            pc += 1;
            continue;
        }
        if (op == 0x12) {
            JValue value;
            int32_t imm = (words[0] >> 12) & 0xf;
            if (imm & 0x8) {
                imm -= 16;
            }
            memset(&value, 0, sizeof(value));
            value.i = imm;
            if (set_register(regs, reg_count, (words[0] >> 8) & 0xf, value) != 0) {
                goto out;
            }
            pc += 1;
            continue;
        }
        if (op == 0x60) {
            JValue value;
            int32_t field_value;
            if (read_insns(code, pc, 2, words) != 0 ||
                dex_file_field_int(art_method_dex(method), words[1], &field_value) != 0) {
                fprintf(stderr, "unresolved static field\n");
                goto out;
            }
            memset(&value, 0, sizeof(value));
            value.i = field_value;
            if (set_register(regs, reg_count, words[0] >> 8, value) != 0) {
                goto out;
            }
            pc += 2;
            continue;
        }
        if (op == 0x6e || op == 0x6f || op == 0x70 || op == 0x71) {
            if (read_insns(code, pc, 3, words) != 0 ||
                invoke(rt, method, words, regs, reg_count, &last_result) != 0) {
                goto out;
            }
            pc += 3;
            continue;
        }
        fprintf(stderr, "unsupported opcode %#x\n", op);
        goto out;
    }

out:
    free(regs);
    if (rc == 0 && result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    return rc;
}
