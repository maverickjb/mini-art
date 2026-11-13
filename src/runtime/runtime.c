#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ArtMethod {
    DexFile *dex;
    uint32_t method_idx;
    int is_native;
    ArtNativeFn fn;
    struct ArtMethod *next;
};

struct NativeEntry {
    char *class_desc;
    char *name;
    char *proto;
    ArtNativeFn fn;
    struct NativeEntry *next;
};

struct ResolveCache {
    DexFile *dex;
    uint32_t method_idx;
    ArtMethod *method;
    struct ResolveCache *next;
};

struct Runtime {
    char *boot_dex_path;
    ClassLinker *linker;
    Heap *heap;
    ArtThread *thread;
    DexFile **dexes;
    size_t dex_count;
    struct ArtMethod *methods;
    struct NativeEntry *natives;
    struct ResolveCache *resolved;
    const char *(*res_string)(void *cookie, uint32_t res_id);
    void *res_cookie;
};

static const char *framework_super(const char *desc)
{
    if (strcmp(desc, "Landroid/app/Activity;") == 0) {
        return "Landroid/content/Context;";
    }
    if (strcmp(desc, "Landroid/content/Context;") == 0 ||
        strcmp(desc, "Landroid/widget/Toast;") == 0 ||
        strcmp(desc, "Landroid/os/Bundle;") == 0) {
        return "Ljava/lang/Object;";
    }
    return NULL;
}

static const char *super_of(const Runtime *rt, const char *desc)
{
    size_t i;

    for (i = 0; i < rt->dex_count; i++) {
        const char *super = dex_file_superclass(rt->dexes[i], desc);
        if (super != NULL) {
            return super;
        }
    }
    return framework_super(desc);
}

static ArtMethod *new_method(Runtime *rt)
{
    ArtMethod *method = calloc(1, sizeof(*method));
    if (method == NULL) {
        return NULL;
    }
    method->next = rt->methods;
    rt->methods = method;
    return method;
}

static ArtNativeFn find_native(const Runtime *rt, const char *class_desc, const char *name,
                               const char *proto)
{
    struct NativeEntry *entry;

    for (entry = rt->natives; entry != NULL; entry = entry->next) {
        if (strcmp(entry->class_desc, class_desc) == 0 && strcmp(entry->name, name) == 0 &&
            strcmp(entry->proto, proto) == 0) {
            return entry->fn;
        }
    }
    return NULL;
}

static ArtMethod *find_code_method(Runtime *rt, const char *class_desc, const char *name,
                                   const char *proto)
{
    size_t i;

    for (i = 0; i < rt->dex_count; i++) {
        uint32_t method_idx;
        ArtMethod *method;
        if (dex_file_find_method(rt->dexes[i], class_desc, name, proto, &method_idx) != 0) {
            continue;
        }
        method = new_method(rt);
        if (method == NULL) {
            return NULL;
        }
        method->dex = rt->dexes[i];
        method->method_idx = method_idx;
        return method;
    }
    return NULL;
}

Runtime *runtime_create(const char *boot_dex_path)
{
    Runtime *rt = calloc(1, sizeof(*rt));
    if (rt == NULL) {
        return NULL;
    }

    if (boot_dex_path != NULL) {
        rt->boot_dex_path = strdup(boot_dex_path);
        if (rt->boot_dex_path == NULL) {
            free(rt);
            return NULL;
        }
    }

    rt->heap = heap_create();
    rt->thread = thread_attach();
    rt->linker = class_linker_create(rt);
    if (rt->heap == NULL || rt->thread == NULL || rt->linker == NULL ||
        well_known_classes_init(rt) != 0 || log_jni_register(rt) != 0 ||
        system_jni_register(rt) != 0) {
        runtime_destroy(rt);
        return NULL;
    }

    return rt;
}

void runtime_destroy(Runtime *rt)
{
    size_t i;

    if (rt == NULL) {
        return;
    }
    while (rt->methods != NULL) {
        ArtMethod *next = rt->methods->next;
        free(rt->methods);
        rt->methods = next;
    }
    while (rt->natives != NULL) {
        struct NativeEntry *next = rt->natives->next;
        free(rt->natives->class_desc);
        free(rt->natives->name);
        free(rt->natives->proto);
        free(rt->natives);
        rt->natives = next;
    }
    while (rt->resolved != NULL) {
        struct ResolveCache *next = rt->resolved->next;
        free(rt->resolved);
        rt->resolved = next;
    }
    for (i = 0; i < rt->dex_count; i++) {
        dex_file_close(rt->dexes[i]);
    }
    free(rt->dexes);
    class_linker_destroy(rt->linker);
    thread_detach(rt->thread);
    heap_destroy(rt->heap);
    free(rt->boot_dex_path);
    free(rt);
}

