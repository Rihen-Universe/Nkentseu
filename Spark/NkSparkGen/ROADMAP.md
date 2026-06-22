# NkSparkGen — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Back-end RISC-V RV32I | ❌ | Sélection + ABI + encodage |
| Allocation registres | ❌ | Linear-scan (ou pile au départ) |
| Encodage instructions | ❌ | IR/instr → octets (validé objdump) |
| Sections + relocations | ❌ | Sortie pour NkSparkLink |
| Back-end ARM Cortex-M | ❌ | Thumb-2 (après RISC-V) |
| Back-end AVR | ❌ | ATmega (plus tard) |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ RV32I : sous-ensemble pour M0 (arith, load/store, branchements, appels, csr basiques).
- ❌ ABI RV32I documentée.
- ❌ Encodeur + golden-tests contre `riscv64-... objdump`.
- ❌ Allocation registres linear-scan.

## Bugs
- (aucun)

## Dépendances
- NkSparkCore, NkSparkIR.
