# NKRHI — Backend Software (rasterizer CPU) — Roadmap

> Roadmap dédiée au **rendu logiciel** de NKRHI (dossier `src/NKRHI/Software/`).
> Objectif : rendre le rasterizer CPU **fiable, net et puissant**, utilisable sur
> une machine **sans carte graphique** (esprit « rendu CPU type ZBrush »), en
> suivant les leçons de Scratchapixel (archive locale `D:\Scratchapixel\markdown`).
> Périmètre strict : **on ne touche qu'au backend Software**, rien d'autre.
> Dernière mise à jour : 2026-07-04.

État actuel : le backend est un **rasterizer scanline 3D mono-thread** piloté par
`NkSoftwareDevice` ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp), 1419 l.),
avec vertex/pixel shaders émulés en callbacks C++ (`NkFunction`). Il est
**validé bout-en-bout** (Phong + shadow map) via `NkRHIDemoFull -bsw`, mais le
chemin de rendu réellement exécuté (`swfast::*`, [NkSWFastPath.h](NkSWFastPath.h))
souffre de lacunes de correction (interpolation affine, pas de clipping, état
pipeline ignoré) et de perf (mono-thread). Un second rasterizer plus correct
(`NkSWRasterizer`, dans [NkSoftwareDevice.cpp](NkSoftwareDevice.cpp)) existe mais
**n'est jamais appelé** par le rejeu de commandes → duplication morte à résorber.

---

## ▶ VM BYTECODE NkSL (étape 2) — DÉCOUVERTE : la VM existe déjà !

**NKSL a déjà `CodeGen/Bytecode/` + `VM/` complets.** Ne PAS réécrire — brancher + étendre.
- Cible **`NkSLTarget::NK_BYTECODE`** ([NkSLTypes.h:59](../../../NKSL/src/NKSL/Core/NkSLTypes.h)) → `result.bytecode`.
- `NkSLByteCodeDeserialize(blob, size, NkSLByteProgram&)` → programme (`stage`, `code`, `constants`,
  `inputs`/`outputs` offset-float, `uniforms` byteOffset, `samplers` par binding).
- `NkSLVM::Execute(prog, NkSLVMEnv&)` — env : `inputs`(float[]), `outputs`(float[]), `uniforms`(blob),
  callbacks `sampleTex`/`sampleShadow`/`texSize`.
- **Template fonctionnel** : `NkRHIDemoFullSL.cpp:1247-1330` (opt-in `NK_SW_VM=1`) — montre vertFn/fragFn
  qui remplissent l'env et appellent la VM. Connu : plus lent (interprété par pixel) + résidus lighting.

