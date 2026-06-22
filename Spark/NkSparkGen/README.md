# NkSparkGen

Génération de code : IR → instructions machine de la cible (par back-end ISA).

## Rôle

- **Sélection d'instructions** : motifs IR → instructions de l'ISA.
- **Allocation de registres** : *linear-scan* (option de démarrage : machine à pile sans
  regalloc, puis montée en gamme).
- **ABI maison** par ISA : registres args/retour, callee/caller-saved, pile, IRQ.
- **Encodage** : instructions → octets, conformes à la spec ISA.
- Produit des sections objet (`.text`, `.rodata`, …) + relocations pour NkSparkLink.

## Back-ends (ordre d'arrivée)

1. **RISC-V RV32I** (cible pilote — encodage régulier, idéal pour démarrer).
2. **ARM Cortex-M (Thumb-2)** (STM32 & co).
3. **AVR** (Arduino / ATmega).

Chaque back-end documente son ABI et sa table d'encodage dans un sous-dossier dédié.

## Dépendances

- NkSparkCore, NkSparkIR.
