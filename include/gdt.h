/* naos — B4: a kernel-owned GDT (Global Descriptor Table).
 * GRUB left us running on its own GDT; we install ours (a flat GDT with 5 entries:
 * null, kernel code/data, user code/data) to control segmentation ourselves and to
 * prepare for ring 3 later. See docs/howto/04-gdt.md. */
#ifndef NAOS_GDT_H
#define NAOS_GDT_H

void gdt_init(void);   /* build the GDT and load it (lgdt + segment reload) */

#endif /* NAOS_GDT_H */
