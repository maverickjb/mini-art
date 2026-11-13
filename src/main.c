#include "framework.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    Runtime *rt;
    Apk *apk;
    LoadedApk *loaded;
    int rc;

    if (argc != 3 || strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "usage: mini-art run <apk>\n");
        return 2;
    }

    rt = runtime_create("boot/boot.dex");
    if (rt == NULL) {
        fprintf(stderr, "runtime_create failed\n");
        return 1;
    }

    apk = apk_open(argv[2]);
    if (apk == NULL) {
        fprintf(stderr, "failed to open %s\n", argv[2]);
        runtime_destroy(rt);
        return 1;
    }

    loaded = loaded_apk_create(rt, apk);
    if (loaded == NULL) {
        fprintf(stderr, "failed to load %s\n", argv[2]);
        apk_close(apk);
        runtime_destroy(rt);
        return 1;
    }

    rc = activity_thread_launch(rt, loaded);
    loaded_apk_destroy(loaded);
    apk_close(apk);
    runtime_destroy(rt);
    return rc == 0 ? 0 : 1;
}
