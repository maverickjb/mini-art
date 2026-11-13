#ifndef MINI_ART_APK_H
#define MINI_ART_APK_H

#include <stddef.h>
#include <stdint.h>

typedef struct Apk Apk;

Apk *apk_open(const char *path);
void apk_close(Apk *apk);
const char *apk_path(const Apk *apk);

const char *apk_launcher_activity(const Apk *apk);
size_t apk_dex_count(const Apk *apk);
int apk_read_dex(const Apk *apk, size_t index, uint8_t **out_bytes, size_t *out_size);

int apk_read_entry(const Apk *apk, const char *name, uint8_t **out_bytes, size_t *out_size);

int binary_xml_find_launcher(const uint8_t *xml, size_t size, char *out, size_t out_cap);

typedef struct ResourceTable ResourceTable;

ResourceTable *resource_table_open(const uint8_t *arsc, size_t size);
void resource_table_close(ResourceTable *table);
const char *resource_table_string(const ResourceTable *table, uint32_t res_id);
int resource_table_check(const uint8_t *arsc, size_t size);

#endif
