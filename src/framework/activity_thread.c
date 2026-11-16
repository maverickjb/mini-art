#include "framework.h"
#include "java_object.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define TOAST_MAGIC 0x54534f54u

struct Toast {
    uint32_t magic;
    int32_t duration;
    char message[256];
};

static int native_return_void(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)args;
    (void)argc;
    (void)result;
    return 0;
}

static int native_get_application_context(Runtime *rt, const JValue *args, uint32_t argc,
                                          JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    *result = args[0];
    return 0;
}

static int native_toast_make_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;
    struct Toast *toast;

    if (argc < 3 || result == NULL) {
        return -1;
    }
    text = runtime_resource_string(rt, (uint32_t)args[1].i);
    fprintf(stderr, "Toast.makeText id=%#x -> %s\n", (unsigned)args[1].i,
            text != NULL ? text : "(missing)");
    if (text == NULL) {
        fprintf(stderr, "resource string %#x not found\n", (unsigned)args[1].i);
        return -1;
    }
    toast = runtime_alloc(rt, sizeof(*toast));
    if (toast == NULL) {
        return -1;
    }
    toast->magic = TOAST_MAGIC;
    toast->duration = args[2].i;
    snprintf(toast->message, sizeof(toast->message), "%s", text);
    memset(result, 0, sizeof(*result));
    result->l = toast;
    return 0;
}

static int native_toast_show(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    struct Toast *toast;

    (void)rt;
    (void)result;
    if (argc < 1 || args[0].l == NULL) {
        return -1;
    }
    toast = args[0].l;
    if (toast->magic != TOAST_MAGIC) {
        return -1;
    }
    fprintf(stderr, "Toast.show: %s\n", toast->message);
    if (ui_show_toast(toast->message, toast->duration) != 0) {
        fprintf(stderr, "toast window failed\n");
        return -1;
    }
    return 0;
}

static const Apk *current_apk;
static Runtime *current_rt;
static JavaObject *content_views[256];
static int content_view_count;
static int content_open;

static const char *arg_text(const JValue *args, uint32_t index)
{
    if (args == NULL || !java_object_check(args[index].l)) {
        return "";
    }
    return java_object_text(args[index].l);
}

static JavaObject *new_string(Runtime *rt, const char *text)
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
    left = arg_text(args, 0);
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
    result->i = strstr(arg_text(args, 0), arg_text(args, 1)) != NULL;
    return 0;
}

static int native_string_length(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    result->i = utf16_length(arg_text(args, 0));
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
    text = arg_text(args, 0);
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
    text = arg_text(args, 0);
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
    object = new_string(rt, slice);
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
    text = arg_text(args, 0);
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
    object = new_string(rt, copy);
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
    object = new_string(rt, buf);
    if (object == NULL) {
        return -1;
    }
    result->l = object;
    return 0;
}

static int native_string_value_of_double(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    char buf[64];
    JavaObject *object;

    if (argc < 1 || result == NULL) {
        return -1;
    }
    snprintf(buf, sizeof(buf), "%.12g", args[0].d);
    object = new_string(rt, buf);
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
    object = new_string(rt, java_object_text(args[0].l));
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

static int native_builder_append_double(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
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
    value = strtol(arg_text(args, 0), &end, 10);
    if (end == arg_text(args, 0)) {
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
    result->d = strtod(arg_text(args, 0), &end);
    if (end == arg_text(args, 0)) {
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
    result->i = strcasecmp(arg_text(args, 0), "true") == 0;
    return 0;
}

static int native_math1(Runtime *rt, const JValue *args, uint32_t argc, JValue *result, double (*fn)(double))
{
    (void)rt;
    if (argc < 1 || result == NULL || fn == NULL) {
        return -1;
    }
    result->d = fn(args[0].d);
    return 0;
}

static int native_to_radians(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    result->d = args[0].d * (acos(-1.0) / 180.0);
    return 0;
}

static int native_math_sin(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, sin);
}

static int native_math_cos(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, cos);
}

static int native_math_tan(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, tan);
}

