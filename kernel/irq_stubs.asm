; naos — B6: hardware IRQ stubs (vectors 32..47).
; Like the exception stubs, but they funnel into irq_handler and RETURN (iret +
; end-of-interrupt) instead of panicking. See docs/howto/06-keyboard-timer.md.
bits 32

extern irq_handler

%macro IRQ 2            ; %1 = IRQ number (0..15), %2 = IDT vector (32..47)
global irq%1
irq%1:
    cli
    push dword 0        ; no error code for IRQs (keep the registers layout)
    push dword %2       ; interrupt number
    jmp irq_common
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

irq_common:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp            ; arg: struct registers*
    call irq_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8          ; drop int_no + (fake) err_code
    sti
    iret
