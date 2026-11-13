#include "dex.h"

#include "dex_internal.h"

#include <stdlib.h>
#include <string.h>

struct DexFile {
    uint8_t *bytes;
    size_t size;
    uint32_t string_ids_size;
    uint32_t string_ids_off;
    uint32_t type_ids_size;
    uint32_t type_ids_off;
    uint32_t proto_ids_size;
    uint32_t proto_ids_off;
    uint32_t field_ids_size;
    uint32_t field_ids_off;
    uint32_t method_ids_size;
    uint32_t method_ids_off;
    uint32_t class_defs_size;
    uint32_t class_defs_off;
    char **protos;
    uint32_t *code_off;
    CodeItem *code;
    int *field_has_int;
    int32_t *field_int;
};

static uint16_t ru16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static uint32_t ru32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static int read_u16(const DexFile *dex, uint32_t off, uint16_t *out)
{
    if ((size_t)off + 2 > dex->size) {
        return -1;
    }
    *out = ru16(dex->bytes + off);
    return 0;
}

static int read_u32(const DexFile *dex, uint32_t off, uint32_t *out)
{
    if ((size_t)off + 4 > dex->size) {
        return -1;
    }
    *out = ru32(dex->bytes + off);
    return 0;
}

static int read_uleb(const DexFile *dex, size_t *off, uint32_t *out)
{
    uint32_t result = 0;
    int shift = 0;

    for (;;) {
        uint8_t byte;
        if (*off >= dex->size || shift > 28) {
            return -1;
        }
        byte = dex->bytes[(*off)++];
        result |= (uint32_t)(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            *out = result;
            return 0;
        }
        shift += 7;
    }
}

static const char *dex_string(const DexFile *dex, uint32_t index)
{
    uint32_t off;
    size_t cursor;
    uint32_t unused;

    if (index >= dex->string_ids_size) {
        return NULL;
    }
    if (read_u32(dex, dex->string_ids_off + index * 4, &off) != 0) {
        return NULL;
    }
    cursor = off;
    if (read_uleb(dex, &cursor, &unused) != 0 || cursor >= dex->size) {
        return NULL;
    }
    return (const char *)(dex->bytes + cursor);
}

static const char *type_descriptor(const DexFile *dex, uint32_t type_idx)
{
    uint32_t desc_idx;

    if (type_idx >= dex->type_ids_size) {
        return NULL;
    }
    if (read_u32(dex, dex->type_ids_off + type_idx * 4, &desc_idx) != 0) {
        return NULL;
    }
    return dex_string(dex, desc_idx);
}

static char *build_proto(const DexFile *dex, uint32_t proto_idx)
{
    uint32_t shorty_idx;
    uint32_t return_idx;
    uint32_t params_off;
    const char *ret;
    char *proto;
    size_t cap;
    size_t len;
    uint32_t count = 0;
    uint32_t i;

    (void)shorty_idx;
    if (proto_idx >= dex->proto_ids_size) {
        return NULL;
    }
    if (read_u32(dex, dex->proto_ids_off + proto_idx * 12, &shorty_idx) != 0 ||
        read_u32(dex, dex->proto_ids_off + proto_idx * 12 + 4, &return_idx) != 0 ||
        read_u32(dex, dex->proto_ids_off + proto_idx * 12 + 8, &params_off) != 0) {
        return NULL;
    }
    ret = type_descriptor(dex, return_idx);
    if (ret == NULL) {
        return NULL;
    }

    cap = strlen(ret) + 3;
    if (params_off != 0) {
        if (read_u32(dex, params_off, &count) != 0) {
            return NULL;
        }
        for (i = 0; i < count; i++) {
            uint16_t type_idx;
            const char *desc;
            if (read_u16(dex, params_off + 4 + i * 2, &type_idx) != 0) {
                return NULL;
            }
            desc = type_descriptor(dex, type_idx);
            if (desc == NULL) {
                return NULL;
            }
            cap += strlen(desc);
        }
    }

    proto = malloc(cap);
    if (proto == NULL) {
        return NULL;
    }
    proto[0] = '(';
    len = 1;
    for (i = 0; i < count; i++) {
        uint16_t type_idx;
        const char *desc;
        size_t n;
        if (read_u16(dex, params_off + 4 + i * 2, &type_idx) != 0) {
            free(proto);
            return NULL;
        }
        desc = type_descriptor(dex, type_idx);
        n = strlen(desc);
        memcpy(proto + len, desc, n);
        len += n;
    }
    proto[len++] = ')';
    memcpy(proto + len, ret, strlen(ret) + 1);
    return proto;
}

