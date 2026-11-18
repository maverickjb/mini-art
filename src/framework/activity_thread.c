#include "framework.h"
#include "java_object.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    if (native_register_framework(rt) != 0 ||
        class_descriptor(launcher, descriptor, sizeof(descriptor)) != 0) {
        goto out;
    }
    fprintf(stderr, "call %s->onCreate(Landroid/os/Bundle;)V\n", descriptor);
    on_create = runtime_find_method(rt, descriptor, "onCreate", "(Landroid/os/Bundle;)V");
    if (on_create == NULL) {
        fprintf(stderr, "failed to find %s.onCreate\n", launcher);
        goto out;
    }
    native_activity_prepare(apk, rt);
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
    if (native_activity_content_open()) {
        ui_run();
    }

out:
    native_activity_clear();
    runtime_set_resource_string(rt, NULL, NULL);
    resource_table_close(table);
    free(arsc);
    return rc;
}
