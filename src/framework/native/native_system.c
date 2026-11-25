#include "native_internal.h"

#include <stdint.h>
#include <stdio.h>
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

int native_register_system(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Ljava/lang/System;", "currentTimeMillis", "()J", native_current_time_millis},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
