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
  Vérifié (audit 2026-07-26) : aucune trace d'un tel flag/mode dans
  `NKAutograd/src/NKAutograd/NkVar.{h,cpp}`. Reste légitimement ⬜, `Kernel/AI/ROADMAP.md`
  ne prétend pas non plus que ce point est livré.
- ⬜ Détachement (stop-gradient). Vérifié (audit 2026-07-26) : pas de méthode `Detach`/
  `StopGrad` trouvée. Reste légitimement ⬜, aucun désaccord avec la roadmap globale.

## Plus tard
- ⬜ Dérivées d'ordre supérieur. Vérifié (audit 2026-07-26) : rien trouvé, reste ⬜.
- ⬜ Checkpointing du graphe (économie mémoire). Vérifié (audit 2026-07-26) : rien
  trouvé, reste ⬜.
- ✅ **Compatibilité GPU (gradients en compute)** — corrigé (audit 2026-07-26), c'était le
  plus gros désaccord trouvé dans ce module : `NKAutograd/src/NKAutograd/NkVar.cpp`
  calcule bel et bien les **backwards** directement sur GPU quand les opérandes y résident,
  pas seulement les forwards — ex. (lignes indicatives) `AccumGrad(n->a,
  NkGpuConv3DBackwardX(g, ...))`/`NkGpuConv3DBackwardW(...)` pour Conv3D, et des branches
  `if (x.Value().Device() == NkDevice::NK_GPU)` guardant les kernels GPU natifs de
  LayerNorm (`NkGpuLayerNormStd`), SoftmaxCausal (`NkGpuSoftmaxCausal`), GELU
  (`NkGpuGelu`), Embedding (`NkGpuEmbedding`), MaxPool2D (fwd+bwd), Conv2D (im2col/col2im
  GPU). Confirmé par `Kernel/AI/ROADMAP.md` : « Autograd conv GPU-résident bout-en-bout
  (2026-07-05) » et « RÉSIDENCE GPU 100% COMPLÈTE (2026-07-05) : plus AUCUN repli CPU »,
  avec non-régression `NKAutogradTest` 20/20 (les branches GPU sont gardées par
  `if device==GPU`, le chemin CPU reste inchangé). La roadmap globale avait raison ; ce
  sous-ROADMAP était en retard sur ce point précis.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
