/* naos — B2 : point d'entrée C du kernel.
 * But : prouver que kmain() s'exécute, chargé par GRUB via Multiboot. On écrit
 * directement dans la mémoire vidéo VGA (0xB8000) — le driver écran arrive en B3.
 * Appelé par boot/boot.asm (_start). Voir docs/HOWTO.md §2. */

void kmain(void)
{
    const char *msg = "naos B2: kmain() running, loaded by GRUB via Multiboot.";
    volatile unsigned short *vga = (unsigned short *)0xB8000;

    for (int i = 0; msg[i]; i++)
        vga[i] = (unsigned short)(unsigned char)msg[i] | (0x0A << 8); /* vert clair */

    for (;;)
        __asm__ volatile ("hlt");
}
