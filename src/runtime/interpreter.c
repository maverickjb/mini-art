#include "runtime.h"
#include "art_field.h"
#include "java_object.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t invoke_mu;
static pthread_once_t invoke_once = PTHREAD_ONCE_INIT;
static _Thread_local int invoke_depth;

static void invoke_mu_init(void)
{
    pthread_mutex_init(&invoke_mu, NULL);
}

static void invoke_lock(void)
{
    pthread_once(&invoke_once, invoke_mu_init);
    if (invoke_depth++ == 0) {
        pthread_mutex_lock(&invoke_mu);
    }
}

static void invoke_unlock(void)
{
    if (invoke_depth == 0) {
        return;
    }
    if (--invoke_depth == 0) {
        pthread_mutex_unlock(&invoke_mu);
    }
}

/* A nested frame only increments invoke_depth. Unlocking once before join or
 * sleep would leave the outer frame holding invoke_mu, and the worker would
 * block in runtime_invoke forever. Drop every hold owned by this thread. */
static int invoke_suspend(void)
{
    int saved = invoke_depth;

    if (saved > 0) {
        invoke_depth = 0;
        pthread_mutex_unlock(&invoke_mu);
    }
    return saved;
}

static void invoke_resume(int saved)
{
    if (saved > 0) {
        pthread_mutex_lock(&invoke_mu);
        invoke_depth = saved;
    }
}

