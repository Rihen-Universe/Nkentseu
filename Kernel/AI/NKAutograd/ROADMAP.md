# NKAutograd — Roadmap

> Le moteur du gradient (Phase 2). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — graphe + backward (Phase 2) ✅
- ✅ Graphe define-by-run : chaque op (`autograd::`) fait le forward via `ops::` et
  enregistre un nœud (`NkVarNode` : tag `NkAutoOp` + parents refcomptés, alloués via
  NKMemory comme `NkTensorStorage`). **Sans STL** : pas de `std::function`, le backward
  dispatche sur le tag (cf `src/NKAutograd/NkVar.{h,cpp}`).
- ✅ Dérivées : Add, Sub, Mul, **Matmul** (dA=dC·Bᵀ, dB=Aᵀ·dC), Relu, Sigmoid
  (y−y²), Tanh (1−y²), Sum, MSE. **Unbroadcast** des gradients (somme sur les axes
  broadcastés) pour le biais `[1,H]` additionné à `[B,H]`.
- ✅ `Backward()` : tri topologique post-ordre (DFS + `seen`), amorce la perte scalaire
  à 1, remonte en ordre inverse ; accumulation via `AccumGrad`.
- 🎯 ✅ **Prouvé** (app `NKAutogradTest`, `jenga run`) : **6/6 gradients** == différences
  finies centrées (erreurs 1e-4…1e-6) pour Mul, Tanh, Sigmoid, ReLU, Matmul, MSE.

## Jalon 2 — entraînement réel 🟡
- ✅ Accumulation des gradients (`AccumGrad` : `grad += contrib`) + `ZeroGrad()`
  (remet le graphe atteignable à zéro).
- ✅ **Preuve end-to-end** : MLP 2-8-1 (tanh + sigmoid + MSE) entraîné sur **XOR** par
  **SGD manuel** construit avec le seul autograd → perte 0.253 → **4.4e-5**, **4/4
  prédictions correctes**. (Le jalon Phase 2 « XOR qui converge » est atteint ; NKNN/
  NKOptim vont formaliser couches & optimiseurs.)
- ⬜ Mode **sans gradient** (inférence) — flag global pour ne pas construire le graphe.
- ⬜ Détachement (stop-gradient).

## Plus tard
- ⬜ Dérivées d'ordre supérieur.
- ⬜ Checkpointing du graphe (économie mémoire).
- ⬜ Compatibilité GPU (gradients en compute).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
