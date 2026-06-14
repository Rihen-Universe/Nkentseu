# NKAutograd — Roadmap

> Le moteur du gradient (Phase 2). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — graphe + backward (Phase 2)
- ⬜ Enregistrer les opérations (tape) pendant le forward.
- ⬜ Dérivées des ops de base (add, mul, matmul, activations).
- ⬜ `Backward()` : rétropropagation depuis un scalaire de perte.
- 🎯 Le gradient d'un petit calcul correspond au calcul analytique.

## Jalon 2 — entraînement réel
- ⬜ Accumulation + remise à zéro des gradients.
- ⬜ Mode **sans gradient** (inférence).
- ⬜ Détachement (stop-gradient).

## Plus tard
- ⬜ Dérivées d'ordre supérieur.
- ⬜ Checkpointing du graphe (économie mémoire).
- ⬜ Compatibilité GPU (gradients en compute).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
