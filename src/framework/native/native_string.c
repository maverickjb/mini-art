#include "native_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

const char *native_arg_text(const JValue *args, uint32_t index)
{
    if (args == NULL || !java_object_check(args[index].l)) {
        return "";
    }
    return java_object_text(args[index].l);
}

JavaObject *native_new_string(Runtime *rt, const char *text)
{
    JavaObject *object = java_object_new(rt, "Ljava/lang/String;");
    if (object != NULL) {
        java_object_set_text(object, text != NULL ? text : "");
    }
    return object;
}

static int utf8_decode(const unsigned char *text, uint32_t *code)
{
    if (text[0] < 0x80) {
        *code = text[0];
        return text[0] == 0 ? 0 : 1;
    }
    if ((text[0] & 0xe0) == 0xc0 && (text[1] & 0xc0) == 0x80) {
        *code = ((uint32_t)(text[0] & 0x1f) << 6) | (text[1] & 0x3f);
        return 2;
    }
    if ((text[0] & 0xf0) == 0xe0 && (text[1] & 0xc0) == 0x80 && (text[2] & 0xc0) == 0x80) {
        *code = ((uint32_t)(text[0] & 0x0f) << 12) | ((uint32_t)(text[1] & 0x3f) << 6) |
                (text[2] & 0x3f);
        return 3;
    }
    *code = text[0];
    return 1;
}

static int utf8_encode(uint32_t code, char *out)
{
    if (code < 0x80) {
        out[0] = (char)code;
        return 1;
    }
    if (code < 0x800) {
        out[0] = (char)(0xc0 | (code >> 6));
        out[1] = (char)(0x80 | (code & 0x3f));
        return 2;
    }
    out[0] = (char)(0xe0 | (code >> 12));
    out[1] = (char)(0x80 | ((code >> 6) & 0x3f));
    out[2] = (char)(0x80 | (code & 0x3f));
    return 3;
}

static int utf16_length(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int count = 0;

    while (*cursor != 0) {
        uint32_t code;
        cursor += utf8_decode(cursor, &code);
        count += code > 0xffff ? 2 : 1;
    }
    return count;
}

static int utf16_offset(const char *text, int index)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int count = 0;
    int offset = 0;

    if (index < 0) {
        return -1;
    }
    while (cursor[offset] != 0 && count < index) {
        uint32_t code;
        int size = utf8_decode(cursor + offset, &code);
        count += code > 0xffff ? 2 : 1;
        offset += size;
    }
    return count == index ? offset : -1;
}

static int append_text(JavaObject *builder, const char *extra)
{
    const char *current;
    size_t current_len;
    size_t extra_len;
    char *combined;

    if (!java_object_check(builder)) {
        return -1;
    }
    current = java_object_text(builder);
    current_len = strlen(current);
    extra_len = extra != NULL ? strlen(extra) : 0;
    combined = malloc(current_len + extra_len + 1);
    if (combined == NULL) {
        return -1;
    }
    memcpy(combined, current, current_len);
    if (extra_len > 0) {
        memcpy(combined + current_len, extra, extra_len);
    }
    combined[current_len + extra_len] = '\0';
    java_object_set_text(builder, combined);
    free(combined);
    return 0;
}

static int native_string_equals(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *left;
    const char *right;

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    left = native_arg_text(args, 0);
    right = java_object_check(args[1].l) ? java_object_text(args[1].l) : NULL;
    result->i = right != NULL && strcmp(left, right) == 0;
    return 0;
}

static int native_string_contains(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    result->i = strstr(native_arg_text(args, 0), native_arg_text(args, 1)) != NULL;
    return 0;
}

static int native_string_length(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    result->i = utf16_length(native_arg_text(args, 0));
    return 0;
}

