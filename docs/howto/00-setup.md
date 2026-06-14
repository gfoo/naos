[← HOWTO contents](../HOWTO.md)

## Part 0 — Setup, QEMU & first boot

B0 has a single goal: to have a **build chain that works** and the **proof** that we can
run our own code on the (emulated) machine. No protected mode, no GDT and no C yet — that
comes in B1 and B2. Here, we set the stage and get a boot sector to print one line.

> **Launching a brick.** There is **no** generic `make run`: each brick has its own named
> target — **`make run-b0`** (this part), **`make run-b1`** (Part 1), `run-b2`, `run-b3`…
> That way we always know *which* brick we're launching. B0/B1 are flat binaries (from
> `boot/boot.asm.b0` / `.b1`); B2+ are kernels loaded by GRUB. **All of Part 0 is launched
> with `make run-b0`.**

**In this part:**
- 0.1 — The stage: QEMU, a complete PC in software
- 0.2 — The full chain: from the power button to your code
- 0.3 — The boot sector, built step by step
- 0.4 — Display in depth (the great forgotten one)
- 0.5 — Reading the binary with `hexdump`
- 0.6 — Verify in QEMU

**Key terms (quick reference):**

- **Real mode** — the 16-bit mode the CPU starts in, 8086-compatible (1978).
- **Firmware** — the software burned into the machine, which runs before any OS (here SeaBIOS, provided by QEMU).
- **Boot sector** — the first 512 bytes of a bootable disk.
- **`0x7C00`** — the address where the firmware loads the boot sector (IBM convention, 1981).
- **`0xAA55`** — mandatory signature at offsets 510-511, otherwise the disk is non-bootable.
- **Flat binary** — raw machine code, with no format header (no ELF).
- **`int 0x10`** — display service provided by the BIOS (real mode only).
- **`0xB8000`** — the VGA text video memory (what the VGA hardware displays).

---

### 0.1 — The stage: QEMU, a complete PC in software

QEMU is not "a program launcher": it's an **entire computer emulated in software**,
firmware included. Running `make run-b0` is *powering on a virtual PC*.

What you set, and what QEMU provides on its own:

| Component | How / flag | Do you provide it? |
|---|---|---|
| CPU | `qemu-system-i386` (x86 32-bit) | you choose it |
| RAM | `-m 128M` (default is enough) | optional |
| Disk | `-drive format=raw,file=...` | **yes** (our image) |
| **ROM + firmware** | **SeaBIOS, loaded by default** | ❌ no — QEMU provides it |
| VGA screen, PS/2 keyboard | automatic | no |

#### Installing the dependencies (Debian/Ubuntu)

B0 only requires the **NASM** assembler and the **QEMU** emulator.

> **`apt install nasm qemu-system-x86`** — installs NASM and the QEMU suite (which provides
> `qemu-system-i386`, our 32-bit emulator).

```bash
sudo apt update
sudo apt install -y nasm qemu-system-x86
nasm --version
qemu-system-i386 --version
```

#### Creating the project structure

```bash
mkdir -p boot kernel include toolchain build
```

| Directory | Role |
|---|---|
| `boot/` | bootstrap code (assembly) |
| `kernel/` | kernel code (C, starting at B2) |
| `include/` | shared headers |
| `toolchain/` | tooling scripts (cross-compiler, for B2) |
| `build/` | generated artifacts (git-ignored) |

#### The launch command, flag by flag

> **`qemu-system-i386 -drive format=raw,file=build/naos.img`** — powers on a 32-bit x86
> virtual PC with our image as the hard disk.

- `qemu-system-i386`: the emulator, 32-bit x86 CPU.
- `-drive format=raw,file=...`: attaches our image as a disk. `raw` = raw bytes (not
  a compressed format like `qcow2`).
- The **firmware (SeaBIOS) is loaded automatically** — we never specify it.

Flags that will come in handy later (debugging and verification):

| Flag | Role |
|---|---|
| `-display none` | no window ("headless" mode) |
| `-no-reboot` | **quit** instead of rebooting → reveals triple faults |
| `-d int,cpu_reset -D file.log` | logs interrupts and CPU resets |
| `-s` | opens a **GDB** stub on port 1234 |
| `-S` | **freezes** the CPU at startup (waits for GDB) |
| `-qmp unix:sock,server,nowait` | programmatic control (screenshot, state) |

