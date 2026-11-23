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

static int native_audio_attributes_set_usage(Runtime *rt, const JValue *args, uint32_t argc,
                                              JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioAttributes.Builder.setUsage %d\n", args[1].i);
    result->l = args[0].l;
    return 0;
}

static int native_audio_attributes_set_content_type(Runtime *rt, const JValue *args, uint32_t argc,
                                                    JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioAttributes.Builder.setContentType %d\n", args[1].i);
    result->l = args[0].l;
    return 0;
}

static int native_audio_attributes_build(Runtime *rt, const JValue *args, uint32_t argc,
                                         JValue *result)
{
    JavaObject *attributes;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    attributes = java_object_new(rt, "Landroid/media/AudioAttributes;");
    if (attributes == NULL) {
        return -1;
    }
    fprintf(stderr, "AudioAttributes.Builder.build\n");
    result->l = attributes;
    return 0;
}

static int native_audio_format_builder_init(Runtime *rt, const JValue *args, uint32_t argc,
                                            JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioFormat.Builder.<init>\n");
    return 0;
}

static int native_audio_format_set_sample_rate(Runtime *rt, const JValue *args, uint32_t argc,
                                               JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioFormat.Builder.setSampleRate %d\n", args[1].i);
    result->l = args[0].l;
    return 0;
}

static int native_audio_format_set_encoding(Runtime *rt, const JValue *args, uint32_t argc,
                                            JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioFormat.Builder.setEncoding %d\n", args[1].i);
    result->l = args[0].l;
    return 0;
}

static int native_audio_format_set_channel_mask(Runtime *rt, const JValue *args, uint32_t argc,
                                                 JValue *result)
{
    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioFormat.Builder.setChannelMask %d\n", args[1].i);
    result->l = args[0].l;
    return 0;
}

static int native_audio_format_build(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *format;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    format = java_object_new(rt, "Landroid/media/AudioFormat;");
    if (format == NULL) {
        return -1;
    }
    fprintf(stderr, "AudioFormat.Builder.build\n");
    result->l = format;
    return 0;
}

static int native_audio_track_builder_init(Runtime *rt, const JValue *args, uint32_t argc,
                                           JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "AudioTrack.Builder.<init>\n");
    return 0;
}

static int return_builder(const JValue *args, uint32_t argc, JValue *result, const char *label)
{
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    if (argc >= 2 && java_object_check(args[1].l)) {
        fprintf(stderr, "%s %s\n", label, java_object_class(args[1].l));
    } else if (argc >= 2) {
        fprintf(stderr, "%s %d\n", label, args[1].i);
    } else {
        fprintf(stderr, "%s\n", label);
    }
    result->l = args[0].l;
    return 0;
}

static int native_audio_track_set_attributes(Runtime *rt, const JValue *args, uint32_t argc,
                                             JValue *result)
{
    (void)rt;
    return return_builder(args, argc, result, "AudioTrack.Builder.setAudioAttributes");
}

static int native_audio_track_set_format(Runtime *rt, const JValue *args, uint32_t argc,
                                         JValue *result)
{
    (void)rt;
    return return_builder(args, argc, result, "AudioTrack.Builder.setAudioFormat");
}

static int native_audio_track_set_buffer_size(Runtime *rt, const JValue *args, uint32_t argc,
                                              JValue *result)
{
    (void)rt;
    return return_builder(args, argc, result, "AudioTrack.Builder.setBufferSizeInBytes");
}

static int native_audio_track_set_transfer_mode(Runtime *rt, const JValue *args, uint32_t argc,
                                                JValue *result)
{
    (void)rt;
    return return_builder(args, argc, result, "AudioTrack.Builder.setTransferMode");
}

static int native_audio_track_build(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *track;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    track = java_object_new(rt, "Landroid/media/AudioTrack;");
    if (track == NULL) {
        return -1;
    }
    fprintf(stderr, "AudioTrack.Builder.build\n");
    result->l = track;
    return 0;
}

