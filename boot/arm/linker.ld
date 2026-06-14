/* naos — B0 ARM memory layout (AArch64).
 * The RAM of the QEMU 'virt' machine starts at 0x40000000; with -kernel, QEMU
 * loads the ELF according to its addresses. We place the kernel at 0x40080000
 * (QEMU virt convention) and reserve the stack at the end of the image. */
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text   : { KEEP(*(.text.boot)) *(.text) }   /* _start first */
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }

    . = ALIGN(16);
    . = . + 0x4000;        /* 16 KiB of stack */
    stack_top = .;
}
