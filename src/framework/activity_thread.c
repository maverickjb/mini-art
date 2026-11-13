#include "framework.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOAST_MAGIC 0x54534f54u

struct Toast {
    uint32_t magic;
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
    if (text == NULL) {
        fprintf(stderr, "resource string %#x not found\n", (unsigned)args[1].i);
        return -1;
    }
    toast = runtime_alloc(rt, sizeof(*toast));
    if (toast == NULL) {
        return -1;
    }
    toast->magic = TOAST_MAGIC;
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
    puts(toast->message);
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
        {"Landroid/content/Context;", "getApplicationContext", "()Landroid/content/Context;",
         native_get_application_context},
        {"Landroid/widget/Toast;", "makeText", "(Landroid/content/Context;II)Landroid/widget/Toast;",
         native_toast_make_text},
        {"Landroid/widget/Toast;", "show", "()V", native_toast_show},
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
    on_create = runtime_find_method(rt, descriptor, "onCreate", "(Landroid/os/Bundle;)V");
    if (on_create == NULL) {
        fprintf(stderr, "failed to find %s.onCreate\n", launcher);
        goto out;
    }
    activity = runtime_alloc(rt, 1);
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

out:
    runtime_set_resource_string(rt, NULL, NULL);
    resource_table_close(table);
    free(arsc);
    return rc;
}
