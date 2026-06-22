# NkSparkCLI

Driver du compilateur : l'outil `sparkc` qui orchestre toute la chaîne.

## Rôle

- Point d'entrée en ligne de commande : `sparkc build projet.spark --target rv32i --emit hex`.
- Orchestration du pipeline : Lex → Parse → Sema → IR → Gen → Link.
- Options : cible ISA, profil matériel (NkSparkHAL), format de sortie (HEX/BIN/ELF),
  niveau d'optimisation, chemins d'include, sortie de diagnostics.
- **Intégration build** : descripteur **Jenga** pour builder `sparkc` lui-même, et
  helpers pour compiler/flasher/émuler un projet `.spark`.
- Sous-commandes prévues : `build`, `emit` (dump tokens/AST/IR/asm), `run` (émulateur
  QEMU/Renode/simavr), `flash` (programmateur externe).

## Notes

- `sparkc` est l'**outil** (C++ / Nkentseu) ; il ne s'exécute pas sur la cible.

## Dépendances

- Tous les modules NkSpark (orchestrateur).
