; naos — B2/B3 : Multiboot header + ASM stub -> kmain (C)
; -----------------------------------------------------------------------------
; Starting from B2, we no longer build the boot sector by hand (cf. B1, kept in
; boot/boot.asm.b1). We delegate startup to GRUB via the Multiboot 1 spec:
; GRUB loads us in 32-bit protected mode (A20 + GDT + PE already done), at 1 MB.
;
; This file provides three things:
;   1. the MULTIBOOT HEADER, which GRUB looks for in the first 8 KB of the binary;
;   2. a STACK (the Multiboot spec does not guarantee a usable esp);
;   3. the entry point _start, which calls kmain() (our C code).
;
; Assembled as ELF32 (-f elf32), linked by linker.ld. See docs/howto/02-multiboot.md.

bits 32

; --- Multiboot 1 header ---
MB_ALIGN    equ 1 << 0                 ; modules aligned on pages
MB_MEMINFO  equ 1 << 1                 ; provide the memory map (memmap)
MB_FLAGS    equ MB_ALIGN | MB_MEMINFO
MB_MAGIC    equ 0x1BADB002             ; magic number recognized by GRUB
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS) ; magic + flags + checksum must sum to 0

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

; --- stack: 16 KiB in the BSS (uninitialized) ---
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; --- entry point ---
section .text
global _start
extern kmain
_start:
    mov esp, stack_top                 ; install the stack (grows downward)
    push ebx                           ; Multiboot info pointer (GRUB/QEMU set ebx)
    push eax                           ; Multiboot magic (eax = 0x2BADB002)
    call kmain                         ; kmain(uint32_t magic, uint32_t mb_info)
.hang:
    cli
    hlt                                ; if kmain returns: halt for good
    jmp .hang
