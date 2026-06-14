# NKTimerHW — Roadmap

> La base de temps du système (Phase 2). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — temps de base (Phase 2)
- ⬜ Initialiser un timer matériel (PIT/HPET/TSC x86 ; timer générique ARM).
- ⬜ Compteur **monotone** (temps écoulé depuis le boot).
- ⬜ Attente active calibrée (`Sleep` court pour les pilotes).

## Jalon 2 — tick périodique
- ⬜ Tick périodique via IRQ (NKInterrupt).
- ⬜ Compteur de ticks + conversion en millisecondes/microsecondes.
- 🎯 Une horloge avance ; on peut mesurer un `dt`.

## Jalon 3 — intégration moteur
- ⬜ Alimenter le **backend Bare de NKTime** (le moteur lit ce temps).
- ⬜ Fournir le tick de préemption à [NKScheduler](../NKScheduler/README.md).

## Plus tard
- ⬜ Timers « one-shot » programmables (deadlines).
- ⬜ Timers à haute précision pour l'audio.
- ⬜ Étalonnage sur la vraie carte.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
