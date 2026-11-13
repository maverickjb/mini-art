#include "runtime.h"

#include <stdlib.h>

struct ClassLinker {
    Runtime *runtime;
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
    free(linker);
}
