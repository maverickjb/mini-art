#include "java_object.h"

#include <stdlib.h>
#include <string.h>

#define JAVA_OBJECT_MAGIC 0x4a4f424au

struct FieldSlot {
    const char *name;
    JValue value;
    struct FieldSlot *next;
};

struct JavaObject {
    uint32_t magic;
    char *class_desc;
    char *text;
    struct FieldSlot *fields;
    JavaObject *listener;
    void *widget;
    uint8_t *elements;
    int32_t length;
    int width;
    uint32_t id;
    int has_id;
};

JavaObject *java_object_new(Runtime *rt, const char *class_desc)
{
    JavaObject *object;

    (void)rt;
    if (class_desc == NULL) {
        return NULL;
    }
    object = calloc(1, sizeof(*object));
    if (object == NULL) {
        return NULL;
    }
    object->magic = JAVA_OBJECT_MAGIC;
    object->class_desc = strdup(class_desc);
    if (object->class_desc == NULL) {
        free(object);
        return NULL;
    }
    return object;
}

static int array_width(const char *desc)
{
    if (desc == NULL || desc[0] != '[') {
        return 0;
    }
    switch (desc[1]) {
    case 'B':
    case 'Z':
        return 1;
    case 'S':
    case 'C':
        return 2;
    case 'I':
    case 'F':
        return 4;
    case 'J':
    case 'D':
        return 8;
    default:
        return (int)sizeof(void *);
    }
}

JavaObject *java_object_new_array(Runtime *rt, const char *class_desc, int32_t length)
{
    JavaObject *object;
    int width;

    if (length < 0 || length > 1 << 20) {
        return NULL;
    }
    width = array_width(class_desc);
    if (width == 0) {
        return NULL;
    }
    object = java_object_new(rt, class_desc);
    if (object == NULL) {
        return NULL;
    }
    object->width = width;
    object->length = length;
    if (length > 0) {
        object->elements = calloc((size_t)length, (size_t)width);
        if (object->elements == NULL) {
            return NULL;
        }
    }
    return object;
}

int java_object_array_data(const JavaObject *object, int32_t *length, int *width, void **data)
{
    if (!java_object_check(object) || object->width == 0) {
        return -1;
    }
    if (length != NULL) {
        *length = object->length;
    }
    if (width != NULL) {
        *width = object->width;
    }
    if (data != NULL) {
        *data = object->elements;
    }
    return 0;
}

int java_object_check(const void *value)
{
    const JavaObject *object = value;
    return object != NULL && object->magic == JAVA_OBJECT_MAGIC;
}

const char *java_object_class(const JavaObject *object)
{
    if (!java_object_check(object)) {
        return NULL;
    }
    return object->class_desc;
}

void java_object_set_text(JavaObject *object, const char *text)
{
    char *copy;

    if (!java_object_check(object)) {
        return;
    }
    copy = strdup(text != NULL ? text : "");
    if (copy == NULL) {
        return;
    }
    free(object->text);
    object->text = copy;
}

const char *java_object_text(const JavaObject *object)
{
    if (!java_object_check(object) || object->text == NULL) {
        return "";
    }
    return object->text;
}

int java_object_set_field(JavaObject *object, const char *name, JValue value)
{
    struct FieldSlot *slot;

    if (!java_object_check(object) || name == NULL) {
        return -1;
    }
    for (slot = object->fields; slot != NULL; slot = slot->next) {
        if (strcmp(slot->name, name) == 0) {
            slot->value = value;
            return 0;
        }
    }
    slot = calloc(1, sizeof(*slot));
    if (slot == NULL) {
        return -1;
    }
    slot->name = name;
    slot->value = value;
    slot->next = object->fields;
    object->fields = slot;
    return 0;
}

int java_object_get_field(const JavaObject *object, const char *name, JValue *out)
{
    struct FieldSlot *slot;

    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (!java_object_check(object) || name == NULL) {
        return -1;
    }
    for (slot = object->fields; slot != NULL; slot = slot->next) {
        if (strcmp(slot->name, name) == 0) {
            if (out != NULL) {
                *out = slot->value;
            }
            return 0;
        }
    }
    return 0;
}

void java_object_set_listener(JavaObject *object, JavaObject *listener)
{
    if (java_object_check(object)) {
        object->listener = listener;
    }
}

JavaObject *java_object_listener(const JavaObject *object)
{
    if (!java_object_check(object)) {
        return NULL;
    }
    return object->listener;
}

void java_object_set_id(JavaObject *object, uint32_t id)
{
    if (!java_object_check(object)) {
        return;
    }
    object->id = id;
    object->has_id = 1;
}

int java_object_has_id(const JavaObject *object, uint32_t id)
{
    return java_object_check(object) && object->has_id && object->id == id;
}

void java_object_set_widget(JavaObject *object, void *widget)
{
    if (java_object_check(object)) {
        object->widget = widget;
    }
}

void *java_object_widget(const JavaObject *object)
{
    if (!java_object_check(object)) {
        return NULL;
    }
    return object->widget;
}
