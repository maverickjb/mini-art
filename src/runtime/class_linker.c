#include "class_linker.h"

#include "art_field.h"
#include "art_method.h"

#include <stdlib.h>
#include <string.h>

#define CLASS_BUCKETS 1024

enum {
    CLASS_KIND_STUB = 0,
    CLASS_KIND_DEX = 1
};

enum {
    CLASS_INIT_NONE = 0,
    CLASS_INIT_RUNNING = 1,
    CLASS_INIT_DONE = 2,
    CLASS_INIT_FAILED = 3
};

/* static_fields[i] describes static_values[i]. init_state is NONE, RUNNING
 * while this initialization is already on the stack, DONE, or FAILED. */
struct ArtClass {
    const char *descriptor;
    const char *superclass;
    ArtClass *super;
    ArtMethod *methods;
    uint32_t methods_count;
    ArtField *static_fields;
    JValue *static_values;
    struct DexEncodedValue *static_initial;
    uint32_t static_count;
    int init_state;
    int kind;
    int super_unknown;
    ArtClass *next;
};

struct ClassNode {
    ArtClass *klass;
    struct ClassNode *next;
};

struct ClassLinker {
    Runtime *runtime;
    ArtClass *all;
    struct ClassNode *buckets[CLASS_BUCKETS];
};

struct LoadState {
    ClassLinker *linker;
    DexFile *dex;
    int failed;
};

ClassLinker *class_linker_create(Runtime *rt)
{
    ClassLinker *linker = calloc(1, sizeof(*linker));
    if (linker == NULL) {
        return NULL;
    }
    linker->runtime = rt;
    return linker;
}

void class_linker_destroy(ClassLinker *linker)
{
    size_t i;

    if (linker == NULL) {
        return;
    }
    for (i = 0; i < CLASS_BUCKETS; i++) {
        while (linker->buckets[i] != NULL) {
            struct ClassNode *next = linker->buckets[i]->next;
            free(linker->buckets[i]);
            linker->buckets[i] = next;
        }
    }
    while (linker->all != NULL) {
        ArtClass *next = linker->all->next;
        free(linker->all->methods);
        free(linker->all->static_fields);
        free(linker->all->static_values);
        free(linker->all->static_initial);
        free(linker->all);
        linker->all = next;
    }
    free(linker);
}

static unsigned hash_descriptor(const char *descriptor)
{
    unsigned hash = 5381;

    while (*descriptor != '\0') {
        hash = hash * 33u + (unsigned char)*descriptor++;
    }
    return hash;
}

ArtClass *class_linker_find(const ClassLinker *linker, const char *descriptor)
{
    struct ClassNode *node;

    if (linker == NULL || descriptor == NULL) {
        return NULL;
    }
    for (node = linker->buckets[hash_descriptor(descriptor) % CLASS_BUCKETS]; node != NULL;
         node = node->next) {
        if (strcmp(node->klass->descriptor, descriptor) == 0) {
            return node->klass;
        }
    }
    return NULL;
}

ArtClass *class_linker_super(const ArtClass *klass)
{
    if (klass == NULL) {
        return NULL;
    }
    return klass->super;
}

const char *class_linker_descriptor(const ArtClass *klass)
{
    if (klass == NULL) {
        return NULL;
    }
    return klass->descriptor;
}

ArtMethod *class_linker_find_declared(const ArtClass *klass, const char *name, const char *proto)
{
    uint32_t i;

    if (klass == NULL || name == NULL || proto == NULL) {
        return NULL;
    }
    for (i = 0; i < klass->methods_count; i++) {
        ArtMethod *method = &klass->methods[i];
        if (method->name != NULL && strcmp(method->name, name) == 0 &&
            strcmp(method->proto, proto) == 0) {
            return method;
        }
    }
    return NULL;
}

static void clear_statics(ArtClass *klass)
{
    free(klass->static_fields);
    free(klass->static_values);
    free(klass->static_initial);
    klass->static_fields = NULL;
    klass->static_values = NULL;
    klass->static_initial = NULL;
    klass->static_count = 0;
}

