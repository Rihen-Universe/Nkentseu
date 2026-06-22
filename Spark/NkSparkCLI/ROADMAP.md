# NkSparkCLI — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Parsing d'arguments | ❌ | `build/emit/run/flash` + options |
| Orchestration pipeline | ❌ | Lex→Parse→Sema→IR→Gen→Link |
| Intégration Jenga | ❌ | Build de `sparkc` + helpers projet |
| `emit` (debug) | ❌ | Dump tokens/AST/IR/asm |
| `run` (émulateur) | ❌ | QEMU/Renode/simavr |
| `flash` | ❌ | openocd/st-flash/avrdude/dfu |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ `sparkc build` minimal (source → HEX) pour M0.
- ❌ `sparkc run` via QEMU (boucle d'itération sans matériel).
- ❌ `emit` pour debugger chaque étage.
- ❌ `flash` réel quand le matériel arrive.

## Bugs
- (aucun)

## Dépendances
- Tous les modules NkSpark.
