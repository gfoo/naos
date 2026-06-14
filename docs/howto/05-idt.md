[← HOWTO contents](../HOWTO.md)

## Part 5 — B5: IDT and CPU exceptions

A GDT (B4) tells the CPU about *segments*; an **IDT** (Interrupt Descriptor Table) tells it
where to jump when something *interrupts* normal flow — a CPU **exception** (divide-by-zero,
page fault…) or, later, a hardware **IRQ** (B6). B5 builds the IDT, wires the 32 CPU-exception
vectors to assembly **stubs** that funnel into a single C handler, and proves it by triggering a
divide-by-zero.

> **Files:** `include/idt.h`, `include/isr.h`, `kernel/idt.c`, `kernel/isr_stubs.asm`,
> `kernel/isr.c`. Build/run: `make run-b5`.

**In this part:**
- 5.1 — What the IDT is, and why
- 5.2 — A gate descriptor
- 5.3 — The ISR stubs: a uniform stack frame
- 5.4 — The common path and the C handler
- 5.5 — Verify (divide-by-zero)

**Key terms (quick reference):**

- **IDT** — 256 entries; index = interrupt vector (0–31 CPU exceptions, 32+ free for IRQs/syscalls).
- **Gate descriptor** — an IDT entry: handler address + code selector + type (we use *32-bit interrupt gate*).
- **ISR** (*Interrupt Service Routine*) — the handler that runs for a vector.
- **Stub** — the tiny asm entry point per vector; it normalizes state then jumps to common code.
- **Error code** — some exceptions push an extra word; we push a fake one for the rest to keep one layout.

---

### 5.1 — What the IDT is, and why

When the CPU takes an interrupt (vector *n*), it looks up entry *n* of the IDT and jumps to the
handler address stored there. Without a valid IDT, any interrupt → fault → triple fault. So
before we can react to *anything* (exceptions now, keyboard/timer in B6), we must install one.
The IDT has 256 entries; vectors **0–31 are CPU exceptions** (fixed by Intel), 32+ are free
(we'll use them for hardware IRQs in B6).

`idt_init()` is the same shape as `gdt_init()`: fill a static `idt[256]`, point an `idt_ptr`
(size − 1, base) at it, and load it with `lidt`. Entries we don't fill stay zero (present bit
0) — an unexpected vector then faults rather than jumping into garbage.

### 5.2 — A gate descriptor

```c
struct idt_entry {
    uint16_t base_low;    /* handler address [0:15]      */
    uint16_t selector;    /* code segment selector (0x08) */
    uint8_t  zero;        /* always 0                     */
    uint8_t  flags;       /* P, DPL, gate type            */
    uint16_t base_high;   /* handler address [16:31]      */
} __attribute__((packed));
```

`flags = 0x8E` = present, DPL 0, type `0xE` (**32-bit interrupt gate** — entering it clears IF,
masking further interrupts while we handle this one). `selector = 0x08` is our kernel code
segment from the GDT (B4): the handler runs in ring 0.

### 5.3 — The ISR stubs: a uniform stack frame

C can't be an interrupt entry point (it can't `iret`, and it expects a set-up environment), so
each vector gets a tiny asm **stub** in `kernel/isr_stubs.asm`. The wrinkle: some exceptions
push an **error code**, others don't. To give the C handler *one* layout, the no-error stubs
push a fake 0:

```nasm
%macro ISR_NOERR 1
isr%1:
    cli
    push dword 0          ; fake error code
    push dword %1         ; interrupt number
    jmp isr_common
%endmacro
%macro ISR_ERR 1          ; CPU already pushed the error code
isr%1:
    cli
    push dword %1
    jmp isr_common
%endmacro
```

Then `ISR_NOERR`/`ISR_ERR` are invoked for vectors 0–31 (error-code ones: 8, 10–14, 17, 21, 30).

### 5.4 — The common path and the C handler

`isr_common` saves all registers, switches to the kernel data segment, and calls C with a
pointer to the saved state, then restores and `iret`s:

```nasm
isr_common:
    pusha
    mov ax, ds
    push eax              ; save data segment
    mov ax, 0x10          ; kernel data
    mov ds, ax  / es / fs / gs
    push esp              ; arg: struct registers*
    call isr_handler
    add esp, 4
    pop eax               ; restore data segment ...
    popa
    add esp, 8            ; drop int_no + err_code
    sti
    iret
```

The push order *is* the `struct registers` in `include/isr.h` (read low→high: `ds`, the `pusha`
block, `int_no`/`err_code`, then the CPU's `eip`/`cs`/`eflags`). `isr_handler()` (in
`kernel/isr.c`) prints the vector number and exception name. A CPU fault re-runs the faulting
instruction on `iret`, so for B5 an exception is **fatal**: the handler prints and halts (B6's
IRQ handler will instead return).

### 5.5 — Verify (divide-by-zero)

`kmain` triggers a `#DE` after `idt_init()`:

```c
volatile int zero = 0;
volatile int x = 42 / zero;   /* #DE → isr0 → isr_handler() */
```

(`volatile` stops the compiler from folding the division at compile time, so a real `idiv`
runs and faults.)

```bash
make run-b5
make run-b5 QMP=1     # (other terminal) python3 tools/qemu-shot.py
```

Expected: `GDT... ok`, `IDT... ok`, then in red **`[CPU EXCEPTION] #0  Divide-by-zero / Kernel
halted.`** Seeing the handler's message means the divide-by-zero was caught and dispatched.
**Criterion met: divide-by-zero → handler.**

> **Going further → B6.** The IDT also routes **hardware IRQs**. B6 remaps the PIC (so IRQs land
> at vectors 32+ instead of clashing with exceptions), then wires the **timer** (PIT) and the
> **keyboard** (PS/2) — handlers that *return* (`iret` + end-of-interrupt), unlike fatal exceptions.
