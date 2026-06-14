[← HOWTO contents](../HOWTO.md)

## Part 1 — B1: from real mode to 32-bit protected mode

B0 left us in 16-bit **real mode** and delegated everything to the firmware (`int 0x10`). B1 makes
the big leap: switching the CPU into **32-bit protected mode**, the environment where the entire
rest of the OS will run. Four mandatory steps — A20, GDT, PE bit, far jump — each explained then
assembled step by step. At the end, we display **without the BIOS**, by writing straight into video
memory.

> **Where this code lives.** The B1 boot sector is kept in **`boot/boot.asm.b1`** (starting at
> B2, `boot/boot.asm` is reused for the Multiboot header). We launch it from a clone with
> **`make run-b1`** (512-byte flat binary, a pipeline distinct from the B2+ GRUB kernel).

**In this part:**
- 1.1 — Real mode vs protected mode: what really changes
- 1.2 — Overview: the four steps
- 1.3 — A20: the forgotten address line
- 1.4 — The GDT: from "segment = address" to "segment = selector"
- 1.5 — The switch: `CR0` PE bit + far jump
- 1.6 — Displaying without the BIOS: writing to `0xB8000`
- 1.7 — The complete boot sector, built step by step
- 1.8 — Verify in QEMU

**Key terms (quick reference):**

- **Protected mode** — the 32-bit x86 mode: 32-bit addresses (4 GB), memory protection, the foundation of every modern OS.
- **A20** — the 21st address line (bit 20); disabled at power-on for 8086 compatibility.
- **GDT** (*Global Descriptor Table*) — the table describing segments; in protected mode, a segment register holds a **selector** that indexes into it.
- **Segment descriptor** — an 8-byte entry: base, limit, access rights, flags.
- **Selector** — an index (×8) into the GDT, loaded into `CS`/`DS`/… (`0x08` = 1st usable entry).
- **`CR0`** — the control register; its bit 0 (**PE**, *Protection Enable*) turns on protected mode.
- **Far jump** — a `selector:offset` jump that reloads `CS` *and* flushes the prefetch pipeline.
- **Triple fault** — three cascading faults with no handler → the CPU resets (reboot loop).

---

### 1.1 — Real mode vs protected mode: what really changes

The x86 boots in *real mode* to stay compatible with the 8086 of 1978. It is a 16-bit world,
without protection, where an address is computed as `segment × 16 + offset` (hence the historical
~1 MB ceiling). That is where we did B0. *Protected mode* is the other world:

| | Real mode (B0) | Protected mode (B1+) |
|---|---|---|
| Width | 16-bit | **32-bit** (`eax`… registers, 32-bit offsets) |
| Max address | ~1 MB | **4 GB** |
| Segments | `CS`=address÷16 | `CS`=**selector** into a descriptor (base+limit+rights) |
| BIOS services (`int 0x10`…) | **available** | **gone** (the BIOS is 16-bit code) |
| Protection | none | privilege levels (ring 0–3), segment limits |

> **Why not stay in 16-bit and jump straight to the kernel?** We *can* jump to code from real
> mode — B0 does exactly that. But that code would then be **16-bit itself**, locked under ~1 MB,
> with no protection and no paging. And above all: our kernel is compiled by `i686-elf-gcc` into
> **32-bit machine code**, which **does not run** in real mode (different operand sizes and
> addressing). Switching is therefore not a luxury, it is an *execution prerequisite* of the
> kernel. That is why **GRUB switches to protected mode before calling `kmain`** (B2): the jump
> into the kernel happens **already in 32-bit**. B1 does this switch by hand to understand what
> GRUB will spare us.

> **The surprising consequence.** When we switch, we **lose `int 0x10`**: the BIOS is 16-bit code,
> inaccessible in 32-bit. To display, we must now write into video memory *ourselves* (`0xB8000`,
> see 0.4 and 1.6). That is precisely what makes the switch *observable*: if a 32-bit message
> appears, it means we succeeded.

### 1.2 — Overview: the four steps

Entering protected mode is exactly four operations, in this order:

```
16-bit real mode
   │  1. cli                  ← disable interrupts (no IDT yet)
   │  2. enable A20           ← unlock addressing beyond 1 MB
   │  3. lgdt [gdt]           ← load a segment descriptor table
   │  4. CR0.PE = 1           ← THE SWITCH: the CPU is now in protected mode
   ▼  5. jmp 0x08:next        ← far jump: reload CS, flush the prefetch
32-bit protected mode  (first "bits 32" instruction)
```

> **Why `cli` first.** In protected mode, interrupts go through an **IDT** that we don't have yet
> (that will be B5). If a hardware interrupt fired during or after the switch, the CPU would look
> for a non-existent handler → fault → fault → **triple fault** → reset. So we disable interrupts
> (`cli`) and won't re-enable them until B5.

