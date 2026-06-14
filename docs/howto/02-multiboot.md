[← HOWTO contents](../HOWTO.md)

## Part 2 — B2: GRUB + Multiboot + first C kernel

B1 *showed* how to enter protected mode by hand. Now we **delegate** that startup
to GRUB (hybrid choice C4: understand first, delegate next) and we write our first code
in **C**. This is a turning point: the homemade boot sector disappears, replaced by an **ELF kernel**
loaded at 1 MB, produced by a **cross-compiler** and packaged into a **bootable ISO**.

> **Where this code lives.** `boot/boot.asm` (rewritten: Multiboot header + stub), `kernel/kmain.c`,
> `linker.ld`, `grub/grub.cfg`, and a reworked `Makefile`. Toolchain: `i686-elf-gcc`.

**In this part:**
- 2.1 — Why delegate the boot to GRUB
- 2.2 — The Multiboot spec: the contract between GRUB and our kernel
- 2.3 — The cross-compiler: why, and how to build it
- 2.4 — The boot stub in ASM (`boot/boot.asm`)
- 2.5 — The linker script: placing the kernel at 1 MB
- 2.6 — The first `kmain()` in C
- 2.7 — Building the bootable ISO (GRUB)
- 2.8 — Build & verify

**Key terms (quick reference):**

- **GRUB** — standard *bootloader*; knows how to load a **Multiboot** kernel and hands us off in protected mode.
- **Multiboot 1** — spec defining a *header* the kernel exposes and a *state* GRUB guarantees on entry.
- **Cross-compiler** — compiler producing code for a target (`i686-elf`) different from the host.
- **Freestanding** — C code **without** a standard library or OS (no `printf`, no `malloc`).
- **ELF** — Unix object/executable file format; our kernel is a 32-bit ELF.
- **Linker script** (`.ld`) — describes the memory layout of the final binary's sections.
- **`grub-mkrescue`** — builds a bootable ISO containing GRUB + our kernel.

---

### 2.1 — Why delegate the boot to GRUB

B1 made us *understand* A20 / GDT / PE / far jump. Redoing all of that by hand on every
startup — plus loading the kernel from disk (`int 0x13`), the memory map
(`int 0x15`), etc. — would be long and off-topic for learning the *kernel*. GRUB does all that
work and hands us off in a known state:

| On entry to our kernel, GRUB guarantees… | …what that saves us |
|---|---|
| CPU in **32-bit protected mode** | A20, GDT, PE bit, far jump (all of B1) |
| Kernel loaded in memory at **1 MB** | disk sector reads (`int 0x13`) |
| `eax` = magic `0x2BADB002`, `ebx` → Multiboot info | memory detection (`int 0x15` E820) |
| Interrupts **disabled**, no paging | a clean starting point |

> **"Hybrid," reminder of C4.** We keep B1 as a *lesson* (homemade boot sector, in
> `boot.asm.b1`); from B2 onward we *delegate* to GRUB. Later, B11 (optional) will replace
> GRUB with our own loader — once the OS is working.

#### Other bootloaders — and why GRUB became the Linux standard

GRUB isn't the only option, nor was it the first. Its predecessor on Linux was **LILO** (*LInux
LOader*). LILO recorded the *physical block positions* of the kernel on disk, so the smallest
kernel update or move broke the boot until you re-ran `lilo` — and it had no interactive shell.
**GRUB** (*GRand Unified Bootloader*) won because it **reads filesystems** (ext4, FAT…) and
loads the kernel *by path*, with a menu, an interactive shell, Multiboot support (multi-OS), and
both BIOS and UEFI. That filesystem-awareness is the decisive difference.

| | LILO (the old one) | GRUB (the standard) |
|---|---|---|
| Finds the kernel | by **physical block position** | by **reading the filesystem** (by path) |
| After a kernel update | must re-run `lilo`, else boot breaks | nothing to do |
| At boot | no interaction | **menu + interactive shell** |
| Multi-OS / multiboot | limited | native (the **Multiboot** spec we use) |

