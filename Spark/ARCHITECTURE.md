# NkSpark — Architecture

> Document d'architecture du système NkSpark (langage + compilateur embarqué).
> Statut : spécification de scaffold (pas d'implémentation).

## 1. Objectif & contraintes

- Produire du **code machine natif** pour microcontrôleurs **sans C/C++/LLVM** sur le
  chemin de génération.
- **Zéro dépendance runtime** sur la cible (pas de libc imposée).
- Outil compilateur écrit en **C++ zéro-STL** (NKContainers / NKMemory), buildé par **Jenga**.
- Cibles visées (par ordre d'arrivée) : **RISC-V RV32I**, **ARM Cortex-M (Thumb-2)**, **AVR**.

## 2. Couches

```
NkSparkCLI            (driver sparkc : args, orchestration, cache, intégration Jenga)
      ▲
NkSparkLink           (assembleur + linker + layout mémoire + image ELF/HEX/BIN)
      ▲
NkSparkGen            (sélection d'instructions, allocation registres, encodage ISA)
      ▲
NkSparkIR             (IR typée, passes d'optimisation, lowering)
      ▲
NkSparkSema           (résolution de noms, typage, vérifs embarqué : MMIO/volatile/IT)
      ▲
NkSparkParse          (grammaire → AST)
      ▲
NkSparkLex            (source → tokens)
      ▲
NkSparkCore           (diagnostics, source manager, types fondamentaux, arènes)
```

Transverse : **NkSparkHAL** (modèle des périphériques / registres, import SVD →
bindings NkSpark), **NkSparkRT** (startup, table des vecteurs, init mémoire).

## 3. Modèle du langage (spécification cible)

- **Types** : `u8/u16/u32/u64`, `i8/i16/i32/i64`, `bool`, `f32` (optionnel selon ISA),
  tableaux taille fixe `[N]T`, structs, pointeurs `*T`, `*volatile T` (MMIO).
- **Mémoire** : statique + pile ; pas de `new`/GC. Arènes optionnelles côté outils.
- **MMIO** : accès registre typé (`Gpio.PORTA.ODR = ...`) compilé en load/store volatile.
- **Interruptions** : fonctions `@irq(<n>)` placées dans la table des vecteurs.
- **Bits** : opérateurs bit-à-bit, champs de bits, intrinsics (`set_bit`, `clear_bit`).
- **Inline asm / intrinsics** : échappatoire pour instructions registres spécifiques.
- **Modules / imports** : unité de compilation simple, namespaces légers.

## 4. IR (NkSparkIR)

- IR **typée**, style three-address / SSA-light, indépendante de l'ISA.
- Sépare le front-end des back-ends → ajouter une ISA = un nouveau back-end Gen.
- Passes minimales au départ : constant folding, DCE, simplification de flot.

## 5. ABI maison (par ISA)

- Définir : registres d'arguments / de retour, registres callee/caller-saved,
  alignement de pile, convention d'appel des IRQ (sauvegarde de contexte).
- Documenté par back-end dans `NkSparkGen` (un sous-doc par ISA).

## 6. Back-end (NkSparkGen)

- **Sélection d'instructions** : motifs IR → instructions ISA.
- **Allocation de registres** : linear-scan (démarrage possible en machine à pile sans
  regalloc, puis montée en gamme).
- **Encodage** : IR/instr → octets, validés contre la spec ISA + un désassembleur de
  référence (objdump/émulateur) en test.

## 7. Édition de liens & image (NkSparkLink)

- **Sections** : `.text`, `.rodata`, `.data`, `.bss` + relocations.
- **Layout mémoire** : équivalent linker script — placement flash/RAM selon la *memory
  map* de la puce (paramétrable par cible).
- **Formats de sortie** : **Intel HEX** (le plus simple, en premier), **BIN** brut,
  **ELF** (pour debug/objdump/gdb).

## 8. Runtime (NkSparkRT)

- **Reset / crt0** : init du pointeur de pile, copie `.data` flash→RAM, mise à zéro
  `.bss`, appel de `main`, boucle de garde.
- **Table des vecteurs** : reset + IRQ (générée depuis les `@irq`).
- Écrit en NkSpark + un minimum d'asm/octets pour l'amorçage.

## 9. Accès matériel (NkSparkHAL)

- Définitions des registres périphériques par puce. Import **CMSIS-SVD** (XML) →
  génération de bindings NkSpark typés (GPIO/UART/TIMER/…).
- Cartes mémoire et fréquences d'horloge paramétrées par cible.

## 10. Outillage & test

- **Émulateurs** : QEMU (riscv32 / cortex-m), Renode, simavr (AVR).
- **Flash réel** : openocd / st-flash / avrdude / dfu-util (protocole externe au
  compilateur ; un flasher SWD/JTAG maison est une étape ultérieure).
- **Tests** : golden-tests d'encodage (comparaison désassemblage), tests d'exécution
  en émulateur (blink, UART echo, timer).

## 11. Jalons

1. **M0 — Blink** : `u32` + boucles + appels + MMIO → toggle GPIO en QEMU (RISC-V).
2. **M1 — UART** : sortie série (printf maison).
3. **M2 — Timers + IRQ** : interruptions, ordonnancement coopératif.
4. **M3 — 2ᵉ ISA** : ARM Cortex-M via la même IR.
5. **M4 — HAL SVD** : génération de bindings depuis SVD.
6. **M5 — Optimisations** : regalloc linear-scan, passes IR.