static int fill_statics(ArtClass *klass, const struct DexStaticField *statics, uint32_t count)
{
    ArtField *fields = NULL;
    JValue *values = NULL;
    struct DexEncodedValue *initial = NULL;
    uint32_t i;

    if (count > 0) {
        fields = calloc(count, sizeof(*fields));
        values = calloc(count, sizeof(*values));
        initial = calloc(count, sizeof(*initial));
        if (fields == NULL || values == NULL || initial == NULL) {
            free(fields);
            free(values);
            free(initial);
            return -1;
        }
        for (i = 0; i < count; i++) {
            fields[i].declaring = klass;
            fields[i].name = statics[i].name;
            fields[i].type = statics[i].type;
            fields[i].slot = i;
            fields[i].kind = statics[i].kind;
            initial[i] = statics[i].value;
            if (statics[i].kind == ART_FIELD_INT && statics[i].value.kind == DEX_ENCODED_INT) {
                values[i].i = statics[i].value.i;
            } else if (statics[i].kind == ART_FIELD_WIDE &&
                       statics[i].value.kind == DEX_ENCODED_WIDE) {
                values[i].j = statics[i].value.j;
            }
        }
    }
    clear_statics(klass);
    klass->static_fields = fields;
    klass->static_values = values;
    klass->static_initial = initial;
    klass->static_count = count;
    return 0;
}

ArtField *class_linker_find_static(ArtClass *klass, const char *name)
{
    uint32_t i;

    if (klass == NULL || name == NULL) {
        return NULL;
    }
    for (i = 0; i < klass->static_count; i++) {
        if (klass->static_fields[i].name != NULL &&
            strcmp(klass->static_fields[i].name, name) == 0) {
            return &klass->static_fields[i];
        }
    }
    return NULL;
}

JValue *class_linker_static_value(ArtField *field)
{
    if (field == NULL || field->declaring == NULL || field->slot >= field->declaring->static_count) {
        return NULL;
    }
    return &field->declaring->static_values[field->slot];
}

int class_linker_init_status(const ArtClass *klass)
{
    if (klass == NULL || klass->init_state == CLASS_INIT_FAILED) {
        return -1;
    }
    if (klass->init_state == CLASS_INIT_NONE) {
        return 0;
    }
    return 1;
}

int class_linker_begin_init(ArtClass *klass)
{
    if (klass == NULL) {
        return -1;
    }
    if (klass->init_state == CLASS_INIT_FAILED) {
        return -1;
    }
    if (klass->init_state == CLASS_INIT_DONE || klass->init_state == CLASS_INIT_RUNNING) {
        return 1;
    }
    klass->init_state = CLASS_INIT_RUNNING;
    return 0;
}

void class_linker_finish_init(ArtClass *klass, int ok)
{
    if (klass == NULL || klass->init_state != CLASS_INIT_RUNNING) {
        return;
    }
    klass->init_state = ok ? CLASS_INIT_DONE : CLASS_INIT_FAILED;
}

const char *class_linker_framework_superclass(const char *descriptor)
{
    if (strcmp(descriptor, "Ljava/lang/Object;") == 0) {
        return NULL;
    }
    if (strcmp(descriptor, "Landroid/app/Activity;") == 0) {
        return "Landroid/content/Context;";
    }
    if (strcmp(descriptor, "Landroid/view/SurfaceView;") == 0) {
        return "Landroid/view/View;";
    }
    if (strcmp(descriptor, "Landroid/content/Context;") == 0 ||
        strcmp(descriptor, "Landroid/view/View;") == 0 ||
        strcmp(descriptor, "Landroid/widget/Toast;") == 0 ||
        strcmp(descriptor, "Landroid/os/Bundle;") == 0) {
        return "Ljava/lang/Object;";
    }
    return NULL;
}

static int insert_class(ClassLinker *linker, ArtClass *klass)
{
    unsigned bucket = hash_descriptor(klass->descriptor) % CLASS_BUCKETS;
    struct ClassNode *node = calloc(1, sizeof(*node));

    if (node == NULL) {
        return -1;
    }
    node->klass = klass;
    node->next = linker->buckets[bucket];
    linker->buckets[bucket] = node;
    klass->next = linker->all;
    linker->all = klass;
    return 0;
}

static int fill_methods(ArtClass *klass, DexFile *dex, const struct DexDeclaredMethod *methods,
                        uint32_t method_count)
{
    ArtMethod *table = NULL;
    uint32_t i;

    if (method_count > 0) {
        table = calloc(method_count, sizeof(*table));
        if (table == NULL) {
            return -1;
        }
        for (i = 0; i < method_count; i++) {
            const char *name = NULL;
            const char *proto = NULL;
            if (dex_file_method_at(dex, methods[i].method_idx, NULL, &name, &proto) != 0) {
                free(table);
                return -1;
            }
            table[i].dex = dex;
            table[i].method_idx = methods[i].method_idx;
            table[i].access_flags = methods[i].access_flags;
            table[i].code_off = methods[i].code_off;
            table[i].name = name;
            table[i].proto = proto;
        }
    }
    free(klass->methods);
    klass->methods = table;
    klass->methods_count = method_count;
    return 0;
}

