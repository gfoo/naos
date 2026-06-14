# naos — DESIGN-LOG

> Journal de conception : trace de nos discussions, décisions et raisonnements.
> Complète `PLAN.md` (le *quoi* à faire) en gardant le *pourquoi* et les échanges qui
> ont mené aux choix. Alimenté à la demande, par entrées datées.

---

## 2026-06-07 — Choix techniques C1–C4

### C1 — Architecture : x86 32-bit ✅
- 32-bit suffisant pour un projet pédagogique.
- Le 64-bit forcerait l'activation du paging *avant même* d'entrer en long mode → noie
  l'apprentissage des fondamentaux. Le higher-half (B8) reste simple en 32-bit.
- Tension notée pour plus tard : l'objectif « tout construire soi-même » (OS entièrement
  maison, jusqu'à son propre bootloader UEFI) pousserait vers le 64-bit pour cohérence. À
  reconsidérer si on décide d'écrire notre propre loader UEFI. Reporté.

### C2 — Émulateur : QEMU + Bochs ✅
- **QEMU** = runner quotidien (`make run-bN`), rapide, stub GDB.
- **Bochs** = microscope de débogage (débogueur intégré pensé osdev) pour B1–B5 :
  inspecter real mode, GDT/IDT, `CR0`/`CR3` instruction par instruction, et surtout
  diagnostiquer les **triple-faults** que QEMU encaisse en silence (reset).
- Non exclusifs : QEMU pour itérer, Bochs quand un boot part en vrille.
- Écartés : VirtualBox/VMware (pas d'introspection bas-niveau), 86Box/PCem (overkill).

### C3 — Langages : C + NASM ✅
- C = kernel (drivers, paging, scheduler). NASM = ce que C ne peut pas exprimer
  (`lgdt`, `iret`, `cli`, real mode, far jump). « Glue » = stub ASM→`kmain()` + wrappers
  d'interruptions.
- Syntaxe Intel (NASM) plutôt qu'AT&T (GAS) : c'est celle de toute la doc osdev.
- Pas de C++/Rust : pour ne pas masquer les fondamentaux derrière un runtime.

### C4 — Stratégie de boot : hybride ✅
- Phase 1 (B1) : boot sector maison **jetable** → *comprendre* real mode, `0x7C00`,
  A20, GDT, passage en mode protégé.
- Phase 2 (B2) : déléguer à GRUB/Multiboot → livré en mode protégé 32-bit, kernel à 1 Mo,
  + memory map (utile dès B7).
- Évite le double piège : « tout main » (enlisement) vs « GRUB direct » (real mode jamais vu).
- **Question rouverte, reportée** : B1→B2 reste-t-il « maison *puis* GRUB », ou bien
  « maison *jusqu'au bout*, sans GRUB » pour rester fidèle au « tout soi-même » ? Les deux
  défendables. À trancher le moment venu.

### Méthode — HOWTO.md
- Chaque brique consignée au fil de l'eau dans `docs/HOWTO.md` : explicatif et formateur,
  assez détaillé pour réalisation **à la main** OU **avec l'assistant** — choix laissé à
  l'utilisateur à chaque brique. Démarre à B0.

---

## 2026-06-07 — Notes conceptuelles : la chaîne de boot (du bouton power à `kmain()`)

Compréhension construite par questions/réponses. Points retenus :

### Qui décide quoi (chaîne de responsabilité)
- **Fondeur CPU (Intel/AMD)** : au reset, le CPU exécute *toujours* `0xFFFFFFF0` (reset
  vector), en real mode 16-bit. Câblé dans le silicium — c'est le CPU qui décide, seul.
- **Fabricant de carte mère** : *route* l'adresse `0xFFFFFFF0` vers la puce flash (ROM).
  Il garantit qu'il y a quelque chose à cette adresse ; il ne « force » pas le CPU.
- **Fournisseur de firmware (IBV : AMI, Phoenix, Insyde)** : écrit le code du firmware,
  personnalisé par le fabricant de carte.

### Firmware / BIOS / UEFI
- **Firmware** = catégorie (tout logiciel gravé dans du matériel). **BIOS** et **UEFI** =
  deux *types* de firmware. Frères, pas parent-enfant. La ROM contient l'un OU l'autre.
- **BIOS** = *Basic Input/Output System*, terme de 1975 (Kildall, CP/M), repris par IBM
  en 1981. Décrit son métier : services d'I/O de base (`int 0x10` écran, `int 0x13` disque).
- **UEFI** = firmware moderne, surtout en C, basé sur le socle open source **TianoCore
  EDK II** (UEFI Forum/Intel). C'est d'abord une *spécification* publiée ; « firmware UEFI »
  = un firmware qui l'implémente.
- **GRUB n'est PAS un firmware** : c'est un bootloader (sur disque), chargé PAR le firmware.

### La puce ROM
- Puce flash dédiée, **non-volatile** et **immédiatement lisible sans config** : obligatoire
  à cause du paradoxe de l'amorçage (au power-on, la RAM n'est pas configurée, le disque
  inaccessible). Le firmware ne peut être ni en RAM ni sur disque.
