# NkSparkCore — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| SourceManager | ❌ | Chargement fichiers, offset→ligne/colonne |
| Diagnostics | ❌ | Erreurs/avertissements + extrait source |
| SourceSpan / SourceLoc | ❌ | Positions et plages |
| String interning | ❌ | Table d'identifiants |
| Arènes (AST/IR) | ❌ | Allocation par bloc sur NKMemory |
| Result / asserts | ❌ | Gestion d'erreurs utilitaire |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ SourceManager + mapping de positions (base de tous les diagnostics).
- ❌ Système de diagnostics avec rendu lisible (extrait + caret).
- ❌ Arènes mémoire pour AST/IR.
- ❌ String interning pour identifiants/mots-clés.

## Bugs
- (aucun)

## Dépendances
- NKCore, NKMemory, NKContainers.