int runtime_load_dex(Runtime *rt, const uint8_t *bytes, size_t size)
{
    DexFile *dex;
    DexFile **grown;

    if (rt == NULL) {
        return -1;
    }
    dex = dex_file_open(bytes, size);
    if (dex == NULL) {
        return -1;
    }
    grown = realloc(rt->dexes, (rt->dex_count + 1) * sizeof(*grown));
    if (grown == NULL) {
        dex_file_close(dex);
        return -1;
    }
    rt->dexes = grown;
    rt->dexes[rt->dex_count++] = dex;
    return 0;
}

int runtime_register_native(Runtime *rt, const char *class_desc, const char *name,
                            const char *proto, ArtNativeFn fn)
{
    struct NativeEntry *entry;

    if (rt == NULL || class_desc == NULL || name == NULL || proto == NULL || fn == NULL) {
        return -1;
    }
    entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return -1;
    }
    entry->class_desc = strdup(class_desc);
    entry->name = strdup(name);
    entry->proto = strdup(proto);
    entry->fn = fn;
    if (entry->class_desc == NULL || entry->name == NULL || entry->proto == NULL) {
        free(entry->class_desc);
        free(entry->name);
        free(entry->proto);
        free(entry);
        return -1;
    }
    entry->next = rt->natives;
    rt->natives = entry;
    return 0;
}

void runtime_set_resource_string(Runtime *rt, const char *(*lookup)(void *cookie, uint32_t res_id),
                                 void *cookie)
{
    if (rt == NULL) {
        return;
    }
    rt->res_string = lookup;
    rt->res_cookie = cookie;
}

void *runtime_alloc(Runtime *rt, size_t size)
{
    if (rt == NULL) {
        return NULL;
    }
    return heap_alloc(rt->heap, size);
}

const char *runtime_resource_string(const Runtime *rt, uint32_t res_id)
{
    if (rt == NULL || rt->res_string == NULL) {
        return NULL;
    }
    return rt->res_string(rt->res_cookie, res_id);
}

ArtMethod *runtime_find_method(Runtime *rt, const char *class_desc, const char *name,
                               const char *proto)
{
    if (rt == NULL) {
        return NULL;
    }
    return find_code_method(rt, class_desc, name, proto);
}

ArtMethod *runtime_resolve_method(Runtime *rt, DexFile *dex, uint32_t method_idx)
{
    const char *class_desc;
    const char *name;
    const char *proto;
    const char *cursor;
    int depth;
    struct ResolveCache *cached;
    ArtMethod *method;

    if (rt == NULL || dex == NULL) {
        return NULL;
    }
    for (cached = rt->resolved; cached != NULL; cached = cached->next) {
        if (cached->dex == dex && cached->method_idx == method_idx) {
            return cached->method;
        }
    }
    if (dex_file_method_at(dex, method_idx, &class_desc, &name, &proto) != 0) {
        return NULL;
    }

    method = NULL;
    cursor = class_desc;
    for (depth = 0; cursor != NULL && depth < 16; depth++) {
        ArtNativeFn fn;
        method = find_code_method(rt, cursor, name, proto);
        if (method != NULL) {
            break;
        }
        fn = find_native(rt, cursor, name, proto);
        if (fn != NULL) {
            method = new_method(rt);
            if (method == NULL) {
                return NULL;
            }
            method->is_native = 1;
            method->fn = fn;
            break;
        }
        cursor = super_of(rt, cursor);
        method = NULL;
    }
    if (method == NULL) {
        fprintf(stderr, "unresolved %s->%s%s\n", class_desc, name, proto);
        return NULL;
    }

    cached = malloc(sizeof(*cached));
    if (cached != NULL) {
        cached->dex = dex;
        cached->method_idx = method_idx;
        cached->method = method;
        cached->next = rt->resolved;
        rt->resolved = cached;
    }
    return method;
}

int runtime_invoke(Runtime *rt, ArtMethod *method, JValue *args, JValue *result)
{
    if (rt == NULL || method == NULL) {
        return -1;
    }
    return interpreter_invoke(rt, method, args, result);
}

int art_method_is_native(const ArtMethod *method)
{
    return method != NULL && method->is_native;
}

DexFile *art_method_dex(const ArtMethod *method)
{
    if (method == NULL) {
        return NULL;
    }
    return method->dex;
}

uint32_t art_method_id(const ArtMethod *method)
{
    if (method == NULL) {
        return 0;
    }
    return method->method_idx;
}

ArtNativeFn art_method_native(const ArtMethod *method)
{
    if (method == NULL) {
        return NULL;
    }
    return method->fn;
}