static int native_audio_track_write(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    int32_t length = 0;

    (void)rt;
    if (argc < 4 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    java_object_array_data(args[1].l, &length, NULL, NULL);
    fprintf(stderr, "AudioTrack.write %d\n", length);
    result->i = length;
    return 0;
}

static int native_audio_track_void(Runtime *rt, const JValue *args, uint32_t argc, JValue *result,
                                   const char *label)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "%s\n", label);
    return 0;
}

static int native_audio_track_stop(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_audio_track_void(rt, args, argc, result, "AudioTrack.stop");
}

static int native_audio_track_release(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_audio_track_void(rt, args, argc, result, "AudioTrack.release");
}

static int native_audio_track_play(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    return native_audio_track_void(rt, args, argc, result, "AudioTrack.play");
}

static int native_audio_track_reload(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    if (native_audio_track_void(rt, args, argc, result, "AudioTrack.reloadStaticData") != 0 ||
        result == NULL) {
        return -1;
    }
    result->i = 0;
    return 0;
}

int native_register_media(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Landroid/media/AudioAttributes$Builder;", "<init>", "()V",
         native_audio_attributes_builder_init},
        {"Landroid/media/AudioAttributes$Builder;", "setUsage",
         "(I)Landroid/media/AudioAttributes$Builder;", native_audio_attributes_set_usage},
        {"Landroid/media/AudioAttributes$Builder;", "setContentType",
         "(I)Landroid/media/AudioAttributes$Builder;", native_audio_attributes_set_content_type},
        {"Landroid/media/AudioAttributes$Builder;", "build", "()Landroid/media/AudioAttributes;",
         native_audio_attributes_build},
        {"Landroid/media/AudioFormat$Builder;", "<init>", "()V", native_audio_format_builder_init},
        {"Landroid/media/AudioFormat$Builder;", "setSampleRate",
         "(I)Landroid/media/AudioFormat$Builder;", native_audio_format_set_sample_rate},
        {"Landroid/media/AudioFormat$Builder;", "setEncoding",
         "(I)Landroid/media/AudioFormat$Builder;", native_audio_format_set_encoding},
        {"Landroid/media/AudioFormat$Builder;", "setChannelMask",
         "(I)Landroid/media/AudioFormat$Builder;", native_audio_format_set_channel_mask},
        {"Landroid/media/AudioFormat$Builder;", "build", "()Landroid/media/AudioFormat;",
         native_audio_format_build},
        {"Landroid/media/AudioTrack$Builder;", "<init>", "()V", native_audio_track_builder_init},
        {"Landroid/media/AudioTrack$Builder;", "setAudioAttributes",
         "(Landroid/media/AudioAttributes;)Landroid/media/AudioTrack$Builder;",
         native_audio_track_set_attributes},
        {"Landroid/media/AudioTrack$Builder;", "setAudioFormat",
         "(Landroid/media/AudioFormat;)Landroid/media/AudioTrack$Builder;",
         native_audio_track_set_format},
        {"Landroid/media/AudioTrack$Builder;", "setBufferSizeInBytes",
         "(I)Landroid/media/AudioTrack$Builder;", native_audio_track_set_buffer_size},
        {"Landroid/media/AudioTrack$Builder;", "setTransferMode",
         "(I)Landroid/media/AudioTrack$Builder;", native_audio_track_set_transfer_mode},
        {"Landroid/media/AudioTrack$Builder;", "build", "()Landroid/media/AudioTrack;",
         native_audio_track_build},
        {"Landroid/media/AudioTrack;", "write", "([SII)I", native_audio_track_write},
        {"Landroid/media/AudioTrack;", "stop", "()V", native_audio_track_stop},
        {"Landroid/media/AudioTrack;", "release", "()V", native_audio_track_release},
        {"Landroid/media/AudioTrack;", "play", "()V", native_audio_track_play},
        {"Landroid/media/AudioTrack;", "reloadStaticData", "()I", native_audio_track_reload},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
