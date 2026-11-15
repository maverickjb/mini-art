#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    OP_MOVE_RESULT = 0x0a,
    OP_MOVE_RESULT_WIDE = 0x0b,
    OP_MOVE_RESULT_OBJECT = 0x0c,
    OP_RETURN_VOID = 0x0e,
    OP_RETURN = 0x0f,
    OP_RETURN_WIDE = 0x10,
    OP_RETURN_OBJECT = 0x11,
    OP_CONST_4 = 0x12,
    OP_SGET = 0x60,
    OP_INVOKE_VIRTUAL = 0x6e,
    OP_INVOKE_SUPER = 0x6f,
    OP_INVOKE_DIRECT = 0x70,
    OP_INVOKE_STATIC = 0x71
};

/* One interpreted call. result holds the latest invoke return until a
 * return instruction replaces it with this method's own result. */
typedef struct {
    Runtime *runtime;
    ArtMethod *method;
    const uint16_t *insns;
    uint32_t insn_count;
    JValue *registers;
    uint16_t register_count;
    uint32_t pc;
    JValue result;
} InterpreterFrame;

static int fetch_u16(InterpreterFrame *frame, uint16_t *value)
{
    if (frame->insns == NULL || frame->pc >= frame->insn_count) {
        return -1;
    }
    *value = frame->insns[frame->pc++];
    return 0;
}

static int set_register(InterpreterFrame *frame, unsigned reg, JValue value)
{
    if (reg >= frame->register_count) {
        return -1;
    }
    frame->registers[reg] = value;
    return 0;
}

static int set_int_register(InterpreterFrame *frame, unsigned reg, int32_t value)
{
    JValue slot;

    memset(&slot, 0, sizeof(slot));
    slot.i = value;
    return set_register(frame, reg, slot);
}

static int execute_const_4(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned reg = (instruction >> 8) & 0x0f;
    int32_t value = (instruction >> 12) & 0x0f;

    if (value & 0x08) {
        value -= 16;
    }
    fprintf(stderr, "const/4 v%u, #%d\n", reg, value);
    return set_int_register(frame, reg, value);
}

/* Integer static fields only. The field index is the next code unit. */
static int execute_sget(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t field_idx;
    unsigned dest = instruction >> 8;
    int32_t field_value;
    const char *class_desc = "?";
    const char *name = "?";

    if (fetch_u16(frame, &field_idx) != 0 ||
        dex_file_field_int(art_method_dex(frame->method), field_idx, &field_value) != 0) {
        fprintf(stderr, "unresolved static field\n");
        return -1;
    }
    dex_file_field_at(art_method_dex(frame->method), field_idx, &class_desc, &name, NULL);
    fprintf(stderr, "sget v%u %s->%s = %#x\n", dest, class_desc, name, (unsigned)field_value);
    return set_int_register(frame, dest, field_value);
}

static int execute_move_result(InterpreterFrame *frame, uint16_t instruction, uint8_t opcode)
{
    unsigned dest = instruction >> 8;
    JValue value;
    const char *kind;

    memset(&value, 0, sizeof(value));
    if (opcode == OP_MOVE_RESULT) {
        value.i = frame->result.i;
        kind = "move-result";
    } else if (opcode == OP_MOVE_RESULT_WIDE) {
        value.j = frame->result.j;
        kind = "move-result-wide";
    } else {
        value = frame->result;
        kind = "move-result-object";
    }
    fprintf(stderr, "%s v%u\n", kind, dest);
    return set_register(frame, dest, value);
}

static const char *invoke_kind(uint8_t opcode)
{
    if (opcode == OP_INVOKE_VIRTUAL) {
        return "invoke-virtual";
    }
    if (opcode == OP_INVOKE_SUPER) {
        return "invoke-super";
    }
    if (opcode == OP_INVOKE_DIRECT) {
        return "invoke-direct";
    }
    if (opcode == OP_INVOKE_STATIC) {
        return "invoke-static";
    }
    return "invoke";
}

/* 35c: the first code unit is already consumed. The receiver of an instance
 * call is argument 0 in that encoding, so it is copied with the other args. */
