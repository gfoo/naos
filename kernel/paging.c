/* naos — B8: paging. See include/paging.h and docs/howto/08-paging.md.
 *
 * Minimal 32-bit paging: one page directory + one page table covering the first
 * 4 MB. We map that page table TWICE: at virtual 0 (identity — so the kernel,
 * physically at 1 MB, keeps running after we flip paging on) and at virtual
 * 0xC0000000 (the "higher-half" window). Everything lives in the low 4 MB, which
 * the identity map covers, so CR3 can use the structures' own addresses. */
#include "paging.h"
#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define HIGH_HALF_PD_INDEX 768          /* 0xC0000000 >> 22 */

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t low_page_table[1024]  __attribute__((aligned(4096)));   /* maps 0..4 MB */

void paging_init(void)
{
    /* Identity page table for the first 4 MB: frame i → physical i*4 KB. */
    for (uint32_t i = 0; i < 1024; i++)
        low_page_table[i] = (i * 0x1000u) | PAGE_PRESENT | PAGE_RW;

    /* Empty the directory (entries not present). */
    for (uint32_t i = 0; i < 1024; i++)
        page_directory[i] = PAGE_RW;    /* present bit clear */

    /* Map the same 4 MB table at virtual 0 (identity) and at 0xC0000000 (higher-half). */
    uint32_t pt = ((uint32_t)low_page_table) | PAGE_PRESENT | PAGE_RW;
    page_directory[0]                  = pt;
    page_directory[HIGH_HALF_PD_INDEX] = pt;

    /* Load CR3 with the directory's physical address (= its address; identity), enable PG. */
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)page_directory));
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;                 /* CR0.PG */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}
