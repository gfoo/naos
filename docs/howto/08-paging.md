[← HOWTO contents](../HOWTO.md)

## Part 8 — B8: paging (virtual memory)

B7 tracks *physical* frames. B8 turns on **paging**: the MMU translates **virtual** addresses to
physical ones through page tables. We build a minimal mapping, enable `CR0.PG`, and prove
translation by writing the screen through a **higher-half** virtual address.

> **Files:** `include/paging.h`, `kernel/paging.c`. Build/run: `make run-b8`.

**In this part:**
- 8.1 — Why paging, and the two-level structure
- 8.2 — Identity map + higher-half window
- 8.3 — Enabling paging (CR3, CR0.PG)
- 8.4 — Page faults
- 8.5 — Verify

**Key terms (quick reference):**

- **Paging** — the MMU maps virtual → physical in 4 KB **pages**, via tables, when `CR0.PG=1`.
- **Page directory / page table** — the two levels of 32-bit x86 paging: 1024 entries each; a virtual address splits into `dir(10) | table(10) | offset(12)`.
- **`CR3`** — register holding the physical address of the current page directory.
- **Identity map** — virtual address = physical address (lets running code keep working after the flip).
- **Higher-half** — mapping the kernel's window high (here `0xC0000000`), the usual home for a kernel's address space.
- **Page fault (#PF)** — exception (vector 14) when a virtual address isn't mapped/permitted; `CR2` holds the faulting address.

---

### 8.1 — Why paging, and the two-level structure

Paging decouples the addresses code uses (**virtual**) from where data actually sits
(**physical**). It's the foundation for process isolation, the kernel heap (B9), and later
user space. On 32-bit x86 a virtual address is split:

```
 31      22 21      12 11         0
 | dir(10) | table(10) | offset(12) |
   │          │           └ byte within the 4 KB page
   │          └ index into the page table
   └ index into the page directory (1024 entries → page tables)
```

The **page directory** (1024 entries) points at **page tables** (1024 entries each), each table
entry pointing at a 4 KB physical frame, with flags (present, read/write…).

### 8.2 — Identity map + higher-half window

We use one page table covering the first 4 MB, and install it **twice** in the directory:

```c
for (i = 0; i < 1024; i++)
    low_page_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_RW;   /* 0..4 MB, frame i → phys i*4KB */

page_directory[0]   = (uint32_t)low_page_table | PAGE_PRESENT | PAGE_RW;  /* identity: 0 → 0 */
page_directory[768] = (uint32_t)low_page_table | PAGE_PRESENT | PAGE_RW;  /* 0xC0000000 → 0 */
```

- **Entry 0 (identity)**: virtual `0..4MB` = physical `0..4MB`. Crucial — our kernel runs at
  physical 1 MB, so without identity mapping the instruction *right after* enabling paging would
  fault. (Index `768` = `0xC0000000 >> 22`.)
- **Entry 768 (higher-half)**: virtual `0xC0000000..0xC0400000` → the *same* physical `0..4MB`.
  So `0xC00B8000` aliases physical `0xB8000` (VGA).

> **Why the structures can live in `.bss`.** `CR3` needs the directory's *physical* address.
> Before paging, virtual = physical, and the directory/table sit in the kernel's low-4 MB image,
> which the identity map covers — so `(uint32_t)&page_directory` is correct both before and after.

### 8.3 — Enabling paging (CR3, CR0.PG)

```c
__asm__ volatile ("mov %0, %%cr3" :: "r"((uint32_t)page_directory));   /* point at the directory */
uint32_t cr0; __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
cr0 |= 0x80000000;                                                     /* set PG */
__asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));                        /* paging is now ON */
```

The instruction after this runs *with translation active* — and keeps running because of the
identity map.

### 8.4 — Page faults

Accessing an unmapped (or forbidden) virtual address raises **#PF** (vector 14), with the
faulting address in `CR2`. Our IDT already routes vector 14 to a handler (B5) — for now it
prints and halts. A real VM system would consult the page tables and map a fresh frame (from the
B7 allocator) on demand; that's future work.

### 8.5 — Verify

After `paging_init()`, `kmain` writes a line to the screen **through the higher-half alias**:

```c
volatile uint16_t *hh = (uint16_t *)0xC00B8000;   /* maps to physical 0xB8000 (VGA) */
for (i = 0; msg[i]; i++) hh[row12 + i] = msg[i] | (0x0D << 8);
```

```bash
make run-b8
```

Expected: `Paging on (CR0.PG=1)…`, then a **magenta line** `[higher-half] this text reached VGA
through 0xC00B8000`. That text appears even though we only ever wrote to a *virtual* high
address — the MMU translated it to the VGA frame. **Criterion met: virtual addressing +
higher-half.**

> **Going further → B9.** With physical frames (B7) and paging (B8), B9 builds the **kernel
> heap**: `kmalloc`/`kfree` carving usable memory into variable-size allocations.