static int native_math_sqrt(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, sqrt);
}

static int native_math_log(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, log);
}

static int native_math_log10(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, log10);
}

static int native_math_pow(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 3 || result == NULL) {
        return -1;
    }
    result->d = pow(args[0].d, args[2].d);
    return 0;
}

static int native_exception_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    java_object_set_text(args[0].l, arg_text(args, 1));
    return 0;
}

static void remember_view(JavaObject *object)
{
    if (object != NULL && content_view_count < (int)(sizeof(content_views) / sizeof(content_views[0]))) {
        content_views[content_view_count++] = object;
    }
}

static JavaObject *find_view(uint32_t id)
{
    int i;

    for (i = 0; i < content_view_count; i++) {
        if (java_object_has_id(content_views[i], id)) {
            return content_views[i];
        }
    }
    return NULL;
}

static void build_views(void *parent, const XmlView *node)
{
    for (; node != NULL; node = node->next) {
        if (node->kind == XML_VIEW_BUTTON || node->kind == XML_VIEW_TEXT) {
            const char *class_desc = node->kind == XML_VIEW_BUTTON ? "Landroid/widget/Button;"
                                                                   : "Landroid/widget/TextView;";
            JavaObject *object = java_object_new(current_rt, class_desc);
            void *widget;

            if (object == NULL) {
                continue;
            }
            java_object_set_text(object, node->text);
            if (node->id != 0) {
                java_object_set_id(object, node->id);
            }
            if (node->kind == XML_VIEW_BUTTON) {
                widget = ui_content_button(parent, node->text, object);
            } else {
                widget = ui_content_label(parent, node->text);
            }
            java_object_set_widget(object, widget);
            remember_view(object);
        } else {
            void *box = ui_content_box(parent, node->horizontal);
            build_views(box, node->child);
        }
    }
}

static void on_button_click(void *user)
{
    JavaObject *button = user;
    JavaObject *listener;
    ArtMethod *method;
    JValue args[2];
    JValue result;

    if (!java_object_check(button) || current_rt == NULL) {
        return;
    }
    listener = java_object_listener(button);
    if (!java_object_check(listener)) {
        return;
    }
    method = runtime_find_method(current_rt, java_object_class(listener), "onClick",
                                 "(Landroid/view/View;)V");
    if (method == NULL) {
        fprintf(stderr, "no onClick on %s\n", java_object_class(listener));
        return;
    }
    memset(args, 0, sizeof(args));
    memset(&result, 0, sizeof(result));
    args[0].l = listener;
    args[1].l = button;
    if (runtime_invoke(current_rt, method, args, &result) != 0) {
        fprintf(stderr, "onClick failed\n");
    }
}

static int native_set_content_view(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    uint32_t id;
    const char *path;
    char fallback[64];
    const char *entry;
    uint8_t *xml = NULL;
    size_t xml_size = 0;
    XmlView *root = NULL;
    void *screen;
    void *parent;

    (void)result;
    if (argc < 2 || current_apk == NULL) {
        return -1;
    }
    id = (uint32_t)args[1].i;
    path = runtime_resource_string(rt, id);
    fprintf(stderr, "setContentView %#x %s\n", id, path != NULL ? path : "");
    if (path != NULL && strncmp(path, "res/", 4) == 0) {
        entry = path;
    } else {
        snprintf(fallback, sizeof(fallback), "res/layout/activity_main.xml");
        entry = fallback;
    }
    if (apk_read_entry(current_apk, entry, &xml, &xml_size) != 0 ||
        binary_xml_views(xml, xml_size, &root) != 0) {
        fprintf(stderr, "failed to inflate %s\n", entry);
        free(xml);
        return -1;
    }
    free(xml);
    ui_content_on_click(on_button_click);
    screen = ui_content_begin();
    if (screen == NULL) {
        xml_view_free(root);
        return -1;
    }
    parent = screen;
    if (root->horizontal) {
        parent = ui_content_box(screen, 1);
    }
    build_views(parent, root->child);
    fprintf(stderr, "content view: %d widgets\n", content_view_count);
    xml_view_free(root);
    content_open = 1;
    return 0;
}

