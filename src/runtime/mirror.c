#include "runtime.h"

#include <stdint.h>

typedef struct Object {
    void *clazz;
} Object;

typedef struct Class {
    Object object;
    const char *descriptor;
} Class;

typedef struct String {
    Object object;
    uint32_t count;
    uint16_t *data;
} String;

typedef struct Array {
    Object object;
    uint32_t length;
} Array;

size_t mirror_object_header_size(void)
{
    return sizeof(Object);
}

size_t mirror_class_size(void)
{
    return sizeof(Class);
}

size_t mirror_string_size(void)
{
    return sizeof(String);
}

size_t mirror_array_size(void)
{
    return sizeof(Array);
}
