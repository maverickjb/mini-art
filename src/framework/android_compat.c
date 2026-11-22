#include "framework.h"

int android_compat_init(Runtime *rt, const RuntimeConfig *config)
{
    if (rt == NULL || config == NULL || config->sdk_version <= 0) {
        return -1;
    }
    return runtime_define_static_int(rt, "Landroid/os/Build$VERSION;", "SDK_INT", config->sdk_version);
}