static ArtClass *create_class(ClassLinker *linker, const char *descriptor, const char *superclass,
                              DexFile *dex, const struct DexStaticField *statics,
                              uint32_t static_count, const struct DexDeclaredMethod *methods,
                              uint32_t method_count, int kind)
{
    ArtClass *klass;

    klass = calloc(1, sizeof(*klass));
    if (klass == NULL) {
        return NULL;
    }
    klass->descriptor = descriptor;
    klass->superclass = superclass;
    klass->kind = kind;
    if (fill_methods(klass, dex, methods, method_count) != 0 ||
        fill_statics(klass, statics, static_count) != 0) {
        clear_statics(klass);
        free(klass->methods);
        free(klass);
        return NULL;
    }
    if (insert_class(linker, klass) != 0) {
        clear_statics(klass);
        free(klass->methods);
        free(klass);
        return NULL;
    }
    return klass;
}

static int define_static_value(ClassLinker *linker, const char *descriptor, const char *name,
                               const char *type, int kind, JValue value)
{
    ArtClass *klass;
    ArtField *fields;
    JValue *values;
    struct DexEncodedValue *initial;
    uint32_t count;
    uint32_t i;

    if (linker == NULL || descriptor == NULL || name == NULL || type == NULL) {
        return -1;
    }
    klass = class_linker_find(linker, descriptor);
    if (klass == NULL) {
        klass = create_class(linker, descriptor, "Ljava/lang/Object;", NULL, NULL, 0, NULL, 0,
                             CLASS_KIND_STUB);
        if (klass == NULL) {
            return -1;
        }
    }
    for (i = 0; i < klass->static_count; i++) {
        if (klass->static_fields[i].name != NULL && strcmp(klass->static_fields[i].name, name) == 0) {
            if (klass->static_fields[i].kind != kind) {
                return -1;
            }
            klass->static_values[i] = value;
            return 0;
        }
    }
    count = klass->static_count + 1;
    fields = realloc(klass->static_fields, count * sizeof(*fields));
    if (fields == NULL) {
        return -1;
    }
    klass->static_fields = fields;
    values = realloc(klass->static_values, count * sizeof(*values));
    if (values == NULL) {
        return -1;
    }
    initial = realloc(klass->static_initial, count * sizeof(*initial));
    if (initial == NULL) {
        klass->static_values = values;
        return -1;
    }
    memset(&fields[count - 1], 0, sizeof(fields[count - 1]));
    memset(&initial[count - 1], 0, sizeof(initial[count - 1]));
    fields[count - 1].declaring = klass;
    fields[count - 1].name = name;
    fields[count - 1].type = type;
    fields[count - 1].slot = count - 1;
    fields[count - 1].kind = kind;
    values[count - 1] = value;
    klass->static_fields = fields;
    klass->static_values = values;
    klass->static_initial = initial;
    klass->static_count = count;
    return 0;
}

int class_linker_define_static_int(ClassLinker *linker, const char *descriptor, const char *name,
                                   int32_t value)
{
    JValue stored;

    memset(&stored, 0, sizeof(stored));
    stored.i = value;
    return define_static_value(linker, descriptor, name, "I", ART_FIELD_INT, stored);
}

int class_linker_define_static_object(ClassLinker *linker, const char *descriptor,
                                      const char *name, const char *type, void *value)
{
    JValue stored;

    memset(&stored, 0, sizeof(stored));
    stored.l = value;
    return define_static_value(linker, descriptor, name, type, ART_FIELD_OBJECT, stored);
}

static int hierarchy_cycle(const ClassLinker *linker)
{
    const ArtClass *klass;

    for (klass = linker->all; klass != NULL; klass = klass->next) {
        const ArtClass *slow = klass;
        const ArtClass *fast = klass->super;

        while (fast != NULL && fast->super != NULL) {
            if (slow == fast) {
                return 1;
            }
            slow = slow->super;
            fast = fast->super->super;
        }
    }
    return 0;
}

