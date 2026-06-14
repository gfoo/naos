/* naos — kernel memory layout (B2+).
 * The kernel is loaded at 1 MB (0x100000): above the reserved low region
 * (IVT, BIOS, VGA video memory at 0xB8000). This is the conventional address of
 * Multiboot kernels. The Multiboot header must appear within the first 8 KB of the
 * file -> we place .multiboot right at the start of .text. */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)      /* Multiboot header first */
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }
}