static int native_string_char_at(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;
    int index;
    uint32_t code;
    int offset;

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    text = native_arg_text(args, 0);
    index = args[1].i;
    offset = utf16_offset(text, index);
    if (offset < 0 || text[offset] == '\0') {
        return -1;
    }
    utf8_decode((const unsigned char *)text + offset, &code);
    result->i = (int32_t)code;
    return 0;
}

static int native_string_substring(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;
    int start;
    int end;
    int length;
    char *slice;
    JavaObject *object;
    int start_at;
    int end_at;

    if (argc < 3 || result == NULL) {
        return -1;
    }
    text = native_arg_text(args, 0);
    start = args[1].i;
    end = args[2].i;
    start_at = utf16_offset(text, start);
    end_at = utf16_offset(text, end);
    if (start_at < 0 || end_at < start_at) {
        return -1;
    }
    length = end_at - start_at;
    slice = malloc((size_t)length + 1);
    if (slice == NULL) {
        return -1;
    }
    memcpy(slice, text + start_at, (size_t)length);
    slice[length] = '\0';
    object = native_new_string(rt, slice);
    free(slice);
    if (object == NULL) {
        return -1;
    }
    result->l = object;
    return 0;
}

static int native_string_replace(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;
    const unsigned char *cursor;
    uint32_t old_char;
    uint32_t new_char;
    char *copy;
    size_t used;
    JavaObject *object;

    if (argc < 3 || result == NULL) {
        return -1;
    }
    text = native_arg_text(args, 0);
    old_char = (uint32_t)args[1].i & 0xffffu;
    new_char = (uint32_t)args[2].i & 0xffffu;
    copy = malloc(strlen(text) * 3 + 1);
    if (copy == NULL) {
        return -1;
    }
    cursor = (const unsigned char *)text;
    used = 0;
    while (*cursor != 0) {
        uint32_t code;
        cursor += utf8_decode(cursor, &code);
        if (code == old_char) {
            code = new_char;
        }
        used += (size_t)utf8_encode(code, copy + used);
    }
    copy[used] = '\0';
    object = native_new_string(rt, copy);
    free(copy);
    if (object == NULL) {
        return -1;
    }
    result->l = object;
    return 0;
}

static int native_string_value_of_int(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    char buf[32];
    JavaObject *object;

    if (argc < 1 || result == NULL) {
        return -1;
    }
    snprintf(buf, sizeof(buf), "%d", args[0].i);
    object = native_new_string(rt, buf);
    if (object == NULL) {
        return -1;
    }
    result->l = object;
    return 0;
}

static int native_string_value_of_double(Runtime *rt, const JValue *args, uint32_t argc,
                                         JValue *result)
{
    char buf[64];
    JavaObject *object;

    if (argc < 1 || result == NULL) {
        return -1;
    }
    snprintf(buf, sizeof(buf), "%.12g", args[0].d);
    object = native_new_string(rt, buf);
    if (object == NULL) {
        return -1;
    }
    result->l = object;
    return 0;
}

static int native_string_to_string(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *object;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    if (strcmp(java_object_class(args[0].l), "Ljava/lang/String;") == 0) {
        result->l = (void *)args[0].l;
        return 0;
    }
    object = native_new_string(rt, java_object_text(args[0].l));
    if (object == NULL) {
        return -1;
    }
    result->l = object;
    return 0;
}

static int native_builder_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    java_object_set_text(args[0].l, "");
    return 0;
}

static int native_builder_init_string(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l) || !java_object_check(args[1].l)) {
        return -1;
    }
    java_object_set_text(args[0].l, java_object_text(args[1].l));
    fprintf(stderr, "StringBuilder.<init> %s\n", java_object_text(args[0].l));
    return 0;
}

static int native_builder_append_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *extra;

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    extra = java_object_check(args[1].l) ? java_object_text(args[1].l) : "null";
    if (append_text(args[0].l, extra) != 0) {
        return -1;
    }
    result->l = (void *)args[0].l;
    return 0;
}