- **ROM vs Flash** : le mot « ROM » a survécu ; le firmware moderne est sur **flash**
  (réinscriptible) → d'où le « flasher son BIOS ». Acteurs distincts : fondeur de la puce
  (Winbond, Macronix, Micron…) ≠ auteur du firmware (IBV) ≠ intégrateur (fabricant carte).
- Taille : vieux BIOS 64–256 Ko ; **UEFI moderne 16–32 Mo** (assez pour l'UI graphique de
  setup avec souris, pile réseau, etc. — c'est une *application UEFI* lancée à la touche
  F2/Suppr, et elle vit entièrement en ROM).

### La chaîne complète
```
ON → CPU exécute 0xFFFFFFF0 (règle CPU) → carte mère route vers la ROM
   → FIRMWARE démarre (BIOS OU UEFI, un seul) → POST → cherche un disque bootable
        ├─ BIOS : lit secteur 0 (512 o / MBR) à 0x7C00, vérifie 0xAA55, saute
        └─ UEFI : monte la partition FAT32 (ESP), exécute un fichier .efi
   → BOOTLOADER (GRUB ou le nôtre) → charge le kernel → kmain()
```
- Cascade « petit/figé charge gros/souple » : puce flash → bootloader → kernel.
- Dans QEMU : firmware = SeaBIOS (legacy) par défaut ; `-kernel` permet de court-circuiter
  GRUB (QEMU joue le mini-bootloader Multiboot).

### MBR vs .efi (où vit le bootloader)
| | MBR (BIOS) | .efi (UEFI) |
|---|---|---|
| Où | Secteur 0 brut, 512 o | Fichier sur partition FAT32 (ESP) |
| FS compris par le firmware ? | Non (octets bruts) | Oui (driver FAT32) |
| Écriture | `dd` (brut) | `cp` (fichier) |
| Limite | 512 o (stage 1) | aucune |
| Format | code brut + 0xAA55 | exécutable PE/COFF |
- USB / disque / SD : aucune différence de principe (block devices), l'ordre de boot choisit.

### « Tout faire soi-même » sans GRUB ?
- **Oui, possible.** GRUB = raccourci pragmatique, pas obligation.
  - Chemin BIOS : c'est *déjà* B1 — boot sector maison via `int 0x13`/`int 0x10`, on
    continue jusqu'à charger le kernel soi-même au lieu de passer la main à GRUB.
  - Chemin UEFI : écrire sa propre app `.efi` (Boot Services : GetMemoryMap, FS, GOP,
    ExitBootServices). Toolchain GNU-EFI ou EDK II.
- **Le firmware, lui, ne s'écrit jamais** : il est dans la puce, on en dépend toujours.
  Le « tout soi-même » réaliste = remplacer GRUB, pas le BIOS/UEFI. La frontière de naos
  commence où le firmware nous donne la main.
- Coût du « tout maison » : écrire les parties ingrates que GRUB offrait (lecture disque
  bas-niveau, parsing ELF, détection mémoire). Plus de travail, parfois peu instructif
  passé la première fois — mais cohérent avec l'objectif « tout construire soi-même ».

### Standards indépendants des fournisseurs (ce qui rend l'OS portable)
- Architecture x86 (Intel/AMD), convention PC/AT (héritage IBM 1984 : `int 0x10`/`0x13`,
  `0x7C00`, `0xAA55`, MBR), spécification UEFI (Boot Services, PE/COFF, GPT, ESP FAT32,
  `EFI/BOOT/BOOTX64.EFI`), spécification Multiboot (contrat bootloader↔kernel).
