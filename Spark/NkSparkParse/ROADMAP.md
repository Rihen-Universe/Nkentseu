# NkSparkParse — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Grammaire EBNF | ❌ | Spécifier la grammaire de référence |
| Nœuds AST | ❌ | Déclarations / instructions / expressions |
| Expressions (précédence) | ❌ | Precedence climbing |
| Attributs | ❌ | `@irq`, `@addr`, `@volatile` |
| Périphériques/registres | ❌ | Déclarations MMIO |
| Inline asm | ❌ | Blocs `asm { }` opaques |
| Récupération d'erreurs | ❌ | Sync sur `;` / `}` |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ Figer la grammaire EBNF (mots-clés ↔ NkSparkLex).
- ❌ AST minimal pour M0 (fonctions, `u32`, boucles, appels, MMIO).
- ❌ Attributs IRQ/addr/volatile.

## Bugs
- (aucun)

## Dépendances
- NkSparkCore, NkSparkLex.
