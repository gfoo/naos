[← HOWTO contents](../HOWTO.md)

## Part 4 — B4: a kernel-owned GDT

GRUB left us running on *its* GDT (cf. B2) — minimal, and not ours to rely on. B4 installs our
own **flat GDT** with five entries (null, kernel code/data, user code/data). We did the GDT
mechanics by hand in B1; here we redo them properly in C, from inside the kernel, and add the
**ring 3** (user) segments we'll need later for system calls.

> **Files:** `include/gdt.h`, `kernel/gdt.c`, `kernel/gdt_flush.asm`. Build/run: `make run-b4`
> (kernel cumulative from B3 + the GDT module).

**In this part:**
- 4.1 — Why a kernel-owned GDT
- 4.2 — Anatomy of a descriptor (recap)
- 4.3 — The five entries
- 4.4 — Loading it: `lgdt` + segment reload
- 4.5 — Verify

**Key terms (quick reference):**

- **GDT** (*Global Descriptor Table*) — the table of segment descriptors the CPU indexes.
- **Descriptor** — an 8-byte entry: base, limit, access rights, flags.
- **Selector** — `index × 8` loaded into a segment register (`0x08`, `0x10`…).
- **DPL** (*Descriptor Privilege Level*) — the ring (0 = kernel, 3 = user) allowed to use a segment.
- **Flat model** — base 0, limit 4 GB for every segment: segmentation is neutralized (paging will do isolation in B8).

---

### 4.1 — Why a kernel-owned GDT

In B2, GRUB handed us a working but opaque GDT — we don't control its layout, and it has no
user-mode segments. A real kernel owns its GDT, for two reasons:

1. **Determinism** — a known, flat GDT we built ourselves, not whatever the bootloader left.
2. **Preparing ring 3** — to run user code later (system calls), we need **user** code/data
   descriptors (DPL 3) alongside the kernel ones (DPL 0). We add them now.

Segmentation itself stays *flat* (base 0, limit 4 GB): we don't use segments to isolate memory —
that's paging's job (B8). The GDT here is about **privilege levels**, not address ranges.

### 4.2 — Anatomy of a descriptor (recap)

Each entry is the historical 8-byte segment descriptor (same one decoded byte-by-byte in B1,
Appendix B). In C, packed so the layout matches the hardware exactly:

```c
struct gdt_entry {
    uint16_t limit_low;     /* limit[0:15]              */
    uint16_t base_low;      /* base[0:15]               */
    uint8_t  base_mid;      /* base[16:23]              */
    uint8_t  access;        /* present, DPL, type       */
    uint8_t  flags_limit;   /* flags (G, D) | limit[16:19] */
    uint8_t  base_high;     /* base[24:31]              */
} __attribute__((packed));
```

- **access byte** `P DPL S type`: `0x9A` = present, ring 0, code (exec+read); `0x92` = ring 0
  data (write); `0xFA`/`0xF2` = the same for ring 3.
- **flags** `0xC0` = `G=1` (limit counted in 4 KB pages → 4 GB) + `D=1` (32-bit operands).

### 4.3 — The five entries

| Index | Selector | access | Segment |
|---|---|---|---|
| 0 | `0x00` | `0x00` | null descriptor (mandatory) |
| 1 | `0x08` | `0x9A` | kernel code (ring 0) |
| 2 | `0x10` | `0x92` | kernel data (ring 0) |
| 3 | `0x18` | `0xFA` | user code (ring 3) |
| 4 | `0x20` | `0xF2` | user data (ring 3) |

`gdt_init()` fills these via a small `set_entry(i, base, limit, access, flags)` helper, then
points a `gdt_ptr` (size − 1, linear address) at the table.

### 4.4 — Loading it: `lgdt` + segment reload

Telling the CPU to use the new table is the same dance as B1: `lgdt`, reload the data segment
registers, and a **far jump** to reload `CS` (which C cannot do). That's `kernel/gdt_flush.asm`:

```nasm
gdt_flush:
    mov eax, [esp + 4]      ; the gdt_ptr passed from C
    lgdt [eax]
    mov ax, 0x10            ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush         ; far jump reloads CS = kernel code selector
.flush:
    ret
```

> **Why the far jump again?** As in B1: writing `CS` directly is impossible; a far jump is the
> only way to reload it, and it also flushes the prefetch so the next instruction is fetched
> under the new descriptor. A wrong descriptor or selector here → triple fault → reboot loop.

### 4.5 — Verify

```bash
make run-b4
make run-b4 QMP=1     # (other terminal) python3 tools/qemu-shot.py
```

Expected: the messages print, ending with **"the kernel now runs on its own GDT."** The point is
*subtle but decisive*: that line is printed **after** `gdt_init()`. If the GDT were malformed,
the far jump would triple-fault and the machine would reboot — you'd never see it. Reaching it
means the reload succeeded. **Criterion met: GDT reloaded, no triple fault.**

> **Going further → B5.** With a proper GDT in place, B5 adds the **IDT** and interrupts: CPU
> exceptions (e.g. divide-by-zero) and hardware IRQs via the PIC, each routed to a handler.
