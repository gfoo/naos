/* naos — layout mémoire B0 ARM (AArch64).
 * La RAM de la machine QEMU 'virt' commence à 0x40000000 ; avec -kernel, QEMU
 * charge l'ELF selon ses adresses. On place le kernel à 0x40080000 (convention
 * QEMU virt) et on réserve la pile en fin d'image. */
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text   : { KEEP(*(.text.boot)) *(.text) }   /* _start en premier */
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }

    . = ALIGN(16);
    . = . + 0x4000;        /* 16 Kio de pile */
    stack_top = .;
}
