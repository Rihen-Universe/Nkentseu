# NkSpark — ROADMAP (système)

> Vue d'ensemble. Chaque module a sa propre ROADMAP détaillée.
> Marqueurs : ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Module | Statut | Détail |
|--------|:------:|--------|
| NkSparkCore | ❌ | Diagnostics, source manager, types, arènes |
| NkSparkLex | ❌ | Tokenizer |
| NkSparkParse | ❌ | AST |
| NkSparkSema | ❌ | Typage, vérifs embarqué |
| NkSparkIR | ❌ | IR typée |
| NkSparkGen | ❌ | Back-end RISC-V (puis ARM, AVR) |
| NkSparkLink | ❌ | Assembleur + linker + image HEX/ELF |
| NkSparkHAL | ❌ | Registres périphériques / SVD |
| NkSparkRT | ❌ | Startup bare-metal |
| NkSparkCLI | ❌ | Driver `sparkc` |

État global : **scaffold documentaire**. Aucune ligne de code. Décisions de cadrage à
figer avant le code (ISA pilote, format de sortie, niveau de pureté).

## Livré

- Scaffold de documentation : README + ARCHITECTURE + ROADMAP (système) et par module.

## En cours

- (rien)

## À venir (ordre conseillé)

1. ❌ Figer le cadrage : **ISA = RISC-V RV32I**, sortie **Intel HEX**, émulateur **QEMU**.
2. ❌ NkSparkCore (diagnostics + source manager) — socle de tout le reste.
3. ❌ NkSparkLex → NkSparkParse → NkSparkSema (front-end ; réutiliser l'expérience NkSL).
4. ❌ NkSparkIR (IR minimale).
5. ❌ NkSparkGen (RISC-V : sélection + encodage ; regalloc plus tard).
6. ❌ NkSparkLink (sections + layout + HEX).
7. ❌ NkSparkRT (crt0 + vecteurs) + NkSparkHAL (GPIO minimal).
8. ❌ **M0 Blink** en QEMU.
9. ❌ NkSparkCLI (`sparkc`) + intégration Jenga.
10. ❌ UART, timers/IRQ, 2ᵉ ISA (ARM), import SVD, optimisations.

## Bugs

- (aucun — pas de code)

## Dépendances

- Écosystème Nkentseu : **NKContainers**, **NKMemory**, **NKCore** (l'outil compilateur).
- Build : **Jenga**.
- Émulation/test : QEMU, Renode, simavr (externes).
- Flash : openocd / st-flash / avrdude / dfu-util (externes).
