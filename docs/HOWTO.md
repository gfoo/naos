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
