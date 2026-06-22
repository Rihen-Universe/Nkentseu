# NkSparkIR

Représentation intermédiaire typée, indépendante de l'ISA.

## Rôle

- Reçoit l'AST typé (NkSparkSema) par **abaissement (lowering)**.
- IR de style **three-address / SSA-light** : fonctions → blocs de base → instructions
  (charges/stockages, arith/bitops, branchements, appels, phi optionnels).
- Découple le front-end des back-ends : **une IR, plusieurs cibles** (RISC-V, ARM, AVR).
- Conserve les types (largeurs, signé/non signé) et les marqueurs **volatile** (MMIO)
  pour ne pas casser les accès registres lors des optimisations.

## Passes (incrémentales)

- Constant folding, propagation de copies, **DCE** (dead code elimination),
  simplification de flot. Toujours **préserver les accès volatile**.

## Dépendances

- NkSparkCore, NkSparkSema.