The `Makefile` exposes a launch target **per brick** (and one for debugging):

> **`make run-b0`** — assembles, builds the image, launches QEMU (normal window).
> **`make debug`** — launches QEMU **frozen** (`-s -S`), waiting for a GDB on `:1234`.

```bash
make run-b0  # launch brick B0
make debug   # then, in another terminal:
             #   gdb -> target remote :1234 -> b *0x7c00 -> continue
```

> **Why `make debug`?** In real mode (B0/B1), there is no `printf` and no error message.
> GDB lets you set a breakpoint at `0x7c00` and step forward **instruction by instruction**
> to inspect the registers. For the trickier bugs in B1 (triple fault), we'll bring out
> **Bochs** (cf. `DESIGN-LOG.md`, C2), whose debugger tells you *why* it crashed.

> **(Optional, for B2) The cross-compiler.** B0 doesn't need it (pure NASM boot sector),
> but you can prepare it: `./toolchain/build-i686-elf.sh` builds `i686-elf-gcc` in
> `~/opt/cross` (~20-40 min). Run once, before B2.

---

### 0.2 — The full chain: from the power button to your code

You have to distinguish **two** chains: the *build* chain (on your machine) and the
*execution* chain (inside QEMU).

#### (a) The build chain — from text to bytes

```
boot/boot.asm   (TEXT, readable)
     │  nasm -f bin        ← translates the text into machine bytes
     ▼
build/boot.bin  (512 raw BYTES)
     │  cp
     ▼
build/naos.img  (the "disk" QEMU mounts)
                 └── its sector 0 = these 512 bytes
```

- A **sector** = a 512-byte block on the disk. **Sector 0** = the boot sector.
  Our "disk" is just a single sector.
- **`boot.asm` (text) never goes onto the disk**: it's `boot.bin` (its translation) that
  does. The `.asm` is the recipe, the `.bin` is the dish.

