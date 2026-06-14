# naos

> Un système d'exploitation x86 minimal, construit de zéro — du boot aux premiers principes.
>
> *Nommé d'après Naos (ζ Puppis), l'une des étoiles les plus chaudes visibles à l'œil nu.*

**naos** est un projet **pédagogique** : écrire un OS x86 32 bits, brique par brique, dans
QEMU, pour *comprendre* chaque couche — du secteur de boot jusqu'au multitâche — au lieu
d'assembler des morceaux tout faits.

## Démarrage rapide

Prérequis (Debian/Ubuntu) :

```bash
sudo apt install -y nasm qemu-system-x86             # B0–B1 (boot sectors)
sudo apt install -y grub-pc-bin xorriso mtools       # B2+ (ISO GRUB)
./toolchain/build-i686-elf.sh                        # cross-compiler i686-elf-gcc (~20-40 min, une fois)
```

Chaque brique a **sa** cible de lancement (pas de `make run` générique : on sait toujours
*quelle* brique on lance) :

```bash
make run-b0     # B0 : « naos B0: it boots! » (boot sector)
make run-b1     # B1 : real mode → mode protégé 32 bits
make run-b2     # B2 : kmain() en C, chargé par GRUB via Multiboot
make run-b3     # B3 : driver écran VGA (couleurs + défilement)
```

Ajoute `QMP=1` pour lancer sans fenêtre + capturer l'écran (`python3 tools/qemu-shot.py`).
Autres cibles :

```bash
make            # construit la dernière brique (build/b3.iso)
make run-kernel # dernier kernel sans GRUB (QEMU -kernel) ; make debug = idem + stub GDB
make clean      # efface build/
make distclean  # remet le dépôt à l'état du dernier commit (confirmation ; préserve .claude/)
```

## Par où commencer

Toute la connaissance du projet tient dans trois documents (le « trépied ») :

| Document | Rôle | Quand le lire |
|---|---|---|
| **[docs/HOWTO.md](docs/HOWTO.md)** | La **recette pas-à-pas**, rejouable de zéro et formatrice, brique par brique. Cours d'assembleur en annexe. | Pour *construire* / apprendre |
| **[docs/PLAN.md](docs/PLAN.md)** | La **feuille de route** : choix techniques (C1–C6) et briques (B0–B12) avec critères de réussite. | Pour savoir *quoi* faire |
| **[docs/DESIGN-LOG.md](docs/DESIGN-LOG.md)** | Le **journal de conception** : le *pourquoi* de chaque choix. | Pour comprendre les décisions |

Le HOWTO peut se suivre **à la main** ou **avec un assistant** — le choix est laissé à chaque brique.

## Choix techniques

x86 32 bits · QEMU (+ Bochs pour le débogage fin) · C + NASM · boot hybride (secteur maison
puis GRUB) · cross-compiler `i686-elf-gcc` · commit direct sur `main` par brique (projet solo).
Détails et raisons dans le DESIGN-LOG.

## Structure

```
naos/
├── boot/        # code d'amorçage (assembleur)
├── kernel/      # noyau (C, à partir de B2)
├── include/     # en-têtes partagés
├── toolchain/   # script de build du cross-compiler
├── Makefile     # build + run QEMU
└── docs/        # PLAN · HOWTO · DESIGN-LOG
```

## Statut

Feuille de route en 13 briques (B0 → B12) ; statuts détaillés dans `docs/PLAN.md`.

- ✅ **B0** — Setup & « It boots »
- ✅ **B1** — Boot sector maison (real mode → mode protégé)
- ✅ **B2** — GRUB / Multiboot + premier kernel C
- ✅ **B3** — Driver écran VGA (couleurs + défilement)
- ⏳ **B4** — GDT propre (kernel)