- GRUB existe en versions séparées compilées contre ces standards : `grub-pc` (BIOS, via
  `int 0x13`) et `grub-efi` (UEFI, via Boot Services). Il « connaît » le firmware parce que
  le firmware obéit à un standard publié — aucun code par-vendeur.

---

## 2026-06-07 — On ne code pas un BIOS / les trois « interruptions »

Clarification d'un malentendu : **on n'écrit jamais de BIOS.** Notre OS commence là où le
firmware nous donne la main (`0x7C00`). On code le boot sector (B1) et le kernel (B2+),
pas le firmware.

Trois choses différentes derrière le mot « interruption » :
1. **App de setup UEFI (touche F2/Suppr)** : ce n'est pas une interruption ni nous — c'est
   une application *du firmware*, dans la ROM. Jamais touchée.
2. **Interruptions BIOS qu'on *appelle* en B1** (`int 0x10` écran, `int 0x13` disque) : on
   ne les écrit pas, on les *utilise* via l'instruction `int`. Le firmware fournit ces
   services en real mode. ⚠️ Dès le passage en mode protégé (fin B1), **ces services
   disparaissent** → d'où la nécessité d'écrire nos propres driver écran (B3), IDT (B5),
   handler clavier (B6).
3. **Nos propres handlers qu'on *écrira* (B5/B6)** : IDT + PIC (B5), handler clavier IRQ1
   (B6). Là, « une touche déclenche une interruption qui exécute *notre* code » — mais
   c'est notre kernel, pas un BIOS.

Distinction clé : **interruptions logicielles** (`int 0x10` — on appelle le BIOS, real mode)
vs **interruptions matérielles** (IRQ — le clavier nous interrompt, on fournit le handler).
B1 *consomme* les premières ; B5/B6 *produisent* les secondes. Écrire un OS = non pas écrire
un BIOS, mais **se passer du BIOS**.

---

## 2026-06-07 — QEMU émule la machine entière, firmware compris

QEMU n'est pas « un lanceur de kernel » : c'est un **PC complet en logiciel**, jusqu'au
firmware. Ce qu'on fixe vs ce que QEMU apporte :

| Composant | Flag QEMU | On le fournit ? |
|---|---|---|
| CPU | `-cpu`, `-smp` | on le choisit |
| RAM | `-m 128M` | on fixe la taille |
| Disque | `-drive`, `-hda`, `-cdrom` | notre image |
| Chipset/carte mère | `-machine pc` (i440FX) / `q35` | choisi |
| **ROM + firmware** | **SeaBIOS, fourni d'office** | ❌ non, QEMU le donne |
| VGA, clavier PS/2, PIC, PIT | automatiques | non |

- Par défaut QEMU charge **SeaBIOS** (BIOS legacy open source) dans la ROM émulée. Le CPU
  émulé démarre à `0xFFFFFFF0` → exécute SeaBIOS → exactement comme le vrai matériel.
- C'est pour ça que B1 marche : SeaBIOS fournit `int 0x10`/`int 0x13` comme un vrai BIOS.
  La chaîne power→firmware→bootloader→kernel existe à l'identique *dans* QEMU.
- On verra le texte SeaBIOS une fraction de seconde avant que notre code prenne la main.
- **UEFI dans QEMU** : remplacer le firmware par **OVMF** (UEFI open source, compilé depuis
  EDK II) via `-bios OVMF.fd` / `-pflash` → machine UEFI pour tester un loader `.efi`.
- Cohérent avec la frontière établie : le firmware, on ne l'écrit jamais — vrai matériel
  comme émulation. QEMU respecte la même limite (SeaBIOS donné, non modifié).
- Pratique : `make run-b0` ≈ `qemu-system-i386 -drive ...` et SeaBIOS charge notre secteur de boot.

---

## 2026-06-07 — C5 (toolchain) + résolution des questions rouvertes

### C5 — Toolchain : cross-compiler `i686-elf-gcc` ✅
- Construit dès B0. Zéro hypothèse hôte (recommandation OSDev).
- Raccourci `gcc -m32 -ffreestanding` **écarté** : marche à B0–B3 mais peut injecter du code
  spécifique Linux (stack protector, relocations) → triple-faults inexplicables à B5/B8.
