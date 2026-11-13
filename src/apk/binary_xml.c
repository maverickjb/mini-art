#include "apk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t ru16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static uint32_t ru32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static int decode_utf16(const uint8_t *bytes, size_t size, size_t *off, char **out)
{
    uint32_t chars;
    uint32_t i;
    char *text;
    size_t len = 0;

    if (*off + 2 > size) {
        return -1;
    }
    chars = ru16(bytes + *off);
    *off += 2;
    if ((chars & 0x8000) != 0) {
        if (*off + 2 > size) {
            return -1;
        }
        chars = ((chars & 0x7fff) << 16) | ru16(bytes + *off);
        *off += 2;
    }
    if (*off + (size_t)chars * 2 + 2 > size) {
        return -1;
    }
    text = malloc((size_t)chars * 3 + 1);
    if (text == NULL) {
        return -1;
    }
    for (i = 0; i < chars; i++) {
        uint16_t unit = ru16(bytes + *off + i * 2);
        if (unit < 0x80) {
            text[len++] = (char)unit;
        } else if (unit < 0x800) {
            text[len++] = (char)(0xc0 | (unit >> 6));
            text[len++] = (char)(0x80 | (unit & 0x3f));
        } else {
            text[len++] = (char)(0xe0 | (unit >> 12));
            text[len++] = (char)(0x80 | ((unit >> 6) & 0x3f));
            text[len++] = (char)(0x80 | (unit & 0x3f));
        }
    }
    text[len] = '\0';
    *off += (size_t)chars * 2 + 2;
    *out = text;
    return 0;
}

static int decode_length(const uint8_t *bytes, size_t size, size_t *off, uint32_t *out)
{
    uint32_t n;
    if (*off >= size) {
        return -1;
    }
    n = bytes[(*off)++];
    if ((n & 0x80) != 0) {
        if (*off >= size) {
            return -1;
        }
        n = ((n & 0x7f) << 8) | bytes[(*off)++];
    }
    *out = n;
    return 0;
}

static int decode_utf8(const uint8_t *bytes, size_t size, size_t *off, char **out)
{
    uint32_t unused;
    uint32_t nbytes;
    char *text;

    if (decode_length(bytes, size, off, &unused) != 0 ||
        decode_length(bytes, size, off, &nbytes) != 0 || *off + nbytes + 1 > size) {
        return -1;
    }
    text = malloc((size_t)nbytes + 1);
    if (text == NULL) {
        return -1;
    }
    memcpy(text, bytes + *off, nbytes);
    text[nbytes] = '\0';
    *off += (size_t)nbytes + 1;
    *out = text;
    return 0;
}

static int read_string_pool(const uint8_t *chunk, size_t size, char ***out_strings, uint32_t *out_count)
{
    uint16_t header_size;
    uint32_t count;
    uint32_t flags;
    uint32_t strings_start;
    char **strings;
    uint32_t i;
    int utf8;

    if (size < 28 || ru16(chunk) != 0x0001) {
        return -1;
    }
    header_size = ru16(chunk + 2);
    count = ru32(chunk + 8);
    flags = ru32(chunk + 16);
    strings_start = ru32(chunk + 20);
    utf8 = (flags & 0x100) != 0;
    if (header_size > size || strings_start > size ||
        (size_t)header_size + (size_t)count * 4 > size) {
        return -1;
    }
    strings = calloc(count ? count : 1, sizeof(*strings));
    if (strings == NULL) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        size_t off = strings_start + ru32(chunk + header_size + i * 4);
        if ((utf8 ? decode_utf8(chunk, size, &off, &strings[i])
                  : decode_utf16(chunk, size, &off, &strings[i])) != 0) {
            uint32_t j;
            for (j = 0; j < i; j++) {
                free(strings[j]);
            }
            free(strings);
            return -1;
        }
    }
    *out_strings = strings;
    *out_count = count;
    return 0;
}

static const char *pool_get(char **strings, uint32_t count, uint32_t index)
{
    if (index == 0xffffffff || index >= count || strings[index] == NULL) {
        return "";
    }
    return strings[index];
}

static void qualify_class(const char *package, const char *name, char *out, size_t cap)
{
    if (name[0] == '\0') {
        out[0] = '\0';
        return;
    }
    if (strchr(name, '.') != NULL && name[0] != '.') {
        snprintf(out, cap, "%s", name);
        return;
    }
    if (name[0] == '.') {
        snprintf(out, cap, "%s%s", package, name);
    } else {
        snprintf(out, cap, "%s.%s", package, name);
    }
}

static int element_layout(const uint8_t *node, size_t node_size, uint32_t *name_at,
                          uint32_t *attr_at, uint16_t *attr_count, uint16_t *attr_size)
{
    uint32_t marker;

    if (node_size < 32) {
        return -1;
    }
    marker = ru32(node + 20);
    if ((marker & 0xffff) == 0x14 && ((marker >> 16) & 0xffff) >= 0x14) {
        *name_at = 16;
        *attr_at = 32;
        *attr_count = ru16(node + 24);
        *attr_size = (uint16_t)(marker >> 16);
    } else if (node_size >= 36) {
        uint32_t standard = ru32(node + 24);
        *name_at = 20;
        *attr_at = 36;
        *attr_count = ru16(node + 28);
        *attr_size = (uint16_t)(standard >> 16);
    } else {
        return -1;
    }
    if (*attr_size < 20) {
        return -1;
    }
    if ((size_t)*attr_at + (size_t)*attr_count * *attr_size > node_size) {
        return -1;
    }
    return 0;
}

