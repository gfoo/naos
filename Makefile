# naos — Makefile
# -----------------------------------------------------------------------------
# Une cible `run-bN` par brique, pour lancer et comprendre chacune isolément :
#   make run-b0   → B0 : boot sector minimal (message via int 0x10)        [bin plat]
#   make run-b1   → B1 : real mode → mode protégé 32 bits                  [bin plat]
#   make run-b2   → B2 : kmain() en C, chargé par GRUB (écrit à 0xB8000)   [kernel/ISO]
#   make run-b3   → B3 : driver écran VGA (couleurs + défilement)          [kernel/ISO]
#   make run-kernel / make debug → dernier kernel, sans GRUB (QEMU -kernel)
# (Pas de cible `run` générique : on lance toujours une brique nommée, sans ambiguïté.)
# Deux pipelines : binaire plat (B0/B1, nasm -f bin, 0x7C00) vs kernel ELF
# Multiboot chargé par GRUB (B2+). Les briques kernel sont cumulatives :
# B3 = B2 + driver ; on garde un snapshot du kmain de chaque brique (kmain.bN.c).

# Outils (surchargables : `make QEMU=... CROSS=...`)
NASM  ?= nasm
QEMU  ?= qemu-system-i386
CROSS ?= $(HOME)/opt/cross/bin
CC    := $(CROSS)/i686-elf-gcc

# Répertoires
BUILD := build

# Cross-compilation freestanding : pas de libc, pas d'hypothèse hôte.
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

# Objets par brique kernel (cumulatif : B3 = B2 + driver vga).
B2_OBJS := $(BUILD)/boot.o $(BUILD)/kmain.b2.o
B3_OBJS := $(BUILD)/boot.o $(BUILD)/kmain.o $(BUILD)/vga.o

# Dernière brique (cible de `make run-kernel` / `make debug` / `make` par défaut).
LAST := b3

.PHONY: all run-b0 run-b1 run-b2 run-b3 run-b0-arm run-kernel debug clean distclean

# Socket QMP : ouvert par `make run-bN QMP=1` pour capturer l'écran (tools/qemu-shot.py).
# (bloc ifdef et pas $(if …) : $(if) couperait sur les virgules de `,server,nowait`)
QMP_SOCK  ?= /tmp/naos-qmp.sock
QEMU_OPTS :=
ifdef QMP
QEMU_OPTS := -display none -qmp unix:$(QMP_SOCK),server,nowait
endif

all: $(BUILD)/$(LAST).iso

$(BUILD):
	mkdir -p $(BUILD)

# --- Compilation -------------------------------------------------------------
# Stub Multiboot : NASM en ELF32 (objet relogeable, pas un binaire plat).
$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

# Modules C du kernel (couvre kmain.o, vga.o, et le snapshot kmain.b2.o).
$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# --- Kernels ELF par brique --------------------------------------------------
# Lié avec le cross-gcc (et non ld nu) pour récupérer libgcc. Le layout (kernel
# à 1 Mo, .multiboot en tête) est imposé par linker.ld ; on valide l'en-tête.
$(BUILD)/b2.kernel: $(B2_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(LDFLAGS) $(B2_OBJS)
	@grub-file --is-x86-multiboot $@ && echo "OK : $@ est Multiboot" || (echo "ERREUR Multiboot" && false)

$(BUILD)/b3.kernel: $(B3_OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(LDFLAGS) $(B3_OBJS)
	@grub-file --is-x86-multiboot $@ && echo "OK : $@ est Multiboot" || (echo "ERREUR Multiboot" && false)

# --- ISO bootable GRUB (une par brique kernel) -------------------------------
$(BUILD)/%.iso: $(BUILD)/%.kernel grub/grub.cfg
	mkdir -p $(BUILD)/iso-$*/boot/grub
	cp $< $(BUILD)/iso-$*/boot/naos.kernel
	cp grub/grub.cfg $(BUILD)/iso-$*/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso-$* 2>/dev/null

# --- Boot sectors « binaire plat » (B0/B1) -----------------------------------
# 512 o chargés à 0x7C00 par le BIOS — pipeline distinct du kernel GRUB.
# Sources = snapshots boot/boot.asm.bN.
$(BUILD)/%.bin: boot/boot.asm.% | $(BUILD)
	$(NASM) -f bin $< -o $@

# --- Lancement ---------------------------------------------------------------
# Une cible par brique (pas de `run` générique). Chacune accepte QMP=1
# (headless + socket QMP, pour python3 tools/qemu-shot.py).
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

# --- B0 sur ARM (AArch64) -----------------------------------------------------
# Pendant de la Partie 0 sur ARM (cf. docs/howto/00-setup.md §0.7). Pipeline
# totalement distinct du x86 : cross-compiler aarch64, ELF chargé par QEMU
# 'virt' (-kernel, pas de BIOS/boot sector), sortie sur l'UART série (pas de VGA).
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

# Boot ARM : QEMU 'virt', console série sur le terminal. Quitter : Ctrl-A puis X.
run-b0-arm: $(ARM_ELF)
	$(ARM_QEMU) -machine virt -cpu cortex-a53 -nographic -kernel $(ARM_ELF)

# Itération rapide : QEMU charge le dernier kernel SANS GRUB (loader -kernel).
run-kernel: $(BUILD)/$(LAST).kernel
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -kernel $< $(QEMU_OPTS)

# QEMU figé en attente de GDB (port 1234). Autre terminal : gdb -> target remote :1234.
debug: $(BUILD)/$(LAST).kernel
	$(QEMU) -kernel $< -s -S

clean:
	rm -rf $(BUILD)

# Remet le dépôt à l'état du DERNIER COMMIT : annule les modifications locales
# (suivies) ET supprime tous les fichiers non suivis/ignorés — SAUF .claude/.
# Destructif et irréversible : demande confirmation.
distclean:
	@echo "⚠  distclean va :"
	@echo "   - annuler toutes tes modifications locales (git reset --hard)"
	@echo "   - supprimer les fichiers non suivis/ignorés ci-dessous (sauf .claude/) :"
	@git clean -ndx -e .claude | sed 's/^/     /' || true
	@printf "Continuer ? [y/N] "; read ans; \
	  if [ "$$ans" = "y" ]; then \
	    git reset --hard HEAD && git clean -fdx -e .claude; \
	  else \
	    echo "Annulé."; \
	  fi
