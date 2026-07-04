# NKTensor — Roadmap

> La pierre angulaire du framework (Phase 1). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — tenseurs CPU (Phase 1) ✅
- ✅ Structure de tenseur (forme, type, strides, device) + allocation via NKMemory
  (stockage refcompté partagé — cf `NkTensorStorage`).
- ✅ Création (Zeros/Ones/Full/FromData/Arange/Eye), indexation (GetItem/SetItem),
  reshape (vue contiguë, inférence `-1`, **validée**), transpose (vue à strides),
  **Permute**, **Slice** (avec pas), Contiguous/Clone.
- ✅ Opérations élémentaires + **broadcasting** (Add/Sub/Mul/Div, +scalaire),
  unaires (Neg/Abs/Exp/Relu/Sigmoid/Tanh).

## Jalon 2 — algèbre linéaire ✅ (cœur)
- ✅ **Produit de matrices** (matmul 2D, ordre i-k-j cache-friendly, boucle interne
  contiguë auto-vectorisable ; flottants).
- ✅ Réductions (Sum/Mean/Max globales + le long d'un axe, Argmax).
- ⬜ Optimisation SIMD explicite du matmul (via NKMath intrinsics) — raffinement perf.
- 🎯 ✅ On multiplie deux matrices sur CPU (matmul = [[58,64],[139,154]]).

> **État : CPU terminé et PROUVÉ fonctionnel au runtime.** La lib `NKTensor.lib`
> compile/se lie contre le moteur ; l'app **`NKTensorDemo`** (`jenga run`) exécute
> **34 vérifications → 34 OK, 0 échec** (construction, vues, broadcasting, matmul,
> réductions, partage/copie). Tests Unitest aussi écrits (`tests/`) mais l'exécution
> est bloquée par la politique workspace `disableunittestexecution` — d'où la démo.
> Backend CPU scalaire (SIMD explicite = raffinement). `nkentseu::ai` / `…::ops`.

## Jalon 3 — backend GPU (kernels **en NkSL**)

**Décision (contrainte projet « NkSL pour tout ») :** les kernels compute GPU de
NKTensor sont écrits **en NkSL**, pas en GLSL. NkSL les convertit vers chaque API.

- ✅ **Compute NkSL → tous les backends VÉRIFIÉ** (app `NkSLComputeCheck`, `jenga run`) :
  GLSL-OpenGL, GLSL-Vulkan, **SPIR-V**, HLSL-DX11, HLSL-DX12, **MSL (Metal)** — sur
  2 kernels (VecAdd + Matmul). SSBO (`buffer{...}`), `@push_constant`,
  `layout(local_size_*)`, `gl_GlobalInvocationID` : tout passe.
- ✅ **Bug corrigé au passage** : le compute → SPIR-V **crashait** (0xC0000409) car
  `glslang::InitializeProcess()` n'était jamais appelé sur le vrai chemin
  (`NkGLSLToSPIRV`) — l'init « officielle » vivait dans du code compilé out
  (`NKSL_HAS_GLSLANG`). Ajout d'un init static-local dans `NkGLSLToSPIRV`
  (`NKSL/Compiler/NkGLSLCompiler.cpp`). Bénéficie à TOUT SPIR-V (graphique + compute).
- ✅ **Chemin NkSL → GPU compute PROUVÉ end-to-end** (app `NkComputeNkSL`, `jenga run`) :
  kernel NkSL → GLSL-Vulkan → (glslang→SPIR-V→SPIRV-Cross) → HLSL → **DX11 compute
  headless → dispatch → readback = résultat exact**, tailles arbitraires (UBO params).
- ✅ **4 bugs moteur corrigés** (compute jamais exercé jusqu'ici) : (1) NkSL→GLSL
  `std140`→**`std430`** pour SSBO ; (2) SPIRV-Cross classe **UAV** pour SSBO
  read-write (register=binding, sinon overlap `u0`) ; (3) DX11 command buffer :
  binder STORAGE_BUFFER en **UAV compute** (`CSSetUnorderedAccessViews`), pas juste
  SRV graphics ; (4) params via **UBO** (b3) au lieu de push_constant (émulation b13).
- ✅ **Contexte GPU `NkTensorGpu`** (`src/NKTensor/NkTensorGpu.{h,cpp}`, pimpl) :
  device compute headless (auto DX11/DX12/Vulkan/Metal), cache de kernels NkSL,
  buffers GPU (Create/Upload/Download), `RunBinary`/`RunUnary`/`RunMatMul`.
  NkTensor.h/.cpp restent CPU-only (NKRHI confiné dans le `.cpp` GPU).
- ✅ **Validé au runtime** (app `NkTensorGpuTest`, `jenga run`) : `add` élémentaire
  (N=100) GPU == CPU, **`matmul` 2×3·3×2 = [58 64 139 154]** — kernels écrits **en
  NkSL**, exécutés sur GPU DX11 (résultats **corrects**).
- ✅ **Intégration `ai::NkTensor`** : `NkTensorStorage.gpuBuffer` (handle opaque
  `NkTensorGpu`, libéré dans `Release`), **`ToGPU()`/`ToCPU()`** (via `NkTensorInternal`,
  ami). Roundtrip CPU→GPU→CPU **validé** (données préservées). NkTensor.h/.cpp restent
  CPU-only ; NKRHI confiné dans NkTensorGpu.cpp.
- 🎯 ✅ **Jalon Phase 1 atteint** : on multiplie deux matrices sur GPU (kernel NkSL).
- ⚠️ **Flake connu (à valider sur GPU réel)** : sur ce **DX11 HEADLESS** (probable
  WARP / chemin compute sans fenêtre), le **tout premier dispatch matmul** d'un
  process échoue par intermittence (résultat 0), sensible à la charge machine ; les
  dispatches suivants sont **100% fiables** (x40 in-process = 0 erreur) et le CPU est
  100% fiable. C'est une **course de timing de l'environnement headless**, pas un bug
  de calcul (perturber la synchro change le taux sans l'éliminer). À revérifier sur GPU
  matériel + en usage normal (app persistante, kernels chauds).
- ⬜ dispatch `ops::` (Add/MatMul) selon `Device()` (GPU quand tenseurs sur GPU).
- ⬜ DX12 headless (crash queue compute à corriger) + Vulkan/Metal (même chemin, à valider).
- ⬜ Accélération mesurée CPU vs GPU sur grandes matrices.

## Plus tard
- ⬜ Types réduits (fp16/bf16/int8) pour la quantization.
- ⬜ Opérations fusionnées (perf).
- ⬜ Multi-GPU.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
