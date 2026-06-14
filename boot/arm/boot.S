/* naos — B0 (ARM / AArch64) : stub d'amorçage.
 * -----------------------------------------------------------------------------
 * Contraste avec le x86 (boot/boot.asm.b0) : ici PAS de BIOS, PAS de boot sector,
 * PAS de real mode. QEMU (-machine virt -kernel) charge cet ELF et démarre le CPU
 * directement en 64 bits, à _start. On pose juste une pile et on saute en C.
 * Assembleur GNU as (syntaxe ARM), pas NASM. Voir docs/howto/00-setup.md §0.7. */

.section ".text.boot"
.global _start
_start:
    ldr     x0, =stack_top      // adresse du sommet de pile (literal pool)
    mov     sp, x0              // installer la pile (ARM : SP dédié)
    bl      kmain               // -> notre code C
1:  wfe                         // si kmain revient : on s'endort (Wait For Event)
    b       1b
