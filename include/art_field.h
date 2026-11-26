#ifndef MINI_ART_ART_FIELD_H
#define MINI_ART_ART_FIELD_H

#include <stdint.h>

typedef struct ArtClass ArtClass;

enum {
    ART_FIELD_INT = 0,
    ART_FIELD_WIDE = 1,
    ART_FIELD_OBJECT = 2
};

/* Identity of a resolved field. The live value is static_values[slot] on the
 * declaring class. This struct does not hold that value. */
struct ArtField {
    ArtClass *declaring;
    const char *name;
    const char *type;
    uint32_t slot;
    int kind;
};

#endif