- Chaîne : `i686-elf-gcc -c` (→ `.o` ELF) → `i686-elf-ld -T linker.ld` (→ ELF kernel) →
  `objcopy -O binary` pour B1 (jette l'ELF, garde les octets bruts du boot sector).

### Note conceptuelle — statique & format de sortie
- Tout est **statique par obligation** (pas d'OS, pas de lien dynamique, `-nostdlib`, pas de
  crt0, point d'entrée maison). Mais ce n'est pas un « ELF statique Linux ».
- Format par brique : **B1 = binaire plat** (octets bruts, BIOS ne parse pas l'ELF) ;
  **B2+ = ELF** chargé par GRUB (qui parse l'ELF et place les segments).
- Lié à **adresse absolue** (`0x100000` = 1 Mo) via `linker.ld`, `-fno-pic -fno-pie` (pas PIE).
- ELF = **enveloppe** autour des instructions x86-32 (métadonnées + code) ; le CPU n'exécute
  que les instructions, un *chargeur* (GRUB) ouvre l'enveloppe. Règle générale : plus le
  chargeur est intelligent, plus la boîte peut être riche (BIOS→brut, GRUB→ELF, UEFI→PE/COFF).

### Note conceptuelle — pourquoi GRUB & dépendances
- Le « passe-plat maison » est faisable (B1 étendu : `int 0x13` + binaire plat). GRUB n'est
  pas obligatoire ; sa vraie valeur = **carte mémoire E820 servie gratuitement** + plus de
  géométrie disque à gérer + kernel chargé comme un fichier.
- Hiérarchie des dépendances :
  - **Firmware (SeaBIOS)** = 1re dépendance logicielle chronologiquement, mais **non remplaçable**
    (plancher, jamais écrit — vrai matériel comme QEMU).
  - **Toolchain** = dépendances *build-time*, pas dans l'OS final.
  - **GRUB** = 1re dépendance tierce **embarquée dans la chaîne de boot ET remplaçable**.
    C'est la seule couture empruntée qu'on peut couper → la « frontière de souveraineté ».

### RÉSOLUTION des questions rouvertes
- **Question C4 (GRUB vs loader maison)** → **tranché : on garde l'hybride.** GRUB reste pour
  B2+. Raison : faire son loader *avant* d'avoir un kernel = déboguer deux inconnues à la fois ;
  le faire *après* (avec une référence qui boote) = risque divisé par dix. Le « tout maison »
  n'est pas abandonné, juste **différé** à une brique tardive.
  → Ajout **B11 — Bootloader maison (optionnel, tardif)** : remplacer GRUB, code E820 maison.
- **Question C1 (32 vs 64-bit, cohérence avec l'objectif tout-maison)** → **tranché : on reste 32-bit** pour toute
  la feuille de route principale. La migration 64-bit est **différée** à une brique tardive.
  → Ajout **B12 — Passage en 64-bit (optionnel, tardif)** : long mode, paging pré-long-mode,
  GDT 64-bit, toolchain `x86_64-elf`. Cohérent avec « construit entièrement » (loader UEFI maison
  → 64-bit naturel), mais sans alourdir l'apprentissage des fondamentaux au début.
- **Principe directeur retenu** : les briques de *souveraineté* (couper GRUB, migrer 64-bit) se
  font **une fois l'OS fonctionnel**, pas en ouverture — quand elles sont faciles et instructives
  plutôt que coûteuses et bloquantes.

---

## 2026-06-07 — C6 (workflow) : phase « choix techniques » clôturée

### C6 — Workflow : issue→branche→PR par brique ✅
- Choix : **méthodologie complète** (pas le commit direct ni la version allégée).
- Mapping : 1 brique `Bx` = 1 issue `type:feature` → branche `feature/Bx-...` →
  commits `feat(Bx): ...` → PR `Closes #N` → merge **squash** → `❌`→`✅` dans PLAN.md.
- Les IDs du projet sont les briques **Bx** (schéma maison, cohérent avec PLAN.md), traités
  comme des features. Pas d'issues GitHub redondantes avec autre chose : PLAN + DESIGN-LOG
  donnent le contexte, l'issue trace l'exécution.
- Conséquence immédiate : **interdiction d'écrire du code sur `main`** (règle méthodo). Les
  fichiers `docs/` édités jusqu'ici = conception, pas du code → OK sur main. B0 (Makefile,
  toolchain, structure) exigera la branche.

→ **Les 6 choix techniques (C1–C6) sont tranchés. Section 0 du PLAN entièrement verte.**

---

## 2026-06-07 — HOWTO : exigence renforcée + format adopté

### Méthode HOWTO renforcée (supersede la note du 2026-06-07 / C1–C4)
Double exigence explicite (plus fort que « explicatif » seul) :
1. **Rejouable de zéro** — un lecteur parti d'une machine vierge reproduit *exactement* la
   brique : commandes, fichiers, contenus, vérification. Recette reproductible, pas un récit.
2. **Formateur** — explique le *pourquoi* de chaque étape (comprendre, pas copier).
- Écrit **en parallèle** du code (jamais après coup). **Définition de « brique finie »** :
  issue → branche → code **+ section HOWTO rejouable** → critère QEMU vérifié → PR → merge →
  `✅`. Une brique sans sa section HOWTO rejouable **n'est pas finie**, même si le code marche.
- Ordre de travail d'une brique : théorie → code → **vérif QEMU** → HOWTO rejouable → PR
  (le HOWTO vient après que ça marche — sinon on documente du faux — mais avant le merge).

### Format adopté (inspiré de `../../llm/dcsr-llm/TUTORIAL_fr.md`)
- **TOC d'abord**, puis **Objectif** (résultats en puces).
- Dans chaque brique : **Concept → commande → vérification**.
- **Encadré d'annotation de commande** : `> **`commande`** — ce qu'elle fait` *avant* chaque
  bloc de code (pattern le plus précieux : transforme un copier-coller en apprentissage).
- **Blocs de code copiables** (garantie « rejouable »).
- Deux apartés en blockquote : `> **Pourquoi ?**` (raisonnement) et `> **Point clé**` (piège/vérif).
- **Tableaux** pour paramètres/comparaisons ; `---` entre parties.
- **Squelette en 5 temps de chaque brique** : Concept → Termes clés → Étapes reproductibles
  → Vérification (critère QEMU, identique à PLAN.md) → Pour aller plus loin.
- Squelette `docs/HOWTO.md` posé (objectif + conventions + TOC B0–B12) ; parties remplies
  brique par brique.

### Trépied documentaire
- **PLAN.md** = le *quoi* (feuille de route, critères, statuts).
- **DESIGN-LOG.md** = le *pourquoi des choix* (décisions, raisonnements, échanges).
- **HOWTO.md** = le *comment* (recette rejouable et formatrice, brique par brique).

---

## 2026-06-09 — C6 révisé : commit direct sur `main`

- Après B0 + CHORE-001 menés avec le cycle complet (issue→branche→PR→merge squash), la
  cérémonie s'avère **trop lourde** pour un projet perso solo. Décision : **commit direct sur
  `main`**, pas de branche, pas de PR, pas d'issue.
- Override assumé : contredit la règle globale `workflow.md` (« never write code on main »)
  et le choix C6 initial. Justifié par le contexte (solo, pédagogique, historique linéaire
  suffisant). Le `main` n'a aucune protection de branche, donc rien ne bloque.
- Conventions conservées : messages de commit conventionnels (`feat(Bx):`, `docs:`…).
- Reste possible ponctuellement si besoin (ex. expérimentation risquée), mais ce n'est plus
  le défaut.

---

## 2026-06-14 — Identité visuelle de l'écran de boot (prototypée sur GRUB hôte)

> Décisions de *look* pour l'écran de démarrage de naos (B1 affichage real mode, B3 driver
> écran, B11 bootloader maison), distillées d'un prototype « thème matrix » mené sur le GRUB
> de la machine hôte (Ubuntu). Le **GRUB hôte n'est qu'un banc d'essai** : ce qui compte ici,
> c'est l'identité retenue + comment elle se réimplémente *nativement* dans naos.

### Identité retenue
- **Palette « matrix »** : vert phosphore sur noir. Sélection = vert vif, texte secondaire = vert sombre.
- **Police** : **IBM VGA 8×16** (look BIOS d'époque, pixels carrés).
- **Mise en page** : **calée à gauche**, **sans cadre**, façon log terminal/BIOS (pas de boîte centrée).
- **Texture « rough »** : glyphes volontairement *imparfaits* (scanlines + bruit) → effet CRT usé.
- **Indicateur d'attente** : **barre de progression** (carrés pleins, le dernier « plus clair »)
  pilotée par le décompte d'auto-boot. Alternative explorée : spinner `|/-\`.

### Cadeau pour naos : tout ça est quasi-natif en mode texte VGA
naos en real mode (B1) puis via le buffer texte `0xB8000` (B3) tombe pile sur ces briques :
- **La police IBM VGA 8×16 est GRATUITE** : c'est la police ROM du VGA, celle qu'`int 0x10` et
  le mode texte `03h` utilisent par défaut. Le look « BIOS » qu'on a cherché des heures à recréer
  sur GRUB, naos l'a **par défaut**.
- **Le vert sur noir = un octet d'attribut.** En mode texte, chaque cellule = `[char][attr]`,
  `attr = (bg<<4) | fg` :

  | Effet | Attribut | fg |
  |---|---|---|
  | Vert vif / noir (sélection) | `0x0A` | 10 = vert clair |
  | Vert sombre / noir (texte) | `0x02` | 2 = vert |

- **Nuances de vert sans vraie palette** : CP437 fournit des caractères de **trame** →
  `0xB0` ░, `0xB1` ▒, `0xB2` ▓, `0xDB` █. La barre « carrés pleins + dernier plus clair » se fait
  *directement* : `█████▓` (pleins + dernier `0xB2`). C'est l'astuce de dithering qu'on a dû
  bricoler sur GRUB — **native ici**.
- **Barre / spinner** : trivial, naos dessine lui-même son décompte. Spinner = réécrire `|`,`/`,`-`,`\`
  sur place ; barre = N blocs `0xDB` + reste en `0xB0`.

### Si naos passe au graphique (VESA real mode / GOP UEFI, ou B11+)
Plus de police ROM ni d'attributs : on rend des pixels. Leçons du proto GRUB :
- **Police = bitmap.** Effet « rough » = *cuire* l'imperfection dans les glyphes (mettre des pixels
  à 0 : 1 ligne sur 3 + ~10 % de bruit). En mode texte on peut même **téléverser une police 8×16
  custom** dans le générateur de caractères VGA (ports `0x3C4`/`0x3CE`, ou `int 0x10` AX=`1110h`)
  → effet rough **sans** passer au graphique.
- **Monochrome ⇒ dithering** pour simuler une 2ᵉ teinte (1 pixel sur 2 = vert « plus clair »).
- **Vert phosphore en RGB** (si palette dispo) : `#1f9e1f` (sombre), `#33d633` (médian),
  `#7dff7d` (vif), `#0e7a0e` (atténué) ; fond `#000000`.

### Garde-fous appris (sur GRUB, principe réutilisable)
- **Taille de police vs résolution** : un glyphe trop grand pour la résolution courante est rejeté
  (« glyphs too large » → fallback police minuscule). En graphique : **fixer la résolution** et
  tailler la police pour elle, jamais subir un mode « auto » imprévisible (l'eDP 4K de l'hôte
  n'exposait que le natif → 48 px y devenait illisible).
- **Astuce « le décompte EST l'indicateur »** : GRUB ne sait pas animer du texte, mais il redessine
  `%d` chaque seconde ; en **détournant les glyphes des chiffres** (0-9 → frames de spinner, ou →
  barres à 10 niveaux) on obtient une animation *gratuite*. naos n'a pas cette contrainte (il anime
  ce qu'il veut), mais l'idée « mapper une valeur sur un glyphe » reste maligne.
- **Secure Boot bloque les polices custom de GRUB** (lockdown) — sans objet pour naos (notre loader),
  mais c'est pourquoi le thème hôte exigeait Secure Boot *off*.
- **Banc d'essai sans reboot** : prototype validé en bootant GRUB dans **QEMU + OVMF** avec capture
  QMP — même démarche que la vérif des briques naos (cf. recette QEMU headless).

> **Pourquoi consigner ça** : le jour où on met couleur / police / indicateur de progression dans
> l'écran de boot de naos, on part de cette identité (vert matrix, IBM 8×16, gauche, barre de blocs)
> en sachant que **le mode texte VGA la donne presque gratuitement** — inutile de refaire le détour
> graphique du proto GRUB.
