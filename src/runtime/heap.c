#include "runtime.h"

#include <stdlib.h>

struct Block {
    struct Block *next;
    uint8_t data[];
};

struct Heap {
    struct Block *blocks;
};

Heap *heap_create(void)
{
    return calloc(1, sizeof(Heap));
}

void heap_destroy(Heap *heap)
{
    struct Block *block;

    if (heap == NULL) {
        return;
    }
    block = heap->blocks;
    while (block != NULL) {
        struct Block *next = block->next;
        free(block);
        block = next;
    }
    free(heap);
}

void *heap_alloc(Heap *heap, size_t size)
{
    struct Block *block;

    if (heap == NULL || size == 0) {
        return NULL;
    }
    block = calloc(1, sizeof(*block) + size);
    if (block == NULL) {
        return NULL;
    }
    block->next = heap->blocks;
    heap->blocks = block;
    return block->data;
}
