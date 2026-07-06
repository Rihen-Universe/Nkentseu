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
- ✅ **Validé multi-backend sur GPU réel (NVIDIA RTX 3070, headless)** — apps
  `NkComputeNkSL` (VecAdd) **et** `NkTensorGpuTest` (add N=100 + **matmul** + roundtrip
  + `ops::Matmul`) : **4/4 backends OK, résultat exact** sur les deux apps.
  | Backend | Compute NkSL | Note |
  |---|---|---|
  | **Vulkan** | ✅ | corrigé (pipeline layout) — **backend par défaut de NKTensor** |
  | **OpenGL** | ✅ | fix headless (fenêtre cachée) OK |
  | **DX11**   | ✅ | **20/20 runs OK** — flake non reproduit (était contention GPU inter-process) |
  | **DX12**   | ✅ | **corrigé** (3 bugs : queue, source dangling, UAV RAW+binding) |
- ✅ **Bug Vulkan compute corrigé** (7ᵉ) : `CreateComputePipeline` créait le pipeline
  layout **sans les descriptor set layouts** (`setLayoutCount=0`) → SSBO/UBO non
  bindés → résultat 0. Le graphique le faisait, le compute l'ignorait. Corrigé +
  `NkTensorGpu`/`NkComputeNkSL` créent le layout **avant** le pipeline et le passent.
- ✅ **DX12 compute corrigé** (3 bugs, 8ᵉ-10ᵉ) : (1) command list **DIRECT** toujours
  (une liste COMPUTE soumise sur queue DIRECT → « type must match queue » → device
  removal) ; (2) **source HLSL dangling** — `NkShaderStageDesc.hlslSource` est un
  `const char*` non possédé ; le holder `NkShaderConvertResult` était scopé dans le
  bloc `if` → détruit avant `CreateShader` → source réallouée corrompue (« not valid
  UTF-8 », identifiants tronqués) ; hissé au scope fonction (fix dans `NkTensorGpu` ET
  `NkComputeNkSL`) ; (3) **UAV STRUCTURED au lieu de RAW** — les SSBO read-write sont
  émis `RWByteAddressBuffer` (RAW) par SPIRV-Cross ; la vue UAV était créée STRUCTURED
  (stride 4) → incompatible → writes perdues → 0. UAV passée en `R32_TYPELESS +
  FLAG_RAW`. (4) `BindDescriptorSet` : en **compute**, router `NK_STORAGE_BUFFER` vers
  **UAV u\<binding\>** (`mMergedUav`) au lieu de SRV t0 (miroir du fix DX11).
- ✅ **Flake DX11 non reproduit** : **20/20 runs matmul OK** + `NkComputeNkSL` 8/8 ×
  4 backends. Le 3/5 initial coïncidait avec un autre process GPU (RenderDemo) →
  contention 1er-dispatch, pas un bug DX11 intrinsèque. Aucun code DX11 modifié depuis.
- ✅ **Override diagnostic `NK_TENSOR_API`** = `vulkan|opengl|dx11|dx12|metal` pour
  forcer un backend (validation par backend) ; sinon auto (Vulkan préféré).
- ✅ **Dispatch `ops::` selon `Device()`** : `ops::Add` et `ops::Matmul` routent
  automatiquement vers le GPU (kernels NkSL) quand un opérande est sur GPU, sinon CPU.
  « **Même API, deux backends** » — validé : `ops::Matmul(gpuA, gpuB)` → [58 64 139 154].
  (`NkGpuAdd`/`NkGpuMatmul` déplacent au besoin les opérandes sur GPU.)
- ⬜ Étendre le dispatch GPU à Sub/Mul/Div + activations + réductions (kernels NkSL).
- ⬜ Metal (même chemin NkSL→MSL, à valider sur matériel Apple).
- ⬜ Corriger le chemin **dxc SM6** (bug d'encodage source « not valid UTF-8 » ;
  contourné par le fallback fxc cs_5_1, mais le SM6 natif serait préférable).
- ✅ **Accélération GPU mesurée** (`NKGpuBenchTest`) : matmul GPU vs CPU, **résultats
  identiques** (err 0), speedup **20× (256²) → 124× (384²) → 162× (512²)** — croît avec
  la taille. Confirme le payoff : porter l'entraînement (Dense/conv) sur GPU = ~100× plus
  vite. (1er appel GPU = init device + compil kernel ~350 ms, puis 1-2 ms.)
- 🟡 **GPU dans l'entraînement — 1re étape faite** : `ops::Matmul` route **auto** les
  grandes matrices (M·N·K ≥ 8e6) vers le GPU (upload→NkSL→download, retombe CPU si indispo).
  Comme l'autograd (forward + backward) passe par `ops::Matmul`, l'entraînement de gros
  modèles accélère (VAE MNIST hidden 512 : ~1.3 s/époque vs 2.2 s CPU pour 4× moins gros ;
  Vulkan confirmé actif). ⚠️ **Amdahl** : seuls les matmuls sont sur GPU ; les ops
  élémentaires (Relu/Sigmoid/Add/Mul) restent CPU et deviennent le goulot.
- ✅ **Conv GPU (im2col + matmul)** : `autograd::Conv2D` réécrite en **im2col → ops::Matmul**
  (le matmul auto-GPU). Correct (gradient-checks 19/19, err 0 vs naïve) et **93× plus vite**
  en régime sur une grosse conv `[8,64,32,32]*[128,64,3,3]` (`NKConvBenchTest` : 13379 ms CPU
  → 144 ms GPU). Rend les CNN pratiques à entraîner. (Overhead im2col/permute CPU → sous le
  162× du matmul pur, mais transparent : tout le code conv en profite.)
- ⬜ **Plein speedup GPU restant** : (1) ops élémentaires sur GPU (kernels NkSL) ;
  (2) tenseurs **résidents GPU** à travers le graphe autograd (éviter transferts) ;
  (3) im2col/permute sur GPU aussi ; (4) Conv3D en im2col.

## Plus tard
- ⬜ Types réduits (fp16/bf16/int8) pour la quantization.
- ⬜ Opérations fusionnées (perf).
- ⬜ Multi-GPU.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