static int link_supers(ClassLinker *linker)
{
    int guard;

    for (guard = 0; guard < 64; guard++) {
        ArtClass *klass;
        int progress = 0;

        for (klass = linker->all; klass != NULL; klass = klass->next) {
            ArtClass *parent;
            const char *parent_name;

            if (klass->super != NULL || klass->superclass == NULL) {
                continue;
            }
            parent = class_linker_find(linker, klass->superclass);
            if (parent == NULL) {
                parent_name = class_linker_framework_superclass(klass->superclass);
                parent = create_class(linker, klass->superclass, parent_name, NULL, NULL, 0, NULL,
                                      0, CLASS_KIND_STUB);
                if (parent == NULL) {
                    return -1;
                }
                if (parent_name == NULL && strcmp(parent->descriptor, "Ljava/lang/Object;") != 0) {
                    parent->super_unknown = 1;
                }
                progress = 1;
            }
            if (parent != klass) {
                klass->super = parent;
                progress = 1;
            }
        }
        if (!progress) {
            for (klass = linker->all; klass != NULL; klass = klass->next) {
                if (klass->super_unknown) {
                    continue;
                }
                if (klass->superclass != NULL && klass->super == NULL) {
                    return -1;
                }
            }
            if (hierarchy_cycle(linker)) {
                return -1;
            }
            return 0;
        }
    }
    return -1;
}

static int on_dex_class(void *user, const char *descriptor, const char *superclass,
                        const struct DexStaticField *statics, uint32_t static_count,
                        const struct DexDeclaredMethod *methods, uint32_t method_count)
{
    struct LoadState *state = user;
    ArtClass *existing = class_linker_find(state->linker, descriptor);

    if (existing != NULL && existing->kind == CLASS_KIND_DEX) {
        return 0;
    }
    if (existing != NULL) {
        if (fill_methods(existing, state->dex, methods, method_count) != 0 ||
            fill_statics(existing, statics, static_count) != 0) {
            state->failed = 1;
            return -1;
        }
        existing->kind = CLASS_KIND_DEX;
        existing->descriptor = descriptor;
        existing->superclass = superclass;
        existing->super = NULL;
        existing->super_unknown = 0;
        return 0;
    }
    if (create_class(state->linker, descriptor, superclass, state->dex, statics, static_count,
                     methods, method_count, CLASS_KIND_DEX) == NULL) {
        state->failed = 1;
        return -1;
    }
    return 0;
}

static int class_is_live(const ArtClass *all, const ArtClass *klass)
{
    const ArtClass *cursor;

    for (cursor = all; cursor != NULL; cursor = cursor->next) {
        if (cursor == klass) {
            return 1;
        }
    }
    return 0;
}

static void rollback_to(ClassLinker *linker, ArtClass *previous_head)
{
    ArtClass *doomed = NULL;
    ArtClass *kept;
    size_t i;

    while (linker->all != previous_head) {
        ArtClass *klass = linker->all;
        linker->all = klass->next;
        klass->next = doomed;
        doomed = klass;
    }
    for (kept = linker->all; kept != NULL; kept = kept->next) {
        ArtClass *dead;
        for (dead = doomed; dead != NULL; dead = dead->next) {
            if (kept->super == dead) {
                kept->super = NULL;
            }
        }
    }
    while (doomed != NULL) {
        ArtClass *next = doomed->next;
        free(doomed->methods);
        free(doomed->static_fields);
        free(doomed->static_values);
        free(doomed->static_initial);
        free(doomed);
        doomed = next;
    }
    for (i = 0; i < CLASS_BUCKETS; i++) {
        struct ClassNode **link = &linker->buckets[i];
        while (*link != NULL) {
            if (!class_is_live(linker->all, (*link)->klass)) {
                struct ClassNode *dead = *link;
                *link = dead->next;
                free(dead);
            } else {
                link = &(*link)->next;
            }
        }
    }
}

int class_linker_add_dex(ClassLinker *linker, DexFile *dex)
{
    struct LoadState state;
    ArtClass *previous_head;

    if (linker == NULL || dex == NULL) {
        return -1;
    }
    previous_head = linker->all;
    state.linker = linker;
    state.dex = dex;
    state.failed = 0;
    if (dex_file_visit_classes(dex, on_dex_class, &state) != 0 || state.failed ||
        link_supers(linker) != 0) {
        rollback_to(linker, previous_head);
        return -1;
    }
    return 0;
}
