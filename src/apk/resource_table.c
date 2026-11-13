#include "apk.h"

#include <stdlib.h>
#include <string.h>

typedef struct ResString {
    uint32_t id;
    char *text;
} ResString;

struct ResourceTable {
    ResString *items;
    size_t count;
};

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
    if ((size_t)header_size + (size_t)count * 4 > size || strings_start > size) {
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

static int add_string(ResourceTable *table, uint32_t id, const char *text)
{
    ResString *grown;
    char *copy;
    size_t i;

    for (i = 0; i < table->count; i++) {
        if (table->items[i].id == id) {
            copy = strdup(text);
            if (copy == NULL) {
                return -1;
            }
            free(table->items[i].text);
            table->items[i].text = copy;
            return 0;
        }
    }
    copy = strdup(text);
    if (copy == NULL) {
        return -1;
    }
    grown = realloc(table->items, (table->count + 1) * sizeof(*grown));
    if (grown == NULL) {
        free(copy);
        return -1;
    }
    table->items = grown;
    table->items[table->count].id = id;
    table->items[table->count].text = copy;
    table->count++;
    return 0;
}

static int parse_type(ResourceTable *table, const uint8_t *chunk, size_t size,
                      uint32_t package_id, char **strings, uint32_t string_count)
{
    uint16_t header_size;
    uint8_t type_id;
    uint32_t entry_count;
    uint32_t entries_start;
    uint32_t i;

    if (size < 20) {
        return -1;
    }
    header_size = ru16(chunk + 2);
    type_id = chunk[8];
    entry_count = ru32(chunk + 12);
    entries_start = ru32(chunk + 16);
    if ((size_t)header_size + (size_t)entry_count * 4 > size || entries_start > size) {
        return -1;
    }
    for (i = 0; i < entry_count; i++) {
        uint32_t offset = ru32(chunk + header_size + i * 4);
        size_t entry;
        uint16_t flags;
        uint8_t value_type;
        uint32_t data;
        uint32_t res_id;
        if (offset == 0xffffffff) {
            continue;
        }
        entry = (size_t)entries_start + offset;
        if (entry + 16 > size) {
            return -1;
        }
        flags = ru16(chunk + entry + 2);
        if ((flags & 0x0001) != 0) {
            continue;
        }
        value_type = chunk[entry + 11];
        data = ru32(chunk + entry + 12);
        if (value_type != 0x03 || data >= string_count || strings[data] == NULL) {
            continue;
        }
        res_id = (package_id << 24) | ((uint32_t)type_id << 16) | i;
        if (add_string(table, res_id, strings[data]) != 0) {
            return -1;
        }
    }
    return 0;
}

ResourceTable *resource_table_open(const uint8_t *arsc, size_t size)
{
    ResourceTable *table;
    char **strings = NULL;
    uint32_t string_count = 0;
    size_t cursor;
    int saw_pool = 0;

    if (arsc == NULL || size < 12 || ru16(arsc) != 0x0002) {
        return NULL;
    }
    table = calloc(1, sizeof(*table));
    if (table == NULL) {
        return NULL;
    }
    cursor = ru16(arsc + 2);
    while (cursor + 8 <= size) {
        uint16_t type = ru16(arsc + cursor);
        uint32_t chunk_size = ru32(arsc + cursor + 4);
        if (chunk_size < 8 || cursor + chunk_size > size) {
            break;
        }
        if (type == 0x0001 && !saw_pool) {
            if (read_string_pool(arsc + cursor, chunk_size, &strings, &string_count) != 0) {
                resource_table_close(table);
                return NULL;
            }
            saw_pool = 1;
        } else if (type == 0x0200 && strings != NULL) {
            uint32_t package_id = ru32(arsc + cursor + 8) & 0xff;
            uint16_t header_size = ru16(arsc + cursor + 2);
            size_t child = cursor + header_size;
            size_t package_end = cursor + chunk_size;
            if (header_size < 12 || package_end > size) {
                resource_table_close(table);
                free(strings);
                return NULL;
            }
            while (child + 8 <= package_end) {
                uint16_t child_type = ru16(arsc + child);
                uint32_t child_size = ru32(arsc + child + 4);
                if (child_size < 8 || child + child_size > package_end) {
                    break;
                }
                if (child_type == 0x0201 &&
                    parse_type(table, arsc + child, child_size, package_id, strings,
                               string_count) != 0) {
                    uint32_t i;
                    for (i = 0; i < string_count; i++) {
                        free(strings[i]);
                    }
                    free(strings);
                    resource_table_close(table);
                    return NULL;
                }
                child += child_size;
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
    return table;
}

void resource_table_close(ResourceTable *table)
{
    size_t i;

    if (table == NULL) {
        return;
    }
    for (i = 0; i < table->count; i++) {
        free(table->items[i].text);
    }
    free(table->items);
    free(table);
}

const char *resource_table_string(const ResourceTable *table, uint32_t res_id)
{
    size_t i;

    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < table->count; i++) {
        if (table->items[i].id == res_id) {
            return table->items[i].text;
        }
    }
    return NULL;
}

int resource_table_check(const uint8_t *arsc, size_t size)
{
    ResourceTable *table = resource_table_open(arsc, size);
    if (table == NULL) {
        return -1;
    }
    resource_table_close(table);
    return 0;
}
