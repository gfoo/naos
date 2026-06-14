# naos — PLAN

> OS pédagogique x86, construit de A à Z sur QEMU. Objectif : **comprendre** chaque
> couche, des premiers principes au multitâche. Statut binaire : `❌` pas commencé,
> `✅` fait. "En cours" = existence d'une branche/PR sur GitHub.

> **Méthode.** Chaque brique est consignée au fil de l'eau dans `docs/HOWTO.md`, un
> document explicatif et formateur, écrit **en parallèle** du code (pas après coup). Double
> exigence :
> 1. **Rejouable de zéro** — un lecteur parti d'une machine vierge doit pouvoir reproduire
>    *exactement* la brique en suivant le HOWTO : commandes, fichiers, contenus, vérification.
>    C'est une recette reproductible, pas seulement un récit.
> 2. **Formateur** — il explique le *pourquoi* de chaque étape, pour comprendre et apprendre,
>    pas juste copier.
>
> Il permet ainsi une **réalisation à la main** par le lecteur **ou avec l'aide de l'assistant**
> — choix laissé à l'utilisateur à chaque brique. Règle de travail : aucune brique n'est
> « finie » tant que sa section HOWTO ne permet pas de la rejouer intégralement.

## 0. Choix techniques (à discuter point par point)

| # | Point | Choix proposé | Statut |
|---|-------|---------------|--------|
| C1 | Architecture | x86 32-bit (i386) — suffisant pour un projet pédagogique | ✅ validé |
| C2 | Émulateur | QEMU (`qemu-system-i386`) en runner + Bochs en débogueur de secours (boot/GDT/IDT) | ✅ validé |
| C3 | Langages | C (kernel) + NASM (boot & glue) — syntaxe Intel, pas de C++/Rust pour rester aux fondamentaux | ✅ validé |
| C4 | Stratégie de boot | **Hybride** : boot sector maison (comprendre, jetable) → puis GRUB/Multiboot (déléguer) | ✅ validé |
| C5 | Toolchain | cross-compiler `i686-elf-gcc` construit dès B0 — zéro hypothèse hôte (reco OSDev) | ✅ validé |
| C6 | Workflow | **Commit direct sur `main`** (projet perso solo) : pas de branche/PR/issue ; commits conventionnels `feat(Bx): ...` / `docs: ...`. Révise le choix initial (issue→branche→PR), abandonné comme trop lourd ici. | ✅ validé |

> Ces 6 points sont discutés un par un avant tout code. Voir le détail de chaque choix
> et son raisonnement plus bas.

## 1. Le système de boot (référence)

Chaîne x86 avant le code C :

1. Power-on → CPU en **real mode** 16-bit ; exécution du firmware (BIOS).
2. **BIOS POST** → recherche d'un disque bootable.
3. BIOS charge les **512 premiers octets** à l'adresse **`0x7C00`**.
4. Signature obligatoire **`0xAA55`** (offsets 510-511), sinon non-bootable.
5. Saut vers `0x7C00` : `DL` = disque de boot, `CS:IP` → notre code.
6. Boot sector (real mode) : affichage via `int 0x10`, lecture disque `int 0x13`.
7. Passage **mode protégé** 32-bit : A20 + GDT + bit `PE` de `CR0` + far jump.
8. Chargement du kernel (typiquement à **`0x100000`** = 1 Mo) et saut.

Repère mémoire : **`0xB8000`** = buffer texte VGA (80×25, 1 octet char + 1 octet couleur).

> Avec GRUB/Multiboot, les étapes 6-8 sont déléguées : on est livré en mode protégé.
> L'approche hybride fait *d'abord* écrire 6-7 soi-même (comprendre), *puis* déléguer.

## 2. Feuille de route — briques

Chaque brique : théorie → code → vérification observable dans QEMU.

