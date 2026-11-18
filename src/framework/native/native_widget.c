#include "native_internal.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define TOAST_MAGIC 0x54534f54u

struct Toast {
    uint32_t magic;
    int32_t duration;
    char message[256];
};

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

static int native_get_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *text;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    text = native_new_string(rt, java_object_text(args[0].l));
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

int native_register_widget(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Landroid/widget/Toast;", "makeText", "(Landroid/content/Context;II)Landroid/widget/Toast;",
         native_toast_make_text},
        {"Landroid/widget/Toast;", "show", "()V", native_toast_show},
        {"Landroid/widget/TextView;", "getText", "()Ljava/lang/CharSequence;", native_get_text},
        {"Landroid/widget/TextView;", "setText", "(Ljava/lang/CharSequence;)V", native_set_text},
        {"Landroid/widget/Button;", "getText", "()Ljava/lang/CharSequence;", native_get_text},
        {"Landroid/widget/Button;", "setOnClickListener", "(Landroid/view/View$OnClickListener;)V",
         native_set_click_listener},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
