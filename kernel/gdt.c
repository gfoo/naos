/* naos — B4: kernel-owned GDT. See include/gdt.h and docs/howto/04-gdt.md. */
#include "gdt.h"
#include <stdint.h>

/* One GDT entry: the historical 8-byte segment descriptor, with base and limit
 * split into pieces for 80286 compatibility. Packed so the layout matches the
 * hardware byte for byte. */
struct gdt_entry {
    uint16_t limit_low;     /* limit[0:15] */
    uint16_t base_low;      /* base[0:15] */
    uint8_t  base_mid;      /* base[16:23] */
    uint8_t  access;        /* present, DPL, type */
    uint8_t  flags_limit;   /* flags (G, D) | limit[16:19] */
    uint8_t  base_high;     /* base[24:31] */
} __attribute__((packed));

/* The value loaded by lgdt: table size minus one, then linear base address. */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define GDT_ENTRIES 5
static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gp;

/* In gdt_flush.asm: lgdt, then reload the segment registers (the C compiler
 * can't reload CS, that needs a far jump). */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].flags_limit = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[i].base_high   = (base >> 24) & 0xFF;
}

void gdt_init(void)
{
    /* Flat model: every segment spans the whole 4 GB (base 0, limit 0xFFFFF with
     * G=1 → pages). access: P|DPL|S|type. flags 0xC0 = G=1 (4 KB granularity), D=1 (32-bit). */
    set_entry(0, 0, 0,       0x00, 0x00);   /* 0x00 null descriptor (mandatory)        */
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xC0);   /* 0x08 kernel code: present, ring0, exec, read */
    set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);   /* 0x10 kernel data: present, ring0, write      */
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xC0);   /* 0x18 user code:   present, ring3, exec, read */
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);   /* 0x20 user data:   present, ring3, write      */

    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;
    gdt_flush((uint32_t)&gp);
}
