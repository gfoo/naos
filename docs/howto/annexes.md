[← Sommaire du HOWTO](../HOWTO.md)

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