static int read_encoded_value(const DexFile *dex, size_t *off, int *is_int, int32_t *out_int)
{
    uint8_t header;
    uint8_t type;
    uint8_t arg;
    size_t nbytes;
    uint32_t value;
    size_t i;

    *is_int = 0;
    *out_int = 0;
    if (*off >= dex->size) {
        return -1;
    }
    header = dex->bytes[(*off)++];
    type = header & 0x1f;
    arg = header >> 5;
    if (type == 0x1c || type == 0x1d) {
        return -1;
    }
    if (type == 0x1e || type == 0x1f) {
        return 0;
    }
    nbytes = (size_t)arg + 1;
    if (*off + nbytes > dex->size || nbytes > 8) {
        return -1;
    }
    value = 0;
    for (i = 0; i < nbytes && i < 4; i++) {
        value |= (uint32_t)dex->bytes[*off + i] << (8 * i);
    }
    if ((type == 0x00 || type == 0x02 || type == 0x03 || type == 0x04) && nbytes <= 4) {
        uint32_t bits = (uint32_t)nbytes * 8;
        if (bits < 32 && (value & (1u << (bits - 1))) != 0) {
            value |= ~((1u << bits) - 1);
        }
        *is_int = 1;
        *out_int = (int32_t)value;
    }
    *off += nbytes;
    return 0;
}

static int scan_class(DexFile *dex, uint32_t class_def_off)
{
    uint32_t class_data_off;
    uint32_t static_values_off;
    size_t cursor;
    uint32_t static_fields;
    uint32_t instance_fields;
    uint32_t direct_methods;
    uint32_t virtual_methods;
    uint32_t i;
    uint32_t field_idx = 0;
    uint32_t method_idx = 0;
    uint32_t value_count = 0;
    size_t value_cursor = 0;
    uint32_t value_index = 0;

    if (read_u32(dex, class_def_off + 24, &class_data_off) != 0 ||
        read_u32(dex, class_def_off + 28, &static_values_off) != 0) {
        return -1;
    }
    if (class_data_off == 0) {
        return 0;
    }
    cursor = class_data_off;
    if (read_uleb(dex, &cursor, &static_fields) != 0 ||
        read_uleb(dex, &cursor, &instance_fields) != 0 ||
        read_uleb(dex, &cursor, &direct_methods) != 0 ||
        read_uleb(dex, &cursor, &virtual_methods) != 0) {
        return -1;
    }

    if (static_values_off != 0) {
        value_cursor = static_values_off;
        if (read_uleb(dex, &value_cursor, &value_count) != 0) {
            return -1;
        }
    }

    for (i = 0; i < static_fields; i++) {
        uint32_t diff;
        uint32_t access;
        int is_int = 0;
        int32_t value = 0;
        if (read_uleb(dex, &cursor, &diff) != 0 || read_uleb(dex, &cursor, &access) != 0) {
            return -1;
        }
        (void)access;
        field_idx += diff;
        if (field_idx >= dex->field_ids_size) {
            return -1;
        }
        if (value_index < value_count) {
            if (read_encoded_value(dex, &value_cursor, &is_int, &value) != 0) {
                return -1;
            }
            value_index++;
        }
        dex->field_has_int[field_idx] = 1;
        dex->field_int[field_idx] = is_int ? value : 0;
    }

    for (i = 0; i < instance_fields; i++) {
        uint32_t diff;
        uint32_t access;
        if (read_uleb(dex, &cursor, &diff) != 0 || read_uleb(dex, &cursor, &access) != 0) {
            return -1;
        }
        (void)diff;
        (void)access;
    }

    for (i = 0; i < direct_methods + virtual_methods; i++) {
        uint32_t diff;
        uint32_t access;
        uint32_t code_off;
        if (i == direct_methods) {
            method_idx = 0;
        }
        if (read_uleb(dex, &cursor, &diff) != 0 || read_uleb(dex, &cursor, &access) != 0 ||
            read_uleb(dex, &cursor, &code_off) != 0) {
            return -1;
        }
        (void)access;
        method_idx += diff;
        if (method_idx >= dex->method_ids_size) {
            return -1;
        }
        dex->code_off[method_idx] = code_off;
        if (code_off != 0) {
            uint32_t insns_size;
            if ((size_t)code_off + 16 > dex->size ||
                read_u32(dex, code_off + 12, &insns_size) != 0 ||
                (size_t)code_off + 16 + (size_t)insns_size * 2 > dex->size) {
                return -1;
            }
            dex->code[method_idx].registers_size = ru16(dex->bytes + code_off);
            dex->code[method_idx].ins_size = ru16(dex->bytes + code_off + 2);
            dex->code[method_idx].count = insns_size;
            dex->code[method_idx].insns = (const uint16_t *)(dex->bytes + code_off + 16);
        }
    }
    return 0;
}

