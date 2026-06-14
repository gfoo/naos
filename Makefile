# naos — Makefile
# -----------------------------------------------------------------------------
# One `run-bN` target per brick, to launch and understand each one in isolation:
#   make run-b0   → B0: minimal boot sector (message via int 0x10)          [flat bin]
#   make run-b1   → B1: real mode → 32-bit protected mode                   [flat bin]
#   make run-b2   → B2: kmain() in C, loaded by GRUB (writes to 0xB8000)    [kernel/ISO]
#   make run-b3   → B3: VGA screen driver (colors + scrolling)              [kernel/ISO]
#   make run-b4   → B4: kernel-owned GDT (null/code/data, ring0+ring3)      [kernel/ISO]
#   make run-kernel / make debug → latest kernel, without GRUB (QEMU -kernel)
# (No generic `run` target: we always launch a named brick, no ambiguity.)
# Two pipelines: flat binary (B0/B1, nasm -f bin, 0x7C00) vs ELF kernel
# Multiboot loaded by GRUB (B2+). The kernel bricks are cumulative:
# B3 = B2 + driver; we keep a snapshot of each brick's kmain (kmain.bN.c).

# Tools (overridable: `make QEMU=... CROSS=...`)
NASM  ?= nasm
QEMU  ?= qemu-system-i386
CROSS ?= $(HOME)/opt/cross/bin
CC    := $(CROSS)/i686-elf-gcc

# Directories
BUILD := build

# Freestanding cross-compilation: no libc, no host assumptions.
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

# Objects per kernel brick (cumulative: each brick = the previous one + new modules).
# kmain is snapshotted per brick (kmain.bN.c); modules (vga, gdt…) are added once.
B2_OBJS := $(BUILD)/boot.o $(BUILD)/kmain.b2.o
B3_OBJS := $(BUILD)/boot.o $(BUILD)/kmain.b3.o $(BUILD)/vga.o
B4_OBJS := $(BUILD)/boot.o $(BUILD)/kmain.o    $(BUILD)/vga.o $(BUILD)/gdt.o $(BUILD)/gdt_flush.o

# Latest brick (target of `make run-kernel` / `make debug` / default `make`).
LAST := b4

.PHONY: all run-b0 run-b1 run-b2 run-b3 run-b4 run-b0-arm run-kernel debug clean distclean

# QMP socket: opened by `make run-bN QMP=1` to capture the screen (tools/qemu-shot.py).
# (ifdef block and not $(if …): $(if) would cut on the commas of `,server,nowait`)
QMP_SOCK  ?= /tmp/naos-qmp.sock
QEMU_OPTS :=
ifdef QMP
QEMU_OPTS := -display none -qmp unix:$(QMP_SOCK),server,nowait
endif

all: $(BUILD)/$(LAST).iso

$(BUILD):
	mkdir -p $(BUILD)

# --- Compilation -------------------------------------------------------------
# Multiboot stub: NASM in ELF32 (relocatable object, not a flat binary).
$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

# Kernel C modules (covers kmain.o, vga.o, gdt.o, and the kmain.bN.o snapshots).
$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel ASM modules (e.g. gdt_flush.asm): NASM in ELF32, like boot.o.
$(BUILD)/%.o: kernel/%.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

