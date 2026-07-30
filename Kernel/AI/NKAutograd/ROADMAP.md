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

## Jalon 2 — entraînement réel ✅
- ✅ Accumulation des gradients (`AccumGrad` : `grad += contrib`) + `ZeroGrad()`
  (remet le graphe atteignable à zéro).
- ✅ **Preuve end-to-end** : MLP 2-8-1 (tanh + sigmoid + MSE) entraîné sur **XOR** par
  **SGD manuel** construit avec le seul autograd → perte 0.253 → **4.4e-5**, **4/4
  prédictions correctes**. (Le jalon Phase 2 « XOR qui converge » est atteint ; NKNN/
  NKOptim vont formaliser couches & optimiseurs.)
- ✅ **Mode sans gradient** (2026-07-26) — `IsGradEnabled()`/`SetGradEnabled(bool)` +
  garde RAII `NkNoGradGuard` (façon `torch.no_grad()`), flag global thread-unsafe
  documenté. `NkMakeOp` court-circuite l'enregistrement du graphe quand actif : le
  forward reste correct (valeur calculée normalement) mais le nœud résultat est une
  feuille ISOLÉE (`requiresGrad=false`, aucun parent retenu) — les nœuds intermédiaires
  ne sont donc plus maintenus vivants par la chaîne de parents et sont libérés dès que
  leur handle `NkVar` temporaire sort de portée. **Prouvé** (`NKAutogradTest`,
  `NkVarNode::LiveCount()` avant/après, mesure réelle) : chaîne de 40 opérations →
  **+81 nœuds vivants retenus** en mode normal vs **+2 seulement** en mode `no_grad`
  (le compteur revient exactement à la ligne de base dans les deux cas une fois le
  résultat relâché : 11→11). Backward() hors de ce mode continue de fonctionner
  identiquement (30 GradCheck + XOR toujours 100% verts, mode activé par défaut à
  `true` = comportement historique inchangé).
- ✅ **Détachement (stop-gradient)** (2026-07-26) — `NkVar::Detach() const` renvoie une
  NOUVELLE feuille (même valeur, storage partagé) sans parent (`requiresGrad=false`) :
  le gradient ne remonte jamais au-delà de ce point. **Prouvé** (`NKAutogradTest`) :
  graphe `y=a*b` → `yd=y.Detach()` → `z=yd*c` → `Backward()` : `dL/dc = 6.0` (exact,
  correct EN AVAL du detach) tandis que `dL/da` et `dL/db` restent **absents**
  (`IsValid()==false` : jamais visités par le tri topologique, EN AMONT du detach).

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