DexFile *dex_file_open(const uint8_t *bytes, size_t size)
{
    DexFile *dex;
    uint32_t i;

    if (bytes == NULL || size < 0x70 || memcmp(bytes, "dex\n", 4) != 0) {
        return NULL;
    }

    dex = calloc(1, sizeof(*dex));
    if (dex == NULL) {
        return NULL;
    }
    dex->bytes = malloc(size);
    if (dex->bytes == NULL) {
        free(dex);
        return NULL;
    }
    memcpy(dex->bytes, bytes, size);
    dex->size = size;

    if (read_u32(dex, 56, &dex->string_ids_size) != 0 ||
        read_u32(dex, 60, &dex->string_ids_off) != 0 ||
        read_u32(dex, 64, &dex->type_ids_size) != 0 ||
        read_u32(dex, 68, &dex->type_ids_off) != 0 ||
        read_u32(dex, 72, &dex->proto_ids_size) != 0 ||
        read_u32(dex, 76, &dex->proto_ids_off) != 0 ||
        read_u32(dex, 80, &dex->field_ids_size) != 0 ||
        read_u32(dex, 84, &dex->field_ids_off) != 0 ||
        read_u32(dex, 88, &dex->method_ids_size) != 0 ||
        read_u32(dex, 92, &dex->method_ids_off) != 0 ||
        read_u32(dex, 96, &dex->class_defs_size) != 0 ||
        read_u32(dex, 100, &dex->class_defs_off) != 0) {
        dex_file_close(dex);
        return NULL;
    }

    if (dex->proto_ids_size > 0) {
        dex->protos = calloc(dex->proto_ids_size, sizeof(*dex->protos));
        if (dex->protos == NULL) {
            dex_file_close(dex);
            return NULL;
        }
        for (i = 0; i < dex->proto_ids_size; i++) {
            dex->protos[i] = build_proto(dex, i);
            if (dex->protos[i] == NULL) {
                dex_file_close(dex);
                return NULL;
            }
        }
    }
    if (dex->method_ids_size > 0) {
        dex->code_off = calloc(dex->method_ids_size, sizeof(*dex->code_off));
        dex->code = calloc(dex->method_ids_size, sizeof(*dex->code));
        if (dex->code_off == NULL || dex->code == NULL) {
            dex_file_close(dex);
            return NULL;
        }
    }
    if (dex->field_ids_size > 0) {
        dex->field_has_int = calloc(dex->field_ids_size, sizeof(*dex->field_has_int));
        dex->field_int = calloc(dex->field_ids_size, sizeof(*dex->field_int));
        if (dex->field_has_int == NULL || dex->field_int == NULL) {
            dex_file_close(dex);
            return NULL;
        }
    }

    for (i = 0; i < dex->class_defs_size; i++) {
        if (scan_class(dex, dex->class_defs_off + i * 32) != 0) {
            dex_file_close(dex);
            return NULL;
        }
    }
    return dex;
}

void dex_file_close(DexFile *dex)
{
    uint32_t i;

    if (dex == NULL) {
        return;
    }
    if (dex->protos != NULL) {
        for (i = 0; i < dex->proto_ids_size; i++) {
            free(dex->protos[i]);
        }
        free(dex->protos);
    }
    free(dex->code_off);
    free(dex->code);
    free(dex->field_has_int);
    free(dex->field_int);
    free(dex->bytes);
    free(dex);
}