int binary_xml_find_launcher(const uint8_t *xml, size_t size, char *out, size_t out_cap)
{
    char **strings = NULL;
    uint32_t string_count = 0;
    size_t cursor;
    int depth = 0;
    int activity_depth = -1;
    int filter_depth = -1;
    int saw_main = 0;
    int saw_launcher = 0;
    char package[256];
    char activity[512];
    char qualified[512];
    int found = 0;

    if (out != NULL && out_cap > 0) {
        out[0] = '\0';
    }
    if (xml == NULL || size < 8 || out == NULL || out_cap == 0 || ru16(xml) != 0x0003) {
        return -1;
    }
    package[0] = '\0';
    activity[0] = '\0';
    cursor = ru16(xml + 2);
    if (cursor < 8 || cursor > size) {
        return -1;
    }

    while (cursor + 8 <= size) {
        uint16_t type = ru16(xml + cursor);
        uint32_t chunk_size = ru32(xml + cursor + 4);
        if (chunk_size < 8 || cursor + chunk_size > size) {
            break;
        }
        if (type == 0x0001 && strings == NULL) {
            if (read_string_pool(xml + cursor, chunk_size, &strings, &string_count) != 0) {
                return -1;
            }
        } else if (type == 0x0102 && strings != NULL) {
            uint32_t name_at;
            uint32_t attr_at;
            uint16_t attr_count;
            uint16_t attr_size;
            const char *element;
            uint16_t a;
            depth++;
            if (element_layout(xml + cursor, chunk_size, &name_at, &attr_at, &attr_count,
                               &attr_size) == 0) {
                element = pool_get(strings, string_count, ru32(xml + cursor + name_at));
                if (strcmp(element, "manifest") == 0) {
                    for (a = 0; a < attr_count; a++) {
                        const uint8_t *attr = xml + cursor + attr_at + (size_t)a * attr_size;
                        const char *attr_name = pool_get(strings, string_count, ru32(attr + 4));
                        if (strcmp(attr_name, "package") == 0 && attr[15] == 0x03) {
                            snprintf(package, sizeof(package), "%s",
                                     pool_get(strings, string_count, ru32(attr + 16)));
                        }
                    }
                } else if (strcmp(element, "activity") == 0) {
                    activity_depth = depth;
                    saw_main = 0;
                    saw_launcher = 0;
                    activity[0] = '\0';
                    for (a = 0; a < attr_count; a++) {
                        const uint8_t *attr = xml + cursor + attr_at + (size_t)a * attr_size;
                        const char *attr_name = pool_get(strings, string_count, ru32(attr + 4));
                        if (strcmp(attr_name, "name") == 0 && attr[15] == 0x03) {
                            snprintf(activity, sizeof(activity), "%s",
                                     pool_get(strings, string_count, ru32(attr + 16)));
                        }
                    }
                } else if (strcmp(element, "intent-filter") == 0 && activity_depth > 0) {
                    filter_depth = depth;
                } else if (filter_depth > 0 &&
                           (strcmp(element, "action") == 0 || strcmp(element, "category") == 0)) {
                    for (a = 0; a < attr_count; a++) {
                        const uint8_t *attr = xml + cursor + attr_at + (size_t)a * attr_size;
                        const char *attr_name = pool_get(strings, string_count, ru32(attr + 4));
                        const char *value;
                        if (strcmp(attr_name, "name") != 0 || attr[15] != 0x03) {
                            continue;
                        }
                        value = pool_get(strings, string_count, ru32(attr + 16));
                        if (strcmp(element, "action") == 0 &&
                            strcmp(value, "android.intent.action.MAIN") == 0) {
                            saw_main = 1;
                        }
                        if (strcmp(element, "category") == 0 &&
                            strcmp(value, "android.intent.category.LAUNCHER") == 0) {
                            saw_launcher = 1;
                        }
                    }
                }
            }
        } else if (type == 0x0103) {
            if (depth == activity_depth) {
                if (saw_main && saw_launcher && !found) {
                    qualify_class(package, activity, qualified, sizeof(qualified));
                    if (qualified[0] != '\0') {
                        snprintf(out, out_cap, "%s", qualified);
                        found = 1;
                    }
                }
                activity_depth = -1;
                filter_depth = -1;
            }
            if (depth == filter_depth) {
                filter_depth = -1;
            }
            if (depth > 0) {
                depth--;
            }
        }
        cursor += chunk_size;
    }

    if (strings != NULL) {
        uint32_t i;
        for (i = 0; i < string_count; i++) {
            free(strings[i]);
        }
        free(strings);
    }
    return found ? 0 : -1;
}