> **Who defines these steps?** Two families. The GDT/`lgdt` (3), the `CR0.PE` bit (4) and the far
> jump (5) — as well as the `cli` (1) — form the **canonical sequence defined by Intel**: the x86
> architecture, *Intel SDM* Vol. 3A §9.9 "Switching to Protected Mode" (protected mode introduced
> with the 80286 in 1982). The CPU reads these structures in hardware. **A20 (2) is the odd one
> out**: it does *not* exist in the CPU architecture. It is a historical graft of the **PC
> platform** — a need created by **IBM** (PC AT, 1984, to preserve the 8086 *wrap-around*),
> enabled depending on the machine via the 8042 keyboard controller, port `0x92` ("Fast A20") or
> `int 0x15`. Hence the lack of a single method: 3/4/5 are clean because Intel standardizes them,
> A20 is dirty because it is IBM/chipset archaeology.

### 1.3 — A20: the forgotten address line

On the 8086, the `segment × 16 + offset` computation could exceed `0xFFFFF` (1 MB); this would
"fold" back to 0 (*wrap-around*). Programs depended on it. When the 80286 added a 21st address
line (A20, bit 20), IBM **disabled it at startup** to preserve that fold. As a result: as long as
A20 is off, an address like `0x100000` (1 MB) folds back onto `0x0` — disastrous as soon as you
address beyond 1 MB (our kernel will sit at 1 MB in B2).

So we enable A20. Three methods exist (keyboard port `0x64`, BIOS `int 0x15`, *Fast A20*); we take
the simplest, **Fast A20** via port `0x92`:

```nasm
    in  al, 0x92        ; read the system control register
    or  al, 0000_0010b  ; bit 1 = A20 enable
    out 0x92, al        ; write it back
```

> **Why Fast A20 here.** It is two instructions, and QEMU (like every modern chipset) supports it.
> The keyboard-controller methods are historically more "universal" but far longer (status waits).
> For a teaching project under QEMU, `0x92` is enough.

### 1.4 — The GDT: segments, selectors, descriptors

This is the conceptual heart of B1. In real mode, `CS = 0x07C0` *meant* "base = `0x7C00`". In
protected mode, a segment register no longer holds an address but a **selector**: an index into a
table, the **GDT** (*Global Descriptor Table*). Each GDT entry is an 8-byte **descriptor**
describing a segment: its **base**, its **limit**, and its **rights**.

We load a "**flat**" GDT: a code segment and a data segment both covering **all** of memory (base
0, limit 4 GB). In other words, we *neutralize* segmentation — memory partitioning will come later
via **paging** (B8), not via segments.

The minimal GDT has three entries:

| Index | Selector | Role |
|---|---|---|
| 0 | `0x00` | **null descriptor** — mandatory; loading `0x00` is a deliberate error |
| 1 | `0x08` | **code** segment (executable, base 0, limit 4 GB) |
| 2 | `0x10` | **data** segment (writable, base 0, limit 4 GB) |

> **Why `0x08` and `0x10`?** A selector is not the raw index: it is `index × 8` (each descriptor is
> 8 bytes), with the low 3 bits used for the privilege level (RPL) and the table choice. Entry 1 →
> `1×8 = 0x08`, entry 2 → `2×8 = 0x10`.

An 8-byte descriptor has a scattered, historical layout (the base and limit fields are split into
pieces to remain 80286-compatible). Here is the flat **code** descriptor, byte by byte — it is
`1001_1010b` / `1100_1111b` that you need to understand:

```nasm
gdt_code:
    dw 0xFFFF        ; limit [0:15]       ┐ limit = 0xFFFFF (20 bits)
    dw 0x0000        ; base  [0:15]       │
    db 0x00          ; base  [16:23]      ├─ base = 0
    db 1001_1010b    ; access byte        │   P=1 DPL=00 S=1 | type=1010 (code, exec, readable)
    db 1100_1111b    ; flags + limit[16:19]  G=1 D=1 0 0 | 1111
    db 0x00          ; base  [24:31]      ┘
```

- **Access byte `1001_1010`**: `P`=present, `DPL`=00 (ring 0), `S`=1 (code/data segment), then the
  type `1010` = *code, executable, readable*. (The **data** segment has `1001_0010`: type `0010` =
  *data, writable*.)
- **Flags `1100`**: `G`=1 → the limit is counted in **4 KB pages** (`0xFFFFF × 4 KB` = 4 GB), and
  `D`=1 → **32-bit** operands/addresses by default. The low 4 bits (`1111`) = limit[16:19].

We tell the CPU where to find this table with a **pseudo-descriptor** (size − 1, then the linear
address) loaded by `lgdt`:

```nasm
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; limit: the size of the GDT minus 1
    dd gdt_start                 ; linear address of the GDT

    ...
    lgdt [gdt_descriptor]
```

### 1.5 — The switch: `CR0` PE bit + far jump

Everything is ready; two final instructions make the leap between worlds:

```nasm
    mov eax, cr0
    or  eax, 1               ; bit 0 = PE (Protection Enable)
    mov cr0, eax             ; <- AT THIS INSTANT, the CPU is in protected mode
    jmp CODE_SEG:protected_mode   ; far jump (CODE_SEG = 0x08)
```

> **Why the far jump is indispensable.** Setting `PE` is not enough: `CS` still holds the old
> 16-bit value, and the CPU has already **prefetched** instructions decoded in 16-bit. A **far
> jump** (`jmp selector:offset`) does two things at once: it reloads `CS` with the code selector
> `0x08` (hence the correct descriptor), and it **flushes the pipeline** — the next instruction is
> re-decoded *in 32-bit*. It is the first "truly" 32-bit instruction, marked `bits 32` in the
> source.

Right after, we reload all the **data segments** with the data selector `0x10` (in real mode they
held 0; they must now point to the data descriptor), and we set up a 32-bit stack:

```nasm
bits 32
protected_mode:
    mov ax, DATA_SEG     ; 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000     ; stack in a free zone
```

### 1.6 — Displaying without the BIOS: writing to `0xB8000`

`int 0x10` is gone. But the text video memory is still there — it is RAM mapped at **`0xB8000`**
(see 0.4): 80×25 **2-byte cells** (low byte = CP437 character, high byte = color **attribute**: 4
bits foreground, 4 bits background). Displaying a green character is just laying down two bytes:

```nasm
    mov esi, msg_pm
    mov edi, 0xB8000
    mov ah, 0x0A         ; attribute: light green (0x0A) on black background
.pm_print:
    lodsb                ; AL = [esi], esi++
    test al, al
    jz .hang
    mov [edi], al        ; character
    mov [edi + 1], ah    ; color
    add edi, 2           ; next cell
    jmp .pm_print
```

> **Why the message lands in the top-left corner.** `0xB8000` = **cell (0,0)** = the upper-left
> corner. So we overwrite the first line (the SeaBIOS version). That is intentional: seeing the
> green text *there* proves that we are writing video memory ourselves, in 32-bit.

### 1.7 — The complete boot sector, built step by step

As in 0.3: we assemble the bricks in order, but here we **test at the end** (the switch does not
break down into observable sub-steps — either we reach 32-bit, or it's a triple fault). The final
skeleton reuses B0 (segments, stack, real-mode message via `int 0x10`) then chains sections 1.3 →
1.6. The complete file is `boot/boot.asm.b1`; its backbone:

```nasm
bits 16
org  0x7C00
start:
    cli / xor ax,ax / mov ds,es,ss / mov sp,0x7C00 / sti   ; (B0)
    mov si, msg_rm  ... int 0x10  ...                       ; real-mode message (B0)
    cli                                                     ; 1.2
    in al,0x92 / or al,2 / out 0x92,al                      ; 1.3  A20
    lgdt [gdt_descriptor]                                   ; 1.4  GDT
    mov eax,cr0 / or eax,1 / mov cr0,eax                    ; 1.5  PE
    jmp CODE_SEG:protected_mode                             ; 1.5  far jump
bits 32
protected_mode:
    mov ax,DATA_SEG / mov ds.. / mov esp,0x90000            ; 1.5  segments + stack
    ... write msg_pm to 0xB8000 ...                          ; 1.6
.hang: hlt / jmp .hang
msg_rm db "naos B1: real mode OK, switching to 32-bit...",13,10,0
msg_pm db "naos B1: 32-bit protected mode!",0
gdt_start: ... gdt_code ... gdt_data ... gdt_end            ; 1.4
gdt_descriptor: dw gdt_end-gdt_start-1 / dd gdt_start
CODE_SEG equ gdt_code-gdt_start    ; 0x08
DATA_SEG equ gdt_data-gdt_start    ; 0x10
times 510-($-$$) db 0
dw 0xAA55
```

> **Still a boot sector.** B1 remains a 512-byte flat binary loaded at `0x7C00` with the `0xAA55`
> signature: it is *us* doing the switch, not GRUB (that's B2). Hence `org 0x7C00` and B0's
> `times … db 0` / `dw 0xAA55`.

### 1.8 — Verify in QEMU

```bash
make run-b1                   # by eye: real-mode message, then green 32-bit message
make run-b1 QMP=1             # (another terminal) python3 tools/qemu-shot.py
```

Expected: the real-mode line "naos B1: real mode OK, switching to 32-bit..." in the SeaBIOS stream,
**then** "naos B1: 32-bit protected mode!" in **green, top-left**. This second message could only
have been written by 32-bit code (the BIOS no longer exists): **the switch succeeded**. B1
criterion met.

> **If it reboots in a loop (triple fault).** Suspect nº 1 is the GDT (a badly encoded descriptor)
> or the far jump (a wrong selector). In real mode there is no error message: bring out `make
> debug` (GDB) or, to see *why* the CPU resets, Bochs (see `DESIGN-LOG.md`, C2).

---

