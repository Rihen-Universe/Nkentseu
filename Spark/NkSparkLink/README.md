# NkSparkLink

Assemblage final : sections objet → **image flashable**.

## Rôle

- **Assembleur** : finalise l'encodage des instructions (résolution des immédiats,
  étiquettes locales) si NkSparkGen émet une forme symbolique.
- **Éditeur de liens** : fusionne les sections (`.text`, `.rodata`, `.data`, `.bss`),
  résout les **relocations** et les symboles inter-fonctions.
- **Layout mémoire** : équivalent d'un *linker script* — place chaque section aux
  adresses flash/RAM selon la **memory map** de la puce (paramétrée par cible).
- **Formats de sortie** :
  - **Intel HEX** (prioritaire — trivial à émettre, accepté par la plupart des flashers) ;
  - **BIN** brut ;
  - **ELF** (debug / objdump / gdb).

## Pourquoi maison

Pour ne dépendre d'aucun autre langage/outil sur le chemin de génération. (binutils
`as`/`ld` restent une option transitoire si la pureté n'est pas requise au début.)

## Dépendances

- NkSparkCore, NkSparkGen (sections + relocations), NkSparkHAL/NkSparkRT (adresses, vecteurs).
