[← HOWTO contents](../HOWTO.md)

## Part 9 — B9: kernel heap (kmalloc / kfree)

The frame allocator (B7) hands out whole 4 KB pages; paging (B8) maps them. But kernel code
needs **variable-size** allocations — a few bytes here, a struct there. B9 builds a **heap**:
`kmalloc(size)` / `kfree(ptr)`, a first-fit free-list allocator.

> **Files:** `include/heap.h`, `kernel/heap.c`. Build/run: `make run-b9`.

**In this part:**
- 9.1 — What a heap adds over frames
- 9.2 — The free-list block
- 9.3 — `kmalloc`: first-fit + split
- 9.4 — `kfree`: free + coalesce
- 9.5 — Verify

**Key terms (quick reference):**

- **Heap** — a region carved into variable-size allocations on demand.
- **Free list** — blocks linked together, each tagged free/used.
- **First-fit** — return the first free block big enough.
- **Split** — when a free block is larger than requested, cut off the remainder as a new free block.
- **Coalesce** — merge adjacent free blocks so the space can be reused for a larger request.

---

### 9.1 — What a heap adds over frames

`pmm_alloc_frame()` gives 4 KB at a time — too coarse for a 16-byte struct, and no way to free
*part* of a page. A heap sits *on top* of memory and sub-divides it. For B9 the arena is a fixed
64 KB array in `.bss` (simple and self-contained); a production kernel would instead grow the
heap by mapping fresh frames (B7) into virtual space (B8).

### 9.2 — The free-list block

Every block carries a small header, then its payload:

```c
typedef struct block {
    size_t        size;   /* payload bytes */
    int           free;
    struct block *next;
} block_t;
```

`heap_init()` starts with one big free block spanning the whole arena.

### 9.3 — `kmalloc`: first-fit + split

Round the request up to 8 bytes, scan for the first free block large enough, and **split** the
remainder into a new free block:

```c
if (b->free && b->size >= size) {
    if (b->size >= size + sizeof(block_t) + ALIGN) {       /* enough to split */
        block_t *nb = (block_t *)((uint8_t *)b + sizeof(block_t) + size);
        nb->size = b->size - size - sizeof(block_t);
        nb->free = 1; nb->next = b->next;
        b->next = nb; b->size = size;
    }
    b->free = 0;
    return (uint8_t *)b + sizeof(block_t);                 /* payload starts after the header */
}
```

### 9.4 — `kfree`: free + coalesce

Recover the header just before the payload, mark it free, and **merge** with the next block if
it's also free (so freed space can serve a bigger later request):

```c
block_t *b = (block_t *)((uint8_t *)ptr - sizeof(block_t));
b->free = 1;
if (b->next && b->next->free) {                            /* forward coalesce */
    b->size += sizeof(block_t) + b->next->size;
    b->next  = b->next->next;
}
```

(We coalesce forward only; merging with the *previous* block would need a back-link — left as a
refinement.)

### 9.5 — Verify

`kmain` allocates three blocks, writes/reads one, frees the middle one, and allocates again:

```c
char *a = kmalloc(16); void *b = kmalloc(100); void *c = kmalloc(4);
for (i=0;i<16;i++) a[i] = 'A'+i;        /* write/read test */
kfree(b);
void *d = kmalloc(50);                  /* first-fit → reuses b's slot */
```

```bash
make run-b9
```

Expected: increasing addresses for `a`, `b`, `c`; `read back: ABCDEFGHIJKLMNOP` (no corruption);
then after `kfree(b)`, `d = kmalloc(50)` printing the *same* address as `b` → **reuses b? YES**.
**Criterion met: reliable dynamic allocation.**

> **Going further → B10.** With a heap, B10 can allocate per-task stacks and control blocks for
> **multitasking**: context switching and a scheduler so two tasks alternate on screen.
