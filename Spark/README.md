# NkSpark — langage & compilateur maison pour l'électronique

> **Nom provisoire** (`NkSpark`, extension `.spark`). À renommer librement si tu veux
> une identité dédiée (comme « Mú »).
> Statut global : **scaffold de documentation** — aucun code encore. Voir [ROADMAP.md](ROADMAP.md).

## Vision

**NkSpark** est un **langage de programmation** et son **compilateur maison** dédiés à
la programmation de **microcontrôleurs / circuits imprimés**. Objectif : écrire des
projets embarqués puis les **compiler en code machine** pour la cible, **sans passer
par C, C++, ni aucun autre langage** sur le chemin du code généré.

- Le **code cible** (ce qui tourne sur la puce) est 100 % NkSpark → binaire. Pas de
  runtime C, pas de libc imposée.
- Le **compilateur lui-même** (`sparkc`) est un outil de l'écosystème Nkentseu (C++
  zéro-STL, NKContainers / NKMemory, build via Jenga) — c'est l'outil, pas la cible.

## Principes

- **Bare-metal d'abord** : pas de GC, pas d'allocation dynamique imposée, types de
  taille fixe, accès mémoire-mappé (MMIO) explicite, interruptions de première classe.
- **Déterministe & prévisible** : ce qui compte en embarqué (timing, taille, RAM).
- **Multi-cible par une IR** : un front-end unique, plusieurs back-ends ISA
  (RISC-V RV32I d'abord, puis ARM Cortex-M, puis AVR).
- **Itération en émulateur** : QEMU / Renode / simavr avant le vrai matériel.
- **Premier jalon** : faire **clignoter une LED** (toggle GPIO) en émulateur.

## Chaîne de compilation (pipeline)

```
source .spark
   │  NkSparkLex      → tokens
   │  NkSparkParse    → AST
   │  NkSparkSema     → AST typé + diagnostics
   │  NkSparkIR       → IR typée (indépendante de l'ISA)
   │  NkSparkGen      → instructions ISA (RISC-V / ARM / AVR)
   │  NkSparkLink     → encodage + layout mémoire → image (ELF / Intel HEX / BIN)
   ▼
flash sur la cible (programmateur externe : openocd / st-flash / avrdude / dfu)
```

Support transverse : **NkSparkCore** (types, diagnostics, gestion des sources),
**NkSparkHAL** (registres périphériques, import SVD), **NkSparkRT** (startup / crt0 /
table des vecteurs), **NkSparkCLI** (le driver `sparkc`).

## Modules

| Module | Rôle |
|--------|------|
| [NkSparkCore](NkSparkCore/README.md) | Types communs, diagnostics, source manager, utilitaires |
| [NkSparkLex](NkSparkLex/README.md) | Analyse lexicale (tokenizer) |
| [NkSparkParse](NkSparkParse/README.md) | Analyse syntaxique → AST |
| [NkSparkSema](NkSparkSema/README.md) | Analyse sémantique, résolution, typage |
| [NkSparkIR](NkSparkIR/README.md) | Représentation intermédiaire typée |
| [NkSparkGen](NkSparkGen/README.md) | Sélection d'instructions + codegen par ISA |
| [NkSparkLink](NkSparkLink/README.md) | Assembleur, éditeur de liens, image (ELF/HEX/BIN) |
| [NkSparkHAL](NkSparkHAL/README.md) | Accès matériel, registres périphériques (SVD) |
| [NkSparkRT](NkSparkRT/README.md) | Runtime bare-metal minimal (startup, vecteurs) |
| [NkSparkCLI](NkSparkCLI/README.md) | Driver compilateur `sparkc` + intégration build |

## Architecture détaillée

Voir [ARCHITECTURE.md](ARCHITECTURE.md) (couches, IR, ABI, formats de sortie, cibles).

## Pour démarrer (quand le code existera)

1. Choisir l'ISA pilote (recommandé : **RISC-V RV32I**).
2. Implémenter Lex → Parse → Sema → IR → Gen (RISC-V) → Link (HEX).
3. Écrire `NkSparkRT` (reset + init `.data/.bss` + appel `main`).
4. Compiler `blink.spark` → `blink.hex` → lancer dans **QEMU**.