enum {
    OP_NOP = 0x00,
    OP_MOVE_RESULT = 0x0a,
    OP_MOVE_RESULT_WIDE = 0x0b,
    OP_MOVE_RESULT_OBJECT = 0x0c,
    OP_RETURN_VOID = 0x0e,
    OP_RETURN = 0x0f,
    OP_RETURN_WIDE = 0x10,
    OP_RETURN_OBJECT = 0x11,
    OP_MOVE = 0x01,
    OP_MOVE_FROM16 = 0x02,
    OP_MOVE_16 = 0x03,
    OP_MOVE_WIDE = 0x04,
    OP_MOVE_WIDE_FROM16 = 0x05,
    OP_MOVE_WIDE_16 = 0x06,
    OP_MOVE_OBJECT = 0x07,
    OP_MOVE_OBJECT_FROM16 = 0x08,
    OP_MOVE_OBJECT_16 = 0x09,
    OP_CONST_4 = 0x12,
    OP_CONST_16 = 0x13,
    OP_CONST = 0x14,
    OP_CONST_HIGH16 = 0x15,
    OP_CONST_WIDE_16 = 0x16,
    OP_CONST_WIDE_32 = 0x17,
    OP_CONST_WIDE = 0x18,
    OP_CONST_WIDE_HIGH16 = 0x19,
    OP_CONST_STRING = 0x1a,
    OP_CHECK_CAST = 0x1f,
    OP_ARRAY_LENGTH = 0x21,
    OP_NEW_INSTANCE = 0x22,
    OP_NEW_ARRAY = 0x23,
    OP_FILLED_NEW_ARRAY = 0x24,
    OP_FILL_ARRAY_DATA = 0x26,
    OP_THROW = 0x27,
    OP_GOTO = 0x28,
    OP_GOTO_16 = 0x29,
    OP_CMPL_FLOAT = 0x2d,
    OP_CMPG_FLOAT = 0x2e,
    OP_CMPL_DOUBLE = 0x2f,
    OP_CMPG_DOUBLE = 0x30,
    OP_CMP_LONG = 0x31,
    OP_IF_EQ = 0x32,
    OP_IF_NE = 0x33,
    OP_IF_LT = 0x34,
    OP_IF_GE = 0x35,
    OP_IF_GT = 0x36,
    OP_IF_LE = 0x37,
    OP_IF_EQZ = 0x38,
    OP_IF_NEZ = 0x39,
    OP_IF_LTZ = 0x3a,
    OP_IF_GEZ = 0x3b,
    OP_IF_GTZ = 0x3c,
    OP_IF_LEZ = 0x3d,
    OP_AGET = 0x44,
    OP_AGET_WIDE = 0x45,
    OP_AGET_OBJECT = 0x46,
    OP_AGET_BOOLEAN = 0x47,
    OP_AGET_BYTE = 0x48,
    OP_AGET_CHAR = 0x49,
    OP_AGET_SHORT = 0x4a,
    OP_APUT = 0x4b,
    OP_APUT_WIDE = 0x4c,
    OP_APUT_OBJECT = 0x4d,
    OP_APUT_BOOLEAN = 0x4e,
    OP_APUT_BYTE = 0x4f,
    OP_APUT_CHAR = 0x50,
    OP_APUT_SHORT = 0x51,
    OP_IGET = 0x52,
    OP_IGET_WIDE = 0x53,
    OP_IGET_OBJECT = 0x54,
    OP_IGET_BOOLEAN = 0x55,
    OP_IGET_BYTE = 0x56,
    OP_IGET_CHAR = 0x57,
    OP_IGET_SHORT = 0x58,
    OP_IPUT = 0x59,
    OP_IPUT_WIDE = 0x5a,
    OP_IPUT_OBJECT = 0x5b,
    OP_IPUT_BOOLEAN = 0x5c,
    OP_IPUT_BYTE = 0x5d,
    OP_IPUT_CHAR = 0x5e,
    OP_IPUT_SHORT = 0x5f,
    OP_SGET = 0x60,
    OP_SGET_OBJECT = 0x62,
    OP_SPUT = 0x67,
    OP_SPUT_OBJECT = 0x69,
    OP_SPUT_BOOLEAN = 0x6a,
    OP_INVOKE_VIRTUAL = 0x6e,
    OP_INVOKE_SUPER = 0x6f,
    OP_INVOKE_DIRECT = 0x70,
    OP_INVOKE_STATIC = 0x71,
    OP_INVOKE_INTERFACE = 0x72,
    OP_INVOKE_VIRTUAL_RANGE = 0x74,
    OP_INVOKE_SUPER_RANGE = 0x75,
    OP_INVOKE_DIRECT_RANGE = 0x76,
    OP_INVOKE_STATIC_RANGE = 0x77,
    OP_INVOKE_INTERFACE_RANGE = 0x78,
    OP_NEG_DOUBLE = 0x80,
    OP_INT_TO_LONG = 0x81,
    OP_INT_TO_FLOAT = 0x82,
    OP_INT_TO_DOUBLE = 0x83,
    OP_LONG_TO_INT = 0x84,
    OP_LONG_TO_FLOAT = 0x85,
    OP_LONG_TO_DOUBLE = 0x86,
    OP_FLOAT_TO_INT = 0x87,
    OP_FLOAT_TO_LONG = 0x88,
    OP_FLOAT_TO_DOUBLE = 0x89,
    OP_DOUBLE_TO_INT = 0x8a,
    OP_DOUBLE_TO_LONG = 0x8b,
    OP_DOUBLE_TO_FLOAT = 0x8c,
    OP_INT_TO_BYTE = 0x8d,
    OP_INT_TO_CHAR = 0x8e,
    OP_INT_TO_SHORT = 0x8f,
    OP_ADD_FLOAT = 0xa6,
    OP_SUB_FLOAT = 0xa7,
    OP_MUL_FLOAT = 0xa8,
    OP_DIV_FLOAT = 0xa9,
    OP_REM_FLOAT = 0xaa,
    OP_ADD_DOUBLE = 0xab,
    OP_SUB_DOUBLE = 0xac,
    OP_MUL_DOUBLE = 0xad,
    OP_DIV_DOUBLE = 0xae,
    OP_REM_DOUBLE = 0xaf,
    OP_ADD_INT = 0x90,
    OP_SUB_INT = 0x91,
    OP_MUL_INT = 0x92,
    OP_DIV_INT = 0x93,
    OP_REM_INT = 0x94,
    OP_AND_INT = 0x95,
    OP_OR_INT = 0x96,
    OP_XOR_INT = 0x97,
    OP_SHL_INT = 0x98,
    OP_SHR_INT = 0x99,
    OP_USHR_INT = 0x9a,
    OP_ADD_LONG = 0x9b,
    OP_SUB_LONG = 0x9c,
    OP_MUL_LONG = 0x9d,
    OP_DIV_LONG = 0x9e,
    OP_REM_LONG = 0x9f,
    OP_AND_LONG = 0xa0,
    OP_OR_LONG = 0xa1,
    OP_XOR_LONG = 0xa2,
    OP_SHL_LONG = 0xa3,
    OP_SHR_LONG = 0xa4,
    OP_USHR_LONG = 0xa5,
    OP_ADD_INT_2ADDR = 0xb0,
    OP_SUB_INT_2ADDR = 0xb1,
    OP_MUL_INT_2ADDR = 0xb2,
    OP_DIV_INT_2ADDR = 0xb3,
    OP_REM_INT_2ADDR = 0xb4,
    OP_AND_INT_2ADDR = 0xb5,
    OP_OR_INT_2ADDR = 0xb6,
    OP_XOR_INT_2ADDR = 0xb7,
    OP_SHL_INT_2ADDR = 0xb8,
    OP_SHR_INT_2ADDR = 0xb9,
    OP_USHR_INT_2ADDR = 0xba,
    OP_ADD_LONG_2ADDR = 0xbb,
    OP_SUB_LONG_2ADDR = 0xbc,
    OP_MUL_LONG_2ADDR = 0xbd,
    OP_DIV_LONG_2ADDR = 0xbe,
    OP_REM_LONG_2ADDR = 0xbf,
    OP_AND_LONG_2ADDR = 0xc0,
    OP_OR_LONG_2ADDR = 0xc1,
    OP_XOR_LONG_2ADDR = 0xc2,
    OP_SHL_LONG_2ADDR = 0xc3,
    OP_SHR_LONG_2ADDR = 0xc4,
    OP_USHR_LONG_2ADDR = 0xc5,
    OP_ADD_INT_LIT16 = 0xd0,
    OP_RSUB_INT = 0xd1,
    OP_MUL_INT_LIT16 = 0xd2,
    OP_DIV_INT_LIT16 = 0xd3,
    OP_REM_INT_LIT16 = 0xd4,
    OP_AND_INT_LIT16 = 0xd5,
    OP_OR_INT_LIT16 = 0xd6,
    OP_XOR_INT_LIT16 = 0xd7,
    OP_ADD_DOUBLE_2ADDR = 0xcb,
    OP_SUB_DOUBLE_2ADDR = 0xcc,
    OP_MUL_DOUBLE_2ADDR = 0xcd,
    OP_DIV_DOUBLE_2ADDR = 0xce,
    OP_REM_DOUBLE_2ADDR = 0xcf,
    OP_ADD_FLOAT_2ADDR = 0xc6,
    OP_SUB_FLOAT_2ADDR = 0xc7,
    OP_MUL_FLOAT_2ADDR = 0xc8,
    OP_DIV_FLOAT_2ADDR = 0xc9,
    OP_REM_FLOAT_2ADDR = 0xca,
    OP_ADD_INT_LIT8 = 0xd8,
    OP_RSUB_INT_LIT8 = 0xd9,
    OP_MUL_INT_LIT8 = 0xda,
    OP_DIV_INT_LIT8 = 0xdb,
    OP_REM_INT_LIT8 = 0xdc,
    OP_AND_INT_LIT8 = 0xdd,
    OP_OR_INT_LIT8 = 0xde,
    OP_XOR_INT_LIT8 = 0xdf
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

static int read_register(const InterpreterFrame *frame, unsigned reg, JValue *out);

static int static_field_names(InterpreterFrame *frame, uint16_t field_idx, const char **class_desc,
                             const char **name)
{
    *class_desc = "?";
    *name = "?";
    return dex_file_field_at(art_method_dex(frame->method), field_idx, class_desc, name, NULL);
}

/* 21c. Reads the current value from the declaring class's static storage. */
static int execute_sget(InterpreterFrame *frame, uint16_t instruction, int kind)
{
    uint16_t field_idx;
    unsigned dest = instruction >> 8;
    JValue value;
    const char *class_desc;
    const char *name;
    const char *op = kind == ART_FIELD_OBJECT ? "sget-object" : "sget";

    if (fetch_u16(frame, &field_idx) != 0 ||
        static_field_names(frame, field_idx, &class_desc, &name) != 0) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    if (runtime_get_static(frame->runtime, art_method_dex(frame->method), field_idx, kind,
                           &value) != 0) {
        fprintf(stderr, "unresolved static field %s %s->%s\n", op, class_desc, name);
        return -1;
    }
    if (kind == ART_FIELD_OBJECT) {
        fprintf(stderr, "sget-object v%u %s->%s = %p\n", dest, class_desc, name, value.l);
    } else {
        fprintf(stderr, "sget v%u %s->%s = %#x\n", dest, class_desc, name, (unsigned)value.i);
    }
    return set_register(frame, dest, value);
}

static int execute_sput(InterpreterFrame *frame, uint16_t instruction, int kind)
{
    uint16_t field_idx;
    unsigned src = instruction >> 8;
    JValue value;
    const char *class_desc;
    const char *name;

    if (fetch_u16(frame, &field_idx) != 0 ||
        static_field_names(frame, field_idx, &class_desc, &name) != 0 ||
        read_register(frame, src, &value) != 0) {
        return -1;
    }
    if (kind == ART_FIELD_OBJECT) {
        fprintf(stderr, "sput-object v%u %s->%s\n", src, class_desc, name);
    } else {
        fprintf(stderr, "sput v%u %s->%s\n", src, class_desc, name);
    }
    if (runtime_set_static(frame->runtime, art_method_dex(frame->method), field_idx, kind,
                           value) != 0) {
        fprintf(stderr, "unresolved static field %s->%s\n", class_desc, name);
        return -1;
    }
    return 0;
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
        value.l = frame->result.l;
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
    if (opcode == OP_INVOKE_VIRTUAL_RANGE) {
        return "invoke-virtual/range";
    }
    if (opcode == OP_INVOKE_SUPER) {
        return "invoke-super";
    }
    if (opcode == OP_INVOKE_SUPER_RANGE) {
        return "invoke-super/range";
    }
    if (opcode == OP_INVOKE_DIRECT) {
        return "invoke-direct";
    }
    if (opcode == OP_INVOKE_DIRECT_RANGE) {
        return "invoke-direct/range";
    }
    if (opcode == OP_INVOKE_STATIC) {
        return "invoke-static";
    }
    if (opcode == OP_INVOKE_STATIC_RANGE) {
        return "invoke-static/range";
    }
    if (opcode == OP_INVOKE_INTERFACE) {
        return "invoke-interface";
    }
    if (opcode == OP_INVOKE_INTERFACE_RANGE) {
        return "invoke-interface/range";
    }
    return "invoke";
}

static int invoke_call(InterpreterFrame *frame, uint8_t opcode, uint16_t method_idx, JValue *args,
                       unsigned argc)
{
    ArtMethod *callee;
    ArtNativeFn fn;
    JValue returned;
    int held;
    const char *class_desc = "?";
    const char *name = "?";
    const char *proto = "";

    dex_file_method_at(art_method_dex(frame->method), method_idx, &class_desc, &name, &proto);
    fprintf(stderr, "%s %s->%s%s\n", invoke_kind(opcode), class_desc, name, proto);

    switch (opcode) {
    case OP_INVOKE_SUPER:
    case OP_INVOKE_SUPER_RANGE:
        callee = runtime_resolve_super(frame->runtime, frame->method, method_idx);
        break;
    case OP_INVOKE_VIRTUAL:
    case OP_INVOKE_VIRTUAL_RANGE: {
        void *object = argc > 0 ? args[0].l : NULL;

        if (object == NULL) {
            return -1;
        }
        if (java_object_check(object)) {
            callee = runtime_resolve_virtual(frame->runtime, java_object_class(object), name,
                                             proto);
        } else {
            /* Toast is not a JavaObject. Resolve the method reference. */
            callee = runtime_resolve_method(frame->runtime, art_method_dex(frame->method),
                                            method_idx);
        }
        break;
    }
    case OP_INVOKE_INTERFACE:
    case OP_INVOKE_INTERFACE_RANGE:
        callee = runtime_resolve_interface(frame->runtime, java_object_class(args[0].l), name,
                                           proto);
        break;
    default:
        callee = runtime_resolve_method(frame->runtime, art_method_dex(frame->method), method_idx);
        break;
    }
    if (callee == NULL) {
        return -1;
    }
    memset(&returned, 0, sizeof(returned));
    held = invoke_suspend();
    if (art_method_is_native(callee)) {
        fn = art_method_native(callee);
        if (fn == NULL || fn(frame->runtime, args, argc, &returned) != 0) {
            invoke_resume(held);
            return -1;
        }
    } else if (interpreter_invoke(frame->runtime, callee, args, &returned) != 0) {
        invoke_resume(held);
        return -1;
    }
    invoke_resume(held);
    frame->result = returned;
    return 0;
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
    unsigned i;

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
    return invoke_call(frame, (uint8_t)(instruction & 0xff), method_idx, args, argc);
}

/* 3rc: AA|op BBBB CCCC. Arguments are the contiguous registers vCCCC .. vCCCC+AA-1. */
static int execute_invoke_range(InterpreterFrame *frame, uint16_t instruction)
{
    uint8_t opcode = (uint8_t)(instruction & 0xff);
    unsigned argc = instruction >> 8;
    uint16_t method_idx;
    uint16_t first;
    JValue *args = NULL;
    unsigned i;
    int rc;

    if (fetch_u16(frame, &method_idx) != 0 || fetch_u16(frame, &first) != 0) {
        return -1;
    }
    if (opcode != OP_INVOKE_STATIC_RANGE && argc == 0) {
        return -1;
    }
    if (argc > frame->register_count || (argc > 0 && first + argc > frame->register_count)) {
        return -1;
    }
    if (argc > 0) {
        args = calloc(argc, sizeof(*args));
        if (args == NULL) {
            return -1;
        }
        for (i = 0; i < argc; i++) {
            args[i] = frame->registers[first + i];
        }
    }
    rc = invoke_call(frame, opcode, method_idx, args, argc);
    free(args);
    return rc;
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

static int32_t sign8(unsigned value)
{
    value &= 0xff;
    return (value & 0x80) != 0 ? (int32_t)value - 256 : (int32_t)value;
}

static int32_t sign16(unsigned value)
{
    value &= 0xffff;
    return (value & 0x8000) != 0 ? (int32_t)value - 65536 : (int32_t)value;
}

static int read_register(const InterpreterFrame *frame, unsigned reg, JValue *out)
{
    if (reg >= frame->register_count || out == NULL) {
        return -1;
    }
    *out = frame->registers[reg];
    return 0;
}

static void branch_to(InterpreterFrame *frame, uint32_t start, int32_t offset)
{
    frame->pc = (uint32_t)((int32_t)start + offset);
}

enum {
    MOVE_12X,
    MOVE_22X,
    MOVE_32X
};

/* 12x is B|A|op. 22x is AA|op BBBB. 32x is 00|op AAAA BBBB. A wide value also copies the next register. */
static int execute_move(InterpreterFrame *frame, uint16_t instruction, int format, int wide)
{
    uint16_t word;
    unsigned dest;
    unsigned src;
    JValue value;
    JValue pair;

    if (format == MOVE_32X) {
        if (fetch_u16(frame, &word) != 0) {
            return -1;
        }
        dest = word;
    } else if (format == MOVE_22X) {
        dest = instruction >> 8;
    } else {
        dest = (instruction >> 8) & 0x0f;
    }
    if (format == MOVE_12X) {
        src = instruction >> 12;
    } else if (fetch_u16(frame, &word) != 0) {
        return -1;
    } else {
        src = word;
    }
    if (dest >= frame->register_count || src >= frame->register_count) {
        return -1;
    }
    if (wide && (dest + 1 >= frame->register_count || src + 1 >= frame->register_count)) {
        return -1;
    }
    if (read_register(frame, src, &value) != 0) {
        return -1;
    }
    if (wide && read_register(frame, src + 1, &pair) != 0) {
        return -1;
    }
    frame->registers[dest] = value;
    if (wide) {
        frame->registers[dest + 1] = pair;
    }
    return 0;
}

static int execute_const16(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t immediate;
    unsigned dest = instruction >> 8;

    if (fetch_u16(frame, &immediate) != 0) {
        return -1;
    }
    return set_int_register(frame, dest, sign16(immediate));
}

/* 21s. The sign-extended literal lives in the destination register's wide slot. */
static int execute_const_wide16(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t immediate;
    unsigned dest = instruction >> 8;
    JValue value;

    if (fetch_u16(frame, &immediate) != 0) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.j = (int64_t)sign16(immediate);
    fprintf(stderr, "const-wide/16 v%u, #%lld\n", dest, (long long)value.j);
    return set_register(frame, dest, value);
}

/* 21h. The 16-bit literal becomes the high half of a 32-bit register. */
static int execute_const_high16(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t immediate;
    unsigned dest = instruction >> 8;
    int32_t value;

    if (fetch_u16(frame, &immediate) != 0) {
        return -1;
    }
    value = (int32_t)((uint32_t)immediate << 16);
    fprintf(stderr, "const/high16 v%u, #%d\n", dest, value);
    return set_int_register(frame, dest, value);
}

/* 31i. The 32-bit literal is sign-extended into the wide slot. */
static int execute_const_wide32(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t lo;
    uint16_t hi;
    unsigned dest = instruction >> 8;
    JValue value;

    if (fetch_u16(frame, &lo) != 0 || fetch_u16(frame, &hi) != 0) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.j = (int64_t)(int32_t)((uint32_t)lo | ((uint32_t)hi << 16));
    fprintf(stderr, "const-wide/32 v%u, #%lld\n", dest, (long long)value.j);
    return set_register(frame, dest, value);
}

/* 51l. Four code units, little-endian, fill the wide slot. */
static int execute_const_wide(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t part;
    unsigned dest = instruction >> 8;
    uint64_t bits = 0;
    unsigned i;
    JValue value;

    for (i = 0; i < 4; i++) {
        if (fetch_u16(frame, &part) != 0) {
            return -1;
        }
        bits |= (uint64_t)part << (16 * i);
    }
    memset(&value, 0, sizeof(value));
    value.j = (int64_t)bits;
    fprintf(stderr, "const-wide v%u, #%lld\n", dest, (long long)value.j);
    return set_register(frame, dest, value);
}

/* 21h. The 16-bit literal becomes the high 16 bits of a 64-bit register. */
static int execute_const_wide_high16(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t immediate;
    unsigned dest = instruction >> 8;
    JValue value;

    if (fetch_u16(frame, &immediate) != 0) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.j = (int64_t)((uint64_t)immediate << 48);
    fprintf(stderr, "const-wide/high16 v%u, #%lld\n", dest, (long long)value.j);
    return set_register(frame, dest, value);
}

static int execute_const(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t lo;
    uint16_t hi;
    unsigned dest = instruction >> 8;
    int32_t value;

    if (fetch_u16(frame, &lo) != 0 || fetch_u16(frame, &hi) != 0) {
        return -1;
    }
    value = (int32_t)((uint32_t)lo | ((uint32_t)hi << 16));
    return set_int_register(frame, dest, value);
}

static int execute_const_string(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t string_idx;
    unsigned dest = instruction >> 8;
    const char *text;
    JavaObject *object;
    JValue value;

    if (fetch_u16(frame, &string_idx) != 0) {
        return -1;
    }
    text = dex_file_string(art_method_dex(frame->method), string_idx);
    if (text == NULL) {
        return -1;
    }
    object = java_object_new(frame->runtime, "Ljava/lang/String;");
    if (object == NULL) {
        return -1;
    }
    java_object_set_text(object, text);
    memset(&value, 0, sizeof(value));
    value.l = object;
    return set_register(frame, dest, value);
}

static int execute_new_instance(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t type_idx;
    unsigned dest = instruction >> 8;
    const char *desc;
    JavaObject *object;
    JValue value;

    if (fetch_u16(frame, &type_idx) != 0) {
        return -1;
    }
    desc = dex_file_type(art_method_dex(frame->method), type_idx);
    if (desc == NULL || runtime_initialize_class(frame->runtime, desc) != 0) {
        return -1;
    }
    object = java_object_new(frame->runtime, desc);
    if (desc == NULL || object == NULL) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.l = object;
    return set_register(frame, dest, value);
}

/* 22c: the length register and destination share the first code unit. */
static int execute_new_array(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned regs = instruction >> 8;
    unsigned dest = regs & 0x0f;
    unsigned length_reg = regs >> 4;
    uint16_t type_idx;
    const char *desc;
    int32_t length;
    JavaObject *array;
    JValue value;

    if (fetch_u16(frame, &type_idx) != 0 || dest >= frame->register_count ||
        length_reg >= frame->register_count) {
        return -1;
    }
    desc = dex_file_type(art_method_dex(frame->method), type_idx);
    if (desc == NULL || desc[0] != '[') {
        return -1;
    }
    length = frame->registers[length_reg].i;
    if (length < 0) {
        return -1;
    }
    array = java_object_new_array(frame->runtime, desc, length);
    if (array == NULL) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.l = array;
    fprintf(stderr, "new-array v%u %s [%d]\n", dest, desc, length);
    return set_register(frame, dest, value);
}

/* 35c. The array is the result of the instruction, read by move-result-object. */
static int execute_filled_new_array(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned argc = instruction >> 12;
    unsigned sources[5];
    uint16_t type_idx;
    uint16_t reg_word;
    const char *desc;
    JavaObject *array;
    void **slots;
    unsigned i;

    if (argc > 5 || fetch_u16(frame, &type_idx) != 0 || fetch_u16(frame, &reg_word) != 0) {
        return -1;
    }
    sources[0] = reg_word & 0xf;
    sources[1] = (reg_word >> 4) & 0xf;
    sources[2] = (reg_word >> 8) & 0xf;
    sources[3] = (reg_word >> 12) & 0xf;
    sources[4] = (instruction >> 8) & 0xf;
    desc = dex_file_type(art_method_dex(frame->method), type_idx);
    if (desc == NULL || desc[0] != '[' || (desc[1] != 'L' && desc[1] != '[')) {
        return -1;
    }
    for (i = 0; i < argc; i++) {
        if (sources[i] >= frame->register_count) {
            return -1;
        }
    }
    array = java_object_new_array(frame->runtime, desc, (int32_t)argc);
    if (array == NULL) {
        return -1;
    }
    if (argc > 0) {
        if (java_object_array_data(array, NULL, NULL, (void **)&slots) != 0 || slots == NULL) {
            return -1;
        }
        for (i = 0; i < argc; i++) {
            slots[i] = frame->registers[sources[i]].l;
        }
    }
    memset(&frame->result, 0, sizeof(frame->result));
    frame->result.l = array;
    fprintf(stderr, "filled-new-array %s [%u]\n", desc, argc);
    return 0;
}

static int execute_fill_array_data(InterpreterFrame *frame, uint16_t instruction, uint32_t start)
{
    unsigned reg = instruction >> 8;
    uint16_t lo;
    uint16_t hi;
    int32_t offset;
    int64_t target;
    const uint16_t *payload;
    uint32_t payload_at;
    int32_t length;
    int width;
    void *data;
    uint32_t size;
    uint32_t element_width;
    uint64_t data_bytes;
    uint64_t data_units;
    JValue slot;

    if (fetch_u16(frame, &lo) != 0 || fetch_u16(frame, &hi) != 0 ||
        read_register(frame, reg, &slot) != 0 ||
        java_object_array_data(slot.l, &length, &width, &data) != 0) {
        return -1;
    }
    offset = (int32_t)((uint32_t)lo | ((uint32_t)hi << 16));
    target = (int64_t)start + offset;
    if (target < 0 || target > frame->insn_count) {
        return -1;
    }
    payload_at = (uint32_t)target;
    if ((payload_at & 1) != 0 || frame->insn_count - payload_at < 4) {
        return -1;
    }
    payload = frame->insns + payload_at;
    if (payload[0] != 0x0300) {
        return -1;
    }
    element_width = payload[1];
    size = (uint32_t)payload[2] | ((uint32_t)payload[3] << 16);
    if (length < 0 || width <= 0 || element_width != (uint32_t)width || size > (uint32_t)length) {
        return -1;
    }
    data_bytes = (uint64_t)size * element_width;
    data_units = (data_bytes + 1) / 2;
    if (data_units > frame->insn_count - payload_at - 4 || data_bytes > SIZE_MAX ||
        (data_bytes > 0 && data == NULL)) {
        return -1;
    }
    if (data_bytes > 0) {
        memcpy(data, payload + 4, (size_t)data_bytes);
    }
    fprintf(stderr, "fill-array-data v%u %u x %u\n", reg, size, element_width);
    return 0;
}

static int execute_check_cast(InterpreterFrame *frame)
{
    uint16_t type_idx;
    return fetch_u16(frame, &type_idx);
}

typedef enum {
    FIELD_VALUE_NORMAL,
    FIELD_VALUE_WIDE,
    FIELD_VALUE_OBJECT,
    FIELD_VALUE_BOOLEAN,
    FIELD_VALUE_BYTE,
    FIELD_VALUE_CHAR,
    FIELD_VALUE_SHORT
} FieldValueKind;

static FieldValueKind field_value_kind(uint8_t opcode)
{
    switch (opcode) {
    case OP_IGET_WIDE:
    case OP_IPUT_WIDE:
        return FIELD_VALUE_WIDE;
    case OP_IGET_OBJECT:
    case OP_IPUT_OBJECT:
        return FIELD_VALUE_OBJECT;
    case OP_IGET_BOOLEAN:
    case OP_IPUT_BOOLEAN:
        return FIELD_VALUE_BOOLEAN;
    case OP_IGET_BYTE:
    case OP_IPUT_BYTE:
        return FIELD_VALUE_BYTE;
    case OP_IGET_CHAR:
    case OP_IPUT_CHAR:
        return FIELD_VALUE_CHAR;
    case OP_IGET_SHORT:
    case OP_IPUT_SHORT:
        return FIELD_VALUE_SHORT;
    default:
        return FIELD_VALUE_NORMAL;
    }
}

static int field_is_store(uint8_t opcode)
{
    switch (opcode) {
    case OP_IPUT:
    case OP_IPUT_WIDE:
    case OP_IPUT_OBJECT:
    case OP_IPUT_BOOLEAN:
    case OP_IPUT_BYTE:
    case OP_IPUT_CHAR:
    case OP_IPUT_SHORT:
        return 1;
    default:
        return 0;
    }
}

/* The opcode chooses the access. The descriptor must agree. */
static int field_type_matches(FieldValueKind kind, const char *type)
{
    if (type == NULL || type[0] == '\0') {
        return 0;
    }
    switch (kind) {
    case FIELD_VALUE_WIDE:
        return type[0] == 'J' || type[0] == 'D';
    case FIELD_VALUE_OBJECT:
        return type[0] == 'L' || type[0] == '[';
    case FIELD_VALUE_BOOLEAN:
        return type[0] == 'Z';
    case FIELD_VALUE_BYTE:
        return type[0] == 'B';
    case FIELD_VALUE_CHAR:
        return type[0] == 'C';
    case FIELD_VALUE_SHORT:
        return type[0] == 'S';
    default:
        return type[0] == 'I' || type[0] == 'F';
    }
}

static void normalize_field_value(FieldValueKind kind, JValue *value)
{
    switch (kind) {
    case FIELD_VALUE_BOOLEAN:
        value->i = value->i != 0;
        break;
    case FIELD_VALUE_BYTE:
        value->i = (int8_t)value->i;
        break;
    case FIELD_VALUE_CHAR:
        value->i = (uint16_t)value->i;
        break;
    case FIELD_VALUE_SHORT:
        value->i = (int16_t)value->i;
        break;
    default:
        break;
    }
}

static int execute_field(InterpreterFrame *frame, uint16_t instruction, uint8_t opcode)
{
    unsigned value_reg = (instruction >> 8) & 0x0f;
    unsigned object_reg = instruction >> 12;
    uint16_t field_idx;
    const char *name = NULL;
    const char *type = NULL;
    JValue object_slot;
    JValue value;
    JavaObject *object;
    FieldValueKind kind = field_value_kind(opcode);

    if (fetch_u16(frame, &field_idx) != 0 ||
        dex_file_field_at(art_method_dex(frame->method), field_idx, NULL, &name, &type) != 0 ||
        !field_type_matches(kind, type) || read_register(frame, object_reg, &object_slot) != 0 ||
        !java_object_check(object_slot.l)) {
        return -1;
    }
    object = object_slot.l;
    if (field_is_store(opcode)) {
        if (read_register(frame, value_reg, &value) != 0) {
            return -1;
        }
        normalize_field_value(kind, &value);
        return java_object_set_field(object, name, value);
    }
    if (java_object_get_field(object, name, &value) != 0) {
        return -1;
    }
    return set_register(frame, value_reg, value);
}

static int compare_if(uint8_t opcode, int32_t left, int32_t right)
{
    switch (opcode) {
    case OP_IF_EQ:
    case OP_IF_EQZ:
        return left == right;
    case OP_IF_NE:
    case OP_IF_NEZ:
        return left != right;
    case OP_IF_LT:
    case OP_IF_LTZ:
        return left < right;
    case OP_IF_GE:
    case OP_IF_GEZ:
        return left >= right;
    case OP_IF_GT:
    case OP_IF_GTZ:
        return left > right;
    case OP_IF_LE:
    case OP_IF_LEZ:
        return left <= right;
    default:
        return 0;
    }
}

/* 23x. vAA receives -1, 0, or 1. cmpl biases a NaN toward -1; cmpg biases it toward 1. */
static int compare_float_values(double left, double right, int nan_bias)
{
    if (isnan(left) || isnan(right)) {
        return nan_bias;
    }
    if (left == right) {
        return 0;
    }
    return left < right ? -1 : 1;
}

static int execute_cmp(InterpreterFrame *frame, uint16_t instruction)
{
    uint8_t opcode = instruction & 0xff;
    unsigned dest = instruction >> 8;
    uint16_t regs;
    unsigned left_reg;
    unsigned right_reg;
    int result;

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    left_reg = regs & 0xff;
    right_reg = regs >> 8;
    if (dest >= frame->register_count || left_reg >= frame->register_count ||
        right_reg >= frame->register_count) {
        return -1;
    }
    switch (opcode) {
    case OP_CMP_LONG: {
        int64_t left = frame->registers[left_reg].j;
        int64_t right = frame->registers[right_reg].j;

        if (left == right) {
            result = 0;
        } else {
            result = left < right ? -1 : 1;
        }
        break;
    }
    case OP_CMPL_FLOAT:
    case OP_CMPG_FLOAT:
        result = compare_float_values(frame->registers[left_reg].f, frame->registers[right_reg].f,
                                      opcode == OP_CMPG_FLOAT ? 1 : -1);
        break;
    case OP_CMPL_DOUBLE:
    case OP_CMPG_DOUBLE:
        result = compare_float_values(frame->registers[left_reg].d, frame->registers[right_reg].d,
                                      opcode == OP_CMPG_DOUBLE ? 1 : -1);
        break;
    default:
        return -1;
    }
    return set_int_register(frame, dest, result);
}

static int execute_if(InterpreterFrame *frame, uint16_t instruction, uint32_t start)
{
    uint8_t opcode = instruction & 0xff;
    uint16_t offset_unit;
    int32_t left;
    int32_t right = 0;
    unsigned left_reg;
    unsigned right_reg;

    if (opcode >= OP_IF_EQZ && opcode <= OP_IF_LEZ) {
        left_reg = instruction >> 8;
    } else {
        left_reg = (instruction >> 8) & 0x0f;
        right_reg = instruction >> 12;
        if (right_reg >= frame->register_count) {
            return -1;
        }
        right = frame->registers[right_reg].i;
    }
    if (fetch_u16(frame, &offset_unit) != 0 || left_reg >= frame->register_count) {
        return -1;
    }
    left = frame->registers[left_reg].i;
    if (compare_if(opcode, left, right)) {
        branch_to(frame, start, sign16(offset_unit));
    }
    return 0;
}

/* Java narrowing: NaN becomes 0, and values outside the range saturate. */
static int32_t java_to_int32(double value)
{
    if (isnan(value)) {
        return 0;
    }
    if (value >= 0x1p31) {
        return INT32_MAX;
    }
    if (value < (double)INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

static int64_t java_to_int64(double value)
{
    if (isnan(value)) {
        return 0;
    }
    if (value >= 0x1p63) {
        return INT64_MAX;
    }
    if (value < (double)INT64_MIN) {
        return INT64_MIN;
    }
    return (int64_t)value;
}

/* 12x conversions. A long or double result stays in vA. */
static int execute_convert(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    JValue value;
    uint8_t opcode = instruction & 0xff;
    int32_t as_int;
    int64_t as_long;
    float as_float;
    double as_double;

    if (read_register(frame, src, &value) != 0) {
        return -1;
    }
    as_int = value.i;
    as_long = value.j;
    as_float = value.f;
    as_double = value.d;
    memset(&value, 0, sizeof(value));
    switch (opcode) {
    case OP_INT_TO_LONG:
        value.j = as_int;
        break;
    case OP_INT_TO_FLOAT:
        value.f = (float)as_int;
        break;
    case OP_INT_TO_DOUBLE:
        value.d = as_int;
        break;
    case OP_LONG_TO_INT:
        value.i = (int32_t)(uint32_t)as_long;
        break;
    case OP_LONG_TO_FLOAT:
        value.f = (float)as_long;
        break;
    case OP_LONG_TO_DOUBLE:
        value.d = (double)as_long;
        break;
    case OP_FLOAT_TO_INT:
        value.i = java_to_int32(as_float);
        break;
    case OP_FLOAT_TO_LONG:
        value.j = java_to_int64(as_float);
        break;
    case OP_FLOAT_TO_DOUBLE:
        value.d = as_float;
        break;
    case OP_DOUBLE_TO_INT:
        value.i = java_to_int32(as_double);
        break;
    case OP_DOUBLE_TO_LONG:
        value.j = java_to_int64(as_double);
        break;
    case OP_DOUBLE_TO_FLOAT:
        value.f = (float)as_double;
        break;
    case OP_INT_TO_BYTE:
        value.i = (int8_t)as_int;
        break;
    case OP_INT_TO_CHAR:
        value.i = (int32_t)(uint16_t)as_int;
        break;
    case OP_INT_TO_SHORT:
        value.i = (int16_t)as_int;
        break;
    default:
        return -1;
    }
    return set_register(frame, dest, value);
}

static int execute_unop(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    JValue value;
    uint8_t opcode = instruction & 0xff;

    if (read_register(frame, src, &value) != 0) {
        return -1;
    }
    if (opcode == OP_NEG_DOUBLE) {
        value.d = -value.d;
    } else {
        double left;
        double right = value.d;
        if (dest >= frame->register_count) {
            return -1;
        }
        left = frame->registers[dest].d;
        if (opcode == OP_ADD_DOUBLE_2ADDR) {
            value.d = left + right;
        } else if (opcode == OP_SUB_DOUBLE_2ADDR) {
            value.d = left - right;
        } else if (opcode == OP_MUL_DOUBLE_2ADDR) {
            value.d = left * right;
        } else if (opcode == OP_DIV_DOUBLE_2ADDR) {
            value.d = right == 0.0 ? 0.0 : left / right;
        } else if (opcode == OP_REM_DOUBLE_2ADDR) {
            value.d = right == 0.0 ? 0.0 : fmod(left, right);
        } else {
            return -1;
        }
    }
    return set_register(frame, dest, value);
}

typedef enum {
    FLOAT_OP_ADD,
    FLOAT_OP_SUB,
    FLOAT_OP_MUL,
    FLOAT_OP_DIV,
    FLOAT_OP_REM
} FloatOp;

static int execute_float_op(InterpreterFrame *frame, unsigned dest, unsigned left, unsigned right,
                            FloatOp op)
{
    JValue value;
    float a;
    float b;

    if (dest >= frame->register_count || left >= frame->register_count ||
        right >= frame->register_count) {
        return -1;
    }
    a = frame->registers[left].f;
    b = frame->registers[right].f;
    memset(&value, 0, sizeof(value));
    switch (op) {
    case FLOAT_OP_ADD:
        value.f = a + b;
        break;
    case FLOAT_OP_SUB:
        value.f = a - b;
        break;
    case FLOAT_OP_MUL:
        value.f = a * b;
        break;
    case FLOAT_OP_DIV:
        value.f = a / b;
        break;
    case FLOAT_OP_REM:
        value.f = fmodf(a, b);
        break;
    default:
        return -1;
    }
    return set_register(frame, dest, value);
}

/* 23x. vAA = vBB <op> vCC. */
static int execute_float_23x(InterpreterFrame *frame, uint16_t instruction, FloatOp op)
{
    unsigned dest = instruction >> 8;
    uint16_t regs;

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    return execute_float_op(frame, dest, regs & 0xff, regs >> 8, op);
}

/* 12x. vA = vA <op> vB. */
static int execute_float_2addr(InterpreterFrame *frame, uint16_t instruction, FloatOp op)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;

    return execute_float_op(frame, dest, dest, src, op);
}

static int execute_double_binop(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = instruction >> 8;
    uint16_t regs;
    unsigned left_reg;
    unsigned right_reg;
    double left;
    double right;
    double result;
    JValue value;
    uint8_t opcode = instruction & 0xff;

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    left_reg = regs & 0xff;
    right_reg = regs >> 8;
    if (left_reg >= frame->register_count || right_reg >= frame->register_count) {
        return -1;
    }
    left = frame->registers[left_reg].d;
    right = frame->registers[right_reg].d;
    if (opcode == OP_ADD_DOUBLE) {
        result = left + right;
    } else if (opcode == OP_SUB_DOUBLE) {
        result = left - right;
    } else if (opcode == OP_MUL_DOUBLE) {
        result = left * right;
    } else if (opcode == OP_DIV_DOUBLE) {
        result = right == 0.0 ? 0.0 : left / right;
    } else if (opcode == OP_REM_DOUBLE) {
        result = right == 0.0 ? 0.0 : fmod(left, right);
    } else {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.d = result;
    return set_register(frame, dest, value);
}

enum {
    EXEC_OK = 0,
    EXEC_ARITHMETIC = 1,
    EXEC_INVALID = -1
};

/* Placeholder until a pending ArithmeticException can be delivered to a handler. */
static int report_op(int status)
{
    if (status == EXEC_ARITHMETIC) {
        fprintf(stderr, "ArithmeticException\n");
    }
    return status == EXEC_OK ? 0 : -1;
}

/* 23x and 12x integer arithmetic. op is the /2addr opcode. Shifts use the low 5 bits. */
static int apply_int_op(int32_t *left_out, int32_t right, uint8_t op)
{
    int32_t left = *left_out;
    unsigned shift;

    switch (op) {
    case OP_ADD_INT_2ADDR:
        left = (int32_t)((uint32_t)left + (uint32_t)right);
        break;
    case OP_SUB_INT_2ADDR:
        left = (int32_t)((uint32_t)left - (uint32_t)right);
        break;
    case OP_MUL_INT_2ADDR:
        left = (int32_t)((uint32_t)left * (uint32_t)right);
        break;
    case OP_DIV_INT_2ADDR:
    case OP_REM_INT_2ADDR:
        if (right == 0) {
            return EXEC_ARITHMETIC;
        }
        if (left == INT32_MIN && right == -1) {
            left = op == OP_DIV_INT_2ADDR ? INT32_MIN : 0;
            break;
        }
        left = op == OP_DIV_INT_2ADDR ? left / right : left % right;
        break;
    case OP_AND_INT_2ADDR:
        left &= right;
        break;
    case OP_OR_INT_2ADDR:
        left |= right;
        break;
    case OP_XOR_INT_2ADDR:
        left ^= right;
        break;
    case OP_SHL_INT_2ADDR:
        left = (int32_t)((uint32_t)left << ((unsigned)right & 31u));
        break;
    case OP_SHR_INT_2ADDR:
        shift = (unsigned)right & 31u;
        if (left < 0 && shift != 0) {
            left = (int32_t)(((uint32_t)left >> shift) | (~0u << (32u - shift)));
        } else {
            left = (int32_t)((uint32_t)left >> shift);
        }
        break;
    case OP_USHR_INT_2ADDR:
        left = (int32_t)((uint32_t)left >> ((unsigned)right & 31u));
        break;
    default:
        return EXEC_INVALID;
    }
    *left_out = left;
    return EXEC_OK;
}

/* 12x. vA = vA <op> vB. */
static int execute_int_2addr(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    int32_t left;

    if (dest >= frame->register_count || src >= frame->register_count) {
        return -1;
    }
    left = frame->registers[dest].i;
    if (report_op(apply_int_op(&left, frame->registers[src].i, instruction & 0xff)) != 0) {
        return -1;
    }
    return set_int_register(frame, dest, left);
}

/* op is the 23x opcode: add-long through ushr-long. Shifts use the low 6 bits. */
static int apply_long_op(int64_t *left_out, int64_t right, uint8_t op)
{
    int64_t left = *left_out;
    unsigned shift;

    switch (op) {
    case OP_ADD_LONG:
        left = (int64_t)((uint64_t)left + (uint64_t)right);
        break;
    case OP_SUB_LONG:
        left = (int64_t)((uint64_t)left - (uint64_t)right);
        break;
    case OP_MUL_LONG:
        left = (int64_t)((uint64_t)left * (uint64_t)right);
        break;
    case OP_DIV_LONG:
    case OP_REM_LONG:
        if (right == 0) {
            return EXEC_ARITHMETIC;
        }
        if (left == INT64_MIN && right == -1) {
            left = op == OP_DIV_LONG ? INT64_MIN : 0;
            break;
        }
        left = op == OP_DIV_LONG ? left / right : left % right;
        break;
    case OP_AND_LONG:
        left &= right;
        break;
    case OP_OR_LONG:
        left |= right;
        break;
    case OP_XOR_LONG:
        left ^= right;
        break;
    case OP_SHL_LONG:
        left = (int64_t)((uint64_t)left << ((unsigned)right & 63u));
        break;
    case OP_SHR_LONG:
        shift = (unsigned)right & 63u;
        if (left < 0 && shift != 0) {
            left = (int64_t)(((uint64_t)left >> shift) | (~0ULL << (64u - shift)));
        } else {
            left = (int64_t)((uint64_t)left >> shift);
        }
        break;
    case OP_USHR_LONG:
        left = (int64_t)((uint64_t)left >> ((unsigned)right & 63u));
        break;
    default:
        return EXEC_INVALID;
    }
    *left_out = left;
    return EXEC_OK;
}

/* 12x. vA = vA <op> vB. op is add-long through ushr-long. */
static int execute_long_2addr(InterpreterFrame *frame, uint16_t instruction, uint8_t op)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    JValue value;
    int64_t left;

    if (dest >= frame->register_count || src >= frame->register_count) {
        return -1;
    }
    left = frame->registers[dest].j;
    if (report_op(apply_long_op(&left, frame->registers[src].j, op)) != 0) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.j = left;
    return set_register(frame, dest, value);
}

/* 23x. vAA = vBB <op> vCC. The 64-bit value lives in the named register. */
static int execute_long_23x(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = instruction >> 8;
    uint16_t regs;
    unsigned left_reg;
    unsigned right_reg;
    int64_t left;
    JValue value;
    uint8_t op = (uint8_t)(instruction & 0xff);

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    left_reg = regs & 0xff;
    right_reg = regs >> 8;
    if (dest >= frame->register_count || left_reg >= frame->register_count ||
        right_reg >= frame->register_count) {
        return -1;
    }
    left = frame->registers[left_reg].j;
    if (report_op(apply_long_op(&left, frame->registers[right_reg].j, op)) != 0) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    value.j = left;
    return set_register(frame, dest, value);
}

/* 23x. vAA = vBB <op> vCC. */
static int execute_int_23x(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = instruction >> 8;
    uint16_t regs;
    unsigned left_reg;
    unsigned right_reg;
    int32_t left;
    uint8_t op = (uint8_t)((instruction & 0xff) + (OP_ADD_INT_2ADDR - OP_ADD_INT));

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    left_reg = regs & 0xff;
    right_reg = regs >> 8;
    if (dest >= frame->register_count || left_reg >= frame->register_count ||
        right_reg >= frame->register_count) {
        return -1;
    }
    left = frame->registers[left_reg].i;
    if (report_op(apply_int_op(&left, frame->registers[right_reg].i, op)) != 0) {
        return -1;
    }
    return set_int_register(frame, dest, left);
}

/* 22s. vA = vB <op> sign-extended CCCC. rsub-int subtracts vB from the literal. */
static int execute_int_lit16(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t literal_bits;
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    int32_t literal;
    int32_t value;
    uint8_t opcode = instruction & 0xff;

    if (fetch_u16(frame, &literal_bits) != 0 || src >= frame->register_count ||
        dest >= frame->register_count) {
        return -1;
    }
    literal = sign16(literal_bits);
    value = frame->registers[src].i;
    switch (opcode) {
    case OP_ADD_INT_LIT16:
        value = (int32_t)((uint32_t)value + (uint32_t)literal);
        break;
    case OP_RSUB_INT:
        value = (int32_t)((uint32_t)literal - (uint32_t)value);
        break;
    case OP_MUL_INT_LIT16:
        value = (int32_t)((uint32_t)value * (uint32_t)literal);
        break;
    case OP_DIV_INT_LIT16:
    case OP_REM_INT_LIT16:
        if (literal == 0) {
            return report_op(EXEC_ARITHMETIC);
        }
        if (value == INT32_MIN && literal == -1) {
            value = opcode == OP_DIV_INT_LIT16 ? INT32_MIN : 0;
            break;
        }
        value = opcode == OP_DIV_INT_LIT16 ? value / literal : value % literal;
        break;
    case OP_AND_INT_LIT16:
        value &= literal;
        break;
    case OP_OR_INT_LIT16:
        value |= literal;
        break;
    case OP_XOR_INT_LIT16:
        value ^= literal;
        break;
    default:
        return -1;
    }
    return set_int_register(frame, dest, value);
}

/* 22b. vAA = vBB <op> sign-extended CC. rsub-int/lit8 subtracts vBB from the literal. */
static int execute_int_lit8(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t word;
    unsigned dest = instruction >> 8;
    unsigned src;
    int32_t literal;
    int32_t value;
    uint8_t opcode = instruction & 0xff;

    if (fetch_u16(frame, &word) != 0) {
        return -1;
    }
    src = word & 0xff;
    if (dest >= frame->register_count || src >= frame->register_count) {
        return -1;
    }
    literal = sign8(word >> 8);
    value = frame->registers[src].i;
    switch (opcode) {
    case OP_ADD_INT_LIT8:
        value = (int32_t)((uint32_t)value + (uint32_t)literal);
        break;
    case OP_RSUB_INT_LIT8:
        value = (int32_t)((uint32_t)literal - (uint32_t)value);
        break;
    case OP_MUL_INT_LIT8:
        value = (int32_t)((uint32_t)value * (uint32_t)literal);
        break;
    case OP_DIV_INT_LIT8:
    case OP_REM_INT_LIT8:
        if (literal == 0) {
            return report_op(EXEC_ARITHMETIC);
        }
        if (value == INT32_MIN && literal == -1) {
            value = opcode == OP_DIV_INT_LIT8 ? INT32_MIN : 0;
            break;
        }
        value = opcode == OP_DIV_INT_LIT8 ? value / literal : value % literal;
        break;
    case OP_AND_INT_LIT8:
        value &= literal;
        break;
    case OP_OR_INT_LIT8:
        value |= literal;
        break;
    case OP_XOR_INT_LIT8:
        value ^= literal;
        break;
    default:
        return -1;
    }
    return set_int_register(frame, dest, value);
}

/* 12x. vA = vB.length. */
static int execute_array_length(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    JValue array_slot;
    int32_t length;

    if (read_register(frame, src, &array_slot) != 0 || !java_object_check(array_slot.l) ||
        java_object_array_data(array_slot.l, &length, NULL, NULL) != 0) {
        return -1;
    }
    return set_int_register(frame, dest, length);
}

static int aput_matches(uint8_t opcode, const char *desc)
{
    char component;

    if (desc == NULL || desc[0] != '[') {
        return 0;
    }
    component = desc[1];
    switch (opcode) {
    case OP_AGET:
    case OP_APUT:
        return component == 'I' || component == 'F';
    case OP_AGET_WIDE:
    case OP_APUT_WIDE:
        return component == 'J' || component == 'D';
    case OP_AGET_OBJECT:
    case OP_APUT_OBJECT:
        return component == 'L' || component == '[';
    case OP_AGET_BOOLEAN:
    case OP_APUT_BOOLEAN:
        return component == 'Z';
    case OP_AGET_BYTE:
    case OP_APUT_BYTE:
        return component == 'B';
    case OP_AGET_CHAR:
    case OP_APUT_CHAR:
        return component == 'C';
    case OP_AGET_SHORT:
    case OP_APUT_SHORT:
        return component == 'S';
    default:
        return 0;
    }
}

/* 23x. array[index] = value. A wide value lives in the source register. */
static int execute_aput(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned value_reg = instruction >> 8;
    uint16_t regs;
    unsigned array_reg;
    unsigned index_reg;
    JValue array_slot;
    JValue index_slot;
    JValue value;
    const char *desc;
    int32_t length;
    void *data;
    uint8_t opcode = instruction & 0xff;

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    array_reg = regs & 0xff;
    index_reg = regs >> 8;
    if (value_reg >= frame->register_count || array_reg >= frame->register_count ||
        index_reg >= frame->register_count) {
        return -1;
    }
    if (opcode == OP_APUT_WIDE && value_reg + 1 >= frame->register_count) {
        return -1;
    }
    array_slot = frame->registers[array_reg];
    index_slot = frame->registers[index_reg];
    value = frame->registers[value_reg];
    if (!java_object_check(array_slot.l)) {
        return -1;
    }
    desc = java_object_class(array_slot.l);
    if (!aput_matches(opcode, desc) ||
        java_object_array_data(array_slot.l, &length, NULL, &data) != 0) {
        return -1;
    }
    if (index_slot.i < 0 || index_slot.i >= length || data == NULL) {
        return -1;
    }
    switch (opcode) {
    case OP_APUT:
        ((int32_t *)data)[index_slot.i] = value.i;
        break;
    case OP_APUT_WIDE:
        ((int64_t *)data)[index_slot.i] = value.j;
        break;
    case OP_APUT_OBJECT:
        if (value.l != NULL && !java_object_check(value.l)) {
            return -1;
        }
        ((JavaObject **)data)[index_slot.i] = value.l;
        break;
    case OP_APUT_BOOLEAN:
        ((uint8_t *)data)[index_slot.i] = value.i != 0;
        break;
    case OP_APUT_BYTE:
        ((int8_t *)data)[index_slot.i] = (int8_t)value.i;
        break;
    case OP_APUT_CHAR:
        ((uint16_t *)data)[index_slot.i] = (uint16_t)value.i;
        break;
    case OP_APUT_SHORT:
        ((int16_t *)data)[index_slot.i] = (int16_t)value.i;
        break;
    default:
        return -1;
    }
    return 0;
}

/* 23x. vAA = array[index]. A wide result stays in vAA. */
static int execute_aget(InterpreterFrame *frame, uint16_t instruction)
{
    unsigned dest = instruction >> 8;
    uint16_t regs;
    unsigned array_reg;
    unsigned index_reg;
    JValue array_slot;
    JValue index_slot;
    JValue value;
    const char *desc;
    int32_t length;
    void *data;
    int32_t index;
    uint8_t opcode = instruction & 0xff;

    if (fetch_u16(frame, &regs) != 0) {
        return -1;
    }
    array_reg = regs & 0xff;
    index_reg = regs >> 8;
    if (dest >= frame->register_count || array_reg >= frame->register_count ||
        index_reg >= frame->register_count) {
        return -1;
    }
    if (opcode == OP_AGET_WIDE && dest + 1 >= frame->register_count) {
        return -1;
    }
    array_slot = frame->registers[array_reg];
    index_slot = frame->registers[index_reg];
    if (!java_object_check(array_slot.l)) {
        return -1;
    }
    desc = java_object_class(array_slot.l);
    if (!aput_matches(opcode, desc) ||
        java_object_array_data(array_slot.l, &length, NULL, &data) != 0) {
        return -1;
    }
    index = index_slot.i;
    if (index < 0 || index >= length || data == NULL) {
        return -1;
    }
    memset(&value, 0, sizeof(value));
    switch (opcode) {
    case OP_AGET:
        value.i = ((int32_t *)data)[index];
        break;
    case OP_AGET_WIDE:
        value.j = ((int64_t *)data)[index];
        break;
    case OP_AGET_OBJECT:
        value.l = ((JavaObject **)data)[index];
        break;
    case OP_AGET_BOOLEAN:
        value.i = ((uint8_t *)data)[index] != 0;
        break;
    case OP_AGET_BYTE:
        value.i = ((int8_t *)data)[index];
        break;
    case OP_AGET_CHAR:
        value.i = ((uint16_t *)data)[index];
        break;
    case OP_AGET_SHORT:
        value.i = ((int16_t *)data)[index];
        break;
    default:
        return -1;
    }
    return set_register(frame, dest, value);
}

static int execute_method(InterpreterFrame *frame)
{
    for (;;) {
        uint16_t instruction;
        uint8_t opcode;
        uint32_t start = frame->pc;
        uint16_t skipped;
        int failed = 0;

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
            failed = execute_return(frame, instruction, opcode) != 0;
            if (!failed) {
                return 0;
            }
            break;
        case OP_MOVE:
        case OP_MOVE_OBJECT:
            failed = execute_move(frame, instruction, MOVE_12X, 0) != 0;
            break;
        case OP_MOVE_FROM16:
        case OP_MOVE_OBJECT_FROM16:
            failed = execute_move(frame, instruction, MOVE_22X, 0) != 0;
            break;
        case OP_MOVE_16:
        case OP_MOVE_OBJECT_16:
            failed = execute_move(frame, instruction, MOVE_32X, 0) != 0;
            break;
        case OP_MOVE_WIDE:
            failed = execute_move(frame, instruction, MOVE_12X, 1) != 0;
            break;
        case OP_MOVE_WIDE_FROM16:
            failed = execute_move(frame, instruction, MOVE_22X, 1) != 0;
            break;
        case OP_MOVE_WIDE_16:
            failed = execute_move(frame, instruction, MOVE_32X, 1) != 0;
            break;
        case OP_MOVE_RESULT:
        case OP_MOVE_RESULT_WIDE:
        case OP_MOVE_RESULT_OBJECT:
            failed = execute_move_result(frame, instruction, opcode) != 0;
            break;
        case OP_CONST_4:
            failed = execute_const_4(frame, instruction) != 0;
            break;
        case OP_CONST_16:
            failed = execute_const16(frame, instruction) != 0;
            break;
        case OP_CONST_HIGH16:
            failed = execute_const_high16(frame, instruction) != 0;
            break;
        case OP_CONST_WIDE_16:
            failed = execute_const_wide16(frame, instruction) != 0;
            break;
        case OP_CONST_WIDE_32:
            failed = execute_const_wide32(frame, instruction) != 0;
            break;
        case OP_CONST_WIDE:
            failed = execute_const_wide(frame, instruction) != 0;
            break;
        case OP_CONST_WIDE_HIGH16:
            failed = execute_const_wide_high16(frame, instruction) != 0;
            break;
        case OP_CONST:
            failed = execute_const(frame, instruction) != 0;
            break;
        case OP_CONST_STRING:
            failed = execute_const_string(frame, instruction) != 0;
            break;
        case OP_CHECK_CAST:
            failed = execute_check_cast(frame) != 0;
            break;
        case OP_ARRAY_LENGTH:
            failed = execute_array_length(frame, instruction) != 0;
            break;
        case OP_NEW_INSTANCE:
            failed = execute_new_instance(frame, instruction) != 0;
            break;
        case OP_NEW_ARRAY:
            failed = execute_new_array(frame, instruction) != 0;
            break;
        case OP_FILLED_NEW_ARRAY:
            failed = execute_filled_new_array(frame, instruction) != 0;
            break;
        case OP_FILL_ARRAY_DATA:
            failed = execute_fill_array_data(frame, instruction, start) != 0;
            break;
        case OP_AGET:
        case OP_AGET_WIDE:
        case OP_AGET_OBJECT:
        case OP_AGET_BOOLEAN:
        case OP_AGET_BYTE:
        case OP_AGET_CHAR:
        case OP_AGET_SHORT:
            failed = execute_aget(frame, instruction) != 0;
            break;
        case OP_APUT:
        case OP_APUT_WIDE:
        case OP_APUT_OBJECT:
        case OP_APUT_BOOLEAN:
        case OP_APUT_BYTE:
        case OP_APUT_CHAR:
        case OP_APUT_SHORT:
            failed = execute_aput(frame, instruction) != 0;
            break;
        case OP_NOP:
            break;
        case OP_THROW: {
            JValue thrown;
            const char *msg = "exception";

            if (read_register(frame, instruction >> 8, &thrown) == 0 &&
                java_object_check(thrown.l)) {
                msg = java_object_text(thrown.l);
                if (msg == NULL || msg[0] == '\0') {
                    msg = java_object_class(thrown.l);
                }
            }
            fprintf(stderr, "throw %s\n", msg);
            return -1;
        }
        case OP_GOTO:
            branch_to(frame, start, sign8(instruction >> 8));
            break;
        case OP_GOTO_16:
            failed = fetch_u16(frame, &skipped) != 0;
            if (!failed) {
                branch_to(frame, start, sign16(skipped));
            }
            break;
        case OP_CMPL_FLOAT:
        case OP_CMPG_FLOAT:
        case OP_CMPL_DOUBLE:
        case OP_CMPG_DOUBLE:
        case OP_CMP_LONG:
            failed = execute_cmp(frame, instruction) != 0;
            break;
        case OP_IF_EQ:
        case OP_IF_NE:
        case OP_IF_LT:
        case OP_IF_GE:
        case OP_IF_GT:
        case OP_IF_LE:
        case OP_IF_EQZ:
        case OP_IF_NEZ:
        case OP_IF_LTZ:
        case OP_IF_GEZ:
        case OP_IF_GTZ:
        case OP_IF_LEZ:
            failed = execute_if(frame, instruction, start) != 0;
            break;
        case OP_IGET:
        case OP_IGET_WIDE:
        case OP_IGET_OBJECT:
        case OP_IGET_BOOLEAN:
        case OP_IGET_BYTE:
        case OP_IGET_CHAR:
        case OP_IGET_SHORT:
        case OP_IPUT:
        case OP_IPUT_WIDE:
        case OP_IPUT_OBJECT:
        case OP_IPUT_BOOLEAN:
        case OP_IPUT_BYTE:
        case OP_IPUT_CHAR:
        case OP_IPUT_SHORT:
            failed = execute_field(frame, instruction, opcode) != 0;
            break;
        case OP_SGET:
            failed = execute_sget(frame, instruction, ART_FIELD_INT) != 0;
            break;
        case OP_SGET_OBJECT:
            failed = execute_sget(frame, instruction, ART_FIELD_OBJECT) != 0;
            break;
        case OP_SPUT:
            failed = execute_sput(frame, instruction, ART_FIELD_INT) != 0;
            break;
        case OP_SPUT_OBJECT:
            failed = execute_sput(frame, instruction, ART_FIELD_OBJECT) != 0;
            break;
        case OP_SPUT_BOOLEAN:
            failed = fetch_u16(frame, &skipped) != 0;
            break;
        case OP_INVOKE_VIRTUAL:
        case OP_INVOKE_SUPER:
        case OP_INVOKE_DIRECT:
        case OP_INVOKE_STATIC:
        case OP_INVOKE_INTERFACE:
            failed = execute_invoke(frame, instruction) != 0;
            break;
        case OP_INVOKE_VIRTUAL_RANGE:
        case OP_INVOKE_SUPER_RANGE:
        case OP_INVOKE_DIRECT_RANGE:
        case OP_INVOKE_STATIC_RANGE:
        case OP_INVOKE_INTERFACE_RANGE:
            failed = execute_invoke_range(frame, instruction) != 0;
            break;
        case OP_INT_TO_LONG:
        case OP_INT_TO_FLOAT:
        case OP_INT_TO_DOUBLE:
        case OP_LONG_TO_INT:
        case OP_LONG_TO_FLOAT:
        case OP_LONG_TO_DOUBLE:
        case OP_FLOAT_TO_INT:
        case OP_FLOAT_TO_LONG:
        case OP_FLOAT_TO_DOUBLE:
        case OP_DOUBLE_TO_INT:
        case OP_DOUBLE_TO_LONG:
        case OP_DOUBLE_TO_FLOAT:
        case OP_INT_TO_BYTE:
        case OP_INT_TO_CHAR:
        case OP_INT_TO_SHORT:
            failed = execute_convert(frame, instruction) != 0;
            break;
        case OP_NEG_DOUBLE:
        case OP_ADD_DOUBLE_2ADDR:
        case OP_SUB_DOUBLE_2ADDR:
        case OP_MUL_DOUBLE_2ADDR:
        case OP_DIV_DOUBLE_2ADDR:
        case OP_REM_DOUBLE_2ADDR:
            failed = execute_unop(frame, instruction) != 0;
            break;
        case OP_ADD_FLOAT:
            failed = execute_float_23x(frame, instruction, FLOAT_OP_ADD) != 0;
            break;
        case OP_SUB_FLOAT:
            failed = execute_float_23x(frame, instruction, FLOAT_OP_SUB) != 0;
            break;
        case OP_MUL_FLOAT:
            failed = execute_float_23x(frame, instruction, FLOAT_OP_MUL) != 0;
            break;
        case OP_DIV_FLOAT:
            failed = execute_float_23x(frame, instruction, FLOAT_OP_DIV) != 0;
            break;
        case OP_REM_FLOAT:
            failed = execute_float_23x(frame, instruction, FLOAT_OP_REM) != 0;
            break;
        case OP_ADD_FLOAT_2ADDR:
            failed = execute_float_2addr(frame, instruction, FLOAT_OP_ADD) != 0;
            break;
        case OP_SUB_FLOAT_2ADDR:
            failed = execute_float_2addr(frame, instruction, FLOAT_OP_SUB) != 0;
            break;
        case OP_MUL_FLOAT_2ADDR:
            failed = execute_float_2addr(frame, instruction, FLOAT_OP_MUL) != 0;
            break;
        case OP_DIV_FLOAT_2ADDR:
            failed = execute_float_2addr(frame, instruction, FLOAT_OP_DIV) != 0;
            break;
        case OP_REM_FLOAT_2ADDR:
            failed = execute_float_2addr(frame, instruction, FLOAT_OP_REM) != 0;
            break;
        case OP_ADD_DOUBLE:
        case OP_SUB_DOUBLE:
        case OP_MUL_DOUBLE:
        case OP_DIV_DOUBLE:
        case OP_REM_DOUBLE:
            failed = execute_double_binop(frame, instruction) != 0;
            break;
        case OP_ADD_INT:
        case OP_SUB_INT:
        case OP_MUL_INT:
        case OP_DIV_INT:
        case OP_REM_INT:
        case OP_AND_INT:
        case OP_OR_INT:
        case OP_XOR_INT:
        case OP_SHL_INT:
        case OP_SHR_INT:
        case OP_USHR_INT:
            failed = execute_int_23x(frame, instruction) != 0;
            break;
        case OP_ADD_LONG:
        case OP_SUB_LONG:
        case OP_MUL_LONG:
        case OP_DIV_LONG:
        case OP_REM_LONG:
        case OP_AND_LONG:
        case OP_OR_LONG:
        case OP_XOR_LONG:
        case OP_SHL_LONG:
        case OP_SHR_LONG:
        case OP_USHR_LONG:
            failed = execute_long_23x(frame, instruction) != 0;
            break;
        case OP_ADD_INT_2ADDR:
        case OP_SUB_INT_2ADDR:
        case OP_MUL_INT_2ADDR:
        case OP_DIV_INT_2ADDR:
        case OP_REM_INT_2ADDR:
        case OP_AND_INT_2ADDR:
        case OP_OR_INT_2ADDR:
        case OP_XOR_INT_2ADDR:
        case OP_SHL_INT_2ADDR:
        case OP_SHR_INT_2ADDR:
        case OP_USHR_INT_2ADDR:
            failed = execute_int_2addr(frame, instruction) != 0;
            break;
        case OP_ADD_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_ADD_LONG) != 0;
            break;
        case OP_SUB_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_SUB_LONG) != 0;
            break;
        case OP_MUL_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_MUL_LONG) != 0;
            break;
        case OP_DIV_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_DIV_LONG) != 0;
            break;
        case OP_REM_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_REM_LONG) != 0;
            break;
        case OP_AND_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_AND_LONG) != 0;
            break;
        case OP_OR_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_OR_LONG) != 0;
            break;
        case OP_XOR_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_XOR_LONG) != 0;
            break;
        case OP_SHL_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_SHL_LONG) != 0;
            break;
        case OP_SHR_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_SHR_LONG) != 0;
            break;
        case OP_USHR_LONG_2ADDR:
            failed = execute_long_2addr(frame, instruction, OP_USHR_LONG) != 0;
            break;
        case OP_ADD_INT_LIT16:
        case OP_RSUB_INT:
        case OP_MUL_INT_LIT16:
        case OP_DIV_INT_LIT16:
        case OP_REM_INT_LIT16:
        case OP_AND_INT_LIT16:
        case OP_OR_INT_LIT16:
        case OP_XOR_INT_LIT16:
            failed = execute_int_lit16(frame, instruction) != 0;
            break;
        case OP_ADD_INT_LIT8:
        case OP_RSUB_INT_LIT8:
        case OP_MUL_INT_LIT8:
        case OP_DIV_INT_LIT8:
        case OP_REM_INT_LIT8:
        case OP_AND_INT_LIT8:
        case OP_OR_INT_LIT8:
        case OP_XOR_INT_LIT8:
            failed = execute_int_lit8(frame, instruction) != 0;
            break;
        default:
            fprintf(stderr, "unsupported opcode %#x\n", opcode);
            return -1;
        }
        if (failed) {
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
    invoke_lock();
    code = dex_file_code_item(art_method_dex(method), art_method_id(method));
    if (code == NULL) {
        invoke_unlock();
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
        invoke_unlock();
        return -1;
    }
    frame.registers = calloc(frame.register_count ? frame.register_count : 1, sizeof(*frame.registers));
    if (frame.registers == NULL) {
        invoke_unlock();
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
    invoke_unlock();
    return rc;
}
