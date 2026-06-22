# NkSparkRT

Runtime bare-metal minimal : amorçage de la puce avant `main`.

## Rôle

- **Reset / crt0** : point d'entrée au reset → initialise le **pointeur de pile**, copie
  `.data` de la flash vers la RAM, met `.bss` à zéro, puis appelle `main`.
- **Table des vecteurs** : vecteur de reset + vecteurs d'**interruptions**, générée à
  partir des handlers `@irq(n)` du programme.
- **Intrinsics bas niveau** minimaux : barrières, `wfi`/`nop`, accès registres système.
- Écrit en **NkSpark** + un strict minimum d'asm/octets pour l'amorçage (pas de C).

## Notes

- Pas de libc, pas de heap par défaut. Si un tas est voulu, c'est un allocateur explicite
  fourni par l'utilisateur (arène), jamais imposé.
- Le contenu exact dépend de l'ISA/cible (collabore avec NkSparkHAL pour la memory map).

## Dépendances

- NkSparkHAL (memory map, vecteurs), NkSparkLink (placement), NkSparkGen (ISA).