Others worth knowing: `systemd-boot` (simple, UEFI — gaining ground), `rEFInd`,
`Syslinux/ISOLINUX` (live CD/USB), `U-Boot` (embedded and **ARM**), `Windows Boot Manager`, and
**Limine** — the modern favorite in hobby OSDev today: it hands you off straight in 64-bit with a
framebuffer and a clean memory map, with far less friction than GRUB.

> **Why naos stays on GRUB/Multiboot.** Limine is technically nicer for a *modern* 64-bit kernel,
> but GRUB + Multiboot is the **canonical, documented path** (every "Bare Bones" tutorial), it
> matches the plan (C4 "delegate the minimum," and B11 "write your own loader"), and Multiboot 1
> teaches the bootloader↔kernel **contract** in its barest form — exactly the lesson. Limine would
> shine the day we target a 64-bit graphical kernel (B12, optional); we'd reconsider then.

### 2.2 — The Multiboot spec: the contract between GRUB and our kernel

For GRUB to agree to load our binary, it must expose a **Multiboot header**
within its **first 8 KB**: three 32-bit words.

```nasm
MB_MAGIC    equ 0x1BADB002             ; constant recognized by GRUB
MB_FLAGS    equ MB_ALIGN | MB_MEMINFO  ; requested options
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS) ; magic + flags + checksum == 0
```

- **magic** `0x1BADB002`: the signature GRUB looks for.
- **flags**: our requests — here `MB_ALIGN` (align modules on pages) and `MB_MEMINFO`
  (provide the memory map).
- **checksum**: chosen so that `magic + flags + checksum` equals **0** (over 32 bits). This is
  GRUB's integrity check.

