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

.PHONY: all run clean

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

clean:
	rm -rf $(BUILD)
