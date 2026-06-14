# tools/ — outils de développement naos

## `qemu-shot.py` — capture l'écran de QEMU (B0 → B12)

Juste une **photo de l'écran**, rien d'autre : tu lances QEMU headless avec un socket QMP (le
protocole de contrôle de QEMU), le script s'y attache et écrit `build/shot.png`. Tu ouvres
l'image et tu juges toi-même — pas de PASS/FAIL, pas d'OCR.

```sh
make run QMP=1                  # terminal 1 : QEMU sans fenêtre + socket QMP
python3 tools/qemu-shot.py      # terminal 2 : écrit build/shot.png
```

Options : `--sock` (socket QMP, défaut `/tmp/naos-qmp.sock`), `--shot` (PNG, défaut
`build/shot.png`), `--timeout` (défaut 10 s).

Le seul truc malin : **pas de `sleep` fixe**. Capturer trop tôt donne « display not
initialized » ; le script attend que l'écran se *stabilise* (deux captures de taille proche).
Comme le curseur `_` du mode texte clignote pour toujours, il compare la **taille** des PNG
(clignotement ≈ 0,5 %, toléré), pas les octets exacts. Voir `docs/HOWTO.md` § 0.6.

## `vgafont.py` — police VGA 8×16 (préparation B3)

> En **B3** (driver écran), naos remplacera la police CP437 de la ROM par la sienne, en
> écrivant ses glyphes dans le générateur de caractères VGA. Cet outil prépare et inspecte
> cette police **en textuel**, avant même d'avoir le driver.

### Le format (ce que B3 écrira dans la VGA)
Une police mode-texte VGA = **256 glyphes × 16 octets = 4096 octets** :
- glyphe `N` → offset `N*16` ;
- 1 octet = **1 ligne** de 8 pixels (de haut en bas) ;
- bit 7 (`0x80`) = pixel le plus **à gauche**.

Exemple, le `A` (`0x41`) :
```
00 00 10 38 6C C6 C6 FE C6 C6 C6 C6 00 00 00 00
        ...#....
       ..###...
       .##.##..
       ##...##.
       #######.   <- la barre du milieu (0xFE)
       ...
```

### La police originale
`assets/fonts/ibm_vga_8x16.bin` = l'**IBM VGA 8×16** (le look BIOS), générée depuis le TTF
PxPlus du *Ultimate Oldschool PC Font Pack* (int10h.org, CC BY-SA). C'est notre référence.

### Les 4 gestes (tout en texte)
```sh
# RÉCUPÉRER  — fabriquer la police depuis un TTF (Pillow requis)
python3 tools/vgafont.py build assets/fonts/ibm_vga_8x16.bin \
    --ttf "/chemin/PxPlus_IBM_VGA_8x16.ttf"

# CONSULTER  — voir un glyphe en ASCII + ses 16 octets   (char = 'A', 0x41 ou 65)
python3 tools/vgafont.py show assets/fonts/ibm_vga_8x16.bin A
python3 tools/vgafont.py show assets/fonts/ibm_vga_8x16.bin 0xB1   # trame ▒ (55 AA…)

# MODIFIER   — effet "rough" (scanlines+bruit). NB: illisible à 8×16, c'est un effet grand format.
python3 tools/vgafont.py rough assets/fonts/ibm_vga_8x16.bin /tmp/rough.bin

# EXPORTER   — pour embarquer la police dans le kernel en B3
python3 tools/vgafont.py export assets/fonts/ibm_vga_8x16.bin --asm   # tableau NASM (db)
python3 tools/vgafont.py export assets/fonts/ibm_vga_8x16.bin --c     # tableau C
```

### À retenir pour B3
- Le **vert phosphore** ne vient PAS de la police mais de l'**octet d'attribut** (`0x0A` vif /
  `0x02` sombre) — cf. `docs/DESIGN-LOG.md` (entrée « Identité visuelle de l'écran de boot »).
- Les **nuances** (barre de progression « dernier carré plus clair ») sont natives CP437 :
  `0xB0` ░, `0xB1` ▒, `0xB2` ▓, `0xDB` █ — déjà dans la police, rien à dessiner.
- Le **rough** reste pour un futur mode graphique (B11+) : à 8×16 il détruit la lisibilité.