> **Why two files, `boot.bin` *and* `naos.img`?** They encode two distinct roles:
> `boot.bin` = **a component** (the assembled boot sector, product of `boot.asm`);
> `naos.img` = **the bootable disk** that QEMU mounts (and that we'd `dd` onto a USB stick).
> In B0 they are **identical** — hence the simple `cp` — because the disk contains *only*
> the boot sector. But as soon as **B2**, the image will grow:
> `naos.img = [boot sector / GRUB] + [kernel] + …`, **assembled from several
> pieces** (the rule will become a "link + assemble", not a `cp`). Keeping `naos.img`
> distinct from the start stabilizes the `make run-b0` launch target (it always launches
> *the image*, whatever its contents) and installs the **component vs disk** mental model.

#### (b) The execution chain — from power-on to `0x7C00`

```
power on
  ▼  0xFFFFFFF0   ← THE CPU STARTS HERE (reset vector, wired by Intel) = the FIRMWARE
SeaBIOS: POST → looks for a bootable disk → reads sector 0
  ▼  copies the 512 bytes to 0x7C00, checks 0xAA55, then jmp 0x7C00
OUR code starts HERE   ← 0x7C00
```

Two "starts" not to be confused:

| Address | Start of what | Who sets it |
|---|---|---|
| `0xFFFFFFF0` | of the **CPU** (the firmware) at power-on | wired by Intel/AMD — we never touch it |
| `0x7C00` | of **our** code | IBM 1981 convention — our entry point |

#### Three ideas that make everything that follows crystal clear

> **Why `jmp` isn't magic.** The CPU is an infinite loop: *read the instruction
> at `CS:IP`, execute it, advance `IP`, repeat*. A `jmp X` simply writes `X` into
> `IP`. "SeaBIOS jumps to `0x7C00`" = it puts `0x7C00` into `IP`, and the execution flow
> (which never stopped) continues inside *our* bytes.

> **A label = an address.** `start`, `.print` exist only in the source; NASM replaces them
> with numbers. The firmware doesn't know our labels — it jumps to a **hardcoded**
> address (`0x7C00`), a number agreed in advance between two foreign binaries.

> **Code and data = the same bytes.** The CPU decodes whatever `IP` lands on as an
> "instruction". Nothing physically distinguishes code from a text string — hence
> the importance of placing our data *after* the `hlt` (otherwise the CPU would execute it).

---

### 0.3 — The boot sector, built step by step

> **Key point — the right method.** **Never** write the complete boot sector all at once.
> Add a small piece, run `make run-b0`, observe, repeat. The day it breaks, you know
> it's your last addition — not a mystery hidden among 30 lines. We build it here in 4
> increments; **test after each one**.

#### Step a — the skeleton that boots (and does nothing)

```nasm
bits 16                 ; the CPU starts in real mode (16-bit)
org  0x7C00             ; we align on the BIOS load address

start:
    hlt                 ; halt the CPU
    jmp start           ; ... and loop back if it's woken up

times 510-($-$$) db 0   ; padding up to byte 510
dw 0xAA55               ; boot signature (offsets 510-511)
```

`make run-b0` → SeaBIOS prints `Booting from Hard Disk...`, then a **black screen**, with
nothing else. That's normal: we're not displaying anything yet.

> **What does this step verify?** Two things, without displaying: (1) **no** "No bootable
> device" → the `0xAA55` signature is recognized, our sector *is* bootable; (2) **no**
> reboot loop → we did reach `hlt`, so our code runs. The full pipeline
> (build → image → QEMU → our code) is running.

#### Step b — display ONE character

```nasm
bits 16
org  0x7C00

start:
    mov ah, 0x0E        ; "teletype" function of the BIOS video service
    mov al, 'X'         ; the character to display
    int 0x10            ; BIOS call -> it displays AL

.hang:
    hlt
    jmp .hang

times 510-($-$$) db 0
dw 0xAA55
```

`make run-b0` → an `X` appears under the SeaBIOS lines. **Proof: we know how to call the
firmware (`int 0x10`) and it displays.**

> **How does this call work?** `int 0x10` is a *generic function*: `AH` selects the
> sub-function (`0x0E` = teletype), `AL` carries the character. Full mechanism
> (selector, register-arguments, vector table, syscall parallel):
> **[Appendix C — `int 0x10` in detail](annexes.md#appendix-c--int-0x10-in-detail--bios-services--interrupts)**.
> What `int 0x10` does *on screen* is detailed in [0.4](#04--display-in-depth).

#### Step c — display a STRING (the loop)

```nasm
bits 16
org  0x7C00

start:
    mov si, msg         ; SI points to the start of the string
.print:
    lodsb               ; AL = [SI], then SI++
    test al, al         ; null byte? -> end of string
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .print          ; next character

.hang:
    hlt
    jmp .hang

msg db "naos B0: it boots!", 13, 10, 0   ; 13,10 = CR LF ; 0 = end
times 510-($-$$) db 0
dw 0xAA55
```

`make run-b0` → the complete message. **Proof: the display loop and the `msg` data.**

> **Why `msg` *after* `.hang`?** Because the CPU executes in sequence: if it
> ran into the string's bytes, it would decode them as instructions (gibberish,
> then a crash). By placing it after the halt loop, the CPU never reaches it as code
> — only `lodsb` *reads it as data*.

#### Step d — the clean ritual (segments + stack)

So far we've been lucky: the segments and the stack were in a usable state
left by the BIOS. To be **robust** (and indispensable as soon as we touch the stack), we
initialize them explicitly. This is the **final** `boot/boot.asm` of B0:

```nasm
bits 16
org  0x7C00

start:
    cli                 ; no interrupts while we set up the segments
    xor ax, ax          ; AX = 0
    mov ds, ax          ; DS = ES = SS = 0: simple addressing, segments at zero
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; stack just below our code (grows downward)
    sti                 ; re-enable interrupts (the BIOS needs them)

    mov si, msg
.print:
    lodsb
    test al, al
    jz .hang
    mov ah, 0x0E        ; teletype
    mov bh, 0x00        ; video page 0
    int 0x10
    jmp .print

.hang:
    hlt
    jmp .hang

msg db "naos B0: it boots!", 13, 10, 0
times 510-($-$$) db 0
dw 0xAA55
```

> **Why this ritual?** `cli`: no interrupt on a still-inconsistent stack.
> Segments at 0: simple addressing in absolute offsets. `sp = 0x7C00`: the stack grows *below*
> our code, without overwriting it. `sti`: we re-enable interrupts, which `int 0x10`
> needs. **Line-by-line detail in [Appendix B](annexes.md#appendix-b--bootbootasm-line-by-line).**

> **New or rusty in assembly?** See [Appendix A](annexes.md#appendix-a--x86-assembly-refresher-real-mode-16-bit)
> (registers, real mode, Intel syntax, instructions, NASM directives).

---

### 0.4 — Display in depth

We displayed via `int 0x10`. But what *really* happens? We have to separate **two
actors**: the **firmware** (software) and the **VGA hardware**.

#### `int 0x10` = a CALL to the firmware

It is not a "screen register" the firmware watches — it's a **routine that we
invoke**. It runs only when we call it, does its job, and returns control. Its
parameters are passed through registers: `AH` = the function (`0x0E` = teletype), `AL` = the
character, `BH` = the video page. The full mechanism (`AH` = sub-function selector,
registers = arguments, IVT, parallel with system calls) is detailed in
**[Appendix C](annexes.md#appendix-c--int-0x10-in-detail--bios-services--interrupts)**.

> **What is `BH`?** Just the high byte of `BX`. Its "video page" meaning exists only in
> the **`int 0x10` convention** (a *BIOS* standard, not a card one). The *capability*
> of "pages" comes from the VGA hardware (enough memory for several screens); the firmware
> exposes it via `BH`. We set `BH=0` = the default displayed page.

#### What the firmware does under the hood: write to video memory

The text screen is **memory-mapped** at **`0xB8000`**. This address is not RAM:
it's the **memory of the VGA card**. The VGA controller **reads this region continuously** (~60×/s)
and draws on screen what it finds there. `int 0x10`, at bottom, does nothing but **write to
`0xB8000`** for you (handling the cursor and scrolling on top of that).

**2 bytes per screen cell:**

```
0xB8000 : 0x41   ← the CODE of the character ('A' = 0x41 in ASCII)
0xB8001 : 0x0F   ← the ATTRIBUTE: color (low nibble = text, high nibble = background)
                   0x0F = white text (F) on black background (0)
```

An 80×25 screen = 2000 cells × 2 bytes = 4000 bytes starting at `0xB8000`.

#### How the VGA "knows" the shape of the 'A': the CP437 font

The hardware doesn't guess the shape — it does a **lookup in a font table** (the
*character generator*) that maps each code (0–255) to a **bitmap** (a grid of ~8×16
pixels). For each cell: read the code → find the glyph → paint the pixels with the
colors from the attribute. The default font, **CP437**, is loaded at boot by the video
firmware.

> **The "neat trick" (coming in B3).** We'll be able to **replace CP437** with our own
> font: load our bitmaps into the VGA's character generator → our own glyphs
> in text mode. This is an exercise of the **screen driver (B3)**, where we program the
> VGA hardware directly.

#### The unified ASCII chain

The same code `0x6E` (the 'n') travels across **the entire system, with no conversion at all**:

```
source 'n'  →  NASM: byte 0x6E  →  lodsb: AL=0x6E  →  0xB8000: 0x6E  →  font: glyph 'n'
```

ASCII is the **common language** of the source, of memory and of the screen (and of the keyboard in
B6). That's why writing `0x6E` to `0xB8000` displays 'n', with no intermediate translation.

> **Key point — NASM doesn't know CP437.** This chain works by *coincidence*: NASM does not
> *convert* anything, it copies the character's byte as it is in your source file (=
> ASCII for the basic characters, hence `'n'` → `0x6E`). And it happens to be right **because the
> first 128 codes of CP437 *are* ASCII** (CP437 was designed as an extension
> of ASCII). Beyond `0x7F`, the two **diverge**:
>
> | 'é' according to… | byte(s) |
> |---|---|
> | UTF-8 source (what NASM copies) | `0xC3 0xA9` (two bytes!) |
> | CP437 (what the VGA displays) | `0x82` |
>
> An `'é'` typed in a string would therefore be emitted in UTF-8 and interpreted in CP437 → **gibberish
> on screen**. Hence the rule: **boot messages in pure ASCII**; for a special character,
> write its CP437 code as a literal (`db 0x82` for 'é'), don't type it in a string.

#### The division of roles, and the two paths

| Who | Does what |
|---|---|
| **you** | provide the data (character code + color attribute) |
| **VGA hardware** | scans `0xB8000`, does the font lookup, paints the pixels |
| **firmware** | (B0) wraps "write to `0xB8000`" in `int 0x10`; loaded CP437 at boot |

```
B0 (real mode)      : your code ──int 0x10──> [BIOS writes to 0xB8000] ──> VGA displays
B3 (protected mode) : your code ──write to 0xB8000 yourself──────────> VGA displays
```

> **Key point — why `0xB8000` is central.** `int 0x10` (firmware) **disappears** in protected
> mode, but `0xB8000` (hardware) **still works**, whatever the CPU mode. That's
> why writing to `0xB8000` will be the **proof of B1** (we display without the BIOS) and the heart
> of the **B3 screen driver**.

---

### 0.5 — Reading the binary with `hexdump`

The final sector, byte by byte — an excellent low-level debugging reflex.

> **`xxd build/boot.bin`** — displays the 512 bytes in hexadecimal + ASCII.

```bash
make
xxd build/boot.bin
```

Excerpt (yours may differ by a byte or two depending on the code length):

```
00000000: fa31 c08e d88e c08e d0bc 007c fbbe 207c  .1.........|.. |
00000010: ac84 c074 08b4 0eb7 00cd 10eb f3f4 ebfd  ...t............
00000020: 6e61 6f73 2042 303a 2069 7420 626f 6f74  naos B0: it boot
00000030: 7321 0d0a 0000 ...                        s!..............
...
000001f0: 0000 0000 0000 0000 0000 0000 0000 55aa  ..............U.
```

There you can read **the 4 blocks**: code (`0x00`–`0x1F`), string (`0x20`–`0x33`), padding (zeros),
signature `55aa` (offset `0x1FE`).

**Code is also bytes.** Decoding the first ones:

| Bytes | Instruction |
|---|---|
| `fa` | `cli` |
| `31 c0` | `xor ax, ax` |
| `8e d8` / `8e c0` / `8e d0` | `mov ds/es/ss, ax` |
| `bc 00 7c` | `mov sp, 0x7C00` |
| `fb` | `sti` |
| **`be 20 7c`** | **`mov si, 0x7C20`** ← the address of `msg` |
| `ac` | `lodsb` |
| `84 c0` / `74 08` | `test al,al` / `jz` |
| `b4 0e` / `b7 00` / `cd 10` | `mov ah,0x0E` / `mov bh,0x00` / `int 0x10` |
| `eb f3` | `jmp .print` (**relative** jump: goes back 13) |
| `f4` / `eb fd` | `hlt` / `jmp .hang` |

> **The label became an address.** `mov si, msg` was encoded as `be 20 7c` = `mov si,
> 0x7C20`. NASM computed `msg` = `0x7C00 + 0x20` (thanks to `org`) and burned it **hardcoded**.
> No trace of the word "msg" nor ".print": the binary contains only numbers.

> **The string became ASCII bytes.** At offset `0x20`: `6e 61 6f 73` = 'n' 'a' 'o'
> 's'. The `db "naos..."` of the source is just a readable way of writing these codes.

---

### 0.6 — Verify in QEMU

#### By eye (window)

```bash
make run-b0
```

SeaBIOS → `Booting from Hard Disk...` → `naos B0: it boots!` → frozen cursor (the CPU is in
`hlt`). **Success criterion: the message is displayed and the machine doesn't reboot in a
loop** (a reboot loop = triple fault). This is the normal check.

#### Headless capture (when there's no window)

Sometimes we just want an image of the screen — for example to verify remotely, or in a
script. We launch QEMU **headless** with a QMP socket (QEMU's control protocol), then
capture from another terminal:

```bash
make run-b0 QMP=1               # terminal 1: QEMU without a window + QMP socket
python3 tools/qemu-shot.py      # terminal 2: writes build/shot.png
```

Open `build/shot.png` and judge for yourself: no PASS/FAIL, no OCR — just the photo.

> **Why a script rather than a `screendump` + `sleep`?** The boot is near-instantaneous in
> *guest* time, but QMP responds before SeaBIOS has finished in *real* time: capturing too early
> gives "display not initialized". The script **waits for the screen to stabilize** (two
> captures of similar size in a row) instead of betting on a fixed delay. And since the text-mode
> cursor `_` **blinks forever**, it compares the *size* of the PNGs (blinking
> ≈ 0.5%, tolerated) rather than the exact bytes, which are never identical. See `tools/qemu-shot.py`.

---

### 0.7 — On ARM (AArch64): the same brick, a different world

x86 is not the only architecture — ARM (phones, Raspberry Pi, Apple Silicon, AWS Graviton) is
a *different* instruction set, and QEMU emulates it too (`qemu-system-aarch64`). **B0 is the one
Part that transposes cleanly to ARM**, precisely because it touches none of the x86 folklore
(real mode, A20, GDT — that's B1, which has *no* ARM equivalent). The contrast is the lesson:

| | B0 x86 (`boot.asm.b0`) | B0 ARM (`boot/arm/`) |
|---|---|---|
| Assembler | **NASM** (Intel syntax) | **GNU `as`** (ARM syntax) |
| Format | flat binary, 512 bytes | **ELF**, loaded by QEMU `-kernel` |
| Startup | BIOS → `0x7C00`, 16-bit real mode | **directly in 64-bit** at an *exception level*, **no BIOS, no boot sector** |
| QEMU | `qemu-system-i386 -drive…` | `qemu-system-aarch64 -machine virt -kernel…` |
| Display | VGA video memory `0xB8000` | **PL011 UART** (`0x09000000`, a memory-mapped serial port) |
| Verify | screenshot (`qemu-shot.py`) | **read the serial output** (text on stdout) |

#### Prerequisites (Debian/Ubuntu)

```bash
sudo apt install -y qemu-system-arm gcc-aarch64-linux-gnu
```

Unlike x86, **no 40-minute build**: the AArch64 cross-compiler and QEMU-ARM are ready-made
packages.

#### The three files

- **`boot/arm/boot.S`** — AArch64 boot stub: set the stack pointer, `bl kmain`. No mode switch —
  QEMU `virt` already starts the CPU in 64-bit.
- **`kernel/arm/kmain.c`** — writes the string into the PL011 UART data register at `0x09000000`
  (there is no VGA on `virt`).
- **`boot/arm/linker.ld`** — `virt` RAM starts at `0x40000000`; we link the kernel at `0x40080000`.

#### Build & verify

```bash
make run-b0-arm     # QEMU 'virt', serial console on the terminal (Ctrl-A then X to quit)
```

Expected: `naos B0 (ARM): it boots!` on the serial console.

> **Why no screenshot here?** The `virt` machine has no VGA framebuffer. Text goes out on the
> **serial UART**, so the "screen" is a text stream, not pixels — you read it (e.g. with
> `-serial file:out.log`), you don't capture it. That's the x86/ARM contrast made concrete:
> legacy VGA text memory on one side, a memory-mapped UART on the other.

> **Key point — only B0 maps cleanly.** From B1 on, the x86 path is deeply x86-specific (real
> mode, A20, GDT and the far jump have *no* ARM counterpart; ARM boots straight into 64-bit at an
> exception level). The universal *concepts* (privilege levels, MMU/paging, interrupt controller)
> still exist on ARM, but under other names (EL0–EL3, translation tables, GIC) and with entirely
> different code. So naos stays x86; this ARM B0 is a one-off eye-opener.

---

### Going further → B1

- B0 **borrows** the firmware (`int 0x10`). **B1** goes all the way through the real-mode
  startup — **A20** activation, **GDT**, the **PE** bit of `CR0`, **far jump** — to enter
  **32-bit protected mode**. There, `int 0x10` disappears: we'll display by writing directly to
  `0xB8000`.
- The direct `0xB8000` display and the **replacement of the CP437 font**: that will be the **screen
  driver (B3)**.
