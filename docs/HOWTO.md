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
- [Partie 0 — Setup, QEMU & premier boot](#partie-0--setup-qemu--premier-boot)
  - [0.1 — Le décor : QEMU, un PC complet en logiciel](#01--le-décor--qemu-un-pc-complet-en-logiciel)
  - [0.2 — La chaîne complète : du bouton power à ton code](#02--la-chaîne-complète--du-bouton-power-à-ton-code)
  - [0.3 — Le boot sector, construit pas à pas](#03--le-boot-sector-construit-pas-à-pas)
  - [0.4 — L'affichage en profondeur](#04--laffichage-en-profondeur)
  - [0.5 — Lire le binaire avec `hexdump`](#05--lire-le-binaire-avec-hexdump)
  - [0.6 — Vérifier dans QEMU](#06--vérifier-dans-qemu)
- [Partie 1 — B1 : du real mode au mode protégé 32 bits](#partie-1--b1--du-real-mode-au-mode-protégé-32-bits)
  - [1.1 — Real mode vs mode protégé](#11--real-mode-vs-mode-protégé--ce-qui-change-vraiment)
  - [1.2 — Vue d'ensemble : les quatre gestes](#12--vue-densemble--les-quatre-gestes)
  - [1.3 — A20 : la ligne d'adresse oubliée](#13--a20--la-ligne-dadresse-oubliée)
  - [1.4 — La GDT : segments, sélecteurs, descripteurs](#14--la-gdt--segments-sélecteurs-descripteurs)
  - [1.5 — La bascule : `CR0.PE` + far jump](#15--la-bascule--bit-pe-de-cr0--far-jump)
  - [1.6 — Afficher sans le BIOS : `0xB8000`](#16--afficher-sans-le-bios--écrire-dans-0xb8000)
  - [1.7 — Le boot sector complet, pas à pas](#17--le-boot-sector-complet-construit-pas-à-pas)
  - [1.8 — Vérifier dans QEMU](#18--vérifier-dans-qemu)
- [Partie 2 — B2 : GRUB + Multiboot + premier kernel C](#partie-2--b2--grub--multiboot--premier-kernel-c)
  - [2.1 — Pourquoi déléguer le boot à GRUB](#21--pourquoi-déléguer-le-boot-à-grub)
  - [2.2 — La spec Multiboot](#22--la-spec-multiboot--le-contrat-entre-grub-et-notre-kernel)
  - [2.3 — Le cross-compiler](#23--le-cross-compiler--pourquoi-et-comment-le-construire)
  - [2.4 — Le stub d'amorçage en ASM](#24--le-stub-damorçage-en-asm-bootbootasm)
  - [2.5 — Le linker script : kernel à 1 Mo](#25--le-linker-script--placer-le-kernel-à-1-mo)
  - [2.6 — Le premier `kmain()` en C](#26--le-premier-kmain-en-c)
  - [2.7 — Fabriquer l'ISO bootable (GRUB)](#27--fabriquer-liso-bootable-grub)
  - [2.8 — Build & vérifier](#28--build--vérifier)
- [Partie 3 — B3 : driver écran VGA](#partie-3--b3--driver-écran-vga)
  - [3.1 — Le buffer texte VGA](#31--le-buffer-texte-vga-en-détail)
  - [3.2 — L'octet d'attribut : les couleurs](#32--loctet-dattribut--encoder-les-couleurs)
  - [3.3 — L'API et l'état du driver](#33--lapi-et-létat-du-driver)
  - [3.4 — Effacer l'écran : `vga_init`](#34--effacer-lécran--vga_init)
  - [3.5 — `vga_putchar` : contrôle, curseur, retour à la ligne](#35--vga_putchar--caractères-de-contrôle-curseur-retour-à-la-ligne)
  - [3.6 — Le défilement (scroll)](#36--le-défilement-scroll)
  - [3.7 — La démo dans `kmain`](#37--la-démo-dans-kmain)
  - [3.8 — Vérifier](#38--vérifier)
- [Annexes](#annexes)
  - [Annexe A — Rappels d'assembleur x86](#annexe-a--rappels-dassembleur-x86-real-mode-16-bits)
  - [Annexe B — `boot.asm` ligne par ligne](#annexe-b--bootbootasm-ligne-par-ligne)
  - [Annexe C — `int 0x10` en détail](#annexe-c--int-0x10-en-détail-services-bios--interruptions)

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
| 0  | B0 — Setup & "It boots" (toolchain, Makefile, structure) | ✅ fait |
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

<!-- Les Parties 1 à 12 sont ajoutées ici, une par brique, au moment où la brique est
     réalisée et vérifiée dans QEMU. Chacune suit le squelette ci-dessus. -->

## Partie 0 — Setup, QEMU & premier boot

B0 a un seul but : avoir une **chaîne de build qui marche** et la **preuve** qu'on peut
exécuter notre propre code sur la machine (émulée). Pas encore de mode protégé, de GDT ni de
C — ce sera B1 et B2. Ici, on pose le décor et on fait afficher une ligne à un boot sector.

> **Lancer une brique.** Il n'y a **pas** de `make run` générique : chaque brique a sa cible
> nommée — **`make run-b0`** (cette partie), **`make run-b1`** (Partie 1), `run-b2`, `run-b3`…
> Ainsi on sait toujours *quelle* brique on lance. B0/B1 sont des binaires plats (depuis
> `boot/boot.asm.b0` / `.b1`) ; B2+ des kernels chargés par GRUB. **Toute cette Partie 0 se
> lance avec `make run-b0`.**

**Dans cette partie :**
- 0.1 — Le décor : QEMU, un PC complet en logiciel
- 0.2 — La chaîne complète : du bouton power à ton code
- 0.3 — Le boot sector, construit pas à pas
- 0.4 — L'affichage en profondeur (le grand oublié)
- 0.5 — Lire le binaire avec `hexdump`
- 0.6 — Vérifier dans QEMU

**Termes clés (référence rapide) :**

- **Real mode** — le mode 16 bits dans lequel le CPU démarre, compatible 8086 (1978).
- **Firmware** — le logiciel gravé dans la machine, qui s'exécute avant tout OS (ici SeaBIOS, fourni par QEMU).
- **Boot sector** — les 512 premiers octets d'un disque bootable.
- **`0x7C00`** — l'adresse où le firmware charge le boot sector (convention IBM, 1981).
- **`0xAA55`** — signature obligatoire aux offsets 510-511, sinon disque non-bootable.
- **Binaire plat** — du code machine brut, sans en-tête de format (pas d'ELF).
- **`int 0x10`** — service d'affichage fourni par le BIOS (real mode uniquement).
- **`0xB8000`** — la mémoire vidéo texte (ce que le matériel VGA affiche).

---

### 0.1 — Le décor : QEMU, un PC complet en logiciel

QEMU n'est pas « un lanceur de programme » : c'est un **ordinateur entier émulé en logiciel**,
firmware compris. Faire `make run-b0`, c'est *allumer un PC virtuel*.

Ce que tu fixes, et ce que QEMU fournit tout seul :

| Composant | Comment / flag | Tu le fournis ? |
|---|---|---|
| CPU | `qemu-system-i386` (x86 32 bits) | tu le choisis |
| RAM | `-m 128M` (défaut suffisant) | optionnel |
| Disque | `-drive format=raw,file=...` | **oui** (notre image) |
| **ROM + firmware** | **SeaBIOS, chargé d'office** | ❌ non — QEMU le donne |
| Écran VGA, clavier PS/2 | automatiques | non |

#### Installer les dépendances (Debian/Ubuntu)

B0 ne requiert que l'assembleur **NASM** et l'émulateur **QEMU**.

> **`apt install nasm qemu-system-x86`** — installe NASM et la suite QEMU (qui fournit
> `qemu-system-i386`, notre émulateur 32 bits).

```bash
sudo apt update
sudo apt install -y nasm qemu-system-x86
nasm --version
qemu-system-i386 --version
```

#### Créer la structure du projet

```bash
mkdir -p boot kernel include toolchain build
```

| Dossier | Rôle |
|---|---|
| `boot/` | code d'amorçage (assembleur) |
| `kernel/` | code du noyau (C, à partir de B2) |
| `include/` | en-têtes partagés |
| `toolchain/` | scripts d'outillage (cross-compiler, pour B2) |
| `build/` | artefacts générés (ignoré par git) |

#### La commande de lancement, flag par flag

> **`qemu-system-i386 -drive format=raw,file=build/naos.img`** — allume un PC virtuel x86
> 32 bits avec notre image comme disque dur.

- `qemu-system-i386` : l'émulateur, CPU x86 32 bits.
- `-drive format=raw,file=...` : branche notre image comme disque. `raw` = octets bruts (pas
  un format compressé type `qcow2`).
- Le **firmware (SeaBIOS) est chargé automatiquement** — on ne le précise jamais.

Des flags qui resserviront (debug et vérification) :

| Flag | Rôle |
|---|---|
| `-display none` | pas de fenêtre (mode « headless ») |
| `-no-reboot` | **quitter** au lieu de redémarrer → révèle les triple-faults |
| `-d int,cpu_reset -D fichier.log` | journalise interruptions et resets CPU |
| `-s` | ouvre un stub **GDB** sur le port 1234 |
| `-S` | **gèle** le CPU au démarrage (attend GDB) |
| `-qmp unix:sock,server,nowait` | pilotage programmatique (capture d'écran, état) |

Le `Makefile` expose une cible de lancement **par brique** (et une de debug) :

> **`make run-b0`** — assemble, fabrique l'image, lance QEMU (fenêtre normale).
> **`make debug`** — lance QEMU **figé** (`-s -S`), en attente d'un GDB sur `:1234`.

```bash
make run-b0  # lancer la brique B0
make debug   # puis, dans un autre terminal :
             #   gdb -> target remote :1234 -> b *0x7c00 -> continue
```

> **Pourquoi `make debug` ?** En real mode (B0/B1), il n'y a ni `printf` ni message d'erreur.
> GDB te laisse poser un point d'arrêt à `0x7c00` et avancer **instruction par instruction**
> pour voir les registres. Pour les bugs plus retors de B1 (triple-fault), on sortira
> **Bochs** (cf. `DESIGN-LOG.md`, C2), dont le débogueur dit *pourquoi* ça plante.

> **(Optionnel, pour B2) Le cross-compiler.** B0 n'en a pas besoin (boot sector NASM pur),
> mais on peut le préparer : `./toolchain/build-i686-elf.sh` construit `i686-elf-gcc` dans
> `~/opt/cross` (~20-40 min). À lancer une seule fois, avant B2.

---

### 0.2 — La chaîne complète : du bouton power à ton code

Il faut distinguer **deux** chaînes : celle de *fabrication* (sur ta machine) et celle
d'*exécution* (dans QEMU).

#### (a) La chaîne de build — du texte aux octets

```
boot/boot.asm   (TEXTE, lisible)
     │  nasm -f bin        ← traduit le texte en octets machine
     ▼
build/boot.bin  (512 OCTETS bruts)
     │  cp
     ▼
build/naos.img  (le "disque" que QEMU monte)
                 └── son secteur 0 = ces 512 octets
```

- Un **secteur** = un bloc de 512 octets sur le disque. Le **secteur 0** = le boot sector.
  Notre « disque » ne fait qu'un seul secteur.
- **`boot.asm` (texte) ne va jamais sur le disque** : c'est `boot.bin` (sa traduction) qui y
  va. Le `.asm` est la recette, le `.bin` est le plat.

> **Pourquoi deux fichiers, `boot.bin` *et* `naos.img` ?** Ils encodent deux rôles distincts :
> `boot.bin` = **un composant** (le secteur de boot assemblé, produit de `boot.asm`) ;
> `naos.img` = **le disque bootable** que QEMU monte (et qu'on `dd`-erait sur une clé USB).
> En B0 ils sont **identiques** — d'où le simple `cp` — parce que le disque ne contient *que*
> le boot sector. Mais dès **B2**, l'image grossira :
> `naos.img = [secteur de boot / GRUB] + [kernel] + …`, **assemblée à partir de plusieurs
> pièces** (la règle deviendra un « link + assemble », plus un `cp`). Garder `naos.img`
> distinct dès maintenant stabilise la cible de lancement `make run-b0` (elle lance toujours
> *l'image*, quel qu'en soit le contenu) et installe le modèle mental **composant vs disque**.

#### (b) La chaîne d'exécution — de l'allumage à `0x7C00`

```
power on
  ▼  0xFFFFFFF0   ← LE CPU démarre ICI (reset vector, câblé par Intel) = le FIRMWARE
SeaBIOS : POST → cherche un disque bootable → lit le secteur 0
  ▼  copie les 512 octets à 0x7C00, vérifie 0xAA55, puis jmp 0x7C00
NOTRE code démarre ICI   ← 0x7C00
```

Deux « départs » à ne pas confondre :

| Adresse | Départ de quoi | Qui la fixe |
|---|---|---|
| `0xFFFFFFF0` | du **CPU** (le firmware) au power-on | câblé Intel/AMD — on n'y touche jamais |
| `0x7C00` | de **notre** code | convention IBM 1981 — notre porte d'entrée |

#### Trois idées qui rendent la suite limpide

> **Pourquoi `jmp` n'est pas magique.** Le CPU est une boucle infinie : *lire l'instruction
> à `CS:IP`, l'exécuter, avancer `IP`, recommencer*. Un `jmp X` écrit simplement `X` dans
> `IP`. « SeaBIOS saute à `0x7C00` » = il met `0x7C00` dans `IP`, et le flux d'exécution
> (qui n'a jamais cessé) continue dans *nos* octets.

> **Un label = une adresse.** `start`, `.print` n'existent que dans le source ; NASM les
> remplace par des nombres. Le firmware ne connaît pas nos labels — il saute à une adresse
> **en dur** (`0x7C00`), un nombre convenu d'avance entre deux binaires étrangers.

> **Code et données = les mêmes octets.** Le CPU décode comme « instruction » ce sur quoi
> `IP` tombe. Rien ne distingue physiquement du code d'une chaîne de texte — d'où
> l'importance de placer nos données *après* le `hlt` (sinon le CPU les exécuterait).

---

### 0.3 — Le boot sector, construit pas à pas

> **Point clé — la bonne méthode.** N'écris **jamais** le boot sector complet d'un coup.
> Ajoute un petit bout, lance `make run-b0`, observe, recommence. Le jour où ça plante, tu sais
> que c'est ton dernier ajout — pas un mystère parmi 30 lignes. On construit ici en 4
> incréments ; **teste après chacun**.

#### Étape a — le squelette qui boote (et ne fait rien)

```nasm
bits 16                 ; le CPU démarre en real mode (16 bits)
org  0x7C00             ; on s'aligne sur l'adresse de chargement du BIOS

start:
    hlt                 ; arrête le CPU
    jmp start           ; ... et reboucle s'il est réveillé

times 510-($-$$) db 0   ; padding jusqu'à l'octet 510
dw 0xAA55               ; signature de boot (offsets 510-511)
```

`make run-b0` → SeaBIOS affiche `Booting from Hard Disk...`, puis **écran noir**, sans rien
d'autre. C'est normal : on n'affiche rien encore.

> **Que vérifie cette étape ?** Deux choses, sans afficher : (1) **pas** de « No bootable
> device » → la signature `0xAA55` est reconnue, notre secteur *est* bootable ; (2) **pas**
> de redémarrage en boucle → on a bien atteint `hlt`, donc notre code s'exécute. Le pipeline
> complet (build → image → QEMU → notre code) tourne.

#### Étape b — afficher UN caractère

```nasm
bits 16
org  0x7C00

start:
    mov ah, 0x0E        ; fonction "téléscripteur" du service vidéo BIOS
    mov al, 'X'         ; le caractère à afficher
    int 0x10            ; appel du BIOS -> il affiche AL

.hang:
    hlt
    jmp .hang

times 510-($-$$) db 0
dw 0xAA55
```

`make run-b0` → un `X` apparaît sous les lignes de SeaBIOS. **Preuve : on sait appeler le
firmware (`int 0x10`) et il affiche.**

> **Comment marche cet appel ?** `int 0x10` est une *fonction générique* : `AH` choisit la
> sous-fonction (`0x0E` = téléscripteur), `AL` porte le caractère. Mécanisme complet
> (sélecteur, registres-arguments, table des vecteurs, parallèle syscall) :
> **[Annexe C — `int 0x10` en détail](#annexe-c--int-0x10-en-détail-services-bios--interruptions)**.
> Ce que `int 0x10` fait *à l'écran* est détaillé en [0.4](#04--laffichage-en-profondeur).

#### Étape c — afficher une CHAÎNE (la boucle)

```nasm
bits 16
org  0x7C00

start:
    mov si, msg         ; SI pointe sur le début de la chaîne
.print:
    lodsb               ; AL = [SI], puis SI++
    test al, al         ; octet nul ? -> fin de chaîne
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .print          ; caractère suivant

.hang:
    hlt
    jmp .hang

msg db "naos B0: it boots!", 13, 10, 0   ; 13,10 = CR LF ; 0 = fin
times 510-($-$$) db 0
dw 0xAA55
```

`make run-b0` → le message complet. **Preuve : la boucle d'affichage et la donnée `msg`.**

> **Pourquoi `msg` *après* `.hang` ?** Parce que le CPU exécute en séquence : s'il
> rencontrait les octets de la chaîne, il les décoderait comme des instructions (charabia,
> puis plantage). En la plaçant après la boucle d'arrêt, le CPU ne l'atteint jamais comme code
> — seul `lodsb` la *lit comme donnée*.

#### Étape d — le rituel propre (segments + pile)

Jusqu'ici on a eu de la chance : les segments et la pile étaient dans un état utilisable
laissé par le BIOS. Pour être **robuste** (et indispensable dès qu'on touchera à la pile), on
les initialise explicitement. C'est le `boot/boot.asm` **final** de B0 :

```nasm
bits 16
org  0x7C00

start:
    cli                 ; pas d'interruptions pendant qu'on installe les segments
    xor ax, ax          ; AX = 0
    mov ds, ax          ; DS = ES = SS = 0 : adressage simple, segments à zéro
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; pile juste sous notre code (croît vers le bas)
    sti                 ; on réautorise les interruptions (le BIOS en a besoin)

    mov si, msg
.print:
    lodsb
    test al, al
    jz .hang
    mov ah, 0x0E        ; téléscripteur
    mov bh, 0x00        ; page vidéo 0
    int 0x10
    jmp .print

.hang:
    hlt
    jmp .hang

msg db "naos B0: it boots!", 13, 10, 0
times 510-($-$$) db 0
dw 0xAA55
```

> **Pourquoi ce rituel ?** `cli` : pas d'interruption sur une pile encore incohérente.
> Segments à 0 : adressage simple en offsets absolus. `sp = 0x7C00` : la pile grandit *sous*
> notre code, sans l'écraser. `sti` : on réautorise les interruptions, dont `int 0x10` a
> besoin. **Détail ligne par ligne en [Annexe B](#annexe-b--bootbootasm-ligne-par-ligne).**

> **Nouveau ou rouillé en assembleur ?** Voir l'[Annexe A](#annexe-a--rappels-dassembleur-x86-real-mode-16-bits)
> (registres, real mode, syntaxe Intel, instructions, directives NASM).

---

### 0.4 — L'affichage en profondeur

On a affiché via `int 0x10`. Mais que se passe-t-il *vraiment* ? Il faut séparer **deux
acteurs** : le **firmware** (logiciel) et le **matériel VGA**.

#### `int 0x10` = un APPEL au firmware

Ce n'est pas un « registre écran » que le firmware surveille — c'est une **routine qu'on
invoque**. Elle ne s'exécute que quand on l'appelle, fait son travail, et rend la main. Ses
paramètres passent par les registres : `AH` = la fonction (`0x0E` = téléscripteur), `AL` = le
caractère, `BH` = la page vidéo. Le mécanisme complet (`AH` = sélecteur de sous-fonction,
registres = arguments, IVT, parallèle avec les appels système) est détaillé en
**[Annexe C](#annexe-c--int-0x10-en-détail-services-bios--interruptions)**.

> **`BH`, c'est quoi ?** Juste l'octet haut de `BX`. Son sens « page vidéo » n'existe que dans
> la **convention de `int 0x10`** (un standard *du BIOS*, pas des cartes). La *capacité*
> « pages » vient du matériel VGA (assez de mémoire pour plusieurs écrans) ; le firmware
> l'expose via `BH`. On met `BH=0` = page affichée par défaut.

#### Ce que le firmware fait sous le capot : écrire dans la mémoire vidéo

L'écran texte est **mappé en mémoire** à **`0xB8000`**. Cette adresse n'est pas de la RAM :
c'est la **mémoire de la carte VGA**. Le contrôleur VGA **lit cette zone en continu** (~60×/s)
et dessine à l'écran ce qu'il y trouve. `int 0x10` ne fait, au fond, qu'**écrire dans
`0xB8000`** pour toi (en gérant en plus le curseur et le défilement).

**2 octets par case d'écran :**

```
0xB8000 : 0x41   ← le CODE du caractère ('A' = 0x41 en ASCII)
0xB8001 : 0x0F   ← l'ATTRIBUT : couleur (nibble bas = texte, nibble haut = fond)
                   0x0F = texte blanc (F) sur fond noir (0)
```

Un écran 80×25 = 2000 cases × 2 octets = 4000 octets à partir de `0xB8000`.

#### Comment le VGA « connaît » la forme du 'A' : la police CP437

Le matériel ne devine pas la forme — il fait un **lookup dans une table de police** (le
*character generator*) qui associe chaque code (0–255) à un **bitmap** (grille de ~8×16
pixels). Pour chaque case : lit le code → trouve le glyphe → peint les pixels avec les
couleurs de l'attribut. La police par défaut, **CP437**, est chargée au boot par le firmware
vidéo.

> **Le « super truc » (à venir en B3).** On pourra **remplacer CP437** par notre propre
> police : charger nos bitmaps dans le générateur de caractères du VGA → nos propres glyphes
> en mode texte. C'est un exercice du **pilote écran (B3)**, où l'on programme directement le
> matériel VGA.

#### La chaîne ASCII unifiée

Le même code `0x6E` (le 'n') traverse **tout le système, sans aucune conversion** :

```
source 'n'  →  NASM: octet 0x6E  →  lodsb: AL=0x6E  →  0xB8000: 0x6E  →  police: glyphe 'n'
```

ASCII est la **langue commune** de la source, de la mémoire et de l'écran (et du clavier en
B6). C'est pourquoi écrire `0x6E` à `0xB8000` affiche 'n', sans traduction intermédiaire.

> **Point clé — NASM ne connaît pas CP437.** Cette chaîne marche par *coïncidence* : NASM ne
> *convertit* rien, il recopie l'octet du caractère tel qu'il est dans ton fichier source (=
> ASCII pour les caractères de base, d'où `'n'` → `0x6E`). Et ça tombe juste **parce que les
> 128 premiers codes de CP437 *sont* l'ASCII** (CP437 a été conçue comme une extension
> d'ASCII). Au-delà de `0x7F`, les deux **divergent** :
>
> | 'é' selon… | octet(s) |
> |---|---|
> | source UTF-8 (ce que NASM recopie) | `0xC3 0xA9` (deux octets !) |
> | CP437 (ce que le VGA affiche) | `0x82` |
>
> Un `'é'` tapé dans une chaîne serait donc émis en UTF-8 et interprété en CP437 → **charabia
> à l'écran**. D'où la règle : **messages de boot en ASCII pur** ; pour un caractère spécial,
> écris son code CP437 en dur (`db 0x82` pour 'é'), ne le tape pas dans une chaîne.

#### Le partage des rôles, et les deux voies

| Qui | Fait quoi |
|---|---|
| **toi** | fournis la donnée (code de caractère + attribut couleur) |
| **matériel VGA** | scrute `0xB8000`, fait le lookup police, peint les pixels |
| **firmware** | (B0) emballe « écrire dans `0xB8000` » dans `int 0x10` ; a chargé CP437 au boot |

```
B0 (real mode)    : ton code ──int 0x10──> [BIOS écrit dans 0xB8000] ──> VGA affiche
B3 (mode protégé) : ton code ──écrit toi-même dans 0xB8000──────────> VGA affiche
```

> **Point clé — pourquoi `0xB8000` est central.** `int 0x10` (firmware) **disparaît** en mode
> protégé, mais `0xB8000` (matériel) **marche toujours**, quel que soit le mode CPU. C'est
> pour ça qu'écrire à `0xB8000` sera la **preuve de B1** (on affiche sans le BIOS) et le cœur
> du **driver écran de B3**.

---

### 0.5 — Lire le binaire avec `hexdump`

Le secteur final, octet par octet — un excellent réflexe de débogage bas niveau.

> **`xxd build/boot.bin`** — affiche les 512 octets en hexadécimal + ASCII.

```bash
make
xxd build/boot.bin
```

Extrait (le tien peut différer d'un ou deux octets selon la longueur du code) :

```
00000000: fa31 c08e d88e c08e d0bc 007c fbbe 207c  .1.........|.. |
00000010: ac84 c074 08b4 0eb7 00cd 10eb f3f4 ebfd  ...t............
00000020: 6e61 6f73 2042 303a 2069 7420 626f 6f74  naos B0: it boot
00000030: 7321 0d0a 0000 ...                        s!..............
...
000001f0: 0000 0000 0000 0000 0000 0000 0000 55aa  ..............U.
```

On y lit **les 4 blocs** : code (`0x00`–`0x1F`), chaîne (`0x20`–`0x33`), padding (zéros),
signature `55aa` (offset `0x1FE`).

**Le code, c'est aussi des octets.** Décodage des premiers :

| Octets | Instruction |
|---|---|
| `fa` | `cli` |
| `31 c0` | `xor ax, ax` |
| `8e d8` / `8e c0` / `8e d0` | `mov ds/es/ss, ax` |
| `bc 00 7c` | `mov sp, 0x7C00` |
| `fb` | `sti` |
| **`be 20 7c`** | **`mov si, 0x7C20`** ← l'adresse de `msg` |
| `ac` | `lodsb` |
| `84 c0` / `74 08` | `test al,al` / `jz` |
| `b4 0e` / `b7 00` / `cd 10` | `mov ah,0x0E` / `mov bh,0x00` / `int 0x10` |
| `eb f3` | `jmp .print` (saut **relatif** : recule de 13) |
| `f4` / `eb fd` | `hlt` / `jmp .hang` |

> **Le label est devenu une adresse.** `mov si, msg` s'est encodé `be 20 7c` = `mov si,
> 0x7C20`. NASM a calculé `msg` = `0x7C00 + 0x20` (grâce à `org`) et l'a gravé **en dur**.
> Aucune trace du mot « msg » ni « .print » : le binaire ne contient que des nombres.

> **La chaîne est devenue des octets ASCII.** À l'offset `0x20` : `6e 61 6f 73` = 'n' 'a' 'o'
> 's'. Le `db "naos..."` du source n'est qu'une écriture lisible de ces codes.

---

### 0.6 — Vérifier dans QEMU

#### À l'œil (fenêtre)

```bash
make run-b0
```

SeaBIOS → `Booting from Hard Disk...` → `naos B0: it boots!` → curseur figé (le CPU est en
`hlt`). **Critère de réussite : le message s'affiche et la machine ne redémarre pas en
boucle** (un reboot en boucle = triple-fault). C'est la vérif normale.

#### Capture headless (quand on n'a pas de fenêtre)

Parfois on veut juste une image de l'écran — par exemple pour vérifier à distance, ou en
script. On lance QEMU **headless** avec un socket QMP (le protocole de contrôle de QEMU), puis
on capture depuis un autre terminal :

```bash
make run-b0 QMP=1               # terminal 1 : QEMU sans fenêtre + socket QMP
python3 tools/qemu-shot.py      # terminal 2 : écrit build/shot.png
```

Ouvre `build/shot.png` et juge toi-même : pas de PASS/FAIL, pas d'OCR — juste la photo.

> **Pourquoi un script plutôt qu'un `screendump` + `sleep` ?** Le boot est quasi instantané en
> temps *guest*, mais QMP répond avant que SeaBIOS ait fini en temps *réel* : capturer trop tôt
> donne « display not initialized ». Le script **attend que l'écran se stabilise** (deux
> captures de taille proche d'affilée) au lieu de parier sur un délai fixe. Et comme le curseur
> `_` du mode texte **clignote pour toujours**, il compare la *taille* des PNG (clignotement
> ≈ 0,5 %, toléré) et non les octets exacts, jamais identiques. Voir `tools/qemu-shot.py`.

---

### Pour aller plus loin → B1

- B0 **emprunte** le firmware (`int 0x10`). **B1** va jusqu'au bout du démarrage real mode —
  activation **A20**, **GDT**, bit **PE** de `CR0`, **far jump** — pour entrer en **mode
  protégé 32 bits**. Là, `int 0x10` disparaît : on affichera en écrivant directement dans
  `0xB8000`.
- L'affichage direct `0xB8000` et le **remplacement de la police CP437** : ce sera le **driver
  écran (B3)**.

## Partie 1 — B1 : du real mode au mode protégé 32 bits

B0 nous a déposés en **real mode** 16 bits et a tout délégué au firmware (`int 0x10`). B1 fait
le grand saut : passer le CPU en **mode protégé 32 bits**, l'environnement où tournera tout le
reste de l'OS. Quatre gestes obligatoires — A20, GDT, bit PE, far jump — chacun expliqué puis
assemblé pas à pas. À la fin, on affiche **sans le BIOS**, en écrivant droit dans la mémoire
vidéo.

> **Où vit ce code.** Le boot sector B1 est conservé dans **`boot/boot.asm.b1`** (à partir de
> B2, `boot/boot.asm` est repris pour l'en-tête Multiboot). On le lance depuis un clone avec
> **`make run-b1`** (binaire plat 512 o, pipeline distinct du kernel GRUB de B2+).

**Dans cette partie :**
- 1.1 — Real mode vs mode protégé : ce qui change vraiment
- 1.2 — Vue d'ensemble : les quatre gestes
- 1.3 — A20 : la ligne d'adresse oubliée
- 1.4 — La GDT : de « segment = adresse » à « segment = sélecteur »
- 1.5 — La bascule : bit PE de `CR0` + far jump
- 1.6 — Afficher sans le BIOS : écrire dans `0xB8000`
- 1.7 — Le boot sector complet, construit pas à pas
- 1.8 — Vérifier dans QEMU

**Termes clés (référence rapide) :**

- **Mode protégé** — le mode 32 bits du x86 : adresses 32 bits (4 Go), protection mémoire, base de tout OS moderne.
- **A20** — la 21ᵉ ligne d'adresse (bit 20) ; désactivée à l'allumage par compatibilité 8086.
- **GDT** (*Global Descriptor Table*) — table décrivant les segments ; en mode protégé, un registre de segment contient un **sélecteur** qui l'indexe.
- **Descripteur de segment** — entrée de 8 octets : base, limite, droits d'accès, flags.
- **Sélecteur** — index (×8) dans la GDT, chargé dans `CS`/`DS`/… (`0x08` = 1ʳᵉ entrée utile).
- **`CR0`** — registre de contrôle ; son bit 0 (**PE**, *Protection Enable*) active le mode protégé.
- **Far jump** — saut `selecteur:offset` qui recharge `CS` *et* vide le pipeline de pré-chargement.
- **Triple fault** — trois fautes en cascade sans gestionnaire → le CPU se reset (reboot en boucle).

---

### 1.1 — Real mode vs mode protégé : ce qui change vraiment

Le x86 démarre en *real mode* pour rester compatible avec le 8086 de 1978. C'est un monde
16 bits, sans protection, où une adresse se calcule `segment × 16 + offset` (d'où le plafond
historique ~1 Mo). On y a fait B0. Le *mode protégé* est l'autre monde :

| | Real mode (B0) | Mode protégé (B1+) |
|---|---|---|
| Largeur | 16 bits | **32 bits** (registres `eax`…, offsets 32 bits) |
| Adresse max | ~1 Mo | **4 Go** |
| Segments | `CS`=adresse÷16 | `CS`=**sélecteur** vers un descripteur (base+limite+droits) |
| Services BIOS (`int 0x10`…) | **disponibles** | **disparus** (le BIOS est du code 16 bits) |
| Protection | aucune | niveaux de privilège (ring 0–3), limites de segment |

> **Pourquoi ne pas rester en 16 bits et partir directement vers le kernel ?** On *peut*
> sauter vers du code depuis le real mode — B0 le fait. Mais ce code serait alors **lui-même
> 16 bits**, enfermé sous ~1 Mo, sans protection ni pagination. Et surtout : notre kernel est
> compilé par `i686-elf-gcc` en **code machine 32 bits**, qui **ne s'exécute pas** en real mode
> (tailles d'opérandes et adressage différents). Basculer n'est donc pas un luxe, c'est une
> *condition d'exécution* du kernel. C'est pour ça que **GRUB bascule en mode protégé avant
> d'appeler `kmain`** (B2) : le saut vers le kernel se fait **déjà en 32 bits**. B1 fait cette
> bascule à la main pour comprendre ce que GRUB nous épargnera.

> **La conséquence qui surprend.** En basculant, on **perd `int 0x10`** : le BIOS est du code
> 16 bits, inaccessible en 32 bits. Pour afficher, il faut désormais écrire *soi-même* dans la
> mémoire vidéo (`0xB8000`, cf. 0.4 et 1.6). C'est précisément ce qui rend la bascule
> *observable* : si un message 32 bits apparaît, c'est qu'on a réussi.

### 1.2 — Vue d'ensemble : les quatre gestes

Entrer en mode protégé, c'est exactement quatre opérations, dans cet ordre :

```
real mode 16 bits
   │  1. cli                  ← couper les interruptions (pas encore d'IDT)
   │  2. activer A20          ← débloquer l'adressage au-delà de 1 Mo
   │  3. lgdt [gdt]           ← charger une table de descripteurs de segments
   │  4. CR0.PE = 1           ← BASCULE : le CPU est maintenant en mode protégé
   ▼  5. jmp 0x08:suite       ← far jump : recharge CS, vide le prefetch
mode protégé 32 bits  (première instruction « bits 32 »)
```

> **Pourquoi `cli` d'abord.** En mode protégé, les interruptions passent par une **IDT** qu'on
> n'a pas encore (ce sera B5). Si une interruption matérielle tombait pendant ou après la
> bascule, le CPU chercherait un gestionnaire inexistant → faute → faute → **triple fault** →
> reset. On coupe donc les interruptions (`cli`) et on ne les rallumera qu'en B5.

### 1.3 — A20 : la ligne d'adresse oubliée

Sur le 8086, le calcul `segment × 16 + offset` pouvait dépasser `0xFFFFF` (1 Mo) ; ça
« repliait » vers 0 (*wrap-around*). Des programmes en dépendaient. Quand le 80286 a ajouté
une 21ᵉ ligne d'adresse (A20, le bit 20), IBM l'a **désactivée au démarrage** pour préserver
ce repli. Résultat : tant qu'A20 est off, une adresse comme `0x100000` (1 Mo) se replie sur
`0x0` — désastreux dès qu'on adresse au-delà de 1 Mo (notre kernel ira à 1 Mo en B2).

On active donc A20. Trois méthodes existent (port clavier `0x64`, BIOS `int 0x15`, *Fast A20*) ;
on prend la plus simple, **Fast A20** via le port `0x92` :

```nasm
    in  al, 0x92        ; lire le registre de contrôle système
    or  al, 0000_0010b  ; bit 1 = A20 enable
    out 0x92, al        ; réécrire
```

> **Pourquoi Fast A20 ici.** C'est deux instructions, et QEMU (comme tout chipset moderne) le
> supporte. Les méthodes par contrôleur clavier sont historiquement plus « universelles » mais
> bien plus longues (attentes de status). Pour un projet pédagogique sous QEMU, `0x92` suffit.

### 1.4 — La GDT : segments, sélecteurs, descripteurs

C'est le cœur conceptuel de B1. En real mode, `CS = 0x07C0` *signifiait* « base = `0x7C00` ».
En mode protégé, un registre de segment ne contient plus une adresse mais un **sélecteur** :
un index dans une table, la **GDT** (*Global Descriptor Table*). Chaque entrée de la GDT est un
**descripteur** de 8 octets décrivant un segment : sa **base**, sa **limite**, et ses **droits**.

On charge une GDT « **plate** » (*flat*) : un segment de code et un segment de données couvrant
**toute** la mémoire (base 0, limite 4 Go). Autrement dit, on *neutralise* la segmentation —
le cloisonnement mémoire viendra plus tard via le **paging** (B8), pas via les segments.

La GDT minimale a trois entrées :

| Index | Sélecteur | Rôle |
|---|---|---|
| 0 | `0x00` | **descripteur nul** — obligatoire ; un chargement de `0x00` est une erreur volontaire |
| 1 | `0x08` | segment de **code** (exécutable, base 0, limite 4 Go) |
| 2 | `0x10` | segment de **données** (inscriptible, base 0, limite 4 Go) |

> **Pourquoi `0x08` et `0x10` ?** Un sélecteur n'est pas l'index brut : c'est `index × 8`
> (chaque descripteur fait 8 octets), les 3 bits bas servant au niveau de privilège (RPL) et au
> choix de table. Entrée 1 → `1×8 = 0x08`, entrée 2 → `2×8 = 0x10`.

Un descripteur de 8 octets a un format historique éclaté (les champs base et limite sont coupés
en morceaux pour rester compatible 80286). Voici le descripteur de **code** plat, octet par
octet — c'est `1001_1010b` / `1100_1111b` qu'il faut comprendre :

```nasm
gdt_code:
    dw 0xFFFF        ; limite [0:15]      ┐ limite = 0xFFFFF (20 bits)
    dw 0x0000        ; base  [0:15]       │
    db 0x00          ; base  [16:23]      ├─ base = 0
    db 1001_1010b    ; octet d'accès      │   P=1 DPL=00 S=1 | type=1010 (code, exéc, lisible)
    db 1100_1111b    ; flags + limite[16:19]  G=1 D=1 0 0 | 1111
    db 0x00          ; base  [24:31]      ┘
```

- **Octet d'accès `1001_1010`** : `P`=présent, `DPL`=00 (ring 0), `S`=1 (segment code/data),
  puis le type `1010` = *code, exécutable, lisible*. (Le segment **data** a `1001_0010` : type
  `0010` = *data, inscriptible*.)
- **Flags `1100`** : `G`=1 → la limite se compte en **pages de 4 Ko** (`0xFFFFF × 4 Ko` = 4 Go),
  et `D`=1 → opérandes/adresses **32 bits** par défaut. Les 4 bits bas (`1111`) = limite[16:19].

On dit au CPU où trouver cette table avec un **pseudo-descripteur** (taille − 1, puis adresse
linéaire) chargé par `lgdt` :

```nasm
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; limite : taille de la GDT moins 1
    dd gdt_start                 ; adresse linéaire de la GDT

    ...
    lgdt [gdt_descriptor]
```

### 1.5 — La bascule : bit PE de `CR0` + far jump

Tout est prêt ; deux dernières instructions font le saut de monde :

```nasm
    mov eax, cr0
    or  eax, 1               ; bit 0 = PE (Protection Enable)
    mov cr0, eax             ; <- À CET INSTANT, le CPU est en mode protégé
    jmp CODE_SEG:protected_mode   ; far jump (CODE_SEG = 0x08)
```

> **Pourquoi le far jump est indispensable.** Mettre `PE` ne suffit pas : `CS` contient encore
> l'ancienne valeur 16 bits, et le CPU a déjà **pré-chargé** (prefetch) des instructions
> décodées en 16 bits. Un **far jump** (`jmp selecteur:offset`) fait deux choses d'un coup :
> il recharge `CS` avec le sélecteur de code `0x08` (donc le bon descripteur), et il **vide le
> pipeline** — la prochaine instruction est re-décodée *en 32 bits*. C'est la première
> instruction « vraiment » 32 bits, marquée `bits 32` dans le source.

Juste après, on recharge tous les **segments de données** avec le sélecteur data `0x10` (en
real mode ils valaient 0 ; ils doivent maintenant pointer le descripteur data), et on installe
une pile 32 bits :

```nasm
bits 32
protected_mode:
    mov ax, DATA_SEG     ; 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000     ; pile en zone libre
```

### 1.6 — Afficher sans le BIOS : écrire dans `0xB8000`

`int 0x10` a disparu. Mais la mémoire vidéo texte, elle, est toujours là — c'est de la RAM
mappée à **`0xB8000`** (cf. 0.4) : 80×25 **cellules de 2 octets** (octet bas = caractère CP437,
octet haut = **attribut** couleur : 4 bits avant-plan, 4 bits fond). Afficher un caractère vert,
c'est poser deux octets :

```nasm
    mov esi, msg_pm
    mov edi, 0xB8000
    mov ah, 0x0A         ; attribut : vert clair (0x0A) sur fond noir
.pm_print:
    lodsb                ; AL = [esi], esi++
    test al, al
    jz .hang
    mov [edi], al        ; caractère
    mov [edi + 1], ah    ; couleur
    add edi, 2           ; cellule suivante
    jmp .pm_print
```

> **Pourquoi le message atterrit en haut à gauche.** `0xB8000` = **cellule (0,0)** = coin
> supérieur gauche. On écrase donc la première ligne (la version SeaBIOS). C'est voulu : voir
> le texte vert *là* prouve qu'on écrit la mémoire vidéo nous-mêmes, en 32 bits.

### 1.7 — Le boot sector complet, construit pas à pas

Comme en 0.3 : on assemble les briques dans l'ordre, mais ici on **teste à la fin** (la bascule
ne se découpe pas en sous-étapes observables — soit on arrive en 32 bits, soit triple fault).
Le squelette final reprend B0 (segments, pile, message real mode via `int 0x10`) puis enchaîne
les sections 1.3 → 1.6. Le fichier complet est `boot/boot.asm.b1` ; sa colonne vertébrale :

```nasm
bits 16
org  0x7C00
start:
    cli / xor ax,ax / mov ds,es,ss / mov sp,0x7C00 / sti   ; (B0)
    mov si, msg_rm  ... int 0x10  ...                       ; message real mode (B0)
    cli                                                     ; 1.2
    in al,0x92 / or al,2 / out 0x92,al                      ; 1.3  A20
    lgdt [gdt_descriptor]                                   ; 1.4  GDT
    mov eax,cr0 / or eax,1 / mov cr0,eax                    ; 1.5  PE
    jmp CODE_SEG:protected_mode                             ; 1.5  far jump
bits 32
protected_mode:
    mov ax,DATA_SEG / mov ds.. / mov esp,0x90000            ; 1.5  segments + pile
    ... écrire msg_pm à 0xB8000 ...                          ; 1.6
.hang: hlt / jmp .hang
msg_rm db "naos B1: real mode OK, switching to 32-bit...",13,10,0
msg_pm db "naos B1: 32-bit protected mode!",0
gdt_start: ... gdt_code ... gdt_data ... gdt_end            ; 1.4
gdt_descriptor: dw gdt_end-gdt_start-1 / dd gdt_start
CODE_SEG equ gdt_code-gdt_start    ; 0x08
DATA_SEG equ gdt_data-gdt_start    ; 0x10
times 510-($-$$) db 0
dw 0xAA55
```

> **Toujours un boot sector.** B1 reste un binaire plat de 512 o chargé à `0x7C00` avec la
> signature `0xAA55` : c'est *nous* qui faisons la bascule, pas GRUB (ça, c'est B2). D'où
> `org 0x7C00` et le `times … db 0` / `dw 0xAA55` de B0.

### 1.8 — Vérifier dans QEMU

```bash
make run-b1                   # à l'œil : message real mode, puis message vert 32 bits
make run-b1 QMP=1             # (autre terminal) python3 tools/qemu-shot.py
```

Attendu : la ligne real mode « naos B1: real mode OK, switching to 32-bit... » dans le flux
SeaBIOS, **puis** « naos B1: 32-bit protected mode! » en **vert, en haut à gauche**. Ce second
message ne peut avoir été écrit que par du code 32 bits (le BIOS n'existe plus) : **la bascule
a réussi**. Critère B1 atteint.

> **Si ça reboucle (triple fault).** Le suspect nº 1 est la GDT (descripteur mal encodé) ou le
> far jump (mauvais sélecteur). En real mode il n'y a pas de message d'erreur : sors `make
> debug` (GDB) ou, pour voir *pourquoi* le CPU reset, Bochs (cf. `DESIGN-LOG.md`, C2).

---

## Partie 2 — B2 : GRUB + Multiboot + premier kernel C

B1 a *montré* comment entrer en mode protégé à la main. Maintenant on **délègue** ce démarrage
à GRUB (choix hybride C4 : comprendre d'abord, déléguer ensuite) et on écrit notre premier code
en **C**. C'est un tournant : le boot sector maison disparaît, remplacé par un **kernel ELF**
chargé à 1 Mo, fabriqué par un **cross-compiler** et empaqueté dans une **ISO bootable**.

> **Où vit ce code.** `boot/boot.asm` (réécrit : en-tête Multiboot + stub), `kernel/kmain.c`,
> `linker.ld`, `grub/grub.cfg`, et un `Makefile` refondu. Toolchain : `i686-elf-gcc`.

**Dans cette partie :**
- 2.1 — Pourquoi déléguer le boot à GRUB
- 2.2 — La spec Multiboot : le contrat entre GRUB et notre kernel
- 2.3 — Le cross-compiler : pourquoi, et comment le construire
- 2.4 — Le stub d'amorçage en ASM (`boot/boot.asm`)
- 2.5 — Le linker script : placer le kernel à 1 Mo
- 2.6 — Le premier `kmain()` en C
- 2.7 — Fabriquer l'ISO bootable (GRUB)
- 2.8 — Build & vérifier

**Termes clés (référence rapide) :**

- **GRUB** — *bootloader* standard ; sait charger un kernel **Multiboot** et nous livre en mode protégé.
- **Multiboot 1** — spec définissant un *en-tête* que le kernel expose et un *état* que GRUB garantit à l'entrée.
- **Cross-compiler** — compilateur produisant du code pour une cible (`i686-elf`) différente de l'hôte.
- **Freestanding** — code C **sans** bibliothèque standard ni OS (pas de `printf`, pas de `malloc`).
- **ELF** — format de fichier objet/exécutable Unix ; notre kernel est un ELF 32 bits.
- **Linker script** (`.ld`) — décrit l'agencement mémoire des sections du binaire final.
- **`grub-mkrescue`** — fabrique une ISO bootable contenant GRUB + notre kernel.

---

### 2.1 — Pourquoi déléguer le boot à GRUB

B1 nous a fait *comprendre* A20 / GDT / PE / far jump. Refaire tout ça à la main à chaque
démarrage — plus le chargement du kernel depuis le disque (`int 0x13`), la carte mémoire
(`int 0x15`), etc. — serait long et hors-sujet pour apprendre le *kernel*. GRUB fait tout ce
travail et nous livre dans un état connu :

| À l'entrée de notre kernel, GRUB garantit… | …ce que ça nous épargne |
|---|---|
| CPU en **mode protégé 32 bits** | A20, GDT, bit PE, far jump (tout B1) |
| Kernel chargé en mémoire à **1 Mo** | lecture de secteurs disque (`int 0x13`) |
| `eax` = magic `0x2BADB002`, `ebx` → infos Multiboot | détection mémoire (`int 0x15` E820) |
| Interruptions **coupées**, pas de pagination | un point de départ propre |

> **« Hybride », rappel C4.** On garde B1 comme *leçon* (boot sector maison, dans
> `boot.asm.b1`) ; à partir de B2 on *délègue* à GRUB. Plus tard, B11 (optionnel) remplacera
> GRUB par notre propre loader — une fois l'OS fonctionnel.

### 2.2 — La spec Multiboot : le contrat entre GRUB et notre kernel

Pour que GRUB accepte de charger notre binaire, celui-ci doit exposer un **en-tête Multiboot**
dans ses **8 premiers Ko** : trois mots de 32 bits.

```nasm
MB_MAGIC    equ 0x1BADB002             ; constante reconnue par GRUB
MB_FLAGS    equ MB_ALIGN | MB_MEMINFO  ; options demandées
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS) ; magic + flags + checksum == 0
```

- **magic** `0x1BADB002` : la signature que GRUB cherche.
- **flags** : nos demandes — ici `MB_ALIGN` (aligner les modules sur des pages) et `MB_MEMINFO`
  (fournir la carte mémoire).
- **checksum** : choisi pour que `magic + flags + checksum` fasse **0** (sur 32 bits). C'est la
  vérification d'intégrité de GRUB.

> **Deux nombres magiques à ne pas confondre.** `0x1BADB002` est ce que *nous* mettons dans
> l'en-tête (« charge-moi »). `0x2BADB002` est ce que *GRUB* met dans `eax` à l'entrée (« c'est
> bien moi qui t'ai chargé, en Multiboot »). On exploitera `ebx` (infos mémoire) en B7.

### 2.3 — Le cross-compiler : pourquoi, et comment le construire

Le `gcc` du système produit des exécutables **Linux** : ils supposent une libc, un format de
sortie, un OS sous eux. Notre kernel tourne **sans OS** — il *est* ce qui tournera. On compile
donc *freestanding*, avec un toolchain **`i686-elf`** qui ne fait **aucune hypothèse hôte**
(reco OSDev, choix C5).

```bash
./toolchain/build-i686-elf.sh        # construit binutils + gcc dans ~/opt/cross (~20-40 min)
# prérequis Debian/Ubuntu :
#   sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo wget
~/opt/cross/bin/i686-elf-gcc --version
```

Le script compile **binutils** (assembleur/linker cible) puis **gcc** en `--without-headers
--enable-languages=c` (pas de libc : on n'en a pas). On compile avec `-ffreestanding -nostdlib`
et on **lie avec `i686-elf-gcc`** (pas `ld` nu) pour récupérer `libgcc` (les helpers que gcc
appelle pour, p. ex., les divisions 64 bits).

> **Pourquoi pas `gcc -m32` de l'hôte ?** Ça *peut* marcher, mais le cross-compiler élimine une
> classe entière de bugs sournois (en-têtes hôte tirés par erreur, hypothèses d'ABI, options de
> sécurité Linux injectées). C5 tranche : `i686-elf-gcc`, construit une fois pour toutes.

### 2.4 — Le stub d'amorçage en ASM (`boot/boot.asm`)

GRUB nous livre en 32 bits, mais ne connaît pas `kmain`. Il faut un petit **stub** qui (1)
porte l'en-tête Multiboot, (2) installe une pile, (3) appelle `kmain`. C'est tout `boot.asm`
désormais :

```nasm
bits 32
section .multiboot          ; <- doit tomber dans les 8 premiers Ko (cf. linker.ld)
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384              ; 16 Kio de pile
stack_top:

section .text
global _start
extern kmain
_start:
    mov esp, stack_top      ; installer la pile (croît vers le bas)
    call kmain              ; -> notre C
.hang:
    cli
    hlt
    jmp .hang
```

> **Pourquoi installer la pile soi-même.** La spec Multiboot ne garantit *pas* un `esp`
> utilisable. Or le C a besoin d'une pile dès le premier appel de fonction (variables locales,
> adresses de retour). On réserve donc 16 Kio en `.bss` et on pointe `esp` sur son sommet —
> avant le moindre `call`.

> **Convention d'appel (System V i386).** `call kmain` empile l'adresse de retour et saute ;
> `kmain` ne prend aucun argument et ne renvoie rien. Si jamais elle revenait, le `cli`/`hlt`
> arrête proprement la machine.

### 2.5 — Le linker script : placer le kernel à 1 Mo

Le compilateur produit des sections (`.text`, `.rodata`, `.data`, `.bss`) ; le **linker
script** dit *où* les poser en mémoire et *dans quel ordre* :

```ld
ENTRY(_start)
SECTIONS {
    . = 1M;                              /* le kernel commence à 1 Mo */
    .text BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)                    /* en-tête Multiboot EN PREMIER */
        *(.text)
    }
    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata) }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data) }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }
}
```

- `. = 1M` : l'adresse de départ. **1 Mo** est conventionnel — au-dessus de la zone basse
  réservée (IVT, BIOS, mémoire vidéo VGA à `0xB8000`).
- `*(.multiboot)` placé **en tête** de `.text` : garantit que l'en-tête est dans les 8 premiers
  Ko, sinon GRUB ne le trouve pas et refuse le kernel.
- `ALIGN(4K)` : sections alignées sur des pages — utile dès qu'on activera le paging (B8).

### 2.6 — Le premier `kmain()` en C

En B2, `kmain` n'a qu'à **prouver qu'elle tourne**. Pas encore de driver : on écrit directement
dans la mémoire vidéo (comme en 1.6, mais en C) :

```c
/* kernel/kmain.c (version B2) */
void kmain(void)
{
    const char *msg = "naos B2: kmain() running, loaded by GRUB via Multiboot.";
    volatile unsigned short *vga = (unsigned short *)0xB8000;
    for (int i = 0; msg[i]; i++)
        vga[i] = (unsigned short)(unsigned char)msg[i] | (0x0A << 8); /* vert clair */
    for (;;)
        __asm__ volatile ("hlt");
}
```

> **Pourquoi `volatile`.** Le compilateur, voyant qu'on écrit dans un tableau jamais relu,
> serait tenté de *supprimer* ces écritures. `volatile` lui interdit d'optimiser : chaque
> écriture doit réellement atteindre la mémoire vidéo. (Le vrai driver, B3, généralise ça.)

### 2.7 — Fabriquer l'ISO bootable (GRUB)

GRUB lit sa config dans `grub/grub.cfg` (une seule entrée, démarrage immédiat) :

```
set timeout=0
set default=0
menuentry "naos" {
    multiboot /boot/naos.kernel
    boot
}
```

On assemble une arborescence `boot/grub/` puis on la transforme en ISO avec `grub-mkrescue`
(prérequis : `grub-pc-bin`, `xorriso`, `mtools`). Le `Makefile` s'en charge :

Le `Makefile` s'en charge via une règle *patron* `build/%.iso` (une ISO par brique kernel :
`b2.iso`, `b3.iso`…) :

```make
$(BUILD)/%.iso: $(BUILD)/%.kernel grub/grub.cfg
	mkdir -p $(BUILD)/iso-$*/boot/grub
	cp $< $(BUILD)/iso-$*/boot/naos.kernel
	cp grub/grub.cfg $(BUILD)/iso-$*/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso-$*
```

### 2.8 — Build & vérifier

```bash
make run-b2                   # QEMU boote l'ISO B2 → GRUB → kmain()   (le vrai chemin B2)
make run-b2 QMP=1             # (autre terminal) python3 tools/qemu-shot.py
make run-kernel               # raccourci : QEMU charge le DERNIER kernel SANS GRUB (-kernel)
```

Le `Makefile` valide automatiquement `grub-file --is-x86-multiboot build/b2.kernel` : si
l'en-tête est mal formé, le build échoue *avant* QEMU. Au boot, le message vert prouve que
`kmain()` (du **C**, chargé par **GRUB**) s'exécute. **Critère B2 atteint.**

> **ISO (GRUB) vs `-kernel`.** `make run-b2` boote l'ISO → GRUB charge le kernel : c'est le
> vrai chemin B2. `make run-kernel` charge le *même* kernel via le loader Multiboot intégré de
> QEMU (sans GRUB) — pratique pour itérer, mais le critère B2 dit *via GRUB*, donc `run-b2`
> fait foi.

---

## Partie 3 — B3 : driver écran VGA

En B2, `kmain` écrivait à `0xB8000` à la main, octet par octet. Ce n'est pas tenable : on veut
un vrai **driver écran** — une petite API (`vga_write`, couleurs, défilement) sur laquelle tout
le reste de l'OS affichera. B3 transforme le « poser des octets » en abstraction réutilisable.

> **Où vit ce code.** `include/vga.h` (l'API), `kernel/vga.c` (l'implémentation),
> `kernel/kmain.c` (qui s'en sert). Compilés par le même `Makefile` qu'en B2.

**Dans cette partie :**
- 3.1 — Le buffer texte VGA, en détail
- 3.2 — L'octet d'attribut : encoder les couleurs
- 3.3 — L'API et l'état du driver
- 3.4 — Effacer l'écran : `vga_init`
- 3.5 — `vga_putchar` : caractères de contrôle, curseur, retour à la ligne
- 3.6 — Le défilement (scroll)
- 3.7 — La démo dans `kmain`
- 3.8 — Vérifier

**Termes clés (référence rapide) :**

- **Buffer texte VGA** — RAM mappée à `0xB8000` : 80×25 **cellules** de 2 octets, affichée par le matériel.
- **Cellule** — `uint16_t` : octet bas = caractère (CP437), octet haut = **attribut** couleur.
- **Attribut** — 1 octet : bits 0-3 = avant-plan, bits 4-6 = fond, bit 7 = clignotement.
- **CP437** — le jeu de caractères de la VGA texte (ASCII + accents, semi-graphiques, etc.).
- **Scroll** — quand le curseur dépasse la 25ᵉ ligne, on remonte tout d'une ligne.

---

### 3.1 — Le buffer texte VGA, en détail

Le matériel VGA en mode texte lit en continu une zone de RAM à **`0xB8000`** et l'affiche : une
grille de **80 colonnes × 25 lignes**. Chaque case est une **cellule de 2 octets** :

```
 cellule = | octet 0 : caractère (code CP437) | octet 1 : attribut (couleur) |
 adresse de la cellule (ligne y, colonne x) = 0xB8000 + (y * 80 + x) * 2
```

En C, on traite donc le buffer comme un tableau de `uint16_t` (2 octets) — un mot par cellule :

```c
#define VGA_MEM  ((volatile uint16_t *)0xB8000)
#define COLS 80
#define ROWS 25
```

> **Pourquoi `volatile` (rappel B2).** Le compilateur ne doit jamais « optimiser » nos
> écritures : elles ont un effet de bord matériel (afficher). `volatile` le lui interdit.

### 3.2 — L'octet d'attribut : encoder les couleurs

L'octet haut de chaque cellule est l'**attribut**. La VGA texte a **16 couleurs** ; un attribut
combine avant-plan (4 bits) et fond (3 ou 4 bits) :

```
 bit  7   6 5 4   3 2 1 0
      │   └─┬─┘   └──┬──┘
   clignote fond  avant-plan
```

D'où l'encodage `attribut = avant-plan | (fond << 4)`. On nomme les 16 couleurs dans un `enum`
(`VGA_BLACK`=0 … `VGA_WHITE`=15) :

```c
void vga_set_color(enum vga_color fg, enum vga_color bg) {
    color = (uint8_t)fg | (uint8_t)(bg << 4);
}
```

Et une cellule se fabrique en collant caractère + attribut :

```c
static inline uint16_t cell(char c, uint8_t attr) {
    return (uint16_t)(unsigned char)c | ((uint16_t)attr << 8);
}
```

### 3.3 — L'API et l'état du driver

`include/vga.h` expose le minimum utile, et le driver garde un petit **état global** (position
du curseur + couleur courante) :

```c
void vga_init(void);                              /* efface, curseur (0,0) */
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_putchar(char c);                         /* le cœur : pose + curseur + scroll */
void vga_write(const char *s);                    /* boucle sur une chaîne C */
```

```c
static size_t  row, col;     /* curseur logique */
static uint8_t color;        /* attribut courant */
```

> **Freestanding, rappel.** Pas de `string.h`, pas de `stdio`. `vga_write` est juste
> `while (*s) vga_putchar(*s++);`. Les types `uint16_t`/`size_t` viennent de `<stdint.h>` /
> `<stddef.h>`, fournis par gcc même freestanding (en-têtes « autonomes »).

### 3.4 — Effacer l'écran : `vga_init`

Effacer = remplir les 80×25 cellules d'espaces avec la couleur courante, puis ramener le
curseur en haut à gauche :

```c
void vga_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (size_t y = 0; y < ROWS; y++)
        for (size_t x = 0; x < COLS; x++)
            VGA_MEM[y * COLS + x] = cell(' ', color);
    row = col = 0;
}
```

### 3.5 — `vga_putchar` : caractères de contrôle, curseur, retour à la ligne

C'est le cœur du driver. Il distingue les **caractères de contrôle** (`\n \r \t \b`) du texte
ordinaire, avance le curseur, gère le retour à la ligne en bout de colonne, et déclenche le
défilement en bas d'écran :

```c
void vga_putchar(char c) {
    switch (c) {
    case '\n': col = 0; row++;            break;   /* nouvelle ligne */
    case '\r': col = 0;                   break;   /* retour chariot */
    case '\b': if (col) col--;            break;   /* effacement arrière */
    case '\t': col = (col + 8) & ~(size_t)7; break;/* tabulation (multiple de 8) */
    default:
        VGA_MEM[row * COLS + col] = cell(c, color);
        col++;
    }
    if (col >= COLS) { col = 0; row++; }           /* dépassement de colonne -> ligne suivante */
    if (row >= ROWS) scroll();                     /* dépassement de ligne   -> défilement */
}
```

> **Pourquoi `(col + 8) & ~7`.** C'est l'arrondi au multiple de 8 supérieur : les tabulations
> tombent sur des colonnes 8, 16, 24… `& ~7` met à zéro les 3 bits bas.

### 3.6 — Le défilement (scroll)

Quand `row` atteint 25, on **remonte** les lignes 1→24 vers 0→23, on **vide** la dernière, et
on garde le curseur sur cette dernière ligne :

```c
static void scroll(void) {
    for (size_t y = 1; y < ROWS; y++)                       /* remonter d'une ligne */
        for (size_t x = 0; x < COLS; x++)
            VGA_MEM[(y - 1) * COLS + x] = VGA_MEM[y * COLS + x];
    for (size_t x = 0; x < COLS; x++)                       /* vider la dernière */
        VGA_MEM[(ROWS - 1) * COLS + x] = cell(' ', color);
    row = ROWS - 1;
}
```

> **C'est un défilement « logiciel ».** On recopie réellement la RAM vidéo. La VGA sait aussi
> défiler « matériellement » (en décalant l'adresse de départ du balayage) — plus rapide, mais
> plus subtil. Pour B3, la recopie est limpide et largement assez rapide.

### 3.7 — La démo dans `kmain`

`kmain` exerce tout le driver : un en-tête coloré, puis **30 lignes numérotées** — comme
l'écran n'en montre que 25, les premières défilent hors champ (preuve du scroll) :

```c
void kmain(void) {
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B3: VGA driver online.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Booted by GRUB via Multiboot; kmain() runs in 32-bit C.\n\n");

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    for (unsigned int i = 1; i <= 30; i++) {           /* 30 > 25 -> ça défile */
        vga_write("  line "); put_uint(i); vga_putchar('\n');
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("\nnaos B3: scroll OK.");
    for (;;) __asm__ volatile ("hlt");
}
```

`put_uint` est un mini-`itoa` local (pas de `printf` en freestanding) : il imprime un entier en
décomposant ses chiffres. Un vrai `printf`-like viendra plus tard.

### 3.8 — Vérifier

```bash
make run-b3                   # (= make run : B3 est la dernière brique) ; run-kernel pour itérer
make run-b3 QMP=1             # (autre terminal) python3 tools/qemu-shot.py
```

Attendu : un en-tête vert/gris, puis des lignes cyan numérotées dont les premières ont
**défilé** hors de l'écran, et « naos B3: scroll OK. » en bas. Texte **formaté + couleurs +
défilement** : **critère B3 atteint.**

> **Suite (B3+) — la police.** Remplacer la police CP437 de la ROM par la nôtre (cf.
> `tools/vgafont.py`, `assets/fonts/ibm_vga_8x16.bin`) se fait en écrivant dans le générateur
> de caractères VGA — étape ultérieure du driver.

---

## Annexes

### Annexe A — Rappels d'assembleur x86 (real mode, 16 bits)

À lire avant/pendant B1.

**A.1 Ce qu'est l'assembleur.** L'assembleur est la représentation **lisible** du langage
machine : chaque **mnémonique** (`mov`, `jmp`, `int`…) correspond à une (ou quelques)
instruction(s) machine que le CPU exécute directement. Contrairement au C, il n'y a **pas de
traduction sémantique** : ce que tu écris est, à un encodage près, ce que le CPU fait. On
l'utilise là où le C ne *peut pas* aller (real mode, `lgdt`, `iret`, bascule de mode).
Principe directeur : **tu gères tout à la main** — pas de variables typées, pas de pile
d'appel automatique, pas de vérification. Le CPU fait *exactement* ce que tu dis, erreurs
comprises.

**A.2 Les registres (16 bits).** Un registre = une case mémoire ultra-rapide *dans* le CPU.

| Registre | Nom | Usage conventionnel |
|---|---|---|
| `AX` | Accumulator | calculs, valeur de retour ; `AH`/`AL` = octet haut/bas |
| `BX` | Base | base d'adressage ; `BH`/`BL` |
| `CX` | Counter | compteur de boucle (`loop`, `rep`) ; `CH`/`CL` |
| `DX` | Data | données, port d'E/S ; `DH`/`DL` |
| `SI` | Source Index | pointeur source (`lodsb`, `movsb`) |
| `DI` | Destination Index | pointeur destination |
| `SP` | Stack Pointer | sommet de la pile |
| `BP` | Base Pointer | base d'une trame de pile |
| `IP` | Instruction Pointer | adresse de la prochaine instruction (non modifiable directement) |
| `CS DS ES SS` | Segments | Code / Data / Extra / Stack (voir A.3) |
| `FLAGS` | Drapeaux | résultats de comparaisons (ZF, CF, SF…) |

`AX` est sur 16 bits ; ses moitiés `AH` (bits 8-15) et `AL` (bits 0-7) sont adressables
séparément. Écrire dans `AL` ne touche pas `AH`. En mode protégé (B1+), ces registres
s'étendent à 32 bits : `EAX`, et `AX` en est la moitié basse.

**A.3 Real mode et l'adressage `segment:offset`.** En real mode, une adresse physique se
calcule sur 20 bits :

```
adresse physique = (segment × 16) + offset
```

Exemple : `DS = 0x07C0`, `offset = 0` → `0x7C00` ; même adresse que `DS = 0`, `offset = 0x7C00`.
Il y a donc plusieurs façons d'écrire la même adresse. Dans naos on choisit la plus simple :
**tous les segments à 0**, et on raisonne en offsets absolus depuis 0 (c'est pourquoi
`boot.asm` met `DS = ES = SS = 0`). Pourquoi `0x7C00` ? Convention IBM de 1981 : le BIOS y
charge toujours le boot sector.

**A.4 Syntaxe Intel (NASM) : `instruction destination, source`** (destination à gauche, comme
`dest = src` en C).

```nasm
mov ax, 5      ; ax <- 5         (immédiat)
mov ds, ax     ; ds <- ax        (registre)
mov al, [si]   ; al <- octet à l'adresse SI   (crochets = déréférencement)
mov [di], al   ; octet à l'adresse DI <- al
```

Règles clés : **crochets `[ ]`** = « contenu de la mémoire à cette adresse » (comme `*ptr`) ;
sans crochets = la valeur elle-même (adresse, nombre, registre) ; la **taille** est déduite
des registres (`al` = 1 o, `ax` = 2 o), sinon on la précise (`byte`, `word`).

**A.5 Les instructions qu'on croise dans naos.**

| Instruction | Effet |
|---|---|
| `mov d, s` | copie `s` dans `d` |
| `xor a, a` | met `a` à 0 (xor d'une valeur avec elle-même) — plus court que `mov a,0` |
| `test a, a` | calcule `a AND a` sans stocker, juste pour positionner les drapeaux (ZF=1 si `a`==0) |
| `cmp a, b` | calcule `a - b` sans stocker, positionne les drapeaux |
| `jmp lbl` | saut inconditionnel |
| `jz / jnz lbl` | saut si ZF=1 / ZF=0 (zéro / non zéro) |
| `lodsb` | `AL <- [DS:SI]`, puis `SI++` (charge un octet et avance) |
| `int n` | déclenche l'interruption logicielle n (appel d'un service BIOS) |
| `hlt` | arrête le CPU jusqu'à la prochaine interruption |
| `cli / sti` | désactive / réactive les interruptions matérielles |
| `push / pop` | empile / dépile une valeur (via `SP`) |

**A.6 La pile.** `SP` pointe sur le sommet. Particularité x86 : elle **croît vers le bas**
(`push` *décrémente* `SP`). D'où `mov sp, 0x7C00` : la pile grandit *sous* le code, sans
l'écraser.

**A.7 Directives NASM (≠ instructions).** Une directive s'adresse à l'assembleur, pas au CPU :

| Directive | Rôle |
|---|---|
| `bits 16` | « assemble en code 16 bits » |
| `org 0x7C00` | « ce code vivra à 0x7C00 » → base des calculs d'adresse |
| `db`, `dw`, `dd` | pose des octets / mots (2 o) / double-mots (4 o) bruts |
| `times N x` | répète `x` N fois (padding) |
| `label:` | nom symbolique d'une adresse (global) ; `.label` = local au dernier global |
| `$` / `$$` | adresse courante / début de section (`$-$$` = octets écrits) |

> **Labels globaux vs locaux.** Un label sans point (`start`) est global ; un label avec point
> (`.print`) est **local**, rattaché au dernier global au-dessus (donc `start.print`). Ça
> évite les collisions : tu peux réutiliser `.loop`/`.done` dans plusieurs routines. Règle
> mentale : **global = repères (routines, entrée) ; local (`.`) = cibles de saut internes.**

### Annexe B — `boot/boot.asm` ligne par ligne

```nasm
bits 16
```
**Directive.** On cible le real mode 16 bits, car le CPU démarre dans ce mode. Sans ça, NASM
encoderait des instructions 32 bits, mal interprétées au boot.

```nasm
org  0x7C00
```
**Directive.** On annonce que le code s'exécutera à `0x7C00`. NASM calcule alors toutes les
adresses de labels (comme `msg`) à partir de cette base. Oubli classique : sans `org`,
`mov si, msg` pointerait à côté et on afficherait du charabia.

```nasm
start:
```
**Label.** Point d'entrée symbolique, pour la lisibilité. Le CPU, lui, commence simplement à
`0x7C00` (premier octet du fichier).

```nasm
    cli
```
On **désactive les interruptions matérielles** : pendant qu'on reconfigure segments et pile,
on ne veut pas qu'une interruption survienne sur une pile incohérente.

```nasm
    xor ax, ax
```
`AX <- 0`. Idiome universel : `xor reg, reg` met à zéro en 2 octets (plus court que
`mov ax, 0`). On prépare la valeur 0 pour les segments.

```nasm
    mov ds, ax
    mov es, ax
    mov ss, ax
```
On met `DS`, `ES`, `SS` à 0. **Pourquoi via `AX` ?** Les registres de segment n'acceptent pas
d'immédiat (`mov ds, 0` est illégal) ; il faut passer par un registre général.

```nasm
    mov sp, 0x7C00
```
Sommet de pile à `0x7C00`. Comme la pile croît vers le bas, elle occupe la zone *sous* le code
(`SS:SP` = `0x0000:0x7C00`).

```nasm
    sti
```
On **réactive les interruptions** : la config est finie, et `int 0x10` en a besoin.

```nasm
    mov si, msg
```
`SI` pointe sur le 1er octet de la chaîne (source de `lodsb`). Grâce à `org`, `msg` est
l'adresse réelle en mémoire.

```nasm
.print:
```
Label **local** (le `.` le rattache à `start`). Début de la boucle d'affichage.

```nasm
    lodsb
```
`AL <- [DS:SI]` puis `SI++`. Une instruction qui charge l'octet courant *et* avance le pointeur
— pensée pour parcourir une chaîne.

```nasm
    test al, al
    jz .hang
```
`test al, al` positionne ZF selon `AL`. Si `AL == 0` (fin de chaîne), `jz` saute vers `.hang`.
Principe : on teste (`test`/`cmp`) *puis* on saute (`jz`/`jnz`), en deux temps.

```nasm
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
```
Service vidéo BIOS. `int 0x10` lit `AH` pour la fonction : `0x0E` = téléscripteur (affiche `AL`,
avance le curseur). `BH` = page vidéo 0. On *consomme* un service du firmware (il disparaîtra
en mode protégé).

```nasm
    jmp .print
```
Retour en haut de boucle pour le caractère suivant.

```nasm
.hang:
    hlt
    jmp .hang
```
`hlt` met le CPU en pause jusqu'à une interruption ; si réveillé, `jmp .hang` le rendort.
**Pourquoi ?** Sans ça, le CPU exécuterait les octets suivants (le padding `0x00`, interprété
`add [bx+si], al`) et finirait par planter.

```nasm
msg db "naos B0: it boots!", 13, 10, 0
```
**Données.** `db` pose les octets de la chaîne. `13, 10` = CR LF ; `0` = terminateur lu par
`test al, al`.

```nasm
times 510-($-$$) db 0
```
**Padding.** `$-$$` = octets écrits jusqu'ici. `510 - (ça)` = nombre de zéros pour atteindre
l'octet 510 → la signature tombe pile aux offsets 510-511.

```nasm
dw 0xAA55
```
**Signature de boot.** `dw` pose 2 octets. En little-endian, `0xAA55` s'écrit `55 AA` →
octet 510 = `0x55`, octet 511 = `0xAA`. Sans elle, le BIOS déclare le disque non-bootable.

**Principes à retenir.**
1. **Tout est explicite** : segments, pile, fin de chaîne — rien n'est automatique.
2. **Tester puis sauter** : `test`/`cmp` posent les drapeaux, `jz`/`jnz` décident.
3. **Directive ≠ instruction** : `org`/`times`/`db` parlent à NASM ; `mov`/`int`/`hlt` au CPU.
4. **`org` est vital** : il aligne les adresses calculées sur l'adresse réelle de chargement.
5. **Gare le CPU** (`hlt; jmp $`) au lieu de le laisser tomber dans le vide.

### Annexe C — `int 0x10` en détail (services BIOS & interruptions)

Comprendre `int 0x10`, c'est comprendre **tous** les services BIOS (`int 0x13` disque,
`int 0x16` clavier, `int 0x15` mémoire…) : même mécanisme.

**C.1 Une seule porte, plein de fonctions.** `int 0x10` n'affiche pas « par nature » : c'est
le **point d'entrée unique des services vidéo** du BIOS, qui sait faire des dizaines de
choses. Comment sait-il *laquelle* tu veux ? Il **regarde `AH`** :

| `AH` | Fonction | Arguments |
|---|---|---|
| `0x00` | changer de mode vidéo | `AL` = mode |
| `0x02` | positionner le curseur | `BH` = page, `DH` = ligne, `DL` = colonne |
| `0x06` | faire défiler vers le haut | `AL`, `CX`, `DX`, `BH`… |
| **`0x0E`** | **téléscripteur (afficher 1 caractère)** | **`AL` = caractère, `BH` = page, `BL` = couleur** |
| `0x13` | afficher une chaîne | `ES:BP` = chaîne, `CX` = longueur… |

**C.2 Qui décide que `0x0E` = téléscripteur ?** **IBM**, dans la spécification du BIOS de
1981. C'est une **convention publiée et figée**, clonée par tous les BIOS (SeaBIOS inclus).
Référence canonique : la *Ralf Brown's Interrupt List*. Ce n'est ni le matériel ni le hasard :
c'est un **contrat d'API**, un numéro convenu d'avance — comme `0x7C00` ou `0xAA55`.

**C.3 Les registres = les paramètres.** En real mode, pas de passage d'arguments par la pile
comme en C : **les arguments transitent par les registres**, et la routine BIOS les y lit
directement. D'où la grille de lecture :

- `int 0x10` = **quel service** (vidéo) ;
- `AH` = **quelle sous-fonction** (le « sélecteur de méthode ») ;
- `AL`, `BH`, `BL`… = **les arguments** de cette sous-fonction.

Donc « il affiche `AL` » parce que la spec de la fonction `0x0E` *dit* que le caractère est
dans `AL`. Le BIOS, voyant `AH=0x0E`, va lire `AL`.

**C.4 Le même bout de code, en Python.** `int 0x10` ≈ une grosse fonction qui *dispatche* sur
`AH` et lit ses arguments dans des « registres » :

```python
def int_10h(regs):
    if regs.AH == 0x00:        # changer de mode vidéo
        set_video_mode(regs.AL)
    elif regs.AH == 0x02:      # positionner le curseur
        set_cursor(page=regs.BH, row=regs.DH, col=regs.DL)
    elif regs.AH == 0x0E:      # téléscripteur
        put_char(chr(regs.AL), page=regs.BH)   # <-- lit AL et BH
        advance_cursor()
    # ... autres sous-fonctions ...
```

Et l'assembleur :

```nasm
mov ah, 0x0E
mov al, 'X'
int 0x10
```

… équivaut **exactement** à :

```python
regs.AH = 0x0E       # je veux la fonction "téléscripteur"
regs.AL = ord('X')   # le caractère
int_10h(regs)        # appel
```

Les `mov` = « remplir les cases d'arguments » ; `int 0x10` = « appeler la fonction ».

**C.5 Comment `int n` atteint réellement le BIOS : l'IVT.** En real mode, il existe à
l'adresse `0x0000` une **table des vecteurs d'interruption (IVT)** : 256 entrées, chacune un
pointeur `segment:offset` vers une routine. `int 0x10` fait : *« va lire l'entrée n°`0x10` de
l'IVT, et saute à la routine qui y est inscrite »*. Le BIOS a installé sa routine vidéo dans
ce slot au démarrage. Elle s'exécute, lit tes registres, fait le travail, puis revient avec
`iret`.

```
int 0x10  →  CPU lit IVT[0x10]  →  saute à la routine vidéo du BIOS
          →  la routine lit AH (=0x0E), AL, BH  →  écrit dans 0xB8000  →  iret (retour)
```

**C.6 C'est l'ancêtre des appels système.** Un syscall Linux, c'est le même patron : `eax` =
numéro du syscall (le sélecteur), `ebx/ecx/…` = arguments, puis `int 0x80` (ou `syscall`).
`int 0x10` + `AH` = exactement ça : **un numéro qui dispatche + des registres-arguments + une
instruction qui transfère la main à du code de service.**

> **Point clé.** `AH`/`AL` sont les deux moitiés d'`AX` : la convention met le **sélecteur**
> dans `AH` et la **donnée** dans `AL`, ce qui permet de charger les deux d'un coup —
> `mov ax, 0x0E58` met `AH=0x0E` et `AL=0x58` ('X'). Compact et idiomatique.
