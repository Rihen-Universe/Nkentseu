# NkSparkCore

Socle commun du compilateur NkSpark. Ne dépend d'aucun autre module NkSpark.

## Rôle

- **Diagnostics** : erreurs/avertissements avec position source (fichier, ligne, colonne),
  niveaux, affichage avec extrait de code et soulignement (style compilateur moderne).
- **Source manager** : chargement des fichiers `.spark`, mapping offset → (ligne, colonne),
  unités de compilation, includes/imports.
- **Types fondamentaux** : identifiants, `SourceSpan`, `SourceLoc`, tables de chaînes
  internées (string interning).
- **Mémoire** : arènes / pools pour l'AST et l'IR (allocation rapide, libération en bloc),
  au-dessus de NKMemory.
- **Utilitaires** : petits conteneurs spécialisés, résultats (`Result<T>`), assertions.

## Pourquoi un module à part

Tout le reste (Lex/Parse/Sema/IR/Gen/Link) émet des diagnostics et manipule des spans.
Centraliser ici évite les cycles et garantit des messages d'erreur cohérents.

## Dépendances

- NKCore, NKMemory, NKContainers (écosystème Nkentseu).
