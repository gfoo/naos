# naos — Makefile
# -----------------------------------------------------------------------------
# B2/B3 : kernel C (i686-elf) + stub Multiboot, chargé par GRUB depuis une ISO.
# Cibles : `make` (ISO), `make run` (QEMU/GRUB), `make run-kernel` (QEMU sans
#          GRUB, via -kernel), `make debug` (GDB), `make clean`.
# Le boot sector maison de B1 est conservé dans boot/boot.asm.b1 (cf. HOWTO §1).

# Outils (surchargables : `make QEMU=... CROSS=...`)
NASM  ?= nasm
QEMU  ?= qemu-system-i386
CROSS ?= $(HOME)/opt/cross/bin
CC    := $(CROSS)/i686-elf-gcc

# Répertoires / artefacts
BUILD   := build
ISO_DIR := $(BUILD)/iso
KERNEL  := $(BUILD)/naos.kernel
ISO     := $(BUILD)/naos.iso

# Cross-compilation freestanding : pas de libc, pas d'hypothèse hôte.
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc

OBJS := $(BUILD)/boot.o $(BUILD)/kmain.o $(BUILD)/vga.o

.PHONY: all run run-kernel debug clean distclean

# Socket QMP : ouvert par `make run QMP=1` pour capturer l'écran (tools/qemu-shot.py).
# (bloc ifdef et pas $(if …) : $(if) couperait sur les virgules de `,server,nowait`)
QMP_SOCK  ?= /tmp/naos-qmp.sock
QEMU_OPTS :=
ifdef QMP
QEMU_OPTS := -display none -qmp unix:$(QMP_SOCK),server,nowait
endif

all: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

# Stub Multiboot : NASM en ELF32 (objet relogeable, pas un binaire plat).
$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

# Modules C du kernel.
$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Édition de liens : layout imposé par linker.ld (kernel à 1 Mo). On lie avec le
# cross-gcc (et non ld direct) pour récupérer libgcc (helpers arithmétiques).
$(KERNEL): $(OBJS) linker.ld
	$(CC) -T linker.ld -o $@ $(LDFLAGS) $(OBJS)
	@grub-file --is-x86-multiboot $(KERNEL) \
	  && echo "OK : $(KERNEL) est un binaire Multiboot valide" \
	  || (echo "ERREUR : en-tête Multiboot invalide" && false)

# Image ISO bootable : kernel + config GRUB, empaquetés par grub-mkrescue.
$(ISO): $(KERNEL) grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/naos.kernel
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null

# Démarrage normal : QEMU boote l'ISO, GRUB charge le kernel (le vrai chemin B2).
#   make run        → fenêtre
#   make run QMP=1  → headless + socket QMP (capture : python3 tools/qemu-shot.py)
run: $(ISO)
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -cdrom $(ISO) $(QEMU_OPTS)

# Itération rapide : QEMU charge le kernel SANS GRUB (loader Multiboot intégré).
# Pratique pour boucler vite ; le vrai test B2 reste `make run` (via GRUB).
run-kernel: $(KERNEL)
	@$(if $(QMP),rm -f $(QMP_SOCK))
	$(QEMU) -kernel $(KERNEL) $(QEMU_OPTS)

# QEMU figé en attente de GDB (port 1234). Autre terminal : gdb -> target remote :1234.
debug: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -s -S

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