| ID | Brique | Concept appris | Critère de réussite | Statut |
|----|--------|----------------|---------------------|--------|
| B0 | Setup & "It boots" | Toolchain, Makefile, structure | QEMU démarre sans planter | ✅ |
| B1 | Boot sector maison | Real mode, `0x7C00`, `0xAA55`, `int 0x10`, GDT, mode protégé | Message real mode → bascule 32-bit | ✅ |
| B2 | GRUB/Multiboot + kernel C | Header Multiboot, stub ASM→C, linker script, chargement 1 Mo | `kmain()` s'exécute via GRUB | ✅ |
| B3 | Driver écran VGA | Buffer `0xB8000`, couleurs, scroll, `printf`-like | Texte formaté + défilement | ✅ |
| B4 | GDT propre (kernel) | Segmentation, descripteurs | GDT rechargée, pas de triple-fault | ✅ |
| B5 | IDT + interruptions | IDT, PIC, exceptions CPU (ISR/IRQ) | Division par zéro → handler | ✅ |
| B6 | Clavier + timer | IRQ matérielles, ports I/O (`inb`/`outb`), PS/2, PIT | Saisie clavier affichée + tics timer | ❌ |
| B7 | Mémoire physique | Détection RAM (memmap Multiboot), allocateur de frames | Allouer/libérer une page | ❌ |
| B8 | Paging (mémoire virtuelle) | Tables de pages, `CR3`, page faults | Adressage virtuel + higher-half | ❌ |
| B9 | Heap kernel | `kmalloc`/`kfree` | Allocation dynamique fiable | ❌ |
| B10 | Multitâche | Pile par tâche, context switch, ordonnanceur | Deux tâches alternent à l'écran | ❌ |
> **B10 sera réalisé en deux versions.** D'abord la version **classique** (ci-dessus :
> pile par tâche, context switch, ordonnanceur) — elle reste la pierre de Rosette qui
> donne le vocabulaire (TCB, context switch, run queue). Puis une **réinterprétation LLM**
> (piste « B100 », par paliers crescendo) : l'ordonnanceur devient un gestionnaire de
> contexte, les tâches des intentions, le tick timer un message. Voir DESIGN-LOG 2026-06-14
> « naos comme OS intrinsèquement LLM » pour le raisonnement, le mapping par sous-brique
> et le crescendo proposé (B100.0 → B100.4).

> Au-delà (planifié plus tard) : appels système, ring 3, système de fichiers, exécution
> de programmes utilisateur — réinterprétés eux aussi par la piste LLM le moment venu.

> **Briques de « souveraineté » retirées (2026-06-14)** : les anciens B11 (bootloader
> maison, remplacer GRUB) et B12 (passage 64-bit) sont abandonnés — orthogonaux à la
> direction LLM désormais prioritaire pour l'après-B10. Raisonnement dans le DESIGN-LOG.

## 3. Structure de projet cible

```
naos/
├── README.md
├── Makefile                # build + run QEMU + debug GDB
├── linker.ld               # layout mémoire kernel (dès B2)
├── boot/boot.asm           # boot sector (B0-B1) puis header multiboot (B2)
├── kernel/                 # un module par brique (kmain.c, vga.c, gdt.c, idt.c, …)
├── include/                # headers partagés
├── docs/PLAN.md            # ce fichier
└── docs/HOWTO.md           # journal explicatif & formateur, consigné brique par brique
```

## 4. Vérification

- `make run-bN` → QEMU, contrôle visuel d'une brique (itération quotidienne).
- Débogage rapide : `qemu-system-i386 -s -S` + `gdb` (`target remote :1234`).
- Débogage fin (microscope) : `make bochs` → débogueur intégré Bochs pour inspecter
  real mode, GDT/IDT, `CR0`/`CR3` instruction par instruction, et diagnostiquer les
  triple-faults (là où QEMU reset en silence). Réservé aux briques B1–B5 en cas de blocage.
- Chaque brique a un critère observable ; `✅` seulement si le critère passe.
- Pas de tests auto au début ; assertions kernel + harnais QEMU plus tard.

## Sources

- OSDev Wiki : Boot Sequence, Bootloader, Bare Bones (référence).
- robopenguins.com/x86-boot-loading (boot sector 512 octets).
- cs.vu.nl/~herbertb (kernel QEMU+GRUB), alex.dzyoba.com/blog/multiboot.