# --- ELF kernels per brick ---------------------------------------------------
# Linked with the cross-gcc (and not bare ld) to pull in libgcc. The layout (kernel
# at 1 MB, .multiboot first) is imposed by linker.ld; we validate the header.
$(BUILD)/b2.kernel: $(B2_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(LDFLAGS) $(B2_OBJS)
	@grub-file --is-x86-multiboot $@ && echo "OK: $@ is Multiboot" || (echo "Multiboot ERROR" && false)

$(BUILD)/b3.kernel: $(B3_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(LDFLAGS) $(B3_OBJS)
	@grub-file --is-x86-multiboot $@ && echo "OK: $@ is Multiboot" || (echo "Multiboot ERROR" && false)

$(BUILD)/b4.kernel: $(B4_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(LDFLAGS) $(B4_OBJS)
	@grub-file --is-x86-multiboot $@ && echo "OK: $@ is Multiboot" || (echo "Multiboot ERROR" && false)

# --- GRUB bootable ISO (one per kernel brick) --------------------------------
$(BUILD)/%.iso: $(BUILD)/%.kernel grub/grub.cfg
	mkdir -p $(BUILD)/iso-$*/boot/grub
	cp $< $(BUILD)/iso-$*/boot/naos.kernel
	cp grub/grub.cfg $(BUILD)/iso-$*/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso-$* 2>/dev/null

# --- "Flat binary" boot sectors (B0/B1) --------------------------------------
# 512 B loaded at 0x7C00 by the BIOS — distinct pipeline from the GRUB kernel.
# Sources = boot/boot.asm.bN snapshots.
$(BUILD)/%.bin: boot/boot.asm.% | $(BUILD)
	$(NASM) -f bin $< -o $@

# --- Launch ------------------------------------------------------------------
# One target per brick (no generic `run`). Each one accepts QMP=1
# (headless + QMP socket, for python3 tools/qemu-shot.py).
run-b0: $(BUILD)/b0.bin
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -drive format=raw,file=$< $(QEMU_OPTS)

run-b1: $(BUILD)/b1.bin
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -drive format=raw,file=$< $(QEMU_OPTS)

run-b2: $(BUILD)/b2.iso
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -cdrom $< $(QEMU_OPTS)

run-b3: $(BUILD)/b3.iso
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -cdrom $< $(QEMU_OPTS)

run-b4: $(BUILD)/b4.iso
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -cdrom $< $(QEMU_OPTS)

# --- B0 on ARM (AArch64) ------------------------------------------------------
# Counterpart of Part 0 on ARM (cf. docs/howto/00-setup.md §0.7). Pipeline
# entirely distinct from x86: aarch64 cross-compiler, ELF loaded by QEMU
# 'virt' (-kernel, no BIOS/boot sector), output on the UART serial (no VGA).
ARM_CC     ?= aarch64-linux-gnu-gcc
ARM_QEMU   ?= qemu-system-aarch64
ARM_CFLAGS := -ffreestanding -nostdlib -mgeneral-regs-only -O2 -Wall -Wextra
ARM_ELF    := $(BUILD)/arm/naos-arm.elf

$(BUILD)/arm:
	mkdir -p $(BUILD)/arm

$(BUILD)/arm/boot.o: boot/arm/boot.S | $(BUILD)/arm
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(BUILD)/arm/kmain.o: kernel/arm/kmain.c | $(BUILD)/arm
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@

$(ARM_ELF): $(BUILD)/arm/boot.o $(BUILD)/arm/kmain.o boot/arm/linker.ld
	$(ARM_CC) $(ARM_CFLAGS) -Wl,--no-warn-rwx-segments \
	  -T boot/arm/linker.ld -o $@ $(BUILD)/arm/boot.o $(BUILD)/arm/kmain.o -lgcc

# ARM boot: QEMU 'virt', serial console on the terminal. Quit: Ctrl-A then X.
run-b0-arm: $(ARM_ELF)
	$(ARM_QEMU) -machine virt -cpu cortex-a53 -nographic -kernel $(ARM_ELF)

# Fast iteration: QEMU loads the latest kernel WITHOUT GRUB (-kernel loader).
run-kernel: $(BUILD)/$(LAST).kernel
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -kernel $< $(QEMU_OPTS)

# QEMU frozen waiting for GDB (port 1234). Other terminal: gdb -> target remote :1234.
debug: $(BUILD)/$(LAST).kernel
	$(QEMU) -kernel $< -s -S

clean:
	rm -rf $(BUILD)

# Resets the repo to the state of the LAST COMMIT: discards local (tracked)
# modifications AND removes all untracked/ignored files — EXCEPT .claude/.
# Destructive and irreversible: asks for confirmation.
distclean:
	@echo "⚠  distclean will:"
	@echo "   - discard all your local modifications (git reset --hard)"
	@echo "   - remove the untracked/ignored files listed below (except .claude/):"
	@git clean -ndx -e .claude | sed 's/^/     /' || true
	@printf "Continue? [y/N] "; read ans; \
	  if [ "$$ans" = "y" ]; then \
	    git reset --hard HEAD && git clean -fdx -e .claude; \
	  else \
	    echo "Cancelled."; \
	  fi
