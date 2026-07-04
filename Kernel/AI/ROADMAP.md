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

## Phase 1 — Le calcul (la pierre angulaire) — 🟡 en cours

**Module : NKTensor.**
- ✅ Tenseurs n-dim sur **CPU** : création (Zeros/Ones/Full/Arange/Eye/FromData),
  indexation, vues (reshape/transpose/permute/slice), broadcasting, ops élémentaires
  + unaires.
- ✅ Produit de matrices (matmul 2D) + réductions (Sum/Mean/Max/Argmax global + axe).
- ✅ **Prouvé fonctionnel** : app `NKTensorDemo` (`jenga run`) → **34/34 OK**.
- ✅ Backend **GPU** (NKRHI compute, kernels **écrits en NkSL**) — contexte `NkTensorGpu` :
  `add`/`matmul` NkSL exécutés sur GPU (DX11 headless), validés == CPU (`NkTensorGpuTest`
  → 2/2 OK). Chemin NkSL→GLSL→SPIR-V→HLSL/MSL prouvé + 4 bugs compute moteur corrigés.
- 🎯 **Jalon : multiplier deux matrices, CPU puis GPU.** → CPU ✅ **et GPU ✅**.
  Reste : intégration `ai::NkTensor` (ToGPU/ToCPU + dispatch `ops::`), accélération mesurée,
  DX12/Vulkan/Metal.

## Phase 2 — L'apprentissage

**Modules : NKAutograd, NKNN, NKOptim.**
- ⬜ NKAutograd : graphe de calcul + rétropropagation (mode inverse).
- ⬜ NKNN : couche dense, activations, fonction de perte.
- ⬜ NKOptim : SGD puis Adam.
- 🎯 **Jalon : entraîner un mini-réseau (ex. XOR) qui converge.**

## Phase 3 — Données, entraînement, inférence

**Modules : NKData, NKTrain, NKInfer.**
- ⬜ NKData : chargeur de jeu de données + batchs (ex. MNIST).
- ⬜ NKTrain : boucle d'entraînement + checkpoints + métriques.
- ⬜ NKInfer : charger un modèle entraîné et l'exécuter.
- 🎯 **Jalon : entraîner puis inférer un vrai petit modèle (classer des chiffres MNIST).**

## Phase 4 — La décision

**Modules : NKRL, NKAgent.**
- ⬜ NKRL : interface d'environnement + Q-learning tabulaire, puis DQN.
- ⬜ NKAgent : agent avec mémoire (passé), perception (présent), politique de décision.
- 🎯 **Jalon « ça vit » : un agent apprend tout seul à résoudre un environnement simple.**

## Phase 5 — La vie et l'émergence

**Modules : NKEvolve, NKCivilization.**
- ⬜ NKEvolve : génomes, sélection, mutation, croisement → population qui évolue.
- ⬜ NKCivilization : monde + temps + plusieurs agents qui interagissent (sur NKECS).
- ⬜ Outils d'observation : enregistrer, rejouer, analyser les trajectoires.
- 🎯 **Jalon « ça émerge » : des comportements de société non scriptés apparaissent.**

## Phase 6 — Génération & incarnation

**Modules : NKGen, NKEmbodied.**
- ⬜ NKGen : un petit modèle génératif (image 2D) ; brancher la génération d'assets au moteur.
- ⬜ NKEmbodied : relier une politique à un corps simulé (puis réel via Kernel/Bare).
- 🎯 **Jalon : générer un asset dans le moteur ; piloter un corps par une IA.**

## Phase 7 — Montée en échelle (plus tard)

- ⬜ **LLM** dans NKInfer : architecture transformer, inférence, fine-tune de petits modèles, quantization.
- ⬜ Grande civilisation : plus d'agents, mémoire/réflexion/planification riches (style *generative agents*), prospective.
- ⬜ Robotique réelle / objets intelligents sur **Kernel/Bare**.
- ⬜ Modèles génératifs 3D / animation.

---

[Architecture](ARCHITECTURE.md) · [Modules](README.md)
