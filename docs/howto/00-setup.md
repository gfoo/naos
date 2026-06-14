[← Sommaire du HOWTO](../HOWTO.md)

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
> **[Annexe C — `int 0x10` en détail](annexes.md#annexe-c--int-0x10-en-détail-services-bios--interruptions)**.
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
> besoin. **Détail ligne par ligne en [Annexe B](annexes.md#annexe-b--bootbootasm-ligne-par-ligne).**

> **Nouveau ou rouillé en assembleur ?** Voir l'[Annexe A](annexes.md#annexe-a--rappels-dassembleur-x86-real-mode-16-bits)
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
**[Annexe C](annexes.md#annexe-c--int-0x10-en-détail-services-bios--interruptions)**.

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

