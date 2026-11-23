#include "native_internal.h"

#include <stdio.h>

static int native_audio_attributes_builder_init(Runtime *rt, const JValue *args, uint32_t argc,
                                                JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioAttributes.Builder.<init>\n");
    return 0;
}

int native_register_media(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Landroid/media/AudioAttributes$Builder;", "<init>", "()V",
         native_audio_attributes_builder_init},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
