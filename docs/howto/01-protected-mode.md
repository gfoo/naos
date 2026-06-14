[← Sommaire du HOWTO](../HOWTO.md)

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

> **Qui définit ces étapes ?** Deux familles. La GDT/`lgdt` (3), le bit `CR0.PE` (4) et le far
> jump (5) — ainsi que le `cli` (1) — forment la **séquence canonique définie par Intel** :
> architecture x86, *Intel SDM* Vol. 3A §9.9 « Switching to Protected Mode » (mode protégé
> introduit avec le 80286 en 1982). Le CPU lit ces structures en matériel. **L'A20 (2) est
> l'intrus** : elle n'existe *pas* dans l'architecture du CPU. C'est un greffon historique de la
> **plateforme PC** — besoin créé par **IBM** (PC AT, 1984, pour préserver le *wrap-around* du
> 8086), activé selon les machines via le contrôleur clavier 8042, le port `0x92` (« Fast
> A20 ») ou `int 0x15`. D'où l'absence de méthode unique : 3/4/5 sont propres parce qu'Intel les
> normalise, A20 est sale parce que c'est de l'archéologie IBM/chipset.

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

