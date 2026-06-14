[← HOWTO contents](../HOWTO.md)

## Appendices

### Appendix A — x86 assembly refresher (real mode, 16-bit)

Read before/during B1.

**A.1 What assembly is.** Assembly is the **human-readable** representation of machine
code: each **mnemonic** (`mov`, `jmp`, `int`…) maps to one (or a few) machine
instruction(s) the CPU executes directly. Unlike C, there is **no semantic
translation**: what you write is, up to an encoding, what the CPU does. You use it
where C *cannot* go (real mode, `lgdt`, `iret`, mode switching).
Guiding principle: **you manage everything by hand** — no typed variables, no automatic
call stack, no checking. The CPU does *exactly* what you say, errors
included.

**A.2 The registers (16-bit).** A register = an ultra-fast memory cell *inside* the CPU.

| Register | Name | Conventional usage |
|---|---|---|
| `AX` | Accumulator | computations, return value ; `AH`/`AL` = high/low byte |
| `BX` | Base | addressing base ; `BH`/`BL` |
| `CX` | Counter | loop counter (`loop`, `rep`) ; `CH`/`CL` |
| `DX` | Data | data, I/O port ; `DH`/`DL` |
| `SI` | Source Index | source pointer (`lodsb`, `movsb`) |
| `DI` | Destination Index | destination pointer |
| `SP` | Stack Pointer | top of the stack |
| `BP` | Base Pointer | base of a stack frame |
| `IP` | Instruction Pointer | address of the next instruction (not directly modifiable) |
| `CS DS ES SS` | Segments | Code / Data / Extra / Stack (see A.3) |
| `FLAGS` | Flags | results of comparisons (ZF, CF, SF…) |

`AX` is 16-bit ; its halves `AH` (bits 8-15) and `AL` (bits 0-7) are addressable
separately. Writing to `AL` does not touch `AH`. In protected mode (B1+), these registers
extend to 32 bits: `EAX`, of which `AX` is the low half.

**A.3 Real mode and `segment:offset` addressing.** In real mode, a physical address is
computed over 20 bits:

```
physical address = (segment × 16) + offset
```

Example: `DS = 0x07C0`, `offset = 0` → `0x7C00` ; same address as `DS = 0`, `offset = 0x7C00`.
So there are several ways to write the same address. In naos we pick the simplest one:
**all segments at 0**, and we reason in absolute offsets from 0 (which is why
`boot.asm` sets `DS = ES = SS = 0`). Why `0x7C00`? IBM convention from 1981: the BIOS always
loads the boot sector there.

**A.4 Intel syntax (NASM): `instruction destination, source`** (destination on the left, like
`dest = src` in C).

```nasm
mov ax, 5      ; ax <- 5         (immediate)
mov ds, ax     ; ds <- ax        (register)
mov al, [si]   ; al <- byte at address SI   (brackets = dereference)
mov [di], al   ; byte at address DI <- al
```

Key rules: **brackets `[ ]`** = "contents of memory at this address" (like `*ptr`) ;
without brackets = the value itself (address, number, register) ; the **size** is inferred
from the registers (`al` = 1 byte, `ax` = 2 bytes), otherwise you specify it (`byte`, `word`).

**A.5 The instructions we run into in naos.**

| Instruction | Effect |
|---|---|
| `mov d, s` | copies `s` into `d` |
| `xor a, a` | sets `a` to 0 (xor of a value with itself) — shorter than `mov a,0` |
| `test a, a` | computes `a AND a` without storing, just to set the flags (ZF=1 if `a`==0) |
| `cmp a, b` | computes `a - b` without storing, sets the flags |
| `jmp lbl` | unconditional jump |
| `jz / jnz lbl` | jump if ZF=1 / ZF=0 (zero / non-zero) |
| `lodsb` | `AL <- [DS:SI]`, then `SI++` (loads a byte and advances) |
| `int n` | triggers software interrupt n (call to a BIOS service) |
| `hlt` | halts the CPU until the next interrupt |
| `cli / sti` | disables / re-enables hardware interrupts |
| `push / pop` | pushes / pops a value (via `SP`) |

**A.6 The stack.** `SP` points at the top. x86 quirk: it **grows downward**
(`push` *decrements* `SP`). Hence `mov sp, 0x7C00`: the stack grows *below* the code, without
overwriting it.

**A.7 NASM directives (≠ instructions).** A directive addresses the assembler, not the CPU:

| Directive | Role |
|---|---|
| `bits 16` | "assemble as 16-bit code" |
| `org 0x7C00` | "this code will live at 0x7C00" → base for address computations |
| `db`, `dw`, `dd` | lays down raw bytes / words (2 bytes) / double-words (4 bytes) |
| `times N x` | repeats `x` N times (padding) |
| `label:` | symbolic name of an address (global) ; `.label` = local to the last global |
| `$` / `$$` | current address / start of section (`$-$$` = bytes written) |

> **Global vs local labels.** A label without a dot (`start`) is global ; a label with a dot
> (`.print`) is **local**, attached to the last global above it (so `start.print`). This
> avoids collisions: you can reuse `.loop`/`.done` across several routines. Mental rule:
> **global = landmarks (routines, entry point) ; local (`.`) = internal jump targets.**

### Appendix B — `boot/boot.asm` line by line

```nasm
bits 16
```
**Directive.** We target 16-bit real mode, because the CPU boots in this mode. Without it, NASM
would encode 32-bit instructions, misinterpreted at boot.

```nasm
org  0x7C00
```
**Directive.** We declare that the code will run at `0x7C00`. NASM then computes all
label addresses (like `msg`) from this base. Classic oversight: without `org`,
`mov si, msg` would point off-target and we'd print garbage.

```nasm
start:
```
**Label.** Symbolic entry point, for readability. The CPU itself simply starts at
`0x7C00` (first byte of the file).

```nasm
    cli
```
We **disable hardware interrupts**: while reconfiguring segments and the stack,
we don't want an interrupt to fire on an inconsistent stack.

```nasm
    xor ax, ax
```
`AX <- 0`. Universal idiom: `xor reg, reg` zeroes out in 2 bytes (shorter than
`mov ax, 0`). We prepare the value 0 for the segments.

```nasm
    mov ds, ax
    mov es, ax
    mov ss, ax
```
We set `DS`, `ES`, `SS` to 0. **Why via `AX`?** Segment registers don't accept
an immediate (`mov ds, 0` is illegal) ; you have to go through a general-purpose register.

```nasm
    mov sp, 0x7C00
```
Stack top at `0x7C00`. Since the stack grows downward, it occupies the area *below* the code
(`SS:SP` = `0x0000:0x7C00`).

```nasm
    sti
```
We **re-enable interrupts**: the config is done, and `int 0x10` needs them.

```nasm
    mov si, msg
```
`SI` points at the 1st byte of the string (source for `lodsb`). Thanks to `org`, `msg` is
the real address in memory.

```nasm
.print:
```
**Local** label (the `.` attaches it to `start`). Start of the print loop.

```nasm
    lodsb
```
`AL <- [DS:SI]` then `SI++`. One instruction that loads the current byte *and* advances the
pointer — designed for walking a string.

```nasm
    test al, al
    jz .hang
```
`test al, al` sets ZF according to `AL`. If `AL == 0` (end of string), `jz` jumps to `.hang`.
Principle: you test (`test`/`cmp`) *then* jump (`jz`/`jnz`), in two steps.

```nasm
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
```
BIOS video service. `int 0x10` reads `AH` for the function: `0x0E` = teletype (prints `AL`,
advances the cursor). `BH` = video page 0. We *consume* a firmware service (it will disappear
in protected mode).

```nasm
    jmp .print
```
Back to the top of the loop for the next character.

```nasm
.hang:
    hlt
    jmp .hang
```
`hlt` pauses the CPU until an interrupt ; if woken up, `jmp .hang` puts it back to sleep.
**Why?** Without it, the CPU would execute the following bytes (the `0x00` padding, interpreted
as `add [bx+si], al`) and eventually crash.

```nasm
msg db "naos B0: it boots!", 13, 10, 0
```
**Data.** `db` lays down the bytes of the string. `13, 10` = CR LF ; `0` = terminator read by
`test al, al`.

```nasm
times 510-($-$$) db 0
```
**Padding.** `$-$$` = bytes written so far. `510 - (that)` = number of zeros to reach
byte 510 → the signature lands exactly at offsets 510-511.

```nasm
dw 0xAA55
```
**Boot signature.** `dw` lays down 2 bytes. In little-endian, `0xAA55` is written `55 AA` →
byte 510 = `0x55`, byte 511 = `0xAA`. Without it, the BIOS declares the disk non-bootable.

