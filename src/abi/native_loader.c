#include "abi.h"

#include <dlfcn.h>
#include <stdlib.h>

struct NativeLib {
    void *handle;
};

NativeLib *native_loader_load(const char *path)
{
    NativeLib *lib;
    void *handle;

    if (path == NULL) {
        return NULL;
    }

    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
        return NULL;
    }

    lib = calloc(1, sizeof(*lib));
    if (lib == NULL) {
        dlclose(handle);
        return NULL;
    }
    lib->handle = handle;
    return lib;
}

void native_loader_close(NativeLib *lib)
{
    if (lib == NULL) {
        return;
    }
    if (lib->handle != NULL) {
        dlclose(lib->handle);
    }
    free(lib);
}

void *native_loader_symbol(NativeLib *lib, const char *name)
{
    if (lib == NULL || lib->handle == NULL || name == NULL) {
        return NULL;
    }
    return dlsym(lib->handle, name);
}
