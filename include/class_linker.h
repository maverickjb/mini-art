#ifndef MINI_ART_CLASS_LINKER_H
#define MINI_ART_CLASS_LINKER_H

#include "runtime.h"

int class_linker_add_dex(ClassLinker *linker, DexFile *dex);
ArtClass *class_linker_find(const ClassLinker *linker, const char *descriptor);
ArtClass *class_linker_super(const ArtClass *klass);
const char *class_linker_descriptor(const ArtClass *klass);
ArtMethod *class_linker_find_declared(const ArtClass *klass, const char *name, const char *proto);

#endif
