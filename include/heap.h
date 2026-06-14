/* naos — B9: kernel heap (kmalloc/kfree). See docs/howto/09-heap.md. */
#ifndef NAOS_HEAP_H
#define NAOS_HEAP_H

#include <stddef.h>

void  heap_init(void);
void *kmalloc(size_t size);    /* returns a pointer to >= size bytes, or NULL if full */
void  kfree(void *ptr);

#endif /* NAOS_HEAP_H */
