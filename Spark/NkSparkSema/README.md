# NkSparkSema

Analyse sémantique : valide l'AST et l'annote de types.

## Rôle

- **Résolution de noms** : portées, symboles (fonctions, variables, types, périphériques).
- **Typage** : inférence/vérification, conversions explicites (casts), largeurs entières,
  compatibilité des opérations.
- **Vérifications embarqué** :
  - accès MMIO via pointeurs `*volatile` (pas d'optimisation qui supprime les load/store) ;
  - signatures de handlers `@irq` (pas d'arguments, pas de retour) ;
  - constance / placement (`@addr`) cohérents ;
  - absence d'allocation dynamique implicite.
- Émet des diagnostics précis (via NkSparkCore).

## Sortie

- AST **typé** prêt pour l'abaissement (lowering) vers NkSparkIR.

## Dépendances

- NkSparkCore, NkSparkParse (AST).
