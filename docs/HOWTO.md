# naos — HOWTO: building an x86 OS from scratch

> A **reproducible** and **instructional** guide. Starting from a clean machine, you rebuild
> naos brick by brick, understanding the *why* of every step. Companion to `PLAN.md` (the
> roadmap, the *what*) and `DESIGN-LOG.md` (the decisions, the *why behind the choices*); here
> is the **step-by-step recipe**.

## Contents

This portal holds the shared intro; **the guide itself is split by part**, one file per brick
under [`howto/`](howto/) (easier to read and maintain):

| Part | Brick | File |
|------|-------|------|
| 0 | Setup, QEMU & first boot | [howto/00-setup.md](howto/00-setup.md) |
| 1 | B1 — real mode → 32-bit protected mode | [howto/01-protected-mode.md](howto/01-protected-mode.md) |
| 2 | B2 — GRUB + Multiboot + first C kernel | [howto/02-multiboot.md](howto/02-multiboot.md) |
| 3 | B3 — VGA text driver | [howto/03-vga.md](howto/03-vga.md) |
| 4 | B4 — kernel-owned GDT | [howto/04-gdt.md](howto/04-gdt.md) |
| 5 | B5 — IDT + CPU exceptions | [howto/05-idt.md](howto/05-idt.md) |
| 6 | B6 — keyboard + timer (IRQs) | [howto/06-keyboard-timer.md](howto/06-keyboard-timer.md) |
| 7 | B7 — physical memory | [howto/07-pmm.md](howto/07-pmm.md) |
| 8 | B8 — paging (virtual memory) | [howto/08-paging.md](howto/08-paging.md) |
| 9 | B9 — kernel heap (kmalloc/kfree) | [howto/09-heap.md](howto/09-heap.md) |
| — | Appendices A (x86 asm) · B (`boot.asm` line by line) · C (`int 0x10`) | [howto/annexes.md](howto/annexes.md) |

On this page: [Goal](#goal) · [Prerequisites](#prerequisites) ·
[How to read this guide](#how-to-read-this-guide) · [The bricks](#the-bricks).

## Goal

By the end of this guide, you'll have:

- a **32-bit** x86 OS that boots in QEMU, from boot sector to multitasking;
- **understood every layer**: real mode, protected mode, GDT/IDT, interrupts, physical
  memory, paging, heap, scheduler;
- a **reproducible build chain** (`i686-elf-gcc` cross-compiler, Makefile);
- the ability to **replay or modify each brick** on your own.

### Prerequisites

- Linux (or WSL2 / macOS with minor tweaks), comfortable on the command line.
- **No** prior knowledge of assembly or the x86 architecture is required: each concept is
  introduced the moment it's needed.

## How to read this guide

The guide follows stable conventions, worth learning once:

> **`command`** — a callout before each important command: what it does, in one sentence.
> Understand *before* running.

```bash
# copy-pasteable code block, run as is
echo "example"
```

> **Why?** — an aside explaining the reasoning behind a step. Read it to *understand*, not
> just to copy.

> **Key point** — don't miss it: often a check, a classic pitfall, or a reason why "it
> doesn't work".

**Skeleton of each brick (Part).** Every part follows the same plan, so you always know where
you are:

1. **Concept** — the brick's theory, explained simply.
2. **Key terms** — the vocabulary introduced, defined in one line.
3. **Reproducible steps** — files to create and commands to run, copy-pasteable.
4. **Verification** — the success criterion observable in QEMU (the same as in `PLAN.md`).
5. **Going further** — pitfalls, variants, and links to the next bricks.

> **Key point — project rule.** A brick is "done" only when its Part here lets you **replay it
> in full** from scratch. The HOWTO is written *alongside* the code, never after the fact.

## The bricks

Each brick maps to a Part of this guide (filled in as we go). See `PLAN.md` for the details of
concepts and criteria.

| Part | Brick | Status |
|------|-------|--------|
| [0](howto/00-setup.md) | B0 — Setup & "It boots" (toolchain, Makefile, structure) | ✅ done |
| [1](howto/01-protected-mode.md) | B1 — Home-made boot sector (real mode → protected mode) | ✅ done |
| [2](howto/02-multiboot.md) | B2 — GRUB/Multiboot + C kernel | ✅ done |
| [3](howto/03-vga.md) | B3 — VGA screen driver | ✅ done |
| [4](howto/04-gdt.md) | B4 — Proper GDT (kernel) | ✅ done |
| [5](howto/05-idt.md) | B5 — IDT + CPU exceptions | ✅ done |
| [6](howto/06-keyboard-timer.md) | B6 — Keyboard + timer | ✅ done |
| [7](howto/07-pmm.md) | B7 — Physical memory | ✅ done |
| [8](howto/08-paging.md) | B8 — Paging (virtual memory) | ✅ done |
| [9](howto/09-heap.md) | B9 — Kernel heap | ✅ done |
| 10 | B10 — Multitasking | planned |

---

> **The parts live in [`howto/`](howto/)** — one per brick, added as we go (each follows the
> skeleton above). Start with [howto/00-setup.md](howto/00-setup.md).
