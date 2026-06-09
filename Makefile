# naos — Makefile
# -----------------------------------------------------------------------------
# B0 : assemble le boot sector (NASM -> binaire plat) et lance QEMU.
# Cibles : `make` (build), `make run` (QEMU), `make clean`.

# Outils (surchargables : `make QEMU=... NASM=...`)
NASM ?= nasm
QEMU ?= qemu-system-i386

# Répertoires / artefacts
BUILD    := build
BOOT_DIR := boot
BOOT_BIN := $(BUILD)/boot.bin
IMAGE    := $(BUILD)/naos.img

.PHONY: all run debug clean distclean

all: $(IMAGE)

# Boot sector : NASM en format binaire plat (-f bin), pas d'ELF.
$(BOOT_BIN): $(BOOT_DIR)/boot.asm | $(BUILD)
	$(NASM) -f bin $< -o $@

# Image disque. En B0, l'image EST le boot sector (512 o). Les briques suivantes
# y ajouteront le kernel.
$(IMAGE): $(BOOT_BIN)
	cp $(BOOT_BIN) $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

# Lance QEMU sur l'image brute. Le BIOS (SeaBIOS) charge le secteur 0 à 0x7C00.
run: $(IMAGE)
	$(QEMU) -drive format=raw,file=$(IMAGE)

# Lance QEMU figé, en attente d'un débogueur GDB.
#   -s : ouvre un stub GDB sur le port 1234   -S : gèle le CPU au démarrage
# Dans un autre terminal :  gdb -> target remote :1234 -> b *0x7c00 -> continue
debug: $(IMAGE)
	$(QEMU) -drive format=raw,file=$(IMAGE) -s -S

clean:
	rm -rf $(BUILD)

# Remet le dépôt à l'état du DERNIER COMMIT : annule les modifications locales
# (suivies) ET supprime tous les fichiers non suivis/ignorés — SAUF .claude/
# (réglages locaux). Destructif et irréversible : demande confirmation.
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
