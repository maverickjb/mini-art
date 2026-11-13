#include "apk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "inflate.h"

typedef struct ZipEntry {
    char *name;
    uint16_t method;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint32_t local_offset;
} ZipEntry;

struct Apk {
    char *path;
    uint8_t *bytes;
    size_t size;
    ZipEntry *entries;
    size_t entry_count;
    char *launcher;
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

static const ZipEntry *apk_find(const Apk *apk, const char *name)
{
    size_t i;

    if (apk == NULL || name == NULL) {
        return NULL;
    }
    for (i = 0; i < apk->entry_count; i++) {
        if (strcmp(apk->entries[i].name, name) == 0) {
            return &apk->entries[i];
        }
    }
    return NULL;
}

static int parse_central_directory(Apk *apk)
{
    size_t min_off;
    size_t off;
    uint16_t comment_len;
    uint16_t count;
    uint32_t cd_offset;
    uint32_t cd_size;
    size_t cursor;
    size_t i;

    if (apk->size < 22) {
        return -1;
    }
    min_off = apk->size > 65557 ? apk->size - 65557 : 0;
    off = apk->size - 22;
    for (;;) {
        if (ru32(apk->bytes + off) == 0x06054b50) {
            comment_len = ru16(apk->bytes + off + 20);
            if (off + 22 + comment_len == apk->size) {
                break;
            }
        }
        if (off == min_off) {
            return -1;
        }
        off--;
    }

    count = ru16(apk->bytes + off + 10);
    cd_size = ru32(apk->bytes + off + 12);
    cd_offset = ru32(apk->bytes + off + 16);
    if ((size_t)cd_offset + cd_size > apk->size) {
        return -1;
    }

    apk->entries = calloc(count, sizeof(*apk->entries));
    if (count > 0 && apk->entries == NULL) {
        return -1;
    }
    cursor = cd_offset;
    for (i = 0; i < count; i++) {
        uint16_t name_len;
        uint16_t extra_len;
        uint16_t entry_comment;
        ZipEntry *entry;
        if (cursor + 46 > apk->size || ru32(apk->bytes + cursor) != 0x02014b50) {
            return -1;
        }
        entry = &apk->entries[apk->entry_count];
        entry->method = ru16(apk->bytes + cursor + 10);
        entry->comp_size = ru32(apk->bytes + cursor + 20);
        entry->uncomp_size = ru32(apk->bytes + cursor + 24);
        name_len = ru16(apk->bytes + cursor + 28);
        extra_len = ru16(apk->bytes + cursor + 30);
        entry_comment = ru16(apk->bytes + cursor + 32);
        entry->local_offset = ru32(apk->bytes + cursor + 42);
        if (cursor + 46 + name_len + extra_len + entry_comment > apk->size) {
            return -1;
        }
        entry->name = malloc((size_t)name_len + 1);
        if (entry->name == NULL) {
            return -1;
        }
        memcpy(entry->name, apk->bytes + cursor + 46, name_len);
        entry->name[name_len] = '\0';
        apk->entry_count++;
        cursor += 46 + name_len + extra_len + entry_comment;
    }
    return 0;
}

static int read_launcher(Apk *apk)
{
    uint8_t *xml = NULL;
    size_t xml_size = 0;
    char name[512];

    name[0] = '\0';
    if (apk_read_entry(apk, "AndroidManifest.xml", &xml, &xml_size) != 0) {
        return 0;
    }
    if (binary_xml_find_launcher(xml, xml_size, name, sizeof(name)) == 0) {
        apk->launcher = strdup(name);
    }
    free(xml);
    return apk->launcher == NULL && name[0] != '\0' ? -1 : 0;
}

Apk *apk_open(const char *path)
{
    struct stat st;
    FILE *file;
    Apk *apk;

    if (path == NULL || stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    apk = calloc(1, sizeof(*apk));
    if (apk == NULL) {
        fclose(file);
        return NULL;
    }
    apk->path = strdup(path);
    apk->size = (size_t)st.st_size;
    apk->bytes = malloc(apk->size ? apk->size : 1);
    if (apk->path == NULL || apk->bytes == NULL ||
        fread(apk->bytes, 1, apk->size, file) != apk->size ||
        parse_central_directory(apk) != 0 || read_launcher(apk) != 0) {
        fclose(file);
        apk_close(apk);
        return NULL;
    }
    fclose(file);
    return apk;
}

void apk_close(Apk *apk)
{
    size_t i;

    if (apk == NULL) {
        return;
    }
    for (i = 0; i < apk->entry_count; i++) {
        free(apk->entries[i].name);
    }
    free(apk->entries);
    free(apk->launcher);
    free(apk->bytes);
    free(apk->path);
    free(apk);
}

const char *apk_path(const Apk *apk)
{
    if (apk == NULL) {
        return NULL;
    }
    return apk->path;
}

const char *apk_launcher_activity(const Apk *apk)
{
    if (apk == NULL) {
        return NULL;
    }
    return apk->launcher;
}

size_t apk_dex_count(const Apk *apk)
{
    size_t count = 0;
    size_t i;

    if (apk_find(apk, "classes.dex") != NULL) {
        count = 1;
    }
    for (i = 2;; i++) {
        char name[32];
        snprintf(name, sizeof(name), "classes%zu.dex", i);
        if (apk_find(apk, name) == NULL) {
            break;
        }
        count++;
    }
    return count;
}

int apk_read_entry(const Apk *apk, const char *name, uint8_t **out_bytes, size_t *out_size)
{
    const ZipEntry *entry;
    uint32_t local;
    uint16_t name_len;
    uint16_t extra_len;
    size_t data_off;
    uint8_t *out;

    if (out_bytes != NULL) {
        *out_bytes = NULL;
    }
    if (out_size != NULL) {
        *out_size = 0;
    }
    entry = apk_find(apk, name);
    if (entry == NULL || out_bytes == NULL || out_size == NULL) {
        return -1;
    }
    local = entry->local_offset;
    if ((size_t)local + 30 > apk->size || ru32(apk->bytes + local) != 0x04034b50) {
        return -1;
    }
    name_len = ru16(apk->bytes + local + 26);
    extra_len = ru16(apk->bytes + local + 28);
    data_off = (size_t)local + 30 + name_len + extra_len;
    if (data_off + entry->comp_size > apk->size) {
        return -1;
    }

    out = malloc(entry->uncomp_size ? entry->uncomp_size : 1);
    if (out == NULL) {
        return -1;
    }
    if (entry->method == 0) {
        if (entry->comp_size != entry->uncomp_size) {
            free(out);
            return -1;
        }
        memcpy(out, apk->bytes + data_off, entry->uncomp_size);
    } else if (entry->method == 8) {
        if (raw_inflate(apk->bytes + data_off, entry->comp_size, out, entry->uncomp_size) != 0) {
            free(out);
            return -1;
        }
    } else {
        free(out);
        return -1;
    }

    *out_bytes = out;
    *out_size = entry->uncomp_size;
    return 0;
}

int apk_read_dex(const Apk *apk, size_t index, uint8_t **out_bytes, size_t *out_size)
{
    char name[32];

    if (index == 0) {
        snprintf(name, sizeof(name), "classes.dex");
    } else {
        snprintf(name, sizeof(name), "classes%zu.dex", index + 1);
    }
    return apk_read_entry(apk, name, out_bytes, out_size);
}
