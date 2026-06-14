/* naos — B2: C entry point of the kernel.
 * Goal: prove that kmain() executes, loaded by GRUB via Multiboot. We write
 * directly into VGA video memory (0xB8000) — the screen driver arrives in B3.
 * Called by boot/boot.asm (_start). See docs/howto/02-multiboot.md. */

void kmain(void)
{
    const char *msg = "naos B2: kmain() running, loaded by GRUB via Multiboot.";
    volatile unsigned short *vga = (unsigned short *)0xB8000;

    for (int i = 0; msg[i]; i++)
        vga[i] = (unsigned short)(unsigned char)msg[i] | (0x0A << 8); /* light green */

    for (;;)
        __asm__ volatile ("hlt");
}
