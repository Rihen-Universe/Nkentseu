# NkSparkParse

Analyse syntaxique : transforme le flux de tokens en **AST** (arbre syntaxique).

## Rôle

- Implémente la **grammaire** NkSpark (à figer en EBNF dans ce module).
- Produit l'AST : déclarations (fonctions, structs, constantes, périphériques),
  instructions (if/while/for/return, blocs), expressions (binaires, appels, accès
  champ/index, littéraux, casts), attributs (`@irq(n)`, `@addr(...)`, `@volatile`).
- Parsing descendant (recursive descent + precedence climbing pour les expressions).
- Récupération d'erreurs par points de synchronisation (`;`, `}`) pour rapporter
  plusieurs erreurs.

## Constructions spécifiques embarqué

- Déclarations de **périphériques / registres** mappés en mémoire (adresse + champs).
- Handlers d'**interruption** annotés.
- Blocs `asm { ... }` (inline asm) — parsés en nœuds opaques, validés plus tard.

## Dépendances

- NkSparkCore (arènes AST, diagnostics), NkSparkLex (tokens).
