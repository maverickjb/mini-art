#include "runtime.h"

#include <stdlib.h>

struct ArtThread {
    int exception_pending;
};

ArtThread *thread_attach(void)
{
    return calloc(1, sizeof(ArtThread));
}

void thread_detach(ArtThread *thread)
{
    free(thread);
}
