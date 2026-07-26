# NKNN — Roadmap

> Les briques d'un réseau (Phase 2). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — perceptron (Phase 2) ✅
- ✅ Couche **dense** (`nn::NkDense`, `src/NKNN/NkNN.{h,cpp}`) : `y = x·W + b`,
  W[in,out]/b[1,out] en feuilles NkVar **persistantes** (requiresGrad), init
  Xavier-uniforme, `Forward` (matmul+biais broadcasté), `Parameters()`.
- ✅ Activations : `nn::Relu`/`Sigmoid`/`Tanh` (au-dessus de l'autograd).
- ✅ Pertes : `nn::MSELoss` **et `nn::CrossEntropyLoss`** (softmax + entropie croisée
  fusionnés, stables numériquement — gradient `(softmax−onehot)/B`). Helper `nn::OneHot`.
- 🎯 ✅ **XOR converge** (Dense+tanh+sigmoid+MSE, SGD → perte 1.8e-5, 4/4) **et
  classification 3-classes** (Dense+relu+CrossEntropy, Adam → **100%**), `NKNNTest`.

## Jalon 2 — vision & stabilité 🟡
- ✅ **Convolution 2D** (`nn::NkConv2D`, `src/NKNN/NkConv.{h,cpp}` + op autograd
  `NK_CONV2D`) : poids [Cout,Cin,k,k] init He + biais broadcasté, stride/pad. Forward
  cross-corrélation, **backward dX/dW explicites** (`NkVar.cpp`).
- ✅ **Max-pooling 2D** (`nn::MaxPool2D`, op `NK_MAXPOOL2D`, argmax mémorisé) +
  **Flatten** (`nn::Flatten`, op `NK_RESHAPE`) pour la transition conv→dense.
- ✅ **Gradients Conv2D (dX+dW) et MaxPool2D vérifiés** vs différences finies
  (`NKAutogradTest` → **11/11 OK**, erreurs ~1e-3).
- ✅ **CNN complet prouvé** (`NKConvTest`) : Conv(1→4,3×3)→ReLU→MaxPool→Flatten→Dense→
  CrossEntropy + Adam → classification d'images 8×8 : perte 1.31 → **1.2e-4, 100%**.
- ✅ Conv **transposée** (`nn::` via `autograd::ConvTranspose2D`, op `NK_CONVT2D`, backward
  dX/dW vérifiés) — upsampling appris pour les décodeurs génératifs. + ops `Exp`/`MulScalar`/
  `AddScalar` (VAE).
- ✅ **Conv3D (voxels)** — vérifié dans le code (audit 2026-07-26) : `nn::NkConv3D` **et**
  `nn::NkConvTranspose3D` existent bel et bien (`NKNN/src/NKNN/NkConv.h`, poids
  `[Cout,Cin,k,k,k]`), au-dessus de `autograd::Conv3D`/`ConvTranspose3D` (GPU-résidents,
  cf. `NKTensor/ROADMAP.md`). Correspond à « Résidence GPU 100% COMPLÈTE (2026-07-05) :
  … puis conv3d et convt3d voxel (fwd+dX+dW) » de `Kernel/AI/ROADMAP.md`.
- 🟡 **Normalisation** — nuancé (audit 2026-07-26) : **LayerNorm ✅** livré
  (`nn::NkLayerNorm`, `NKNN/src/NKNN/NkTransformer.h`, affine γ/β, GPU-résident via
  `autograd::LayerNorm`/`NkGpuLayerNormStd`) ; **BatchNorm reste ⬜** (aucune occurrence de
  `BatchNorm` dans `Kernel/AI/`).
- ✅ **Conteneur de modèle + accès paramètres + sérialisation des poids** (2026-07-26) :
  - **Accès paramètres ✅** : chaque couche (`NkDense`, `NkConv2D/3D`, `NkConvTranspose2D/3D`,
    `NkLayerNorm`, `NkMultiHeadAttention`, `NkTransformerBlock`, `NkGPT`, `NkGRUCell`,
    `NkLSTMCell`) expose `Parameters(NkVector<NkVar>&)`.
  - **Sérialisation des poids ✅**, mais implémentée HORS de NKNN : `NKInfer::SaveParams/
    LoadParams` (format « NKMD »), `NKTrain/NkCheckpoint.h` (format « NKTC » v1, modèle+Adam
    génériques, 2026-07-26), `NKGpt` (format « NKGP » v4). Ces briques consomment
    `Parameters()` de NKNN mais vivent dans NKInfer/NKTrain/NKGpt.
  - **Conteneur de modèle générique livré** : `nn::NkSequential` (`NKNN/src/NKNN/NkSequential.h`)
    chaîne une LISTE hétérogène de couches (Dense/Conv2D/LayerNorm/Dropout/fonctions pures
    comme les activations ou MaxPool2D/Flatten) en un seul `Forward(x)`. Zéro STL : polymorphisme
    C++ standard (`NkISeqLayer`, interface virtuelle + destructeur virtuel — même idiome que
    `NkTrainCallback`/`NkSystem` ailleurs dans le moteur) + `NkVector<NkISeqLayer*>` (NKContainers),
    allocation via `memory::NkGetDefaultAllocator()` (New/PushBack, Delete/Clear — idiome de
    `NkMaterialSystem`). Les briques fonctionnelles (`AddFn`) enveloppent une closure dans un
    type concret déduit par template (`NkSeqFn<TFn>`), pas d'érasure de type façon
    `std::function`. `Parameters()`/`SetTraining(bool)` propagés à toute la chaîne.

## Jalon 3 — attention ✅
- ✅ **Embeddings** — vérifié dans le code (audit 2026-07-26) : `autograd::Embedding(table,
  indices)` (`NKAutograd/NkVar.h`, op `NK_EMBEDDING`, backward scatter-add) utilisé par
  `nn::NkGPT` pour l'embedding token (`mTokEmb`) et positionnel appris (`mPosEmb`).
  Correspond à l'item « 4. Embedding (2026-07-06) » de `Kernel/AI/ROADMAP.md`.
- ✅ **Bloc attention / transformer (la base des LLM)** — vérifié dans le code (audit
  2026-07-26) : `nn::NkMultiHeadAttention` (proj Q/K/V/O + masque causal + KV-cache
  `ForwardStep`), `nn::NkTransformerBlock` (pré-LN → attention → résiduel → pré-LN → MLP
  GELU → résiduel), `nn::NkGPT` (embeddings + N blocs + LN finale + tête LM), tous dans
  `NKNN/src/NKNN/NkTransformer.h`. Correspond aux items « 6. Attention multi-têtes causale »
  et « 7. Bloc transformer + modèle NkGPT (2026-07-06) » de `Kernel/AI/ROADMAP.md`, avec le
  petit GPT entraîné réel (perte 5,04→2,21 sur *Le Comte de Monte-Cristo*) comme preuve.
- ✅ **Dropout, activations avancées (GELU)** (2026-07-26) : **GELU ✅**
  (`autograd::Gelu`, op `NK_GELU`, utilisé dans le MLP de `NkTransformerBlock`) ; **Dropout
  livré** : `nn::NkDropout` (`NKNN/src/NKNN/NkDropout.h`) — vraie couche NN, dropout INVERSÉ
  standard (masque Bernoulli(p) par élément en mode entraînement, mise à l'échelle 1/(1-p),
  NO-OP strict en mode évaluation), construite sur les primitives autograd EXISTANTES
  (`Mul`/`MulScalar`, gradient bloqué où le masque vaut 0 — aucune nouvelle primitive
  `NkAutoOp` nécessaire). Distincte de `NKData::AugmentTokenDropout` (augmentation de données
  tokenizer, conservée telle quelle). **Prouvé** (`NKNNTest`) : sur un tenseur de 200 000
  éléments, taux de zéros mesuré = 29.9% (p=0.3 attendu), moyenne après échelle = 1.001
  (≈1.0, confirme l'absence de biais) ; en mode évaluation, 0/200 000 éléments droppés.

## Plus tard
- ✅ **Architectures prêtes à l'emploi (MLP, CNN, petit transformer)** (2026-07-26) :
  **petit transformer ✅** — `nn::NkGPT` EST une architecture prête à l'emploi (vocab/d/
  têtes/couches en paramètres de constructeur), preuve réelle à l'appui (GPT trilingue
  FR/EN/bbj entraîné, paliers 1-4). **MLP et CNN livrés** (`NKNN/src/NKNN/NkArchitectures.h`,
  au-dessus de `nn::NkSequential`) :
  - `nn::NkMLP(layerSizes, dropoutP=0, seed)` : chaîne Dense→ReLU→[Dropout] entre couches
    cachées, dernière couche = Dense nu (logits bruts). **Prouvé** (`NKNNTest`) : `NkMLP`
    à 3 couches Dense (6 paramètres) entraîné par Adam+CrossEntropy sur la classification
    3-classes déjà utilisée par le Jalon 1 → **36/36 = 100% exactitude**, perte finale 5.1e-7.
  - `nn::NkCNN(convChannels, kernel, inputHW, numClasses, pool, seed)` : blocs
    Conv2D→ReLU→[MaxPool2D] puis Flatten→Dense. **Prouvé** (`NKConvTest`) : sur le MÊME jeu
    d'images 8×8 que le CNN écrit à la main (qui atteint 48/48=100%), `NkCNN` (4 paramètres :
    Conv2D+Dense) atteint également **48/48 = 100% exactitude**, perte finale 1.8e-4.
