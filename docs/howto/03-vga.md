[← HOWTO contents](../HOWTO.md)

## Part 3 — B3: VGA screen driver

In B2, `kmain` wrote to `0xB8000` by hand, byte by byte. That doesn't scale: we want
a real **screen driver** — a small API (`vga_write`, colors, scrolling) that everything
else in the OS will display through. B3 turns "lay down bytes" into a reusable abstraction.

> **Where this code lives.** `include/vga.h` (the API), `kernel/vga.c` (the implementation),
> `kernel/kmain.c` (which uses it). Compiled by the same `Makefile` as in B2.

**In this part:**
- 3.1 — The VGA text buffer, in detail
- 3.2 — The attribute byte: encoding colors
- 3.3 — The API and the driver state
- 3.4 — Clearing the screen: `vga_init`
- 3.5 — `vga_putchar`: control characters, cursor, line wrap
- 3.6 — Scrolling
- 3.7 — The demo in `kmain`
- 3.8 — Verify

**Key terms (quick reference):**

- **VGA text buffer** — RAM mapped at `0xB8000`: 80×25 2-byte **cells**, displayed by the hardware.
- **Cell** — `uint16_t`: low byte = character (CP437), high byte = color **attribute**.
- **Attribute** — 1 byte: bits 0-3 = foreground, bits 4-6 = background, bit 7 = blink.
- **CP437** — the VGA text character set (ASCII + accents, box-drawing characters, etc.).
- **Scroll** — when the cursor goes past the 25th row, everything scrolls up by one row.

---

### 3.1 — The VGA text buffer, in detail

The VGA hardware in text mode continuously reads a region of RAM at **`0xB8000`** and displays it: a
grid of **80 columns × 25 rows**. Each slot is a **2-byte cell**:

```
 cell = | byte 0: character (CP437 code) | byte 1: attribute (color) |
 cell address (row y, column x) = 0xB8000 + (y * 80 + x) * 2
```

In C, we therefore treat the buffer as an array of `uint16_t` (2 bytes) — one word per cell:

```c
#define VGA_MEM  ((volatile uint16_t *)0xB8000)
#define COLS 80
#define ROWS 25
```

> **Why `volatile` (B2 recap).** The compiler must never "optimize away" our
> writes: they have a hardware side effect (displaying). `volatile` forbids that.

### 3.2 — The attribute byte: encoding colors

The high byte of each cell is the **attribute**. VGA text has **16 colors**; an attribute
combines foreground (4 bits) and background (3 or 4 bits):

```
 bit  7   6 5 4   3 2 1 0
      │   └─┬─┘   └──┬──┘
    blink  bg    foreground
```

Hence the encoding `attribute = foreground | (background << 4)`. We name the 16 colors in an `enum`
(`VGA_BLACK`=0 … `VGA_WHITE`=15):

```c
void vga_set_color(enum vga_color fg, enum vga_color bg) {
    color = (uint8_t)fg | (uint8_t)(bg << 4);
}
```

And a cell is built by gluing character + attribute together:

```c
static inline uint16_t cell(char c, uint8_t attr) {
    return (uint16_t)(unsigned char)c | ((uint16_t)attr << 8);
}
```

### 3.3 — The API and the driver state

`include/vga.h` exposes the useful minimum, and the driver keeps a small **global state** (cursor
position + current color):

```c
void vga_init(void);                              /* clear, cursor at (0,0) */
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_putchar(char c);                         /* the core: lay down + cursor + scroll */
void vga_write(const char *s);                    /* loop over a C string */
```

```c
static size_t  row, col;     /* logical cursor */
static uint8_t color;        /* current attribute */
```

> **Freestanding, recap.** No `string.h`, no `stdio`. `vga_write` is just
> `while (*s) vga_putchar(*s++);`. The types `uint16_t`/`size_t` come from `<stdint.h>` /
> `<stddef.h>`, provided by gcc even freestanding ("self-contained" headers).

### 3.4 — Clearing the screen: `vga_init`

Clearing = fill all 80×25 cells with spaces in the current color, then bring the
cursor back to the top-left:

```c
void vga_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (size_t y = 0; y < ROWS; y++)
        for (size_t x = 0; x < COLS; x++)
            VGA_MEM[y * COLS + x] = cell(' ', color);
    row = col = 0;
}
```

### 3.5 — `vga_putchar`: control characters, cursor, line wrap

This is the core of the driver. It distinguishes **control characters** (`\n \r \t \b`) from
ordinary text, advances the cursor, handles line wrapping at the end of a column, and triggers
scrolling at the bottom of the screen:

```c
void vga_putchar(char c) {
    switch (c) {
    case '\n': col = 0; row++;            break;   /* newline */
    case '\r': col = 0;                   break;   /* carriage return */
    case '\b': if (col) col--;            break;   /* backspace */
    case '\t': col = (col + 8) & ~(size_t)7; break;/* tab (multiple of 8) */
    default:
        VGA_MEM[row * COLS + col] = cell(c, color);
        col++;
    }
    if (col >= COLS) { col = 0; row++; }           /* column overflow -> next line */
    if (row >= ROWS) scroll();                     /* row overflow    -> scroll */
}
```

> **Why `(col + 8) & ~7`.** This rounds up to the next multiple of 8: tabs
> land on columns 8, 16, 24… `& ~7` zeroes out the low 3 bits.

### 3.6 — Scrolling

When `row` reaches 25, we **scroll** rows 1→24 up to 0→23, **clear** the last one, and
keep the cursor on that last row:

```c
static void scroll(void) {
    for (size_t y = 1; y < ROWS; y++)                       /* scroll up one row */
        for (size_t x = 0; x < COLS; x++)
            VGA_MEM[(y - 1) * COLS + x] = VGA_MEM[y * COLS + x];
    for (size_t x = 0; x < COLS; x++)                       /* clear the last one */
        VGA_MEM[(ROWS - 1) * COLS + x] = cell(' ', color);
    row = ROWS - 1;
}
```

> **This is "software" scrolling.** We actually copy the video RAM. VGA can also
> scroll "in hardware" (by shifting the scan start address) — faster, but
> more subtle. For B3, copying is clear and plenty fast enough.

### 3.7 — The demo in `kmain`

`kmain` exercises the whole driver: a colored header, then **30 numbered lines** — since
the screen only shows 25, the first ones scroll off-screen (proof of scrolling):

```c
void kmain(void) {
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B3: VGA driver online.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Booted by GRUB via Multiboot; kmain() runs in 32-bit C.\n\n");

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    for (unsigned int i = 1; i <= 30; i++) {           /* 30 > 25 -> it scrolls */
        vga_write("  line "); put_uint(i); vga_putchar('\n');
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("\nnaos B3: scroll OK.");
    for (;;) __asm__ volatile ("hlt");
}
```

`put_uint` is a local mini itoa (no `printf` in freestanding): it prints an integer by
breaking it down into digits. A real `printf`-like will come later.

### 3.8 — Verify

```bash
make run-b3                   # (= make run: B3 is the latest brick); run-kernel to iterate
make run-b3 QMP=1             # (other terminal) python3 tools/qemu-shot.py
```

Expected: a green/grey header, then numbered cyan lines whose first ones have
**scrolled** off the screen, and "naos B3: scroll OK." at the bottom. **Formatted text + colors +
scrolling**: **B3 criterion met.**

> **Next (B3+) — the font.** Replacing the CP437 font from ROM with our own (see
> `tools/vgafont.py`, `assets/fonts/ibm_vga_8x16.bin`) is done by writing into the VGA
> character generator — a later step of the driver.

---

