#ifndef MINI_ART_CLASS_LINKER_H
#define MINI_ART_CLASS_LINKER_H

#include "runtime.h"

/* NULL means the parent is not known yet. java.lang.Object also returns NULL
 * because its missing parent is the root. */
const char *class_linker_framework_superclass(const char *descriptor);
int class_linker_add_dex(ClassLinker *linker, DexFile *dex);
ArtClass *class_linker_find(const ClassLinker *linker, const char *descriptor);
ArtClass *class_linker_super(const ArtClass *klass);
const char *class_linker_descriptor(const ArtClass *klass);
ArtMethod *class_linker_find_declared(const ArtClass *klass, const char *name, const char *proto);
ArtField *class_linker_find_static(ArtClass *klass, const char *name);
int class_linker_define_static_int(ClassLinker *linker, const char *descriptor, const char *name,
                                   int32_t value);
int class_linker_define_static_object(ClassLinker *linker, const char *descriptor,
                                      const char *name, const char *type, void *value);
JValue *class_linker_static_value(ArtField *field);
/* 0: not started. 1: initializing or initialized. -1: initialization failed. */
int class_linker_init_status(const ArtClass *klass);
/* 0: caller must run <clinit>. 1: already initialized, or this thread is
 * initializing it. -1: initialization already failed. */
int class_linker_begin_init(ArtClass *klass);
void class_linker_finish_init(ArtClass *klass, int ok);

#endif
