#include "native_internal.h"

#include <math.h>
#include <stdio.h>

static int native_math1(Runtime *rt, const JValue *args, uint32_t argc, JValue *result,
                        double (*fn)(double))
{
    (void)rt;
    if (argc < 1 || result == NULL || fn == NULL) {
        return -1;
    }
    result->d = fn(args[0].d);
    return 0;
}

static int native_to_radians(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL) {
        return -1;
    }
    result->d = args[0].d * (acos(-1.0) / 180.0);
    return 0;
}

static int native_math_sin(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, sin);
}

static int native_math_cos(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, cos);
}

static int native_math_tan(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, tan);
}

static int native_math_sqrt(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, sqrt);
}

static int native_math_log(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, log);
}

static int native_math_log10(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_math1(rt, args, argc, result, log10);
}

static int native_math_min_float(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    float a;
    float b;

    (void)rt;
    if (argc < 2 || result == NULL) {
        return -1;
    }
    a = args[0].f;
    b = args[1].f;
    if (isnan(a)) {
        result->f = a;
    } else if (a == 0.0f && b == 0.0f && signbit(b)) {
        result->f = b;
    } else {
        result->f = a <= b ? a : b;
    }
    return 0;
}

static int native_math_pow(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 3 || result == NULL) {
        return -1;
    }
    result->d = pow(args[0].d, args[2].d);
    return 0;
}

static int native_math_hypot(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 3 || result == NULL) {
        return -1;
    }
    result->d = hypot(args[0].d, args[2].d);
    fprintf(stderr, "Math.hypot %g\n", result->d);
    return 0;
}

int native_register_math(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Ljava/lang/Math;", "toRadians", "(D)D", native_to_radians},
        {"Ljava/lang/Math;", "sin", "(D)D", native_math_sin},
        {"Ljava/lang/Math;", "cos", "(D)D", native_math_cos},
        {"Ljava/lang/Math;", "tan", "(D)D", native_math_tan},
        {"Ljava/lang/Math;", "sqrt", "(D)D", native_math_sqrt},
        {"Ljava/lang/Math;", "log", "(D)D", native_math_log},
        {"Ljava/lang/Math;", "log10", "(D)D", native_math_log10},
        {"Ljava/lang/Math;", "pow", "(DD)D", native_math_pow},
        {"Ljava/lang/Math;", "hypot", "(DD)D", native_math_hypot},
        {"Ljava/lang/Math;", "min", "(FF)F", native_math_min_float},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
