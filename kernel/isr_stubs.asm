; naos — B5: IDT load + CPU-exception stubs (vectors 0..31).
; Each stub pushes a uniform frame (fake error code if the CPU didn't), then jumps
; to a common path that saves registers and calls the C handler. See docs/howto/05-idt.md.
bits 32

extern isr_handler
global idt_flush

; void idt_flush(uint32_t idt_ptr_addr);  — load the IDTR
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; Exceptions that do NOT push an error code: we push a fake 0 to keep one layout.
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0          ; fake error code
    push dword %1         ; interrupt number
    jmp isr_common
%endmacro

; Exceptions that DO push an error code: only push the interrupt number.
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1
    jmp isr_common
%endmacro

ISR_NOERR 0     ; #DE divide-by-zero
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8     ; #DF
ISR_NOERR 9
ISR_ERR   10    ; #TS
ISR_ERR   11    ; #NP
ISR_ERR   12    ; #SS
ISR_ERR   13    ; #GP
ISR_ERR   14    ; #PF
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17    ; #AC
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21    ; #CP
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30    ; #SX
ISR_NOERR 31

; Common path: save state, switch to kernel data segment, call C, restore, iret.
isr_common:
    pusha                 ; edi,esi,ebp,esp,ebx,edx,ecx,eax
    mov ax, ds
    push eax              ; save the data segment
    mov ax, 0x10          ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp              ; arg: struct registers*
    call isr_handler
    add esp, 4
    pop eax               ; restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8            ; drop int_no + err_code
    sti
    iret
