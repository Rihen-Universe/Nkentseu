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
  `AddScalar` (VAE). ⬜ Conv3D (voxels).
- ⬜ **Normalisation** (batch/layer norm).
- ⬜ Conteneur de modèle + accès paramètres + sérialisation des poids.

## Jalon 3 — attention
- ⬜ Embeddings.
- ⬜ Bloc **attention / transformer** (la base des LLM).
- ⬜ Dropout, activations avancées (GELU).

## Plus tard
- ⬜ Architectures prêtes à l'emploi (MLP, CNN, petit transformer).
- ⬜ Couches récurrentes (si utile).
- ⬜ Optimisations GPU des couches lourdes.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
