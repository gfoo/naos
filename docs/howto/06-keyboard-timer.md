[← HOWTO contents](../HOWTO.md)

## Part 6 — B6: keyboard and timer (hardware IRQs)

B5 handled CPU *exceptions*. B6 wires the first **hardware interrupts** (IRQs): the **timer**
(PIT, IRQ0) and the **keyboard** (PS/2, IRQ1). Unlike exceptions, IRQ handlers must *return*
(`iret`) and acknowledge the controller. We also talk to devices through **port I/O** for the
first time.

> **Files:** `include/ports.h`, `pic.h`/`pic.c`, `irq.h`/`irq.c`, `irq_stubs.asm`,
> `timer.h`/`timer.c`, `keyboard.h`/`keyboard.c` (+ `vga_write_at`). Build/run: `make run-b6`.

**In this part:**
- 6.1 — Port I/O: `inb` / `outb`
- 6.2 — Remapping the PIC (and why)
- 6.3 — IRQ stubs and dispatch (EOI, return)
- 6.4 — The PIT timer (IRQ0)
- 6.5 — The PS/2 keyboard (IRQ1)
- 6.6 — Verify

**Key terms (quick reference):**

- **IRQ** — a hardware interrupt request line (IRQ0 timer, IRQ1 keyboard…).
- **PIC** (8259) — the chip that funnels IRQ lines to the CPU; two chained (master/slave).
- **Port I/O** — `inb`/`outb` on I/O ports (separate address space from memory) to talk to devices.
- **PIT** (8253/8254) — the Programmable Interval Timer; channel 0 fires IRQ0 at a chosen rate.
- **EOI** (*End Of Interrupt*) — the acknowledgement the handler sends the PIC so it delivers the next IRQ.

---

### 6.1 — Port I/O: `inb` / `outb`

Devices like the PIC, PIT and keyboard live in the **I/O port** address space, not memory. x86
reaches them with the `in`/`out` instructions, which C can't express — so `include/ports.h`
wraps them in inline asm:

```c
static inline void    outb(uint16_t port, uint8_t val);
static inline uint8_t inb(uint16_t port);
```

### 6.2 — Remapping the PIC (and why)

By default the master PIC delivers IRQ0–7 on **vectors 8–15** — which **collide with CPU
exceptions** (IRQ0 timer would look like `#DF` double fault!). So the first thing B6 does is
remap: master → vectors **32–39**, slave → **40–47** (`pic_remap()` in `pic.c`, via the 8259's
ICW init sequence). Vectors 32+ are exactly the IDT gates B5 left free.

> **Why ack with EOI.** After handling an IRQ, the handler must send an **End Of Interrupt** to
> the PIC (`pic_send_eoi`), or the controller won't deliver that line again. For IRQ 8–15 you ack
> *both* the slave and the master.

### 6.3 — IRQ stubs and dispatch (EOI, return)

`irq_stubs.asm` mirrors the exception stubs (vectors 32–47, uniform frame) but funnels into
`irq_common` → `irq_handler()` and then **returns** (`iret`) instead of panicking. The C
dispatcher routes to a registered per-IRQ handler and always acks:

```c
void irq_handler(struct registers *r) {
    int irq = r->int_no - 32;
    if (handlers[irq]) handlers[irq](r);
    pic_send_eoi(irq);                 /* acknowledge, even with no handler */
}
```

Drivers register with `irq_install_handler(irq, fn)`. The IDT now installs gates 0–31 (B5
exceptions) **and** 32–47 (these IRQ stubs).

### 6.4 — The PIT timer (IRQ0)

`timer_init(hz)` programs PIT channel 0: divisor = `1193182 / hz`, written to port `0x40` after
a mode byte to `0x43`. Its IRQ0 handler just increments a `ticks` counter:

```c
uint32_t divisor = 1193182 / hz;
outb(0x43, 0x36);                          /* channel 0, lo/hi, square wave */
outb(0x40, divisor & 0xFF);
outb(0x40, (divisor >> 8) & 0xFF);
irq_install_handler(0, on_tick);           /* on_tick: ticks++ */
```

### 6.5 — The PS/2 keyboard (IRQ1)

The IRQ1 handler reads the **scancode** from port `0x60`, ignores key-releases (high bit set),
maps the press to ASCII (scancode set 1), and echoes it:

```c
uint8_t sc = inb(0x60);
if (sc & 0x80) return;                     /* release */
char c = scancode_ascii[sc & 0x7F];
if (c) vga_putchar(c);
```

### 6.6 — Verify

`kmain` does `gdt_init(); idt_init(); pic_remap(); timer_init(100); keyboard_init();` then `sti`,
and idles in `hlt`, refreshing a `ticks: N` counter (via `vga_write_at`, top-right) whenever it
changes, while typed keys echo at the cursor.

```bash
make run-b6     # type keys → they appear; the tick counter climbs top-right
```

Headless, you can inject keys over QMP (`send-key`) and screenshot: you'll see the counter at,
say, `ticks: 261` and the typed text `> hello`. **Criterion met: keyboard input displayed +
timer ticks.**

> **Going further → B7.** With interrupts working, B7 turns to memory: read the **Multiboot
> memory map** GRUB gave us and build a **physical frame allocator** (allocate/free a page).