static int native_builder_append_char(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    char extra[4];
    int size;

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    size = utf8_encode((uint32_t)args[1].i & 0xffffu, extra);
    extra[size] = '\0';
    if (append_text(args[0].l, extra) != 0) {
        return -1;
    }
    result->l = (void *)args[0].l;
    return 0;
}

static int native_builder_append_int(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    char buf[32];

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    snprintf(buf, sizeof(buf), "%d", args[1].i);
    if (append_text(args[0].l, buf) != 0) {
        return -1;
    }
    result->l = (void *)args[0].l;
    return 0;
}

static int native_builder_append_double(Runtime *rt, const JValue *args, uint32_t argc,
                                        JValue *result)
{
    char buf[64];

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    snprintf(buf, sizeof(buf), "%.12g", args[1].d);
    if (append_text(args[0].l, buf) != 0) {
        return -1;
    }
    result->l = (void *)args[0].l;
    return 0;
}

static int native_parse_int(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    char *end = NULL;
    long value;

    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    value = strtol(native_arg_text(args, 0), &end, 10);
    if (end == native_arg_text(args, 0)) {
        return -1;
    }
    result->i = (int32_t)value;
    return 0;
}

static int native_parse_double(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    char *end = NULL;

    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    result->d = strtod(native_arg_text(args, 0), &end);
    if (end == native_arg_text(args, 0)) {
        return -1;
    }
    return 0;
}

static int native_parse_boolean(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    result->i = strcasecmp(native_arg_text(args, 0), "true") == 0;
    return 0;
}

int native_register_string(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Ljava/lang/String;", "equals", "(Ljava/lang/Object;)Z", native_string_equals},
        {"Ljava/lang/String;", "contains", "(Ljava/lang/CharSequence;)Z", native_string_contains},
        {"Ljava/lang/String;", "length", "()I", native_string_length},
        {"Ljava/lang/String;", "charAt", "(I)C", native_string_char_at},
        {"Ljava/lang/String;", "substring", "(II)Ljava/lang/String;", native_string_substring},
        {"Ljava/lang/String;", "replace", "(CC)Ljava/lang/String;", native_string_replace},
        {"Ljava/lang/String;", "valueOf", "(I)Ljava/lang/String;", native_string_value_of_int},
        {"Ljava/lang/String;", "valueOf", "(D)Ljava/lang/String;", native_string_value_of_double},
        {"Ljava/lang/String;", "toString", "()Ljava/lang/String;", native_string_to_string},
        {"Ljava/lang/CharSequence;", "toString", "()Ljava/lang/String;", native_string_to_string},
        {"Ljava/lang/StringBuilder;", "<init>", "()V", native_builder_init},
        {"Ljava/lang/StringBuilder;", "<init>", "(Ljava/lang/String;)V", native_builder_init_string},
        {"Ljava/lang/StringBuilder;", "append", "(Ljava/lang/String;)Ljava/lang/StringBuilder;",
         native_builder_append_text},
        {"Ljava/lang/StringBuilder;", "append", "(Ljava/lang/Object;)Ljava/lang/StringBuilder;",
         native_builder_append_text},
        {"Ljava/lang/StringBuilder;", "append", "(C)Ljava/lang/StringBuilder;",
         native_builder_append_char},
        {"Ljava/lang/StringBuilder;", "append", "(I)Ljava/lang/StringBuilder;",
         native_builder_append_int},
        {"Ljava/lang/StringBuilder;", "append", "(D)Ljava/lang/StringBuilder;",
         native_builder_append_double},
        {"Ljava/lang/StringBuilder;", "toString", "()Ljava/lang/String;", native_string_to_string},
        {"Ljava/lang/Integer;", "parseInt", "(Ljava/lang/String;)I", native_parse_int},
        {"Ljava/lang/Double;", "parseDouble", "(Ljava/lang/String;)D", native_parse_double},
        {"Ljava/lang/Boolean;", "parseBoolean", "(Ljava/lang/String;)Z", native_parse_boolean},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