- ✅ **Couches récurrentes (si utile)** — vérifié dans le code (audit 2026-07-26) :
  `nn::NkGRUCell` et `nn::NkLSTMCell` (`NKNN/src/NKNN/NkRnn.{h,cpp}`), construites sur
  l'autograd (portes via `NkDense`+Sigmoid/Tanh, donc entraînables par NKOptim), + utilitaires
  de déroulé de séquence (`GRURunSeq`/`LSTMRunSeq`/`StackTime`). Utilisées réellement par
  `NKSpeech::NkASRModel` (GRU bidirectionnel + CTC, `NKASRTest` 3/3).
- 🟡 **Optimisations GPU des couches lourdes** — nuancé (audit 2026-07-26) : très largement
  fait — `NKAutograd/src/NKAutograd/NkVar.cpp` dispatche vers des kernels GPU natifs pour
  Conv2D/Conv3D (fwd+dX+dW), MaxPool2D (fwd+bwd), LayerNorm, Softmax(Causal), GELU, Embedding,
  batched-matmul (donc l'attention) — cf. `Kernel/AI/ROADMAP.md` « RÉSIDENCE GPU 100% COMPLÈTE
  (2026-07-05) ». Reste non couvert : backend **Metal** jamais validé sur matériel Apple, et
  le chemin **dxc SM6** natif (contourné par fxc) — cf. `NKTensor/ROADMAP.md` Jalon 3.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
