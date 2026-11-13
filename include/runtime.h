#ifndef MINI_ART_RUNTIME_H
#define MINI_ART_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "dex.h"

typedef struct Runtime Runtime;
typedef struct ClassLinker ClassLinker;
typedef struct Heap Heap;
typedef struct ArtThread ArtThread;
typedef struct ArtMethod ArtMethod;

typedef union JValue {
    int32_t i;
    int64_t j;
    uint32_t u;
    float f;
    double d;
    void *l;
} JValue;

typedef int (*ArtNativeFn)(Runtime *rt, const JValue *args, uint32_t argc, JValue *result);

Runtime *runtime_create(const char *boot_dex_path);
void runtime_destroy(Runtime *rt);

int runtime_load_dex(Runtime *rt, const uint8_t *bytes, size_t size);
int runtime_register_native(Runtime *rt, const char *class_desc, const char *name,
                            const char *proto, ArtNativeFn fn);
void runtime_set_resource_string(Runtime *rt,
                                 const char *(*lookup)(void *cookie, uint32_t res_id),
                                 void *cookie);
void *runtime_alloc(Runtime *rt, size_t size);
const char *runtime_resource_string(const Runtime *rt, uint32_t res_id);

ArtMethod *runtime_find_method(Runtime *rt, const char *class_desc,
                               const char *name, const char *proto);
ArtMethod *runtime_resolve_method(Runtime *rt, DexFile *dex, uint32_t method_idx);
int runtime_invoke(Runtime *rt, ArtMethod *method, JValue *args, JValue *result);

int art_method_is_native(const ArtMethod *method);
DexFile *art_method_dex(const ArtMethod *method);
uint32_t art_method_id(const ArtMethod *method);
ArtNativeFn art_method_native(const ArtMethod *method);

Heap *heap_create(void);
void heap_destroy(Heap *heap);
void *heap_alloc(Heap *heap, size_t size);

ArtThread *thread_attach(void);
void thread_detach(ArtThread *thread);

ClassLinker *class_linker_create(Runtime *rt);
void class_linker_destroy(ClassLinker *linker);

int interpreter_invoke(Runtime *rt, ArtMethod *method, JValue *args, JValue *result);
int jni_call(ArtThread *thread, ArtMethod *method, JValue *args, JValue *result);

int well_known_classes_init(Runtime *rt);
size_t well_known_class_count(void);
const char *well_known_class_descriptor(size_t index);

size_t mirror_object_header_size(void);
size_t mirror_class_size(void);
size_t mirror_string_size(void);
size_t mirror_array_size(void);

int log_jni_register(Runtime *rt);
int system_jni_register(Runtime *rt);

#endif
