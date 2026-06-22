# NkSparkRT — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Reset / crt0 | ❌ | SP, copie `.data`, zéro `.bss`, appel `main` |
| Table des vecteurs | ❌ | Reset + IRQ (depuis `@irq`) |
| Intrinsics bas niveau | ❌ | `wfi`/`nop`, barrières |
| Profil RV32I | ❌ | Amorçage cible pilote |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ crt0 RV32I minimal pour M0 (suffisant pour atteindre `main` et blinker).
- ❌ Table des vecteurs (reset d'abord, IRQ ensuite pour M2).
- ❌ Intrinsics nécessaires (idle/barrières).

## Bugs
- (aucun)

## Dépendances
- NkSparkHAL, NkSparkLink, NkSparkGen.
