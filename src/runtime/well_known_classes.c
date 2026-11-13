#include "runtime.h"

static const char *kWellKnown[] = {
    "Ljava/lang/Object;",
    "Ljava/lang/String;",
    "Ljava/lang/Class;",
    "Ljava/lang/System;",
    "Ljava/lang/Thread;",
    "Landroid/app/Application;",
    "Landroid/app/Activity;",
    "Landroid/content/Context;",
    "Landroid/content/Intent;",
    "Landroid/os/Bundle;",
    "Landroid/os/Looper;",
    "Landroid/os/Handler;",
    "Landroid/util/Log;",
};

size_t well_known_class_count(void)
{
    return sizeof(kWellKnown) / sizeof(kWellKnown[0]);
}

const char *well_known_class_descriptor(size_t index)
{
    if (index >= well_known_class_count()) {
        return NULL;
    }
    return kWellKnown[index];
}

int well_known_classes_init(Runtime *rt)
{
    (void)rt;
    return well_known_class_count() == 0 ? -1 : 0;
}
