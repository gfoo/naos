; naos — B4: load the GDT and reload the segment registers.
; void gdt_flush(uint32_t gdt_ptr_addr);   (cdecl: argument at [esp+4])
; Reloading CS requires a far jump, which C cannot express — hence this stub.
bits 32

global gdt_flush
gdt_flush:
    mov eax, [esp + 4]      ; address of the gdt_ptr struct
    lgdt [eax]              ; load GDTR

    mov ax, 0x10           ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush        ; far jump: reload CS with the kernel code selector
.flush:
    ret
