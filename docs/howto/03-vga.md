[← Sommaire du HOWTO](../HOWTO.md)

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

