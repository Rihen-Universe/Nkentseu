# NkSparkSema — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Portées & symboles | ❌ | Résolution de noms |
| Typage | ❌ | Vérif/inférence, largeurs entières, casts |
| MMIO / volatile | ❌ | Sémantique d'accès registre |
| Handlers IRQ | ❌ | Signatures contraintes |
| Anti-alloc dynamique | ❌ | Refus d'allocation implicite |
| AST typé (sortie) | ❌ | Annotation pour le lowering IR |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ Résolution + typage minimal pour M0.
- ❌ Sémantique volatile/MMIO (correcte dès le début, sinon bugs runtime).
- ❌ Contraintes IRQ.

## Bugs
- (aucun)

## Dépendances
- NkSparkCore, NkSparkParse.
