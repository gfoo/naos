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
firmware compris. Faire `make run`, c'est *allumer un PC virtuel*.

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

Le `Makefile` expose deux cibles principales :

> **`make run`** — assemble, fabrique l'image, lance QEMU (fenêtre normale).
> **`make debug`** — lance QEMU **figé** (`-s -S`), en attente d'un GDB sur `:1234`.

```bash
make run     # itération quotidienne
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
> distinct dès maintenant stabilise la cible `make run` (elle lance toujours *l'image*, quel
> qu'en soit le contenu) et installe le modèle mental **composant vs disque**.

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
> Ajoute un petit bout, lance `make run`, observe, recommence. Le jour où ça plante, tu sais
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

`make run` → SeaBIOS affiche `Booting from Hard Disk...`, puis **écran noir**, sans rien
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

`make run` → un `X` apparaît sous les lignes de SeaBIOS. **Preuve : on sait appeler le
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

`make run` → le message complet. **Preuve : la boucle d'affichage et la donnée `msg`.**

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

**À la main (fenêtre) :**

```bash
make run
```

SeaBIOS → `Booting from Hard Disk...` → `naos B0: it boots!` → curseur figé (le CPU est en
`hlt`). **Critère de réussite : le message s'affiche et la machine ne redémarre pas en
boucle** (un reboot en boucle = triple-fault).

**Sans tête (automatisable, réutilisable à chaque brique) :** on lance QEMU headless, on
capture l'écran par QMP, et on détecte les triple-faults.

```bash
rm -f build/qemu.log build/shot.png /tmp/naos-qmp.sock
qemu-system-i386 -drive format=raw,file=build/naos.img -display none -no-reboot \
  -qmp unix:/tmp/naos-qmp.sock,server,nowait -d int,cpu_reset -D build/qemu.log &
QPID=$!
python3 - <<'PY'
import socket, json, time
for _ in range(200):
    try:
        s = socket.socket(socket.AF_UNIX); s.connect("/tmp/naos-qmp.sock"); break
    except OSError: time.sleep(0.02)
def line():
    b=b""
    while b"\n" not in b: b += s.recv(4096)
    return b
line()                                                   # message d'accueil QMP
def cmd(d): s.sendall((json.dumps(d)+"\n").encode()); return line()
cmd({"execute":"qmp_capabilities"})
time.sleep(2.5)                                          # laisser SeaBIOS booter (temps réel)
cmd({"execute":"screendump","arguments":{"filename":"build/shot.png","format":"png"}})
PY
kill $QPID 2>/dev/null
grep -iE "triple|shutdown" build/qemu.log && echo "PLANTAGE" || echo "OK : pas de triple-fault"
```

Ouvre ensuite `build/shot.png` : tu dois y voir `naos B0: it boots!`.

> **Pourquoi le `time.sleep(2.5)` ?** Le boot est quasi instantané en temps *guest*, mais
> QMP répond bien avant que SeaBIOS ait fini en temps *réel*. Capturer trop tôt donne une
> image « Guest has not initialized the display (yet) ». On laisse la machine vivre quelques
> secondes avant la photo.

---

### Pour aller plus loin → B1

- B0 **emprunte** le firmware (`int 0x10`). **B1** va jusqu'au bout du démarrage real mode —
  activation **A20**, **GDT**, bit **PE** de `CR0`, **far jump** — pour entrer en **mode
  protégé 32 bits**. Là, `int 0x10` disparaît : on affichera en écrivant directement dans
  `0xB8000`.
- L'affichage direct `0xB8000` et le **remplacement de la police CP437** : ce sera le **driver
  écran (B3)**.

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