static int native_find_view_by_id(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *view;

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    view = find_view((uint32_t)args[1].i);
    if (view == NULL) {
        fprintf(stderr, "findViewById %#x missing\n", (unsigned)args[1].i);
        return -1;
    }
    result->l = view;
    return 0;
}

static int native_set_click_listener(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    java_object_set_listener(args[0].l, args[1].l);
    return 0;
}

static int native_get_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *text;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    text = new_string(rt, java_object_text(args[0].l));
    if (text == NULL) {
        return -1;
    }
    result->l = text;
    return 0;
}

static int native_set_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    text = java_object_check(args[1].l) ? java_object_text(args[1].l) : "";
    java_object_set_text(args[0].l, text);
    ui_content_set_text(java_object_widget(args[0].l), text);
    return 0;
}

static int register_framework(Runtime *rt)
{
    static const struct {
        const char *class_desc;
        const char *name;
        const char *proto;
        ArtNativeFn fn;
    } natives[] = {
        {"Ljava/lang/Object;", "<init>", "()V", native_return_void},
        {"Landroid/app/Activity;", "<init>", "()V", native_return_void},
        {"Landroid/app/Activity;", "onCreate", "(Landroid/os/Bundle;)V", native_return_void},
        {"Landroid/app/Activity;", "finish", "()V", native_return_void},
        {"Landroid/app/Activity;", "setContentView", "(I)V", native_set_content_view},
        {"Landroid/app/Activity;", "findViewById", "(I)Landroid/view/View;", native_find_view_by_id},
        {"Landroidx/appcompat/app/AppCompatActivity;", "<init>", "()V", native_return_void},
        {"Landroidx/appcompat/app/AppCompatActivity;", "onCreate", "(Landroid/os/Bundle;)V",
         native_return_void},
        {"Landroidx/appcompat/app/AppCompatActivity;", "setContentView", "(I)V", native_set_content_view},
        {"Landroidx/appcompat/app/AppCompatActivity;", "findViewById", "(I)Landroid/view/View;",
         native_find_view_by_id},
        {"Landroid/content/Context;", "getApplicationContext", "()Landroid/content/Context;",
         native_get_application_context},
        {"Landroid/widget/Toast;", "makeText", "(Landroid/content/Context;II)Landroid/widget/Toast;",
         native_toast_make_text},
        {"Landroid/widget/Toast;", "show", "()V", native_toast_show},
        {"Landroid/widget/TextView;", "getText", "()Ljava/lang/CharSequence;", native_get_text},
        {"Landroid/widget/TextView;", "setText", "(Ljava/lang/CharSequence;)V", native_set_text},
        {"Landroid/widget/Button;", "getText", "()Ljava/lang/CharSequence;", native_get_text},
        {"Landroid/widget/Button;", "setOnClickListener", "(Landroid/view/View$OnClickListener;)V",
         native_set_click_listener},
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
        {"Ljava/lang/Math;", "toRadians", "(D)D", native_to_radians},
        {"Ljava/lang/Math;", "sin", "(D)D", native_math_sin},
        {"Ljava/lang/Math;", "cos", "(D)D", native_math_cos},
        {"Ljava/lang/Math;", "tan", "(D)D", native_math_tan},
        {"Ljava/lang/Math;", "sqrt", "(D)D", native_math_sqrt},
        {"Ljava/lang/Math;", "log", "(D)D", native_math_log},
        {"Ljava/lang/Math;", "log10", "(D)D", native_math_log10},
        {"Ljava/lang/Math;", "pow", "(DD)D", native_math_pow},
        {"Ljava/lang/RuntimeException;", "<init>", "(Ljava/lang/String;)V", native_exception_init},
    };
    size_t i;

    for (i = 0; i < sizeof(natives) / sizeof(natives[0]); i++) {
        if (runtime_register_native(rt, natives[i].class_desc, natives[i].name, natives[i].proto,
                                    natives[i].fn) != 0) {
            return -1;
        }
    }
    return 0;
}

