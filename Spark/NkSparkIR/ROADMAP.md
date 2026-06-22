# NkSparkIR — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Modèle IR | ❌ | Fonctions / blocs / instructions typées |
| Lowering AST→IR | ❌ | Abaissement depuis l'AST typé |
| Marqueurs volatile | ❌ | Préservation des accès MMIO |
| Passes de base | ❌ | Const fold, DCE, simplif. de flot |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ IR minimale (suffisante pour M0 : entiers, branchements, appels, load/store volatile).
- ❌ Lowering depuis l'AST typé.
- ❌ Passes triviales (avec garde volatile).

## Bugs
- (aucun)

## Dépendances
- NkSparkCore, NkSparkSema.
