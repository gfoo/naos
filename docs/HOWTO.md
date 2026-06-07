# naos — HOWTO : construire un OS x86 de zéro

> Guide **rejouable** et **formateur**. Parti d'une machine vierge, vous reproduisez naos
> brique par brique, en comprenant le *pourquoi* de chaque étape. Compagnon de `PLAN.md`
> (la feuille de route, le *quoi*) et de `DESIGN-LOG.md` (les décisions, le *pourquoi des
> choix*) ; ici, c'est la **recette pas-à-pas**.

## Table des matières

- [Objectif](#objectif)
  - [Prérequis](#prérequis)
- [Comment lire ce guide](#comment-lire-ce-guide)
- [Les briques](#les-briques)

## Objectif

À la fin de ce guide, vous aurez :

- un OS x86 **32-bit** qui boote dans QEMU, du secteur de boot au multitâche ;
- **compris chaque couche** : real mode, mode protégé, GDT/IDT, interruptions, mémoire
  physique, paging, heap, ordonnanceur ;
- une **chaîne de build reproductible** (cross-compiler `i686-elf-gcc`, Makefile) ;
- la capacité de **rejouer ou modifier chaque brique** de façon autonome.

### Prérequis

- Linux (ou WSL2 / macOS avec adaptations mineures), à l'aise en ligne de commande.
- **Aucune** connaissance préalable de l'assembleur ou de l'architecture x86 n'est requise :
  chaque concept est introduit au moment où il sert.

## Comment lire ce guide

Le guide suit des conventions stables, à connaître une fois :

> **`commande`** — encadré qui précède chaque commande importante : ce qu'elle fait, en une
> phrase. On comprend *avant* d'exécuter.

```bash
# bloc de code copiable, à exécuter tel quel
echo "exemple"
```

> **Pourquoi ?** — aparté qui explique le raisonnement derrière une étape. À lire pour
> *comprendre*, pas seulement copier.

> **Point clé** — à ne pas rater : souvent une vérification, un piège classique, ou une
> raison pour laquelle « ça ne marche pas ».

**Squelette de chaque brique (Partie).** Toutes les parties suivent le même plan, pour que
vous sachiez toujours où vous êtes :

1. **Concept** — la théorie de la brique, expliquée simplement.
2. **Termes clés** — le vocabulaire introduit, défini en une ligne.
3. **Étapes reproductibles** — fichiers à créer et commandes à lancer, copiables tels quels.
4. **Vérification** — le critère de réussite observable dans QEMU (le même que dans `PLAN.md`).
5. **Pour aller plus loin** — pièges, variantes, et liens vers les briques suivantes.

> **Point clé — règle du projet.** Une brique n'est « finie » que lorsque sa Partie ici
> permet de la **rejouer intégralement** depuis zéro. Le HOWTO est écrit *en parallèle* du
> code, jamais après coup.

## Les briques

Chaque brique correspond à une Partie de ce guide (remplie au fil de l'eau). Voir `PLAN.md`
pour le détail des concepts et critères.

| Partie | Brique | Statut |
|--------|--------|--------|
| 0  | B0 — Setup & "It boots" (toolchain, Makefile, structure) | à venir |
| 1  | B1 — Boot sector maison (real mode → mode protégé) | à venir |
| 2  | B2 — GRUB/Multiboot + kernel C | à venir |
| 3  | B3 — Driver écran VGA | à venir |
| 4  | B4 — GDT propre (kernel) | à venir |
| 5  | B5 — IDT + interruptions | à venir |
| 6  | B6 — Clavier + timer | à venir |
| 7  | B7 — Mémoire physique | à venir |
| 8  | B8 — Paging (mémoire virtuelle) | à venir |
| 9  | B9 — Heap kernel | à venir |
| 10 | B10 — Multitâche | à venir |
| 11 | B11 — Bootloader maison *(optionnel, tardif)* | à venir |
| 12 | B12 — Passage en 64-bit *(optionnel, tardif)* | à venir |

---

<!-- Les Parties 0 à 12 sont ajoutées ici, une par brique, au moment où la brique est
     réalisée et vérifiée dans QEMU. Chacune suit le squelette ci-dessus. -->

## Partie 0 — Setup & « It boots »

### Concept

Avant d'écrire un OS, il faut une **chaîne de build qui marche** et la preuve qu'on
peut exécuter notre propre code sur la machine (émulée). C'est tout l'objet de B0.

La chaîne, du fichier source à l'écran :

```
boot/boot.asm  --(nasm -f bin)-->  boot.bin (512 octets bruts)
                                       │  copie
                                       ▼
                                  naos.img (image disque)
                                       │  qemu charge le disque
                                       ▼
        SeaBIOS lit le secteur 0 -> le pose à 0x7C00 -> vérifie 0xAA55 -> y saute
                                       │
                                       ▼
                          NOTRE code s'exécute (affiche un message, puis halte)
```

On reste **minimal** : un boot sector en assembleur qui affiche une ligne via un
service du BIOS, puis arrête le CPU. Pas encore de mode protégé, de GDT ni de C —
ce sera B1 et B2. B0 prouve seulement que *tout le pipeline* fonctionne.

### Termes clés

- **Real mode** — le mode 16 bits dans lequel le CPU démarre, compatible avec le
  8086 de 1980. Le BIOS et nos premières instructions y tournent.
- **Boot sector** — les 512 premiers octets d'un disque bootable.
- **`0x7C00`** — l'adresse mémoire où le BIOS charge le boot sector (convention figée
  depuis l'IBM PC de 1981).
- **`0xAA55`** — la signature obligatoire aux offsets 510-511 ; sans elle, le BIOS
  considère le disque comme non-bootable.
- **Binaire plat** — du code machine brut, sans en-tête de format (pas d'ELF). Le BIOS
  ne sait pas parser un format ; il copie des octets et saute dedans.
- **`int 0x10`** — interruption logicielle fournie par le BIOS pour l'affichage. La
  fonction `0x0E` (téléscripteur) écrit un caractère à l'écran.

### Étapes reproductibles

#### 1. Installer les dépendances (Debian/Ubuntu)

B0 ne requiert que l'assembleur **NASM** et l'émulateur **QEMU**.

> **`apt install nasm qemu-system-x86`** — installe l'assembleur NASM et la suite QEMU
> (qui fournit `qemu-system-i386`, notre émulateur 32 bits).

```bash
sudo apt update
sudo apt install -y nasm qemu-system-x86
```

Vérifiez :

```bash
nasm --version
qemu-system-i386 --version
```

#### 2. Créer la structure du projet

```bash
mkdir -p boot kernel include toolchain build
```

| Dossier | Rôle |
|---|---|
| `boot/` | code d'amorçage (assembleur) |
| `kernel/` | code du noyau (C, à partir de B2) |
| `include/` | en-têtes partagés |
| `toolchain/` | scripts d'outillage (cross-compiler) |
| `build/` | artefacts générés (ignoré par git) |

#### 3. Écrire le boot sector — `boot/boot.asm`

```nasm
bits 16                 ; le CPU démarre en real mode (16 bits)
org  0x7C00             ; le BIOS charge ce secteur à 0x7C00 -> on s'aligne dessus

start:
    cli                 ; pas d'interruptions pendant qu'on installe les segments
    xor ax, ax          ; AX = 0
    mov ds, ax          ; DS = ES = SS = 0 : adressage simple, segments à zéro
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; pile juste sous notre code (croît vers le bas)
    sti                 ; on réautorise les interruptions (le BIOS en a besoin)

    mov si, msg         ; SI pointe sur la chaîne à afficher
.print:
    lodsb               ; AL = [DS:SI], puis SI++
    test al, al         ; octet nul ? -> fin de chaîne
    jz .hang
    mov ah, 0x0E        ; int 0x10, fonction 0x0E = téléscripteur (affiche AL)
    mov bh, 0x00        ; page vidéo 0
    int 0x10            ; service BIOS d'affichage
    jmp .print

.hang:
    hlt                 ; arrête le CPU jusqu'à la prochaine interruption
    jmp .hang           ; si réveillé, on se rendort : boucle d'arrêt propre

msg db "naos B0: it boots!", 13, 10, 0   ; 13,10 = CR LF ; 0 = fin de chaine

times 510-($-$$) db 0   ; padding : zéros jusqu'à l'octet 510
dw 0xAA55               ; signature little-endian -> octets 0x55 0xAA
```

> **Pourquoi `org 0x7C00` ?** L'assembleur calcule l'adresse de `msg` en partant de cette
> base. Si on l'oubliait, `mov si, msg` pointerait à la mauvaise adresse et on afficherait
> n'importe quoi. On dit à NASM « ce code vivra à 0x7C00 » parce que c'est là que le BIOS
> le met.

> **Pourquoi `hlt` puis `jmp` ?** `hlt` met le CPU en pause (économie d'énergie) jusqu'à une
> interruption. Si une interruption le réveille, le `jmp` le renvoie dormir. Sans cette
> boucle, le CPU exécuterait les octets suivants (le padding, des zéros = `add [bx+si],al`)
> et finirait par planter.

#### 4. Écrire le `Makefile`

```makefile
NASM ?= nasm
QEMU ?= qemu-system-i386

BUILD    := build
BOOT_DIR := boot
BOOT_BIN := $(BUILD)/boot.bin
IMAGE    := $(BUILD)/naos.img

.PHONY: all run clean
all: $(IMAGE)

$(BOOT_BIN): $(BOOT_DIR)/boot.asm | $(BUILD)
	$(NASM) -f bin $< -o $@

$(IMAGE): $(BOOT_BIN)
	cp $(BOOT_BIN) $(IMAGE)

$(BUILD):
	mkdir -p $(BUILD)

run: $(IMAGE)
	$(QEMU) -drive format=raw,file=$(IMAGE)

clean:
	rm -rf $(BUILD)
```

> **`nasm -f bin`** — assemble en **binaire plat** (pas d'ELF). C'est indispensable ici : le
> BIOS attend des octets bruts, pas un format.

> **Point clé — tabulations.** Dans un `Makefile`, les lignes de recette (sous chaque cible)
> doivent commencer par une **tabulation**, jamais par des espaces. Si vous copiez-collez et
> obtenez `Makefile:N: *** missing separator. Stop.`, c'est ça : remplacez les espaces de
> début de ligne par une vraie tabulation.

#### 5. (Optionnel, pour B2) Construire le cross-compiler

B0 n'en a pas besoin, mais autant le préparer. Le script `toolchain/build-i686-elf.sh`
télécharge et compile binutils + gcc ciblés `i686-elf` (~20-40 min).

> **`toolchain/build-i686-elf.sh`** — construit le cross-compiler `i686-elf-gcc` dans
> `~/opt/cross`. À lancer une seule fois.

```bash
# Prérequis de compilation
sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev \
                    libmpfr-dev texinfo wget
# Construction (longue)
./toolchain/build-i686-elf.sh
# Puis ajouter au PATH (dans ~/.bashrc) :
export PATH="$HOME/opt/cross/bin:$PATH"
```

### Vérification

> **`make run`** — assemble le boot sector, fabrique l'image, et lance QEMU dessus.

```bash
make run
```

Une fenêtre QEMU s'ouvre, affiche les lignes de SeaBIOS, puis :

```
naos B0: it boots!
```

Le curseur s'arrête : le CPU est en `hlt`. **Critère de réussite : QEMU démarre, affiche
le message, et ne redémarre pas en boucle** (un reboot en boucle = triple-fault, signe que
quelque chose cloche). Fermez la fenêtre pour quitter.

### Pour aller plus loin

- B0 utilise le BIOS (`int 0x10`) pour afficher. Ces services n'existent qu'en real mode :
  dès qu'on passera en mode protégé (B1), ils disparaîtront et on écrira notre propre
  affichage (B3).
- **B1** reprend ce boot sector et va jusqu'au bout du démarrage real mode : activation A20,
  GDT, passage en **mode protégé** 32 bits.
