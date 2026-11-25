#include "native_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const uint64_t RANDOM_MULTIPLIER = 0x5deece66dULL;
static const uint64_t RANDOM_ADDEND = 0xbULL;
static const uint64_t RANDOM_MASK = (1ULL << 48) - 1;
static uint64_t random_uniquifier = 0x9e3779b97f4a7c15ULL;

static int random_set_seed(JavaObject *random, int64_t seed)
{
    JValue value;

    memset(&value, 0, sizeof(value));
    value.j = (int64_t)(((uint64_t)seed ^ RANDOM_MULTIPLIER) & RANDOM_MASK);
    return java_object_set_field(random, "seed", value);
}

static int64_t random_fresh_seed(void)
{
    struct timespec ts;
    uint64_t now = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    random_uniquifier *= 181783497276652981ULL;
    return (int64_t)(random_uniquifier ^ now);
}

static int random_next(JavaObject *random, int bits, int32_t *out)
{
    JValue value;
    uint64_t seed;

    if (java_object_get_field(random, "seed", &value) != 0) {
        return -1;
    }
    seed = ((uint64_t)value.j * RANDOM_MULTIPLIER + RANDOM_ADDEND) & RANDOM_MASK;
    memset(&value, 0, sizeof(value));
    value.j = (int64_t)seed;
    if (java_object_set_field(random, "seed", value) != 0) {
        return -1;
    }
    *out = (int32_t)(seed >> (48 - bits));
    return 0;
}

static int native_random_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l) ||
        random_set_seed(args[0].l, random_fresh_seed()) != 0) {
        return -1;
    }
    fprintf(stderr, "Random.<init>\n");
    return 0;
}

/* java.util.Random.nextInt(int). The result is in [0, bound). */
static int native_random_next_int(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *random;
    int32_t bound;
    int32_t bits;
    int32_t value;
    int32_t limit;

    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    random = args[0].l;
    bound = args[1].i;
    if (bound <= 0 || random_next(random, 31, &bits) != 0) {
        return -1;
    }
    if ((bound & (bound - 1)) == 0) {
        value = (int32_t)(((int64_t)bound * bits) >> 31);
    } else {
        limit = bound - 1;
        for (;;) {
            value = bits % bound;
            if ((int32_t)((uint32_t)bits - (uint32_t)value + (uint32_t)limit) >= 0) {
                break;
            }
            if (random_next(random, 31, &bits) != 0) {
                return -1;
            }
        }
    }
    fprintf(stderr, "Random.nextInt %d -> %d\n", bound, value);
    result->i = value;
    return 0;
}

int native_register_random(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Ljava/util/Random;", "<init>", "()V", native_random_init},
        {"Ljava/util/Random;", "nextInt", "(I)I", native_random_next_int},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