size_t dex_file_class_count(const DexFile *dex)
{
    if (dex == NULL) {
        return 0;
    }
    return dex->class_defs_size;
}

const char *dex_file_class_descriptor(const DexFile *dex, uint32_t class_idx)
{
    uint32_t type_idx;

    if (dex == NULL || class_idx >= dex->class_defs_size) {
        return NULL;
    }
    if (read_u32(dex, dex->class_defs_off + class_idx * 32, &type_idx) != 0) {
        return NULL;
    }
    return type_descriptor(dex, type_idx);
}

const CodeItem *dex_file_code_item(const DexFile *dex, uint32_t method_idx)
{
    if (dex == NULL || method_idx >= dex->method_ids_size || dex->code_off == NULL ||
        dex->code_off[method_idx] == 0) {
        return NULL;
    }
    return &dex->code[method_idx];
}

int dex_file_method_at(const DexFile *dex, uint32_t method_idx,
                       const char **class_desc, const char **name, const char **proto)
{
    uint16_t class_idx;
    uint16_t proto_idx;
    uint32_t name_idx;
    const char *class_name;
    const char *method_name;
    const char *method_proto;

    if (dex == NULL || method_idx >= dex->method_ids_size) {
        return -1;
    }
    if (read_u16(dex, dex->method_ids_off + method_idx * 8, &class_idx) != 0 ||
        read_u16(dex, dex->method_ids_off + method_idx * 8 + 2, &proto_idx) != 0 ||
        read_u32(dex, dex->method_ids_off + method_idx * 8 + 4, &name_idx) != 0) {
        return -1;
    }
    class_name = type_descriptor(dex, class_idx);
    method_name = dex_string(dex, name_idx);
    method_proto = proto_idx < dex->proto_ids_size ? dex->protos[proto_idx] : NULL;
    if (class_name == NULL || method_name == NULL || method_proto == NULL) {
        return -1;
    }
    if (class_desc != NULL) {
        *class_desc = class_name;
    }
    if (name != NULL) {
        *name = method_name;
    }
    if (proto != NULL) {
        *proto = method_proto;
    }
    return 0;
}

int dex_file_find_method(const DexFile *dex, const char *class_desc,
                         const char *name, const char *proto, uint32_t *out_method_idx)
{
    uint32_t i;

    if (dex == NULL || class_desc == NULL || name == NULL || proto == NULL) {
        return -1;
    }
    for (i = 0; i < dex->method_ids_size; i++) {
        const char *method_class;
        const char *method_name;
        const char *method_proto;
        if (dex->code_off[i] == 0) {
            continue;
        }
        if (dex_file_method_at(dex, i, &method_class, &method_name, &method_proto) != 0) {
            return -1;
        }
        if (strcmp(method_class, class_desc) == 0 && strcmp(method_name, name) == 0 &&
            strcmp(method_proto, proto) == 0) {
            if (out_method_idx != NULL) {
                *out_method_idx = i;
            }
            return 0;
        }
    }
    return -1;
}

const char *dex_file_superclass(const DexFile *dex, const char *class_desc)
{
    uint32_t i;

    if (dex == NULL || class_desc == NULL) {
        return NULL;
    }
    for (i = 0; i < dex->class_defs_size; i++) {
        uint32_t type_idx;
        uint32_t super_idx;
        const char *desc;
        if (read_u32(dex, dex->class_defs_off + i * 32, &type_idx) != 0 ||
            read_u32(dex, dex->class_defs_off + i * 32 + 8, &super_idx) != 0) {
            return NULL;
        }
        desc = type_descriptor(dex, type_idx);
        if (desc != NULL && strcmp(desc, class_desc) == 0) {
            if (super_idx == 0xffffffff) {
                return NULL;
            }
            return type_descriptor(dex, super_idx);
        }
    }
    return NULL;
}

int dex_file_field_int(const DexFile *dex, uint32_t field_idx, int32_t *out_value)
{
    if (dex == NULL || field_idx >= dex->field_ids_size || dex->field_has_int == NULL ||
        !dex->field_has_int[field_idx]) {
        return -1;
    }
    if (out_value != NULL) {
        *out_value = dex->field_int[field_idx];
    }
    return 0;
}
