#ifndef MINI_ART_DEX_H
#define MINI_ART_DEX_H

#include <stddef.h>
#include <stdint.h>

typedef struct Apk Apk;
typedef struct DexFile DexFile;
typedef struct CodeItem CodeItem;

DexFile *dex_file_open(const uint8_t *bytes, size_t size);
void dex_file_close(DexFile *dex);

size_t dex_file_class_count(const DexFile *dex);
const char *dex_file_class_descriptor(const DexFile *dex, uint32_t class_idx);

const CodeItem *dex_file_code_item(const DexFile *dex, uint32_t method_idx);
const uint16_t *code_item_insns(const CodeItem *code, uint32_t *out_count);
uint16_t code_item_registers(const CodeItem *code);
uint16_t code_item_ins(const CodeItem *code);

int dex_file_method_at(const DexFile *dex, uint32_t method_idx,
                       const char **class_desc, const char **name, const char **proto);
int dex_file_find_method(const DexFile *dex, const char *class_desc,
                         const char *name, const char *proto, uint32_t *out_method_idx);
const char *dex_file_superclass(const DexFile *dex, const char *class_desc);
int dex_file_field_int(const DexFile *dex, uint32_t field_idx, int32_t *out_value);

size_t multi_dex_count(const Apk *apk);
int multi_dex_read(const Apk *apk, size_t index, uint8_t **out_bytes, size_t *out_size);

#endif
