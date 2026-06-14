/* naos — B7: the Multiboot 1 information structure GRUB passes in ebx.
 * We only declare the fields we use (memory map). See docs/howto/07-pmm.md. */
#ifndef NAOS_MULTIBOOT_H
#define NAOS_MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_FLAG_MMAP        0x40   /* flags bit 6: mmap_* fields are valid */
#define MULTIBOOT_MEMORY_AVAILABLE 1      /* mmap entry type: usable RAM */

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;     /* total size of the mmap buffer */
    uint32_t mmap_addr;       /* physical address of the first entry */
    /* (further fields exist but are unused here) */
} __attribute__((packed));

/* One memory-map entry. `size` excludes itself, so the next entry is at
 * (uint8_t*)entry + entry->size + 4. */
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

#endif /* NAOS_MULTIBOOT_H */
