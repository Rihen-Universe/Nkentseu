# AI — Roadmap globale

> Feuille de route du sous-système NKAI. Esprit : **from scratch, bottom-up, un résultat
> observable par phase**. On préfère un système simple qui *apprend* à un système complet qui ne
> tourne pas. Chaque tier repose sur le précédent.

Légende : ⬜ à faire · 🟡 en cours · ✅ fait.

---

## Phase 0 — Mise en place

- ✅ Intégration Jenga (`NKTensor.jenga` + registre `config/modules.jenga` + workspace),
  namespace `nkentseu::ai`.
- ✅ Voie **GPU** confirmée : **compute écrit en NkSL** (contrainte « NkSL pour tout »),
  converti vers tous les backends. **Vérifié** (`NkSLComputeCheck`) : compute NkSL →
  GLSL/GLSL-Vulkan/**SPIR-V**/HLSL-DX11/HLSL-DX12/**MSL Metal** OK sur 2 kernels.
  Bug corrigé : crash SPIR-V compute (glslang `InitializeProcess` manquant).
  (NKRHI expose aussi `NkMLContext` en GLSL, mais on privilégie NkSL.)
- ✅ Format des tenseurs décidé : stockage refcompté (`NkTensorStorage`), row-major,
  strides en éléments, dtypes f32/f64/i32/i64/u8, alignement 64 via NKMemory.

## Phase 1 — Le calcul (la pierre angulaire) — ✅

**Module : NKTensor.**
- ✅ Tenseurs n-dim sur **CPU** : création (Zeros/Ones/Full/Arange/Eye/FromData),
  indexation, vues (reshape/transpose/permute/slice), broadcasting, ops élémentaires
  + unaires.
- ✅ Produit de matrices (matmul 2D) + réductions (Sum/Mean/Max/Argmax global + axe).
- ✅ **Prouvé fonctionnel** : app `NKTensorDemo` (`jenga run`) → **34/34 OK**.
- ✅ Backend **GPU** (NKRHI compute, kernels **écrits en NkSL**) — contexte `NkTensorGpu` :
  `add`/`matmul` NkSL exécutés sur GPU, validés == CPU. Chemin NkSL→GLSL→SPIR-V→HLSL/MSL
  prouvé + **10 bugs compute moteur corrigés**.
- ✅ **Validé sur les 4 backends Desktop (GPU NVIDIA réel)** : Vulkan, OpenGL, DX11, DX12
  → `NkComputeNkSL` et `NkTensorGpuTest` **4/4 OK** (matmul inclus). Override diagnostic
  `NK_TENSOR_API`. Intégration `ai::NkTensor` (ToGPU/ToCPU + dispatch `ops::`) OK.
- 🎯 ✅ **Jalon atteint : multiplier deux matrices, CPU ✅ ET GPU ✅** (4 backends).
- ⬜ Reste (raffinement) : accélération mesurée CPU vs GPU, Metal sur matériel Apple.

## Phase 2 — L'apprentissage — 🟡 en cours

**Modules : NKAutograd, NKNN, NKOptim.**
- ✅ **NKAutograd** : graphe define-by-run + rétropropagation mode inverse (sans STL,
  dispatch sur tag). Dérivées Add/Sub/Mul/Matmul/Relu/Sigmoid/Tanh/Sum/MSE +
  **SoftmaxCrossEntropy** (fusionné, stable) + unbroadcast. **Prouvé**
  (`NKAutogradTest`) : **8/8 gradients == différences finies**.
- ✅ **NKNN** : `nn::NkDense` (params persistants + Xavier), activations `Relu/Sigmoid/
  Tanh`, pertes `MSELoss` **et `CrossEntropyLoss`** (+ `OneHot`).
- ✅ **NKOptim** : `optim::NkSGD` (+ momentum) **et `optim::NkAdam`** (correction de
  biais ; `ops::Sqrt` ajouté à NKTensor).
- 🎯 ✅ **Jalon Phase 2 atteint et dépassé** (`NKNNTest`) : **XOR** (Dense+SGD → perte
  1.8e-5, 4/4) **et classification 3-classes** (Dense+relu+CrossEntropy+**Adam** →
  **100%**). Pile Phase 2 complète : **NKTensor → NKAutograd → NKNN + NKOptim**. Prête
  pour la Phase 3 (entropie croisée + Adam = tout pour MNIST).

## Phase 3 — Données, entraînement, inférence — 🟡 en cours

**Modules : NKData, NKTrain, NKInfer.**
- ✅ **NKData** : `NkDataset` + `NkDataLoader` (shuffle Fisher-Yates, lots, one-hot) +
  `MakeBlobs` + **chargeur MNIST IDX** (`LoadMnist`). Prouvé (`NKDataTest` 7/7).
- ✅ **NKTrain** : `TrainEpoch` (forward→CE→backward→step, métriques perte+exactitude)
  + `Accuracy`. Prouvé (`NKTrainTest`) : Dense+relu+Dense, Adam+CE → **train/test 100%**.
- ✅ **NKInfer** : `SaveParams`/`LoadParams` (format NKMD) + `Predict`. Prouvé
  (`NKInferTest`) : round-trip **exact** (A entraîné → sauve → B neuf recharge → 100%).
- ⬜ Checkpoints d'optimiseur (reprise) + boucle de validation (NKTrain Jalon 2).
- 🎯 ✅ **Pipeline complet données→train→save→load→infer** validé de bout en bout.
- ✅ **Entraînement sur le VRAI MNIST** (`Datasets/mnist/` téléchargé, `NK_MNIST_DIR`) :
  MLP 784→64→10 (Adam+CE) sur les **60 000 vraies images** → **96.3%** en 3 époques
  (`NKTrainTest`). Le framework from-scratch apprend sur données réelles. (113 s CPU debug
  → confirme le besoin d'accélération GPU pour scaler.)
- ⬜ Checkpoints d'optimiseur (reprise) + boucle de validation (NKTrain Jalon 2) ;
  chargeurs d'autres datasets (Fashion-MNIST = même format IDX ; CIFAR ; 3D via OFF/OBJ).

## Phase 4 — La décision — 🟡 en cours

**Modules : NKRL, NKAgent.**
- ✅ **NKRL** : interface d'environnement + monde-grille + **Q-learning tabulaire**
  (ε-greedy, TD). Prouvé (`NKRLTest`) : l'agent passe de 7.9% à **100%** de réussite,
  politique optimale apprise. ⬜ DQN (Jalon 2, réutilisera NKNN/NKOptim).
- ⬜ NKAgent : agent avec mémoire (passé), perception (présent), politique de décision.
- 🎯 ✅ **Jalon « ça vit » atteint** : un agent apprend tout seul à résoudre un
  environnement simple.

## Phase 5 — La vie et l'émergence

**Modules : NKEvolve, NKCivilization.**
- ⬜ NKEvolve : génomes, sélection, mutation, croisement → population qui évolue.
- ⬜ NKCivilization : monde + temps + plusieurs agents qui interagissent (sur NKECS).
- ⬜ Outils d'observation : enregistrer, rejouer, analyser les trajectoires.
- 🎯 **Jalon « ça émerge » : des comportements de société non scriptés apparaissent.**

## Phase 6 — Génération & incarnation

**Modules : NKGen, NKEmbodied.**
- ⬜ NKGen : un petit modèle génératif (image 2D → texture) ; brancher la génération
  d'assets au moteur. Puis **formes 3D** générées par **catégories** :
  🌿 végétal, 🐾 animal/créature, 🧍 humanoïde, 🌍 monde/terrain (+ rig + animation).
- ⬜ NKEmbodied : relier une politique à un corps simulé (puis réel via Kernel/Bare).
- ⬜ **Acteurs génératifs** : un personnage reçoit un **rôle** et le joue (apparence +
  animation + comportement) → animation 3D & jeux **pilotés par IA** (NKGen + NKAgent).
- 🎯 **Jalon : générer un asset (2D puis 3D) dans le moteur ; piloter un corps par une IA.**

## Phase 7 — Montée en échelle (plus tard)

- ⬜ **LLM** dans NKInfer : architecture transformer, inférence, fine-tune de petits modèles, quantization.
- ⬜ Grande civilisation : plus d'agents, mémoire/réflexion/planification riches (style *generative agents*), prospective.
- ⬜ Robotique réelle / objets intelligents sur **Kernel/Bare**.
- ⬜ Modèles génératifs 3D / animation.

---

[Architecture](ARCHITECTURE.md) · [Modules](README.md)
