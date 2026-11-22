#include "native_internal.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Apk *current_apk;
static Runtime *current_rt;
static JavaObject *content_views[256];
static int content_view_count;
static int content_open;

int native_return_void(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)args;
    (void)argc;
    (void)result;
    return 0;
}

static int native_request_window_feature(Runtime *rt, const JValue *args, uint32_t argc,
                                          JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    fprintf(stderr, "requestWindowFeature %d\n", args[1].i);
    result->i = 1;
    return 0;
}

static int native_get_window(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *activity;
    JavaObject *window;
    JValue stored;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    activity = args[0].l;
    memset(&stored, 0, sizeof(stored));
    java_object_get_field(activity, "window", &stored);
    if (!java_object_check(stored.l)) {
        window = java_object_new(rt, "Landroid/view/Window;");
        if (window == NULL) {
            return -1;
        }
        stored.l = window;
        if (java_object_set_field(activity, "window", stored) != 0) {
            return -1;
        }
    }
    fprintf(stderr, "getWindow\n");
    result->l = stored.l;
    return 0;
}

static int native_window_add_flags(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2) {
        return -1;
    }
    fprintf(stderr, "Window.addFlags %#x\n", (unsigned)args[1].i);
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

void native_activity_prepare(const Apk *apk, Runtime *rt)
{
    current_apk = apk;
    current_rt = rt;
    content_view_count = 0;
    content_open = 0;
}

int native_activity_content_open(void)
{
    return content_open;
}

void native_activity_clear(void)
{
    current_apk = NULL;
    current_rt = NULL;
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

int native_register_activity(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Landroid/app/Activity;", "<init>", "()V", native_return_void},
        {"Landroid/app/Activity;", "onCreate", "(Landroid/os/Bundle;)V", native_return_void},
        {"Landroid/app/Activity;", "requestWindowFeature", "(I)Z", native_request_window_feature},
        {"Landroid/app/Activity;", "getWindow", "()Landroid/view/Window;", native_get_window},
        {"Landroid/view/Window;", "addFlags", "(I)V", native_window_add_flags},
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
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
