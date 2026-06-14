/* naos — layout mémoire du kernel (B2+).
 * Le kernel est chargé à 1 Mo (0x100000) : au-dessus de la zone basse réservée
 * (IVT, BIOS, mémoire vidéo VGA à 0xB8000). C'est l'adresse conventionnelle des
 * kernels Multiboot. L'en-tête Multiboot doit figurer dans les 8 premiers Ko du
 * fichier -> on place .multiboot tout en tête de .text. */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)      /* en-tête Multiboot en premier */
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }
}
