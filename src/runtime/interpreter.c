#include "runtime.h"
#include "art_field.h"
#include "java_object.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    OP_MOVE_WIDE = 0x04,
    OP_MOVE_OBJECT = 0x07,
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
    OP_NEW_INSTANCE = 0x22,
    OP_NEW_ARRAY = 0x23,
    OP_FILL_ARRAY_DATA = 0x26,
    OP_THROW = 0x27,
    OP_GOTO = 0x28,
    OP_GOTO_16 = 0x29,
    OP_IF_EQ = 0x32,
    OP_IF_NE = 0x33,
    OP_IF_LT = 0x34,
    OP_IF_GE = 0x35,
    OP_IF_GT = 0x36,
    OP_IF_LE = 0x37,
    OP_IF_EQZ = 0x38,
    OP_IF_NEZ = 0x39,
    OP_IGET = 0x52,
    OP_IGET_OBJECT = 0x54,
    OP_IPUT = 0x59,
    OP_IPUT_OBJECT = 0x5b,
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
    OP_NEG_DOUBLE = 0x80,
    OP_INT_TO_CHAR = 0x8e,
    OP_ADD_DOUBLE = 0xab,
    OP_SUB_DOUBLE = 0xac,
    OP_MUL_DOUBLE = 0xad,
    OP_DIV_DOUBLE = 0xae,
    OP_REM_DOUBLE = 0xaf,
    OP_MUL_INT_2ADDR = 0xb2,
    OP_ADD_DOUBLE_2ADDR = 0xcb,
    OP_SUB_DOUBLE_2ADDR = 0xcc,
    OP_MUL_DOUBLE_2ADDR = 0xcd,
    OP_DIV_DOUBLE_2ADDR = 0xce,
    OP_REM_DOUBLE_2ADDR = 0xcf,
    OP_ADD_INT_LIT8 = 0xd8
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
    if (opcode == OP_INVOKE_SUPER) {
        return "invoke-super";
    }
    if (opcode == OP_INVOKE_DIRECT) {
        return "invoke-direct";
    }
    if (opcode == OP_INVOKE_STATIC) {
        return "invoke-static";
    }
    if (opcode == OP_INVOKE_INTERFACE) {
        return "invoke-interface";
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

    unsigned opcode = instruction & 0xff;

    dex_file_method_at(art_method_dex(frame->method), method_idx, &class_desc, &name, &proto);
    fprintf(stderr, "%s %s->%s%s\n", invoke_kind((uint8_t)opcode), class_desc, name, proto);

    switch (opcode) {
    case OP_INVOKE_SUPER:
        callee = runtime_resolve_super(frame->runtime, frame->method, method_idx);
        break;
    case OP_INVOKE_VIRTUAL: {
        void *object = args[0].l;

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

static int execute_move(InterpreterFrame *frame, uint16_t instruction, int wide)
{
    unsigned dest = (instruction >> 8) & 0x0f;
    unsigned src = instruction >> 12;
    JValue value;

    if (read_register(frame, src, &value) != 0 || set_register(frame, dest, value) != 0) {
        return -1;
    }
    if (wide && dest + 1 < frame->register_count && src + 1 < frame->register_count) {
        frame->registers[dest + 1] = frame->registers[src + 1];
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

static int execute_field(InterpreterFrame *frame, uint16_t instruction, int store, int is_object)
{
    unsigned value_reg = (instruction >> 8) & 0x0f;
    unsigned object_reg = instruction >> 12;
    uint16_t field_idx;
    const char *name = NULL;
    JValue object_slot;
    JValue value;
    JavaObject *object;

    (void)is_object;
    if (fetch_u16(frame, &field_idx) != 0 ||
        dex_file_field_at(art_method_dex(frame->method), field_idx, NULL, &name, NULL) != 0 ||
        read_register(frame, object_reg, &object_slot) != 0 ||
        !java_object_check(object_slot.l)) {
        return -1;
    }
    object = object_slot.l;
    if (store) {
        if (read_register(frame, value_reg, &value) != 0) {
            return -1;
        }
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
        return left < right;
    case OP_IF_GE:
        return left >= right;
    case OP_IF_GT:
        return left > right;
    case OP_IF_LE:
        return left <= right;
    default:
        return 0;
    }
}

static int execute_if(InterpreterFrame *frame, uint16_t instruction, uint32_t start)
{
    uint8_t opcode = instruction & 0xff;
    uint16_t offset_unit;
    int32_t left;
    int32_t right = 0;
    unsigned left_reg;
    unsigned right_reg;

    if (opcode == OP_IF_EQZ || opcode == OP_IF_NEZ) {
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
    } else if (opcode == OP_INT_TO_CHAR) {
        value.i = (int32_t)(value.i & 0xffff);
    } else if (opcode == OP_MUL_INT_2ADDR) {
        if (dest >= frame->register_count) {
            return -1;
        }
        value.i = frame->registers[dest].i * value.i;
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

static int execute_add_int_lit8(InterpreterFrame *frame, uint16_t instruction)
{
    uint16_t word;
    unsigned dest = instruction >> 8;
    unsigned src;
    int32_t literal;
    int32_t value;

    if (fetch_u16(frame, &word) != 0 || (word & 0xff) >= frame->register_count) {
        return -1;
    }
    src = word & 0xff;
    literal = sign8(word >> 8);
    value = frame->registers[src].i + literal;
    return set_int_register(frame, dest, value);
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
            failed = execute_move(frame, instruction, 0) != 0;
            break;
        case OP_MOVE_WIDE:
            failed = execute_move(frame, instruction, 1) != 0;
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
        case OP_NEW_INSTANCE:
            failed = execute_new_instance(frame, instruction) != 0;
            break;
        case OP_NEW_ARRAY:
            failed = execute_new_array(frame, instruction) != 0;
            break;
        case OP_FILL_ARRAY_DATA:
            failed = execute_fill_array_data(frame, instruction, start) != 0;
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
        case OP_IF_EQ:
        case OP_IF_NE:
        case OP_IF_LT:
        case OP_IF_GE:
        case OP_IF_GT:
        case OP_IF_LE:
        case OP_IF_EQZ:
        case OP_IF_NEZ:
            failed = execute_if(frame, instruction, start) != 0;
            break;
        case OP_IGET:
        case OP_IGET_OBJECT:
            failed = execute_field(frame, instruction, 0, opcode == OP_IGET_OBJECT) != 0;
            break;
        case OP_IPUT:
        case OP_IPUT_OBJECT:
            failed = execute_field(frame, instruction, 1, opcode == OP_IPUT_OBJECT) != 0;
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
        case OP_NEG_DOUBLE:
        case OP_INT_TO_CHAR:
        case OP_MUL_INT_2ADDR:
        case OP_ADD_DOUBLE_2ADDR:
        case OP_SUB_DOUBLE_2ADDR:
        case OP_MUL_DOUBLE_2ADDR:
        case OP_DIV_DOUBLE_2ADDR:
        case OP_REM_DOUBLE_2ADDR:
            failed = execute_unop(frame, instruction) != 0;
            break;
        case OP_ADD_DOUBLE:
        case OP_SUB_DOUBLE:
        case OP_MUL_DOUBLE:
        case OP_DIV_DOUBLE:
        case OP_REM_DOUBLE:
            failed = execute_double_binop(frame, instruction) != 0;
            break;
        case OP_ADD_INT_LIT8:
            failed = execute_add_int_lit8(frame, instruction) != 0;
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
