#include "native_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int native_current_time_millis(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    struct timespec ts;

    (void)rt;
    (void)args;
    (void)argc;
    if (result == NULL || clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return -1;
    }
    result->j = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
    fprintf(stderr, "System.currentTimeMillis %lld\n", (long long)result->j);
    return 0;
}

static int native_load_library(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *name;
    char entry[256];
    uint8_t *bytes = NULL;
    size_t size = 0;

    (void)rt;
    (void)result;
    if (argc < 1) {
        return -1;
    }
    name = native_arg_text(args, 0);
    if (name[0] == '\0') {
        return -1;
    }
    if (snprintf(entry, sizeof(entry), "lib/x86_64/lib%s.so", name) >= (int)sizeof(entry)) {
        return -1;
    }
    if (native_activity_apk() == NULL ||
        apk_read_entry(native_activity_apk(), entry, &bytes, &size) != 0) {
        fprintf(stderr, "System.loadLibrary %s missing %s\n", name, entry);
        return -1;
    }
    free(bytes);
    fprintf(stderr, "System.loadLibrary %s %s %zu\n", name, entry, size);
    return 0;
}

int native_register_system(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Ljava/lang/System;", "currentTimeMillis", "()J", native_current_time_millis},
        {"Ljava/lang/System;", "loadLibrary", "(Ljava/lang/String;)V", native_load_library},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
