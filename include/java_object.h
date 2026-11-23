#ifndef MINI_ART_JAVA_OBJECT_H
#define MINI_ART_JAVA_OBJECT_H

#include "runtime.h"

#include <stdint.h>

typedef struct JavaObject JavaObject;

JavaObject *java_object_new(Runtime *rt, const char *class_desc);
JavaObject *java_object_new_array(Runtime *rt, const char *class_desc, int32_t length);
int java_object_array_data(const JavaObject *object, int32_t *length, int *width, void **data);
int java_object_check(const void *value);
const char *java_object_class(const JavaObject *object);
void java_object_set_text(JavaObject *object, const char *text);
const char *java_object_text(const JavaObject *object);
int java_object_set_field(JavaObject *object, const char *name, JValue value);
int java_object_get_field(const JavaObject *object, const char *name, JValue *out);
void java_object_set_listener(JavaObject *object, JavaObject *listener);
JavaObject *java_object_listener(const JavaObject *object);
void java_object_set_id(JavaObject *object, uint32_t id);
int java_object_has_id(const JavaObject *object, uint32_t id);
void java_object_set_widget(JavaObject *object, void *widget);
void *java_object_widget(const JavaObject *object);

#endif
