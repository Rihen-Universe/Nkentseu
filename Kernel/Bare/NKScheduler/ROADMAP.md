# NKScheduler — Roadmap

> Le partage du temps CPU (Phase 5). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — tâches coopératives (Phase 5)
- ⬜ Représenter une tâche (pile + contexte + état).
- ⬜ Changement de contexte (via NKArch).
- ⬜ `Yield` explicite ; file d'exécution simple (round-robin).
- 🎯 Deux tâches alternent et logguent chacune leur tour.

## Jalon 2 — préemption
- ⬜ Reprise sur **tick** (NKTimerHW + NKInterrupt).
- ⬜ Quantum de temps ; sauvegarde/restauration depuis le contexte d'IRQ.
- ⬜ `Sleep` qui ne brûle pas le CPU (réveil sur deadline).

## Jalon 3 — synchronisation
- ⬜ Primitives d'attente/réveil (sémaphore, mutex, file de messages).
- ⬜ Priorités de tâches.
- ⬜ Tâche idle (mise en veille CPU quand rien à faire).

## Plus tard
- ⬜ Multi-cœur (SMP) avec NKArch.
- ⬜ Affinité, équilibrage de charge.
- ⬜ Isolation processus utilisateur (si on va jusque-là).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
