#include "framework.h"

extern unsigned char framework_dex[];
extern unsigned int framework_dex_len;

int android_compat_init(Runtime *rt, const RuntimeConfig *config)
{
    if (rt == NULL || config == NULL || config->sdk_version <= 0) {
        return -1;
    }
    if (runtime_define_static_int(rt, "Landroid/os/Build$VERSION;", "SDK_INT",
                                  config->sdk_version) != 0 ||
        runtime_load_dex(rt, framework_dex, framework_dex_len) != 0) {
        return -1;
    }
    return 0;
}
