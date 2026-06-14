[← Sommaire du HOWTO](../HOWTO.md)

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