static int class_descriptor(const char *dotted, char *out, size_t cap)
{
    size_t i;
    size_t len = strlen(dotted);

    if (len + 3 > cap) {
        return -1;
    }
    out[0] = 'L';
    for (i = 0; i < len; i++) {
        out[i + 1] = dotted[i] == '.' ? '/' : dotted[i];
    }
    out[len + 1] = ';';
    out[len + 2] = '\0';
    return 0;
}

static const char *lookup_string(void *cookie, uint32_t res_id)
{
    return resource_table_string(cookie, res_id);
}

int activity_thread_launch(Runtime *rt, LoadedApk *loaded)
{
    const char *launcher;
    const Apk *apk;
    uint8_t *arsc = NULL;
    size_t arsc_size = 0;
    ResourceTable *table = NULL;
    size_t dex_count;
    size_t i;
    char descriptor[512];
    ArtMethod *on_create;
    JValue args[2];
    void *activity;
    int rc = -1;

    if (rt == NULL || loaded == NULL) {
        return -1;
    }
    launcher = loaded_apk_launcher(loaded);
    if (launcher == NULL) {
        fprintf(stderr, "no launcher activity in manifest\n");
        return -1;
    }
    apk = loaded_apk_apk(loaded);
    if (apk == NULL) {
        return -1;
    }

    fprintf(stderr, "load resources.arsc\n");
    if (apk_read_entry(apk, "resources.arsc", &arsc, &arsc_size) == 0) {
        table = resource_table_open(arsc, arsc_size);
        if (table == NULL) {
            fprintf(stderr, "failed to read resources.arsc\n");
            goto out;
        }
        runtime_set_resource_string(rt, lookup_string, table);
    }

    dex_count = apk_dex_count(apk);
    if (dex_count == 0) {
        fprintf(stderr, "apk has no classes.dex\n");
        goto out;
    }
    for (i = 0; i < dex_count; i++) {
        uint8_t *bytes = NULL;
        size_t size = 0;
        int loaded_dex;
        char dex_name[32];
        if (i == 0) {
            snprintf(dex_name, sizeof(dex_name), "classes.dex");
        } else {
            snprintf(dex_name, sizeof(dex_name), "classes%zu.dex", i + 1);
        }
        fprintf(stderr, "load %s\n", dex_name);
        if (apk_read_dex(apk, i, &bytes, &size) != 0) {
            fprintf(stderr, "failed to read dex %zu\n", i);
            goto out;
        }
        loaded_dex = runtime_load_dex(rt, bytes, size);
        free(bytes);
        if (loaded_dex != 0) {
            fprintf(stderr, "failed to load dex %zu\n", i);
            goto out;
        }
    }

    if (register_framework(rt) != 0 || class_descriptor(launcher, descriptor, sizeof(descriptor)) != 0) {
        goto out;
    }
    fprintf(stderr, "call %s->onCreate(Landroid/os/Bundle;)V\n", descriptor);
    on_create = runtime_find_method(rt, descriptor, "onCreate", "(Landroid/os/Bundle;)V");
    if (on_create == NULL) {
        fprintf(stderr, "failed to find %s.onCreate\n", launcher);
        goto out;
    }
    current_apk = apk;
    current_rt = rt;
    content_view_count = 0;
    content_open = 0;
    activity = java_object_new(rt, descriptor);
    if (activity == NULL) {
        goto out;
    }
    memset(args, 0, sizeof(args));
    args[0].l = activity;
    if (runtime_invoke(rt, on_create, args, NULL) != 0) {
        fprintf(stderr, "failed to run %s.onCreate\n", launcher);
        goto out;
    }
    rc = 0;
    if (content_open) {
        ui_run();
    }

out:
    current_apk = NULL;
    current_rt = NULL;
    runtime_set_resource_string(rt, NULL, NULL);
    resource_table_close(table);
    free(arsc);
    return rc;
}