static int execute_invoke(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned argc = instruction >> 12;
    unsigned sources[5];
    uint16_t method_idx;
    uint16_t reg_word;
    JValue args[5];
    ArtMethod *callee;
    ArtNativeFn fn;
    unsigned i;
    JValue returned;
    const char *class_desc = "?";
    const char *name = "?";
    const char *proto = "";

    if (argc > 5 || fetch_u16(frame, &method_idx) != 0 || fetch_u16(frame, &reg_word) != 0) {
        return -1;
    }
    sources[0] = reg_word & 0xf;
    sources[1] = (reg_word >> 4) & 0xf;
    sources[2] = (reg_word >> 8) & 0xf;
    sources[3] = (reg_word >> 12) & 0xf;
    sources[4] = (instruction >> 8) & 0xf;

    memset(args, 0, sizeof(args));
    for (i = 0; i < argc; i++) {
        if (sources[i] >= frame->register_count) {
            return -1;
        }
        args[i] = frame->registers[sources[i]];
    }

    dex_file_method_at(art_method_dex(frame->method), method_idx, &class_desc, &name, &proto);
    fprintf(stderr, "%s %s->%s%s\n", invoke_kind(instruction & 0xff), class_desc, name, proto);

    callee = runtime_resolve_method(frame->runtime, art_method_dex(frame->method), method_idx);
    if (callee == NULL) {
        return -1;
    }
    memset(&returned, 0, sizeof(returned));
    if (art_method_is_native(callee)) {
        fn = art_method_native(callee);
        if (fn == NULL || fn(frame->runtime, args, argc, &returned) != 0) {
            return -1;
        }
    } else if (interpreter_invoke(frame->runtime, callee, args, &returned) != 0) {
        return -1;
    }
    frame->result = returned;
    return 0;
}

static int execute_return(InterpreterFrame *frame, uint16_t instruction, uint8_t opcode)
{
    unsigned reg = instruction >> 8;

    if (reg >= frame->register_count) {
        return -1;
    }
    memset(&frame->result, 0, sizeof(frame->result));
    if (opcode == OP_RETURN) {
        frame->result.i = frame->registers[reg].i;
        fprintf(stderr, "return v%u\n", reg);
    } else if (opcode == OP_RETURN_WIDE) {
        frame->result.j = frame->registers[reg].j;
        fprintf(stderr, "return-wide v%u\n", reg);
    } else {
        frame->result = frame->registers[reg];
        fprintf(stderr, "return-object v%u\n", reg);
    }
    return 0;
}

static int execute_method(InterpreterFrame *frame)
{
    for (;;) {
        uint16_t instruction;
        uint8_t opcode;

        if (fetch_u16(frame, &instruction) != 0) {
            return -1;
        }
        opcode = instruction & 0xff;

        switch (opcode) {
        case OP_RETURN_VOID:
            fprintf(stderr, "return-void\n");
            memset(&frame->result, 0, sizeof(frame->result));
            return 0;
        case OP_RETURN:
        case OP_RETURN_WIDE:
        case OP_RETURN_OBJECT:
            if (execute_return(frame, instruction, opcode) != 0) {
                return -1;
            }
            return 0;
        case OP_MOVE_RESULT:
        case OP_MOVE_RESULT_WIDE:
        case OP_MOVE_RESULT_OBJECT:
            if (execute_move_result(frame, instruction, opcode) != 0) {
                return -1;
            }
            break;
        case OP_CONST_4:
            if (execute_const_4(frame, instruction) != 0) {
                return -1;
            }
            break;
        case OP_SGET:
            if (execute_sget(frame, instruction) != 0) {
                return -1;
            }
            break;
        case OP_INVOKE_VIRTUAL:
        case OP_INVOKE_SUPER:
        case OP_INVOKE_DIRECT:
        case OP_INVOKE_STATIC:
            if (execute_invoke(frame, instruction) != 0) {
                return -1;
            }
            break;
        default:
            fprintf(stderr, "unsupported opcode %#x\n", opcode);
            return -1;
        }
    }
}

int interpreter_invoke(Runtime *rt, ArtMethod *method, JValue *args, JValue *result)
{
    const CodeItem *code;
    InterpreterFrame frame;
    uint16_t incoming;
    uint16_t i;
    int rc;
    const char *class_desc = "?";
    const char *name = "?";
    const char *proto = "";

    if (rt == NULL || method == NULL || art_method_is_native(method)) {
        return -1;
    }
    code = dex_file_code_item(art_method_dex(method), art_method_id(method));
    if (code == NULL) {
        return -1;
    }
    dex_file_method_at(art_method_dex(method), art_method_id(method), &class_desc, &name, &proto);
    fprintf(stderr, "enter %s->%s%s\n", class_desc, name, proto);

    memset(&frame, 0, sizeof(frame));
    frame.runtime = rt;
    frame.method = method;
    frame.insns = code_item_insns(code, &frame.insn_count);
    frame.register_count = code_item_registers(code);
    incoming = code_item_ins(code);
    if (frame.insns == NULL || incoming > frame.register_count || (incoming > 0 && args == NULL)) {
        return -1;
    }
    frame.registers = calloc(frame.register_count ? frame.register_count : 1, sizeof(*frame.registers));
    if (frame.registers == NULL) {
        return -1;
    }
    for (i = 0; i < incoming; i++) {
        frame.registers[frame.register_count - incoming + i] = args[i];
    }

    rc = execute_method(&frame);
    if (rc == 0 && result != NULL) {
        *result = frame.result;
    }
    free(frame.registers);
    return rc;
}
