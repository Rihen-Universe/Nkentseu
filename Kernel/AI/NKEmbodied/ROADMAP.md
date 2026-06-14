# NKEmbodied — Roadmap

> Donner un corps à l'intelligence (Phase 6). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — corps simulé (Phase 6)
- ⬜ Abstraction capteurs / actionneurs.
- ⬜ Boucle perception → décision (NKAgent) → action, dans un corps **simulé** (moteur).
- 🎯 Une IA pilote un corps simulé vers un but (ex. atteindre une cible).

## Jalon 2 — contrôle robuste
- ⬜ Boucle de contrôle à fréquence fixe (temps réel souple).
- ⬜ Gestion du bruit des capteurs / des limites des actionneurs.
- ⬜ Sécurité (arrêt, limites).

## Jalon 3 — réel (via Kernel/Bare)
- ⬜ Brancher capteurs/actionneurs réels via [Kernel/Bare](../../Bare/README.md) (NKDriver, NKInput).
- ⬜ Transfert **sim → réel** (calibration, écart de réalité).
- ⬜ Inférence embarquée (NKInfer sur l'appareil).

## Plus tard
- ⬜ Plusieurs robots coordonnés.
- ⬜ Apprentissage en ligne sur le robot.
- ⬜ Objets intelligents variés (au-delà du robot mobile).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
