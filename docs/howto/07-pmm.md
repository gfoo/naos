[← HOWTO contents](../HOWTO.md)

## Part 7 — B7: physical memory

To allocate memory the kernel must first know **what RAM exists** and track which 4 KB
**frames** are free. B7 reads the **Multiboot memory map** GRUB handed us and builds a **bitmap
frame allocator**: `pmm_alloc_frame()` / `pmm_free_frame()`.

> **Files:** `include/multiboot.h`, `include/pmm.h`, `kernel/pmm.c` (+ `boot.asm` now passes the
> Multiboot info pointer to `kmain`). Build/run: `make run-b7`.

**In this part:**
- 7.1 — Getting the Multiboot info into `kmain`
- 7.2 — The memory map
- 7.3 — A bitmap of frames
- 7.4 — Reserve, allocate, free
- 7.5 — Verify

**Key terms (quick reference):**

- **Frame** — a 4 KB unit of *physical* memory; the allocator's grain.
- **Memory map** — the list of RAM regions (available / reserved) the bootloader provides.
- **Bitmap allocator** — one bit per frame (1 = used, 0 = free); simple and compact.
- **Multiboot info** — the struct GRUB passes in `ebx`, holding the map (and more).

---

### 7.1 — Getting the Multiboot info into `kmain`

GRUB leaves `eax = 0x2BADB002` and `ebx = ` address of a `multiboot_info` struct. Until now our
stub ignored them; B7 passes them on:

```nasm
mov esp, stack_top
push ebx                ; Multiboot info pointer
push eax                ; Multiboot magic
call kmain              ; kmain(uint32_t magic, uint32_t mb_info)
```

(Earlier bricks' `kmain(void)` simply ignore the extra arguments — cdecl lets the caller push
them harmlessly.) To get the map, our Multiboot header already requested it (`MB_MEMINFO` flag,
B2). No paging yet (B8), so physical = virtual: we dereference `mmap_addr` directly.

### 7.2 — The memory map

`multiboot_info` points at an array of variable-sized entries; we walk it, looking at
**available** (`type == 1`) regions:

```c
uint32_t addr = mbi->mmap_addr, end = addr + mbi->mmap_length;
while (addr < end) {
    struct multiboot_mmap_entry *e = (void *)addr;
    if (e->type == MULTIBOOT_MEMORY_AVAILABLE) { /* free its frames */ }
    addr += e->size + 4;        /* `size` excludes itself */
}
```

### 7.3 — A bitmap of frames

One bit per 4 KB frame: 1 = used, 0 = free. Covering the 4 GB space is `1M` bits = **128 KB**
in `.bss`:

```c
#define MAX_FRAMES  (1024*1024)
static uint32_t bitmap[MAX_FRAMES/32];
static void mark_used(uint32_t f){ bitmap[f>>5] |=  (1u<<(f&31)); }
static void mark_free(uint32_t f){ bitmap[f>>5] &= ~(1u<<(f&31)); }
```

### 7.4 — Reserve, allocate, free

`pmm_init` starts with **everything used**, frees only the frames the map says are available,
then **re-reserves the low 4 MB** (BIOS area, video memory, our kernel image). Allocation is a
linear scan for the first free bit; freeing clears it:

```c
uint32_t pmm_alloc_frame(void) {
    for (f = RESERVE_END/4096; f < MAX_FRAMES; f++)
        if (!is_used(f)) { mark_used(f); return f * 4096; }
    return 0;                    /* out of memory */
}
void pmm_free_frame(uint32_t addr) { mark_free(addr / 4096); }
```

> **Why reserve the low 4 MB?** Frames 0–0x3FFFFF hold the IVT/BIOS, VGA memory (`0xB8000`) and
> our kernel (loaded at 1 MB). Handing them out would corrupt the running system. A fixed 4 MB
> reserve is a simple, safe over-approximation for now.

### 7.5 — Verify

`kmain` parses the map, then allocates two frames, frees the first, and allocates again:

```bash
make run-b7
```

Expected: `Usable frames: ~32639 (~128 MB)`, then `alloc a = 0x00400000`, `alloc b =
0x00401000`, `free a`, `alloc c = 0x00400000  reuses a? YES`. The reuse proves both allocation
and freeing work. **Criterion met: allocate/free a page.**

> **Going further → B8.** B7 manages *physical* frames. B8 adds **paging**: page tables + `CR3`
> to map *virtual* addresses onto those frames, the page-fault handler, and the higher-half.
