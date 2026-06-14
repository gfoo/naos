/* naos — B9: kernel heap. See include/heap.h and docs/howto/09-heap.md.
 *
 * A first-fit free-list allocator over a fixed 64 KB arena (in .bss). Each block
 * carries a small header {size, free, next}. kmalloc splits a big-enough free
 * block; kfree marks it free and coalesces with the following block. */
#include "heap.h"
#include <stdint.h>

#define HEAP_SIZE (64u * 1024u)
#define ALIGN     8u

static uint8_t heap[HEAP_SIZE] __attribute__((aligned(ALIGN)));

typedef struct block {
    size_t        size;   /* payload bytes (not counting this header) */
    int           free;
    struct block *next;
} block_t;

static block_t *head;

void heap_init(void)
{
    head = (block_t *)heap;
    head->size = HEAP_SIZE - sizeof(block_t);
    head->free = 1;
    head->next = 0;
}

void *kmalloc(size_t size)
{
    size = (size + (ALIGN - 1)) & ~((size_t)(ALIGN - 1));   /* round up to ALIGN */

    for (block_t *b = head; b; b = b->next) {
        if (!b->free || b->size < size)
            continue;
        /* Split if the leftover can hold a header plus a little payload. */
        if (b->size >= size + sizeof(block_t) + ALIGN) {
            block_t *nb = (block_t *)((uint8_t *)b + sizeof(block_t) + size);
            nb->size = b->size - size - sizeof(block_t);
            nb->free = 1;
            nb->next = b->next;
            b->next  = nb;
            b->size  = size;
        }
        b->free = 0;
        return (uint8_t *)b + sizeof(block_t);
    }
    return 0;   /* heap full */
}

void kfree(void *ptr)
{
    if (!ptr)
        return;
    block_t *b = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    b->free = 1;
    /* Forward coalesce: merge with the next block if it is also free. */
    if (b->next && b->next->free) {
        b->size += sizeof(block_t) + b->next->size;
        b->next  = b->next->next;
    }
}