> **Two magic numbers not to confuse.** `0x1BADB002` is what *we* put in
> the header ("load me"). `0x2BADB002` is what *GRUB* puts in `eax` on entry ("it really
> is me who loaded you, via Multiboot"). We'll exploit `ebx` (memory info) in B7.

### 2.3 — The cross-compiler: why, and how to build it

The system's `gcc` produces **Linux** executables: they assume a libc, an output
format, an OS beneath them. Our kernel runs **without an OS** — it *is* what will run. So we compile
*freestanding*, with an **`i686-elf`** toolchain that makes **no host assumptions**
(OSDev recommendation, choice C5).

```bash
./toolchain/build-i686-elf.sh        # builds binutils + gcc into ~/opt/cross (~20-40 min)
# Debian/Ubuntu prerequisites:
#   sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo wget
~/opt/cross/bin/i686-elf-gcc --version
```

The script compiles **binutils** (target assembler/linker) then **gcc** with `--without-headers
--enable-languages=c` (no libc: we don't have one). We compile with `-ffreestanding -nostdlib`
and **link with `i686-elf-gcc`** (not bare `ld`) to pull in `libgcc` (the helpers gcc
calls for, e.g., 64-bit divisions).

> **Why not the host's `gcc -m32`?** It *can* work, but the cross-compiler eliminates a whole
> class of sneaky bugs (host headers pulled in by mistake, ABI assumptions, Linux security
> options injected). C5 settles it: `i686-elf-gcc`, built once and for all.

### 2.4 — The boot stub in ASM (`boot/boot.asm`)

GRUB hands us off in 32 bits, but knows nothing about `kmain`. We need a small **stub** that (1)
carries the Multiboot header, (2) sets up a stack, (3) calls `kmain`. That's all `boot.asm`
is now:

```nasm
bits 32
section .multiboot          ; <- must fall within the first 8 KB (cf. linker.ld)
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384              ; 16 KiB of stack
stack_top:

section .text
global _start
extern kmain
_start:
    mov esp, stack_top      ; set up the stack (grows downward)
    call kmain              ; -> our C
.hang:
    cli
    hlt
    jmp .hang
```

> **Why set up the stack yourself.** The Multiboot spec does *not* guarantee a usable
> `esp`. Yet C needs a stack from the very first function call (local variables,
> return addresses). So we reserve 16 KiB in `.bss` and point `esp` at its top —
> before the slightest `call`.

> **Calling convention (System V i386).** `call kmain` pushes the return address and jumps;
> `kmain` takes no arguments and returns nothing. If it ever did return, the `cli`/`hlt`
> halts the machine cleanly.

### 2.5 — The linker script: placing the kernel at 1 MB

The compiler produces sections (`.text`, `.rodata`, `.data`, `.bss`); the **linker
script** says *where* to place them in memory and *in what order*:

```ld
ENTRY(_start)
SECTIONS {
    . = 1M;                              /* the kernel starts at 1 MB */
    .text BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)                    /* Multiboot header FIRST */
        *(.text)
    }
    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }
}
```

- `. = 1M`: the start address. **1 MB** is conventional — above the reserved low
  region (IVT, BIOS, VGA video memory at `0xB8000`).
- `*(.multiboot)` placed **first** in `.text`: guarantees the header is within the first 8
  KB, otherwise GRUB doesn't find it and rejects the kernel.
- `ALIGN(4K)`: sections page-aligned — useful as soon as we enable paging (B8).

### 2.6 — The first `kmain()` in C

In B2, `kmain` only needs to **prove that it runs**. No driver yet: we write directly
into video memory (like in 1.6, but in C):

```c
/* kernel/kmain.c (B2 version) */
void kmain(void)
{
    const char *msg = "naos B2: kmain() running, loaded by GRUB via Multiboot.";
    volatile unsigned short *vga = (unsigned short *)0xB8000;
    for (int i = 0; msg[i]; i++)
        vga[i] = (unsigned short)(unsigned char)msg[i] | (0x0A << 8); /* light green */
    for (;;)
        __asm__ volatile ("hlt");
}
```

> **Why `volatile`.** The compiler, seeing that we write into an array that's never read back,
> would be tempted to *eliminate* those writes. `volatile` forbids it from optimizing: every
> write must actually reach video memory. (The real driver, B3, generalizes this.)

### 2.7 — Building the bootable ISO (GRUB)

GRUB reads its config from `grub/grub.cfg` (a single entry, immediate startup):

```
set timeout=0
set default=0
menuentry "naos" {
    multiboot /boot/naos.kernel
    boot
}
```

We assemble a `boot/grub/` tree then turn it into an ISO with `grub-mkrescue`
(prerequisites: `grub-pc-bin`, `xorriso`, `mtools`). The `Makefile` handles it:

The `Makefile` handles it via a *pattern* rule `build/%.iso` (one ISO per kernel brick:
`b2.iso`, `b3.iso`…):

```make
$(BUILD)/%.iso: $(BUILD)/%.kernel grub/grub.cfg
	mkdir -p $(BUILD)/iso-$*/boot/grub
	cp $< $(BUILD)/iso-$*/boot/naos.kernel
	cp grub/grub.cfg $(BUILD)/iso-$*/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso-$*
```

### 2.8 — Build & verify

```bash
make run-b2                   # QEMU boots the B2 ISO → GRUB → kmain()   (the real B2 path)
make run-b2 QMP=1             # (other terminal) python3 tools/qemu-shot.py
make run-kernel               # shortcut: QEMU loads the LATEST kernel WITHOUT GRUB (-kernel)
```

The `Makefile` automatically validates `grub-file --is-x86-multiboot build/b2.kernel`: if
the header is malformed, the build fails *before* QEMU. At boot, the green message proves that
`kmain()` (in **C**, loaded by **GRUB**) is executing. **B2 criterion met.**

> **ISO (GRUB) vs `-kernel`.** `make run-b2` boots the ISO → GRUB loads the kernel: that's the
> real B2 path. `make run-kernel` loads the *same* kernel via QEMU's built-in Multiboot
> loader (without GRUB) — handy for iterating, but the B2 criterion says *via GRUB*, so `run-b2`
> is authoritative.

---