**Principles to remember.**
1. **Everything is explicit**: segments, stack, end of string — nothing is automatic.
2. **Test then jump**: `test`/`cmp` set the flags, `jz`/`jnz` decide.
3. **Directive ≠ instruction**: `org`/`times`/`db` talk to NASM ; `mov`/`int`/`hlt` to the CPU.
4. **`org` is vital**: it aligns computed addresses with the real load address.
5. **Park the CPU** (`hlt; jmp $`) instead of letting it fall into the void.

### Appendix C — `int 0x10` in detail (BIOS services & interrupts)

Understanding `int 0x10` means understanding **all** the BIOS services (`int 0x13` disk,
`int 0x16` keyboard, `int 0x15` memory…): same mechanism.

**C.1 One door, many functions.** `int 0x10` doesn't print "by nature": it's
the **single entry point for the BIOS video services**, which can do dozens of
things. How does it know *which one* you want? It **looks at `AH`**:

| `AH` | Function | Arguments |
|---|---|---|
| `0x00` | change video mode | `AL` = mode |
| `0x02` | position the cursor | `BH` = page, `DH` = row, `DL` = column |
| `0x06` | scroll up | `AL`, `CX`, `DX`, `BH`… |
| **`0x0E`** | **teletype (print 1 character)** | **`AL` = character, `BH` = page, `BL` = color** |
| `0x13` | print a string | `ES:BP` = string, `CX` = length… |

**C.2 Who decides that `0x0E` = teletype?** **IBM**, in the 1981 BIOS
specification. It's a **published, frozen convention**, cloned by every BIOS (SeaBIOS included).
Canonical reference: *Ralf Brown's Interrupt List*. It's neither the hardware nor chance:
it's an **API contract**, a number agreed upon in advance — like `0x7C00` or `0xAA55`.

**C.3 The registers = the parameters.** In real mode, no passing arguments on the stack
like in C: **the arguments travel through the registers**, and the BIOS routine reads them
there directly. Hence the reading grid:

- `int 0x10` = **which service** (video) ;
- `AH` = **which sub-function** (the "method selector") ;
- `AL`, `BH`, `BL`… = **the arguments** of that sub-function.

So "it prints `AL`" because the spec of function `0x0E` *says* the character is
in `AL`. The BIOS, seeing `AH=0x0E`, goes to read `AL`.

**C.4 The same piece of code, in Python.** `int 0x10` ≈ a big function that *dispatches* on
`AH` and reads its arguments from "registers":

```python
def int_10h(regs):
    if regs.AH == 0x00:        # change video mode
        set_video_mode(regs.AL)
    elif regs.AH == 0x02:      # position the cursor
        set_cursor(page=regs.BH, row=regs.DH, col=regs.DL)
    elif regs.AH == 0x0E:      # teletype
        put_char(chr(regs.AL), page=regs.BH)   # <-- reads AL and BH
        advance_cursor()
    # ... other sub-functions ...
```

And the assembly:

```nasm
mov ah, 0x0E
mov al, 'X'
int 0x10
```

… is **exactly** equivalent to:

```python
regs.AH = 0x0E       # I want the "teletype" function
regs.AL = ord('X')   # the character
int_10h(regs)        # call
```

The `mov`s = "fill in the argument slots" ; `int 0x10` = "call the function".

**C.5 How `int n` actually reaches the BIOS: the IVT.** In real mode, there exists at
address `0x0000` an **interrupt vector table (IVT)**: 256 entries, each a
`segment:offset` pointer to a routine. `int 0x10` does: *"go read entry no. `0x10` of
the IVT, and jump to the routine registered there"*. The BIOS installed its video routine in
this slot at startup. It runs, reads your registers, does the work, then returns with
`iret`.

```
int 0x10  →  CPU reads IVT[0x10]  →  jumps to the BIOS video routine
          →  the routine reads AH (=0x0E), AL, BH  →  writes to 0xB8000  →  iret (return)
```

**C.6 It's the ancestor of system calls.** A Linux syscall is the same pattern: `eax` =
syscall number (the selector), `ebx/ecx/…` = arguments, then `int 0x80` (or `syscall`).
`int 0x10` + `AH` = exactly that: **a number that dispatches + register-arguments + an
instruction that hands control to service code.**

> **Key point.** `AH`/`AL` are the two halves of `AX`: the convention puts the **selector**
> in `AH` and the **data** in `AL`, which lets you load both at once —
> `mov ax, 0x0E58` sets `AH=0x0E` and `AL=0x58` ('X'). Compact and idiomatic.
