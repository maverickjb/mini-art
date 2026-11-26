#ifndef MINI_ART_NATIVE_INTERNAL_H
#define MINI_ART_NATIVE_INTERNAL_H

#include "framework.h"
#include "java_object.h"

#include <stddef.h>

struct NativeBinding {
    const char *class_desc;
    const char *name;
    const char *proto;
    ArtNativeFn fn;
};

int native_bind(Runtime *rt, const struct NativeBinding *rows, size_t count);
int native_return_void(Runtime *rt, const JValue *args, uint32_t argc, JValue *result);

const char *native_arg_text(const JValue *args, uint32_t index);
JavaObject *native_new_string(Runtime *rt, const char *text);

int native_register_random(Runtime *rt);
int native_register_system(Runtime *rt);
int native_register_thread(Runtime *rt);
int native_register_activity(Runtime *rt);
int native_register_widget(Runtime *rt);
int native_register_media(Runtime *rt);
int native_register_string(Runtime *rt);
int native_register_math(Runtime *rt);

#endif
