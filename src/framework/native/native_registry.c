#include "native_internal.h"

int native_bind(Runtime *rt, const struct NativeBinding *rows, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (runtime_register_native(rt, rows[i].class_desc, rows[i].name, rows[i].proto, rows[i].fn) !=
            0) {
            return -1;
        }
    }
    return 0;
}

static int native_exception_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    java_object_set_text(args[0].l, native_arg_text(args, 1));
    return 0;
}

int native_register_framework(Runtime *rt)
{
    static const struct NativeBinding core[] = {
        {"Ljava/lang/Object;", "<init>", "()V", native_return_void},
        {"Ljava/lang/RuntimeException;", "<init>", "(Ljava/lang/String;)V", native_exception_init},
    };

    if (native_bind(rt, core, sizeof(core) / sizeof(core[0])) != 0 ||
        native_register_random(rt) != 0 || native_register_system(rt) != 0 || native_register_activity(rt) != 0 ||
        native_register_widget(rt) != 0 || native_register_media(rt) != 0 ||
        native_register_string(rt) != 0 || native_register_math(rt) != 0) {
        return -1;
    }
    return 0;
}
