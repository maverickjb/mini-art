#include "class_linker.h"

#include "art_method.h"

#include <stdlib.h>
#include <string.h>

#define CLASS_BUCKETS 1024

struct ArtClass {
    const char *descriptor;
    const char *superclass;
    ArtClass *super;
    ArtMethod *methods;
    uint32_t methods_count;
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

static const char *framework_superclass(const char *descriptor)
{
    if (strcmp(descriptor, "Landroid/app/Activity;") == 0) {
        return "Landroid/content/Context;";
    }
    if (strcmp(descriptor, "Landroid/content/Context;") == 0 ||
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

static ArtClass *create_class(ClassLinker *linker, const char *descriptor, const char *superclass,
                              DexFile *dex, const uint32_t *method_ids, uint32_t method_count)
{
    ArtClass *klass;
    uint32_t i;

    klass = calloc(1, sizeof(*klass));
    if (klass == NULL) {
        return NULL;
    }
    klass->descriptor = descriptor;
    klass->superclass = superclass;
    if (method_count > 0) {
        klass->methods = calloc(method_count, sizeof(*klass->methods));
        if (klass->methods == NULL) {
            free(klass);
            return NULL;
        }
    }
    for (i = 0; i < method_count; i++) {
        const char *name = NULL;
        const char *proto = NULL;
        if (dex_file_method_at(dex, method_ids[i], NULL, &name, &proto) != 0) {
            free(klass->methods);
            free(klass);
            return NULL;
        }
        klass->methods[i].dex = dex;
        klass->methods[i].method_idx = method_ids[i];
        klass->methods[i].name = name;
        klass->methods[i].proto = proto;
    }
    klass->methods_count = method_count;
    if (insert_class(linker, klass) != 0) {
        free(klass->methods);
        free(klass);
        return NULL;
    }
    return klass;
}

static int link_supers(ClassLinker *linker)
{
    int guard;

    for (guard = 0; guard < 64; guard++) {
        ArtClass *klass;
        int progress = 0;

        for (klass = linker->all; klass != NULL; klass = klass->next) {
            ArtClass *parent;

            if (klass->super != NULL || klass->superclass == NULL) {
                continue;
            }
            parent = class_linker_find(linker, klass->superclass);
            if (parent == NULL) {
                parent = create_class(linker, klass->superclass,
                                      framework_superclass(klass->superclass), NULL, NULL, 0);
                if (parent == NULL) {
                    return -1;
                }
                progress = 1;
            }
            if (parent != klass) {
                klass->super = parent;
                progress = 1;
            }
        }
        if (!progress) {
            return 0;
        }
    }
    return 0;
}

static int on_dex_class(void *user, const char *descriptor, const char *superclass,
                        const uint32_t *method_ids, uint32_t method_count)
{
    struct LoadState *state = user;

    if (class_linker_find(state->linker, descriptor) != NULL) {
        return 0;
    }
    if (create_class(state->linker, descriptor, superclass, state->dex, method_ids, method_count) ==
        NULL) {
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