**Plan d'intégration (à faire) :**
1. **Plomber le source NkSL** jusqu'au device : `NkShaderLibrary` (cible software) pose
   `stage.swSource = <nksl original>` (software-only, n'affecte pas GPU).
2. **Chemin VM générique** dans `NkSoftwareDevice::CreateShader` (opt-in `NK_SW_VM`) : si `swSource` →
   compiler `NK_BYTECODE` (vert+frag+**compute**) → deserialize → installer vertFn/fragFn/**computeFn**
   qui montent l'env (via mes accesseurs `SwGetUBOBytes`/`SwGetTexAt` multi-set) et exécutent la VM.
3. **⚠️ MULTI-UBO (le gros morceau)** : la VM lit UN seul blob (byteOffset du 1er UBO). Étendre
   `NkSLByteProgram.uniforms` + `NkSLVMEnv` + le codegen pour taguer chaque uniform par `(set,binding)`
   et fournir plusieurs blobs. **Software-only dans NKSL** (les backends GPU utilisent GLSL/HLSL/SPIRV,
   pas le bytecode) → aucun impact sur eux.
4. **Compute via VM** : `computeFn` exécute la VM par invocation (thread group).
5. **Passes plein écran** (post-process) marcheront alors nativement (vrai shader exécuté).
6. **Cache `.nkbc`** (NkSLByteCodeIO) : compile une fois → cache → interprète (le modèle voulu).

---

## ▶ EN COURS — NKRenderer sur software

**✅ (1) CRASH FBO CORRIGÉ (2026-07-05)** — `CreateTexture` (software) sous-allouait : format inconnu
→ `NkFormatBytesPerPixel`=0 → **0 octet** ; render target R8/RG8 < 4 o/pixel ; **layers/faces** cubemap
(IBL) & array (CSM) non comptés. Le rasterizer/clears écrivent 4 o/pixel → débordement heap → corruption
map → crash aléatoire dans `Find`. Fix **software-only** : `allocBpp = max(bpp||4, RT?4)` × `layers`.
La config **complète** (ombres + post-process) tourne désormais **sans crash**.

**⏳ Reste pour l'image correcte du pipeline complet** : les passes **post-process plein écran** (tonemap/
bloom) reçoivent mon shader taillé 3D (mistransform → blanc) et les cibles **HDR RGBA16F** reçoivent 4 o/px
du rasterizer (mal aligné). → résolu par **(2) la VM bytecode** (exécute VRAIMENT chaque shader). En config
directe (sans offscreen), la géométrie 3D rend correctement.

## ▶ (archive) option A : shader taillé

**Décidé 2026-07-05 : A (fixed-function taillé NKRenderer) puis B (VM bytecode).** Analyse complète :
le bridge générique ne suffit pas (auto-mappe `aNormal`→COLOR ; suppose 1 seule mat4 MVP). NKRenderer :

- **Format vertex** : aPos(0,vec3) aNormal(1) aTangent(2) aUV(3,vec2) aUV2(4) aColor(5,vec4).
- **Transform** : `viewProj × model × pos`.
- **4 descriptor sets** (`NkResources.cpp:192-234`, constantes `NK_BIND_*`) :
  - set 0 Frame : `NK_BIND_CAMERA_UBO` (CameraUBO : view@0, proj@64, **viewProj@128**), `NK_BIND_LIGHTS_UBO`, shadow/IBL.
  - set 1 Object : `NK_BIND_OBJECT_UBO` (**model@0**, normalMatrix@64, **tint@128**).
  - set 2 Material : `NK_BIND_PBR_PARAMS`, `NK_BIND_TEX_ALBEDO` (+normal/ORM/emissive/AO).
  - set 3 PostProcess : image.

**Étapes A (dans l'ordre) :**
1. **Multi-descriptor-set** (bloquant) — `NkSoftwareCommandBuffer::BindDescriptorSet` n'ignore plus
   l'index de set (`:214`), stocke `mBoundDescSets[4]`. Les 4 Draw passent tous les sets à
   `ExecuteDrawFast`. `ResolveResources` agrège UBOs+textures de tous les sets (par set,binding).
2. **Shader taillé** : vertFn (`viewProj(set0,CAMERA@128) × model(set1,OBJECT@0) × aPos`, lit les 6 attrs) ;
   fragFn (Lambert depuis LightsUBO set0 + albedo texture set2 × tint(set1,OBJECT@128) × couleur).
3. **Install** : `CreateShader` — si stage sans `cpuFn` mais avec source NkSL/glsl → installer le shader taillé.
4. **Test** : `renderdemo --backend=sw` → géométrie NKRenderer + éclairage diffus (fin de l'écran noir).

**Puis B** : VM bytecode NkSL pour le vrai PBR (générique, tout shader).

**✅ A VALIDÉ (2026-07-05)** : `renderdemo --backend=sw --demo=2` avec `NK_SW_MINCFG=1` (config 3D
minimale sans passes offscreen, toggle dans `main.cpp` case 2) rend **la géométrie NKRenderer éclairée**
(cube cyan Lambert + sol gris, transform `viewProj×model` correct) — **fin de l'écran noir.** Le shader
taillé (transform + 6 attributs + Lambert + albedo/tint) fonctionne. Confirmé aussi que le crash
ci-dessous est **préexistant** (config minimale = pas de crash ; seule la passe shadow/post-process crashe).

**État 2026-07-05 (détails) :**
- ✅ **Étapes 1-3 faites et compilent** : multi-descriptor-set (device mémorise sets 0/1/2 au replay,
  `SwSetCurDescSet`/`SwCurDescSets`/`SwResetCurDescSets`), accesseurs `SwGetUBOBytes(set,binding)` /
  `SwGetTexAt(set,binding)`, stride courant (`SwSetCurStride`), et **shader taillé** installé dans
  `CreateShader` (source NkSL sans `cpuFn` → vertFn `viewProj×model` + fragFn Lambert/albedo/tint,
  lectures vertex **bornées par le stride**).
- ❌ **BLOQUÉ par un bug PRÉEXISTANT** (indépendant de A) : `renderdemo --backend=sw --demo=2` crashe
  **avant** mon shader, dans `ComputeClipRect`→`GetTex(4528)`→`NkUnorderedMap::Find` (node **dangling**,
  [NkUnorderedMap.h:724](../../../../Foundation/NKContainers/src/NKContainers/Associative/NkUnorderedMap.h)).
  → map `mTextures` **corrompue** (use-after-free/double-free dans le cycle de vie des ressources
  software sous le rendu multi-passes NKRenderer — FBO shadow atlas `fbId=4552`).
- **Prochaine étape** : corriger ce crash ressource software (heap corruption) — c'est le vrai bloquant
  pour valider NKRenderer sur software. Piste : accès concurrents map depuis fragFn (threads worker
  du rasterizer tuilé appellent `GetTex`/`GetDescSet`) OU double-free FBO/texture au DestroyTexture.
  Alternative pour valider A plus tôt : scène NKRenderer **sans passe shadow** (config `cascadeCount=0`).

---

## ▶ Prochaine session (reprise)

**Objectif : activer NkSL sur le backend software** (Phase 5, item 15-16) pour corriger l'**écran
noir de NKRenderer** sur `renderdemo --backend=sw`. Constat de fin de session 2026-07-04 :
- NKRenderer est device-agnostique → il tourne déjà sur le software (mon rasterizer encaisse les draws,
  aucun crash), MAIS **écran noir** car ses shaders **NkSL** (PBR/materials) ne s'exécutent pas :
  la branche SkSL de `NkSoftwareDevice::CreateShader` (~ligne 795) est **commentée**, et le bridge
  [SL/NkSWShaderBridge.h](../SL/NkSWShaderBridge.h) n'est pas branché.
- **Étape 1 (choisie)** : activer le bridge existant (shaders standards MVP+texture+vertex color) →
  vérifier que `renderdemo --backend=sw` affiche enfin quelque chose. Puis VM bytecode pour l'arbitraire.
- Piste bonus déjà prête : le **BPR** (`NK_SW_RT`) collecte la géométrie des draws → NKRenderer + un
  appel `SetRtCamera` = preview software haute qualité sans dépendre des shaders.

---

## Synthèse

| Domaine / Tâche                                             | Statut  | Phase | Effort | Priorité |
|------------------------------------------------------------|---------|-------|--------|----------|
| Rasterizer scanline triangles (`swfast::DrawTriangleFast`) | ✅      | —     | —      | —        |
| Vertex/pixel shaders émulés (`NkFunction`)                 | ✅      | —     | —      | —        |
| Depth buffer D32_FLOAT + test/write                        | ✅      | —     | —      | —        |
| SIMD clears + spans (SSE2/AVX2/NEON, `NkSWPixel.h`)        | ✅      | —     | —      | —        |
| Présentation native multi-OS (GDI/X11/Wayland/…)          | ✅      | —     | —      | —        |
| Fast lanes : depth-only, flat-color SIMD                   | ✅      | —     | —      | —        |
| **Interpolation perspective-correcte (chemin actif)**      | ✅      | 1     | M      | P0       |
| **Unifier sur rasterizer edge-function/barycentrique**     | ✅      | 1     | L      | P0       |
| **Clipping near-plane (Sutherland–Hodgman clip-space)**    | ✅      | 1     | M      | P0       |
| Respect `depthOp` configurable (câblé LESS)                | ✅      | 1     | S      | P0       |
| Respect blend factors configurables (code existant)        | ✅      | 1     | S      | P0       |
| Backface culling par `cullMode` (forcé off)                | ✅      | 1     | S      | P1       |
| Topologies triangle strip/fan (`RasterizeList`)            | ✅      | 1     | M      | P1       |
| Topologies line/point (`RasterizeList`)                    | ⏳      | 1     | S      | P2       |
| `SetViewport` réel (no-op actuel)                          | ❌      | 1     | S      | P1       |
| `PushConstants` transmis aux shaders (jetés actuellement)  | ❌      | 1     | S      | P1       |
| `UpdateBuffer` software (no-op)                            | ❌      | 1     | S      | P2       |
| Anti-aliasing SSAA (opt-in `NK_SW_SSAA=1..4`, resolve box) | ✅      | 2     | L      | P1       |
| Anti-aliasing MSAA par couverture (moins coûteux que SSAA) | ⏳      | 2     | L      | P2       |
| Sampling bilinéaire chemin actif (respecte `magFilter`)    | ✅      | 2     | S      | P1       |
| Sélection mip/LOD (dérivées UV, quad 2×2)                  | ⏳      | 2     | M      | P1       |
| Correction sRGB ↔ linéaire (sampling + sortie)             | ⏳      | 2     | S      | P1       |
| **NkSL software** — activer le bridge existant (std shaders)| 🔶      | 5     | M      | P1       |
| **NkSL VM bytecode CPU** (shaders arbitraires)             | ⏳      | 5     | XL     | P1       |
| Cache bytecode CPU réutilisant `.nksc` (compile une fois)  | ⏳      | 5     | S      | P1       |
| Rasterization tuilée multi-thread (NKThreading, ~1,8× MT)  | ✅      | 3     | L      | P1       |
| Cache triangles pré-projetés/draw (fin du setup redondant) | ✅      | 3     | M      | P1       |
| Binning cross-draw (1 seul Join par render pass)           | ⏳      | 3     | L      | P2       |
| Boucle pixel SIMD (edge-function 4/8 px AVX2)              | ⏳      | 3     | L      | P2       |
| Tri des draws par état (opaque/alpha) hors hot-loop        | ⏳      | 3     | M      | P2       |
| Ray-tracing CPU : Möller-Trumbore + caméra + Lambert + ombre dure | ✅ | 4 | XL | P2 |
| Accélération BVH (median split, traversée stack)           | ✅      | 4     | L      | P2       |
| Ray-tracer multi-thread (par lignes, ~7–10×)               | ✅      | 4     | M      | P2       |
| Intégration device : mode BPR live (NK_SW_RT, view/proj, resolution scale) | ✅ | 4 | M | P2 |
| BPR sur la géométrie + caméra RÉELLES de l'app (collecte draws clip→monde) | ✅ | 4 | M | P2 |
| Matériaux app dans le BPR (réflexions/rugosité par mesh)   | ⏳      | 4     | M      | P3       |
| Ombres douces Monte-Carlo (disque de lumière, spirale Vogel) | ✅    | 4     | L      | P2       |
| Ambient occlusion (AO) Monte-Carlo (hémisphère cosinus)    | ✅      | 4     | M      | P3       |
| Réflexions Whitted + Fresnel-Schlick (récursif, depth 2)   | ✅      | 4     | L      | P3       |
| Réfractions (verre) + Fresnel                              | ⏳      | 4     | M      | P3       |

Légende : ✅ livré · 🔶 partiel/dormant · ⏳ à venir · ❌ bug/manquant · 🚫 hors périmètre.

---

## Approche d'implémentation (décidée 2026-07-04)

**Réécriture du CŒUR de rasterisation uniquement**, pas du backend entier.

- ✅ **Conservé** (marche + validé, orthogonal à la qualité) : `NkSoftwareDevice`
  (impl `NkIDevice`, ressources, swapchain, **présentation native multi-OS**),
  `NkSoftwareCommandBuffer`, `NkSWPixel.h` (ISA pixel + SIMD).
- ♻️ **Réécrit clean-room** : un cœur cohérent unique remplaçant `swfast::*`
  ([NkSWFastPath.h](NkSWFastPath.h)) **et** le `NkSWRasterizer` dormant
  ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp)) — fin de la duplication. Nouveau
  découpage (tout dans `Software/`, donc capté par le glob jenga, aucune modif de build) :

  ```
  Software/Raster/
    NkSWClip.h     — clipping clip-space Sutherland–Hodgman (near → frustum)
    NkSWInterp.h   — varyings perspective-correct (1/w), règle top-left
    NkSWRaster.*   — rasterizer edge-function/barycentrique (base tuilage/SIMD)
    NkSWDepth.h    — test profondeur avec depthOp configurable
    NkSWBlend.h    — blending avec blend factors configurables
    NkSWTile.*     — binning tuiles + parallélisme NKThreading (Phase 3)
  Software/Trace/  — Phase 4 BPR : rayons, Möller-Trumbore, BVH
  ```

**Contrat d'isolation (ne pas perturber les agents GPU DX/GL/VK)** :
- Aucune modif hors `src/NKRHI/Software/`.
- Aucune modif de `Core/*`, `Commands/*`, `SL/*`, ni des fichiers `.jenga`.
- Classe `NkSoftwareDevice` + headers publics (`NkSoftwareDevice.h`, `NkSWPixel.h`,
  `BackbufferPixels()`…) **préservés** → `NkDeviceFactory.cpp:105` et les démos
  Sandbox continuent de compiler sans changement.
- Fichiers software **disjoints** des dossiers `DirectX11/DirectX12/Opengl/Vulkan/Metal`
  → aucun conflit git possible. Travail sur working tree (pas de worktree nécessaire).
- Non-négociable NKMemory : `NkAlloc`/`NkFree` uniquement, jamais de heap CRT.

---

## ✅ Livré (état réel du code)

### v4 — Réécriture du cœur raster (2026-07-04)

Nouveau cœur [Raster/NkSWRasterCore.h](Raster/NkSWRasterCore.h) (namespace `swraster`),
branché dans [NkSWFastPath.h](NkSWFastPath.h) via `RasterizeList` → `swraster::DrawTriangle`.
L'ancien scanline (`DrawTriangleFast`/`SWVert`/`Edge`/`HStep`/`FillSpanFast`) est neutralisé
sous `#if 0 // [mort v4]` (retiré du build ; suppression physique à faire plus tard).

- **Rasterizer edge-function / barycentrique** (bounding-box) remplaçant le scanline affine.
- **Interpolation perspective-correcte** des varyings (1/w) — `RasterScreenTriangle`.
- **Clipping near-plane** Sutherland–Hodgman en clip-space (`ClipNear`, plan `z+w≥0`).
- **`depthOp` configurable** (`DepthPass`), **blend factors configurables** (`BlendFactor`/`OutputPixel`),
  **backface culling** honorant `cullMode`/`frontFace` (winding y-down validé), **topologies**
  triangle-list/strip/fan.
- `CreateGraphicsPipeline` ne force plus `NK_NONE` : `p.cullMode = d.rasterizer.cullMode`.
- **Validé** : build 18/18 vert (0 erreur/warning) + capture `NkRHIDemoFullImage -bsw`
  (cube Phong + sol perspective corrects, aucune régression).

**Phase 2 (partiel)** : sampling **bilinéaire** branché dans le chemin texturé
(`SampleBilinear`/`SampleNearest`, respecte `NkSamplerDesc::magFilter`).
**Anti-aliasing SSAA** livré (opt-in `NK_SW_SSAA=1..4`, défaut 1 = off) : le swapchain est
rendu à `(W*ss)×(H*ss)` ([NkSoftwareDevice.cpp](../NkSoftwareDevice.cpp) `CreateSwapchainObjects`),
puis résolu par box-downsample vers `mResolveBuf` taille fenêtre au `Present` (`ResolveFramebuffer`)
→ les 8 chemins de présentation multi-OS restent inchangés. Le scissor du swapchain est scalé par
`ss` dans `ComputeClipRect` (coords fenêtre → hi-res). Validé : SSAA=1 pixel-identique (0 régression),
SSAA=2 bords lissés. Reste : mip/LOD, sRGB, MSAA par couverture (moins coûteux que SSAA).

**Phase 3 (partiel — tuilage multi-thread)** : `RasterizeList` répartit les tuiles 64×64 sur
`NkThreadPool::GetGlobal()` (pixels disjoints par tuile → sans verrou) ; flag debug `NK_SW_NOMT=1`
force le mono-thread. **Mesure A/B `NkRHIDemoFull -bsw` (Release)** : MT ≈ 54 FPS vs mono ≈ 45 FPS
(**+20 %**) ; en Debug le MT est plus lent (overhead pool). Gain limité car : setup par-tuile
**redondant**, barrière `Join()` **par draw call**, present non-parallèle (Amdahl).
**Optimisation appliquée (2026-07-04)** : cache des triangles **pré-projetés/clippés** par draw
(`swraster::ReadyTri`/`ClipProject`, `ScreenVert` auto-suffisant, buffer thread-local réutilisé) →
fin de la re-projection redondante par tuile. **Nouvelle mesure Release** : MT ≈ **81 FPS** vs mono
≈ 44 FPS (**×1,84**, contre ×1,2 avant). Bilan global : ~13 FPS (Debug mono) → 81 FPS (Release MT) ≈ **×6**.
**Prochaine optimisation (P2)** : binning **cross-draw** (accumuler les triangles de toute la render
pass, 1 seul `Join`) → pousse encore le scaling sur scènes à nombreux draws.

**Phase 4 (fondation ray-tracing)** : nouveau module [Trace/NkSWRayTrace.h](Trace/NkSWRayTrace.h)
(namespace `swtrace`, auto-contenu, math local) — chemin de rendu alternatif « BPR » :
rayons caméra pinhole + intersection **Möller-Trumbore** (`IntersectMT`) + `ClosestHit`/`AnyHit`
brute-force + shading **Lambert + ombre dure** (shadow rays) + ambiant. Self-test déclenché par
`NK_SW_RT_TEST=1` (dans `NkSoftwareDevice::Initialize`) : ray-trace une scène sol+cube+ombre 640×360
→ `Build/Captures/rt_selftest.ppm`. **Validé visuellement** (cube ombré + ombre portée correcte).
**Ombres douces** ajoutées (`Shade`, `kShadowSamples=32`, `kLightConeTan`) : N shadow rays vers un
disque angulaire autour de la lumière, motif spirale de Vogel (golden angle) → pénombre lisse sans bruit.
**AO** Monte-Carlo (`AmbientOcclusion`, hémisphère cosinus 16 samples) + **BVH** (`BVH`, median split,
traversée à pile) ajoutés. Self-test enrichi : sol + cube + **sphère tessellée ~1550 triangles**
→ prouve que le BVH tient la charge (sinon des milliards de tests). Validé visuellement
(cube + sphère, ombres douces des deux, AO de contact). **Ray-tracer multi-thread** (par lignes via
`NkThreadPool`, toggle `NK_SW_RT_NOMT`) : mesuré ~**7–10×** (6507 → 888 ms sur la scène ~1550 tris).
**Réflexions Whitted + Fresnel-Schlick** (`TraceRay` récursif, `kMaxDepth=2`, champ `Tri::reflect`) :
sol + sphère réfléchissants, reflets validés. **État RT : Möller-Trumbore + BVH + MT + ombres douces
+ AO + réflexions.** Reste : **intégration device** (soumettre une vraie scène BPR au lieu du self-test),
réfractions (verre), ombres douces via vraies lumières surfaciques, denoise.

### Base d'origine (héritée)

- **Pipeline programmable CPU** : vertex shader `NkVertexShaderSoftware` et pixel
  shader `NkPixelShaderSoftware` en `NkFunction` ([Core/NkDescs.h](../Core/NkDescs.h):242-244),
  invoqués par vertex ([NkSWFastPath.h](NkSWFastPath.h):542) et par pixel
  ([NkSWFastPath.h](NkSWFastPath.h):397). Fallback fixed-function par stride
  ([NkSoftwareCommandBuffer.cpp](NkSoftwareCommandBuffer.cpp):242).
- **Rasterizer scanline** `swfast::DrawTriangleFast` ([NkSWFastPath.h](NkSWFastPath.h):269) :
  tri par Y, deux arêtes, pas horizontal ; échantillonnage centre-pixel `+0.5`.
- **Depth buffer** D32_FLOAT ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):582), test
  `z > depthRow[x]` + write ([NkSWFastPath.h](NkSWFastPath.h):388-390), flags
  `depthTest`/`depthWrite` respectés.
- **SIMD bas niveau** ([NkSWPixel.h](NkSWPixel.h)) : `FillSpanOpaque`, `BlendSpanAlpha`,
  `BlendSpanAdd` (SSE2/NEON) ; clears AVX2 `FastClearColor`/`FastClearDepth`
  ([NkSWFastPath.h](NkSWFastPath.h):70-86).
- **Fast lanes** : depth-only (passe d'ombre), remplissage couleur-plate SIMD
  ([NkSWFastPath.h](NkSWFastPath.h):353-381).
- **Présentation native** multi-OS + readback `BackbufferPixels()`
  ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):1152).
- **Compute CPU** en callbacks (`NkComputeShaderSoftware`), dispatch direct
  ([NkSoftwareCommandBuffer.cpp](NkSoftwareCommandBuffer.cpp):446).

---

## 🔶 En cours / dormant (code présent mais non branché)

- **`NkSWRasterizer`** ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):284-560) : rasterizer
  **correct** (cull winding, `depthOp` configurable, `BaryInterp` **perspective-correct**
  stockant 1/w à :139, bilinéaire, lignes/points Bresenham) — mais `Execute()` ne
  l'appelle jamais (dispatch 100 % `swfast`). **À réconcilier** avec `swfast` en Phase 1
  (source de vérité pour l'interpolation perspective + cull + depth-op).
- **`NkSWTexture::Sample`** ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):73-96) : bilinéaire
  + wrap, prend un niveau de mip — utilisé seulement par `BlitTexture`/`GenerateMipmaps`,
  pas par le raster. **À brancher** en Phase 2.
- **`BlendColor`/`ApplyBlendFactor`** ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):188-213) :
  blend factors complets déjà écrits, jamais utilisés par le chemin actif. **À brancher** P1.

---

## ⏳ À venir — plan par phases (base d'implémentation)

### Phase 1 — Fiabilité (fondations, zéro régression visuelle attendue)

Objectif : un rasterizer **correct** qui respecte l'état pipeline. Ordre conseillé :

1. **Interpolation perspective-correcte** dans `swfast` (fix n°0).
   - Stocker `1/w` par sommet ; interpoler `attr/w` et `1/w` linéairement en écran ;
     diviser au pixel (`attr = (attr/w) / (1/w)`).
   - Cible : `ToSWVert`/`Edge::Init`/`HStep::Init` ([NkSWFastPath.h](NkSWFastPath.h):114-216)
     qui interpolent aujourd'hui en **affine**. Réutiliser la logique de
     `NkSWRasterizer::BaryInterp` ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):139).
   - Réf. Scratchapixel : `rasterization-practical-implementation/perspective-correct-interpolation-vertex-attributes`,
     `.../visibility-problem-depth-buffer-depth-interpolation`.
2. **Unifier sur un rasterizer edge-function / barycentrique** (remplace le scanline).
   - Une seule fonction de fill barycentrique (fonctions d'arête `EdgeFunction`,
     règle top-left, sous-pixel), substrat naturel pour AA + tuilage + SIMD.
   - Supprime la duplication morte `NkSWRasterizer` vs `swfast`.
   - Réf. : `rasterization-practical-implementation/overview-rasterization-algorithm`,
     `.../rasterization-stage`, `.../projection-stage`.
3. **Clipping near-plane** (puis frustum) en clip-space avant division perspective.
   - Sutherland–Hodgman contre `w > ε` (au minimum near) ; produire de nouveaux
     sommets interpolés. Corrige l'effondrement actuel des sommets `w≈0` à l'origine
     ([NkSWFastPath.h](NkSWFastPath.h):116, `invW=0`) et le reject trivial `w<=0` (:284).
   - Réf. : `perspective-and-orthographic-projection-matrix/projection-matrix-GPU-rendering-pipeline-clipping`.
4. **Respecter l'état pipeline** (brancher l'existant) :
   - `depthOp` configurable (au lieu de LESS câblé) — logique dans
     `NkSWRasterizer::DepthTest` ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):162).
   - Blend factors configurables via `BlendColor`/`ApplyBlendFactor` (déjà écrits).
   - `cullMode` réel (aujourd'hui forcé `NK_NONE` à [NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):821) —
     réf. `ray-tracing-rendering-a-triangle/single-vs-double-sided-triangle-backface-culling`.
   - Topologies dans `RasterizeList` ([NkSWFastPath.h](NkSWFastPath.h):507) : lignes,
     points, strips, fans (aujourd'hui triangle-list uniquement).
   - `SetViewport` réel ([NkSoftwareCommandBuffer.cpp](NkSoftwareCommandBuffer.cpp):167) +
     `PushConstants` transmis ([...]:205, jetés) + `UpdateBuffer` ([...NkSoftwareCommandBuffer.h]:68).

### Phase 2 — Qualité (rendu net « ZBrush »)

5. **Anti-aliasing MSAA par couverture** : N échantillons/pixel via fonctions d'arête
   (masque de couverture), resolve moyenné. 4× ordered-grid minimum.
   Réf. sampling/AA : `monte-carlo-methods-in-practice/monte-carlo-methods`.
6. **Sampling bilinéaire** dans le chemin actif (brancher `NkSWTexture::Sample`).
   Réf. : `introduction-to-texturing/introduction-to-texturing-texture-filtering`,
   `mathematics-physics-for-computer-graphics/interpolation/bilinear-filtering`.
7. **Mip/LOD** : calcul des dérivées UV par quad 2×2, sélection + trilinéaire.
   Réf. : `interpolation/trilinear-interpolation`.
8. **sRGB ↔ linéaire** : décoder les textures sRGB en linéaire, shading en linéaire,
   ré-encoder en sortie. Réf. : `introduction-to-texturing/introduction-to-texturing-color-space`,
   `digital-imaging/colors/color-space`.

### Phase 3 — Puissance (perf réactive sans GPU)

9. **Rasterization tuilée multi-thread** : binning des triangles en tuiles 32×32/64×64,
   rasterisation parallèle via NKThreading ThreadPool (déjà référencé, appels commentés
   [NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):566). Gain visé x4–x8.
10. **Boucle pixel SIMD** : évaluer 4/8 pixels à la fois (edge-function AVX2), quad 2×2
    pour les dérivées ; étendre les primitives SIMD de `NkSWPixel.h` au shading/coverage.
11. **Tri des draws par état** (pipeline/material/blend), spécialiser opaque vs alpha,
    sortir les branches du hot-loop.

### Phase 4 — Puissance « BPR » (ray-tracing CPU)

12. **Chemin ray-tracing CPU** parallèle au rasterizer : génération de rayons caméra,
    intersection **Möller-Trumbore**, accélération **BVH**.
    Réf. : `ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection`,
    `.../barycentric-coordinates`, `ray-tracing-polygon-mesh/*`,
    `ray-tracing-generating-camera-rays/generating-camera-rays`.
13. **Ombres douces + AO** par échantillonnage Monte-Carlo (lumières surfaciques),
    réduction de variance. Réf. : `introduction-to-lighting/*` (area/spherical/rectangular),
    `monte-carlo-methods-in-practice/variance-reduction-methods`.
14. **Réflexions/réfractions Whitted + Fresnel**. Réf. :
    `introduction-to-shading/reflection-refraction-fresnel`,
    `introduction-to-ray-tracing/adding-reflection-and-refraction`.

### Phase 5 — NkSL comme système de shader software (par défaut)

Objectif : rendre **NkSL** le langage de shader du backend software, comme il l'est déjà pour
Vulkan/GL/DX (via SPIR-V/HLSL). Aujourd'hui le software exécute des lambdas C++ écrites à la main.

15. **Activer le bridge existant** [SL/NkSWShaderBridge.h](../SL/NkSWShaderBridge.h) dans
    `CreateShader` (branche SkSL commentée `NkSoftwareDevice.cpp:795`). Le bridge compile NkSL,
    extrait la reflection et génère `vertFn`/`fragFn`. **Limite actuelle** : piloté par la reflection
    (MVP + vertex color + texture), il **n'exécute pas** le corps arbitraire du shader → couvre les
    shaders standards, pas le shading custom (Phong/PBR).
16. **Interpréteur bytecode CPU (VM)** — la vraie cible : NkSL → **IR/bytecode** → petite VM qui
    exécute les instructions (arithmétique, swizzle, built-ins, samplers). **TOUS les stages** :
    vertex, fragment **ET compute** (pas seulement vertex/fragment) — le compute software doit
    aussi passer par la VM (dispatch par thread group). Prévoir l'archi VM stage-agnostique.
    Portable (aucun compilateur runtime requis), couvre **n'importe quel shader**. Alternatives
    écartées : transpile→compile C++ au runtime (besoin d'un compilateur, non portable) ;
    codegen offline (shaders figés au build).
17. **Cache bytecode** : réutiliser l'infra `NkShaderCache` (`.nksc`, FNV-1a 64-bit) déjà en place
    pour SPIR-V → **compilé une seule fois, chargé du cache** ensuite (modèle SPIR-V/DXIL, côté CPU).

---

## ❌ Bugs / lacunes connues (à corriger en Phase 1)

1. **Interpolation affine** dans le chemin actif → UV/textures déformées en perspective
   ([NkSWFastPath.h](NkSWFastPath.h):114-127). Le code perspective-correct existe mais est mort.
2. **Aucun clipping géométrique** : reject trivial `w<=0` puis `invW=0` à l'origine
   ([NkSWFastPath.h](NkSWFastPath.h):116,284) → artefacts près du near-plane.
3. **`depthOp` ignoré** (toujours LESS) malgré l'exposition pipeline
   ([NkSWFastPath.h](NkSWFastPath.h):389).
4. **Blend factors ignorés** : seul alpha-over câblé ([NkSWFastPath.h](NkSWFastPath.h):417),
   `BlendColor` existant non branché.
5. **Backface culling forcé off** ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):821) → overdraw / z-fighting.
6. **Topologie ignorée** : `RasterizeList` traite tout en triangle-list
   ([NkSWFastPath.h](NkSWFastPath.h):514) → lignes/points/strips/fans rendus faux.
7. **Sampling nearest only** dans le chemin actif, mip 0 seulement
   ([NkSWFastPath.h](NkSWFastPath.h):400-410).
8. **No-ops** : `SetViewport` ([NkSoftwareCommandBuffer.cpp](NkSoftwareCommandBuffer.cpp):167),
   `PushConstants` jetés (:205), `UpdateBuffer` ([NkSoftwareCommandBuffer.h](NkSoftwareCommandBuffer.h):68),
   `DispatchIndirect` log-only (:457).
9. **Seuil dégénérescence** `|area2|<0.5` en pixels ([NkSWFastPath.h](NkSWFastPath.h):295)
   peut jeter de fins triangles valides.
10. **`mCaps.drawIndirect=false`** ([NkSoftwareDevice.cpp](NkSoftwareDevice.cpp):557) alors que
    l'indirect est implémenté → capacité sous-déclarée.

---

## Dépendances

- **NKThreading** (ThreadPool) pour la Phase 3 (tuilage parallèle) — déjà dans le moteur.
- **NKMath / NkSIMD** pour la boucle SIMD (Phase 3) et l'algèbre BVH (Phase 4).
- **NKMemory** : allocateurs `NkAlloc`/`NkFree` uniquement (jamais de heap CRT) pour
  les structures BVH / buffers de tuiles.
- Aucune dépendance GPU (par définition). Aucune modif hors `src/NKRHI/Software/`.

---

## Références Scratchapixel (archive locale)

Chemin : `D:\Scratchapixel\markdown\lessons\`. Leçons pivots par phase — rasterization
pratique (Phase 1-2), interpolation (Phase 2), Monte-Carlo (Phase 2 AA + Phase 4),
ray-triangle & polygon-mesh & lighting (Phase 4). Usage **personnel hors ligne** ;
contenu © Scratchapixel (CC BY-NC-ND 4.0) — ne pas redistribuer.

## 📢 Valorisation (règle CLAUDE.md §2bis)
- **2026-07-05** — Plein pipeline software (écran noir → scène complète, 5 bugs : near-clip, plein-écran gl_VertexID, loadOp, format RT HDR, depth par fragment). Publication + article scientifique : `D:\Rihen\Rodolf\Publications_2026-07-05_software-rasterizer\`.
- **2026-07-06** — Ombres portées VSM software (3 parties : support viewport dans le rasterizer sans régression, passe Shadow depth-only `lightVP·model·pos`, échantillonnage atlas + PCF 3×3 dans le fragment géométrie ; cohérence écriture/lecture par construction) **+ instancing software** (`gl_InstanceID` via `SwCurInstance`, re-gen des sommets par instance → ombres des cubes instanciés `ShadowInstanced` lisant l'InstanceUBO). Limites : 1 directionnelle, biais fixe, 8→5 FPS. Publication + article : `D:\Rihen\Rodolf\Publications\09_2026-07-06_software-shadows\`. Fiche : `BugReports/Backends-Rendu/software-ombres-vsm-viewport.md`.
