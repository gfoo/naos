/* naos — B0 (ARM / AArch64): boot stub.
 * -----------------------------------------------------------------------------
 * Contrast with x86 (boot/boot.asm.b0): here NO BIOS, NO boot sector,
 * NO real mode. QEMU (-machine virt -kernel) loads this ELF and starts the CPU
 * directly in 64-bit, at _start. We just set up a stack and jump into C.
 * GNU as assembler (ARM syntax), not NASM. See docs/howto/00-setup.md §0.7. */

.section ".text.boot"
.global _start
_start:
    ldr     x0, =stack_top      // address of the stack top (literal pool)
    mov     sp, x0              // install the stack (ARM: dedicated SP)
    bl      kmain               // -> our C code
1:  wfe                         // if kmain returns: go to sleep (Wait For Event)
    b       1b
