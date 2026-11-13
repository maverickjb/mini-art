#include "framework.h"

#include <stdlib.h>

struct LoadedApk {
    Runtime *runtime;
    Apk *apk;
};

LoadedApk *loaded_apk_create(Runtime *rt, Apk *apk)
{
    LoadedApk *loaded;

    if (rt == NULL || apk == NULL) {
        return NULL;
    }

    loaded = calloc(1, sizeof(*loaded));
    if (loaded == NULL) {
        return NULL;
    }
    loaded->runtime = rt;
    loaded->apk = apk;
    return loaded;
}

void loaded_apk_destroy(LoadedApk *loaded)
{
    free(loaded);
}

const char *loaded_apk_launcher(const LoadedApk *loaded)
{
    if (loaded == NULL) {
        return NULL;
    }
    return apk_launcher_activity(loaded->apk);
}

const Apk *loaded_apk_apk(const LoadedApk *loaded)
{
    if (loaded == NULL) {
        return NULL;
    }
    return loaded->apk;
}
