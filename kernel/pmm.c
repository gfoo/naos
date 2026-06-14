/* naos — B7: physical frame allocator (bitmap). See include/pmm.h and docs/howto/07-pmm.md.
 *
 * One bit per 4 KB physical frame: 1 = used, 0 = free. We start with everything
 * used, free only the frames the Multiboot memory map marks available, then keep
 * the low 4 MB reserved (BIOS area + our kernel image). */
#include "pmm.h"

#define FRAME_SIZE   4096u
#define MAX_FRAMES   (1024u * 1024u)      /* cover the 4 GB 32-bit space */
#define BITMAP_WORDS (MAX_FRAMES / 32u)   /* 32768 words = 128 KB in .bss */
#define RESERVE_END  0x400000u            /* keep the low 4 MB (kernel + low memory) */

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_frames;             /* usable frames found in the map */
static uint32_t used_frames;

static inline void mark_used(uint32_t f) { bitmap[f >> 5] |=  (1u << (f & 31)); }
static inline void mark_free(uint32_t f) { bitmap[f >> 5] &= ~(1u << (f & 31)); }
static inline int  is_used  (uint32_t f) { return bitmap[f >> 5] & (1u << (f & 31)); }

void pmm_init(const struct multiboot_info *mbi)
{
    for (uint32_t i = 0; i < BITMAP_WORDS; i++)
        bitmap[i] = 0xFFFFFFFFu;          /* everything used to begin with */
    used_frames  = MAX_FRAMES;
    total_frames = 0;

    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        uint32_t addr = mbi->mmap_addr;
        uint32_t end  = mbi->mmap_addr + mbi->mmap_length;
        while (addr < end) {
            const struct multiboot_mmap_entry *e = (const struct multiboot_mmap_entry *)addr;
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint64_t last = e->addr + e->len;
                for (uint64_t p = e->addr; p + FRAME_SIZE <= last; p += FRAME_SIZE) {
                    if (p >= 0x100000000ull) break;       /* >4 GB: out of 32-bit range */
                    uint32_t f = (uint32_t)(p / FRAME_SIZE);
                    if (f < MAX_FRAMES && is_used(f)) {
                        mark_free(f);
                        used_frames--;
                        total_frames++;
                    }
                }
            }
            addr += e->size + 4;          /* size excludes itself */
        }
    }

    /* Reserve the low 4 MB (BIOS, video memory, our kernel). */
    for (uint32_t f = 0; f < RESERVE_END / FRAME_SIZE; f++)
        if (!is_used(f)) { mark_used(f); used_frames++; }
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t f = RESERVE_END / FRAME_SIZE; f < MAX_FRAMES; f++)
        if (!is_used(f)) { mark_used(f); used_frames++; return f * FRAME_SIZE; }
    return 0;   /* out of memory */
}

void pmm_free_frame(uint32_t addr)
{
    uint32_t f = addr / FRAME_SIZE;
    if (f < MAX_FRAMES && is_used(f)) { mark_free(f); used_frames--; }
}

uint32_t pmm_total_count(void) { return total_frames; }
uint32_t pmm_free_count(void)  { return MAX_FRAMES - used_frames; }
