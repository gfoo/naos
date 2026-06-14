/* naos — B7: physical memory manager (frame allocator). See docs/howto/07-pmm.md. */
#ifndef NAOS_PMM_H
#define NAOS_PMM_H

#include <stdint.h>
#include "multiboot.h"

void     pmm_init(const struct multiboot_info *mbi);  /* parse the mmap, build the bitmap */
uint32_t pmm_alloc_frame(void);    /* physical address of a free 4 KB frame, or 0 if none */
void     pmm_free_frame(uint32_t addr);
uint32_t pmm_total_count(void);    /* usable frames found in the memory map */
uint32_t pmm_free_count(void);     /* currently free frames */

#endif /* NAOS_PMM_H */
