; naos — B2/B3 : en-tête Multiboot + stub ASM -> kmain (C)
; -----------------------------------------------------------------------------
; À partir de B2, on ne fait plus le boot sector à la main (cf. B1, conservé dans
; boot/boot.asm.b1). On délègue le démarrage à GRUB via la spec Multiboot 1 :
; GRUB nous charge en mode protégé 32 bits (A20 + GDT + PE déjà faits), à 1 Mo.
;
; Ce fichier fournit trois choses :
;   1. l'EN-TÊTE MULTIBOOT, que GRUB cherche dans les 8 premiers Ko du binaire ;
;   2. une PILE (la spec Multiboot ne garantit pas d'esp utilisable) ;
;   3. le point d'entrée _start, qui appelle kmain() (notre code C).
;
; Assemblé en ELF32 (-f elf32), lié par linker.ld. Voir docs/HOWTO.md §2.

bits 32

; --- en-tête Multiboot 1 ---
MB_ALIGN    equ 1 << 0                 ; modules alignés sur des pages
MB_MEMINFO  equ 1 << 1                 ; fournir la carte mémoire (memmap)
MB_FLAGS    equ MB_ALIGN | MB_MEMINFO
MB_MAGIC    equ 0x1BADB002             ; nombre magique reconnu par GRUB
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS) ; magic + flags + checksum doit faire 0

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

; --- pile : 16 Kio dans la BSS (non initialisée) ---
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; --- point d'entrée ---
section .text
global _start
extern kmain
_start:
    mov esp, stack_top                 ; installer la pile (croît vers le bas)
    call kmain                         ; -> C ; ne devrait pas revenir
.hang:
    cli
    hlt                                ; si kmain revient : arrêt définitif
    jmp .hang
