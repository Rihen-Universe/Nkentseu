# NKRenderer — Roadmap

État actuel (mai 2026) : Phases A → G + G.ext M.1..M.5 + M.8 livrées. Pipeline
post-process avec **Bloom Dual-Kawase 11-pass AAA cross-API** + ACES tonemap.
HDR IBL avec cubemap dédié skybox (RGBA32F brut). PBR avec mirror via
tSkyEnvCube pour roughness ~0. SSAO v0 + Voxel AO v0 stable. **Planar
Reflection bugs RÉSOLUS** (2026-05-23). **NkVirtualShadowMaps v0 livré**
(2026-05-23) : multi-lights DIR + SPOT + POINT avec atlas dynamique skyline.
10 démos couvrant tous les matériaux et features.

Cross-API testé sur Vulkan + OpenGL (DX11/DX12/Metal restent à valider).

## ✅ Livré

### Fondations (Phase A → D.3d) — toutes livrées
- PBR forward avec UBO push-constant
- IBL CPU (Lambert irradiance + GGX prefilter + BRDF LUT)
- CSM 1-cascade + soft shadows PCF Poisson + PCSS contact-hardening
- Ring buffer UBO multi-frame
- Tonemap ACES post-process

### Phase D.4 — NkVirtualShadowMaps v0 + v1.A/v1.B ✅ (2026-05-23) ⭐
Refactor majeur shadow system : remplace `NkShadowSystem` (CSM mono-light)
par `NkVirtualShadowMaps` (multi-lights). Style UE5 simplifié.

**V0 — Infrastructure**
- ✅ **Multi-lights shadow** : DIR (CSM cascades) + SPOT (1 tile) + POINT
  (cubemap virtuel 6 faces) dans un seul atlas D32_FLOAT 4096²
- ✅ **Atlas dynamique rectpack skyline** ([NkShadowAtlasPacker](src/NKRenderer/Tools/Shadow/NkShadowAtlasPacker.h)) :
  budget 256 slots, allocation per-frame
- ✅ **Helper sampling unifié** `.glsli` (`SampleLightShadow(lightIdx,...)`)
  intégré dans PBR/Layered/Toon/Anime
- ✅ **Anti-flickering** : mode radius FIXE par cascade (8/16/32/64) +
  center=camPos + texel snap XYZ + ring UBO multi-frame (3 frames in flight)
- ✅ **Validé Demo3D VK + GL** : sun + 2 point lights (red+blue) + 1 spot,
  tous projettent ombres correctement, 17 slots actifs

**V1.A — Cascade fade** ✅
- Blend smooth sur les 15% derniers d'une cascade vers la suivante
- `fadeT = (absDepth/splitFar - 0.85) / 0.15`, clamp [0,1]
- Coût : 2 PCF samples dans la bande de transition (~15% des fragments)

**V1.B — Shadow caching per-light** ✅
- Nouveau flag `NkLightDesc::shadowStatic` (défaut false, safe re-render)
- `NkLightShadowCache` track position/direction/range entre frames
- Si TOUS slots cached → skip render pass entière (preserve atlas)
- Per-tile caching V2 (besoin ClearRect API au RHI)
- Overlay debug `slots: 17 (rend N | cache M)` dans Demo3D

**V1.C — Normal bias world-units** ✅
- Push worldPos le long de N en world units (0.05 = 5cm) avant projection shadow
- Fix peter-panning (décollement ombres pied de caster)
- shadowBias (NDC) réduit de 0.003 → 0.0005 grâce au normal bias

**V1.D — Per-material shadow override** ✅
- `NkMaterial::SetReceiveShadow(bool)` — skip shadow sample sur ce material
- `NkMaterial::SetShadowBiasMul(float)` — multiplicateur du bias
- `NkMaterial::SetCastShadowAlphaTest(bool)` — V2 reserve
- ObjBlock UBO étendu : +`vec4 shadowOverrides` (192 → 208 bytes)
- Helper shader : `SampleLightShadowEx(..., biasMul)` + wrapper compat
- Actif sur **PBR** ; Layered/Toon/Anime ignorent l'override (TODO V2)

**TODOs V2**
- ⏳ **ClearRect API au RHI** : caching per-tile (au lieu de all-or-nothing)
- ⏳ **Dynamic offsets UBO** : scale à 10k+ draws sans descriptor sets
- ⏳ **LOD tile size** adaptatif (distance light/cam)
- ⏳ **Shadow override Layered/Toon/Anime** : ObjectUBO étendu
- ⏳ **Alpha-tested shadow** (foliage) : shader Shadow alpha-aware
- ⏳ **Page-based VSM réel** UE5 (refactor 16k² atlas virtuel pagination 128²)

### Phase G — NkMaterialSystem ✅
- `NkMaterialAsset` (.nkasset JSON) + `NkMaterialInstance`
- Hot-reload des `Resources/NKRenderer/Materials/*.nkasset`
- Built-in : PBR, Toon/Anime, Glass, Skin, Hair, CarPaint, Cloth, Foliage,
  Volume, Water, Particles, Layered (16 dossiers)
- `NkDrawCall3D::material` wired ; metallic/roughness direct shortcuts conservés

### Phase G.ext — Matériaux avancés style UE5 ✅ (sauf M.6, M.7)
- **M.1** ✅ Material Layering (v0 + v1 N=8 layers, Demo8 dédié)
- **M.2** ✅ Material Parameter Collections (Demo5)
- **M.3** ✅ Blend par vertex color (Demo5 painted cube)
- **M.4** ✅ Instances hiérarchiques parent/enfants + override (Demo6)
- **M.5** ✅ Material Functions `.glsli` + #include (Demo7)
- **M.6** ⏳ Vertex Paint runtime (TODO — `mesh->PaintVertex(idx, color)`)
- **M.7** 🚫 Decal Materials (bloqué — besoin G-Buffer depth+normal)
- **M.8** ✅ Multi-slot par sous-mesh (Demo5 cube 6 faces différentes)

### Phase H.6 — Voxel AO v0 ✅ (2026-05-22)
- ✅ NkVoxelAOSystem 64×32×64 RGBA8 + bake CPU + cone-trace 4 cônes×8 samples
- ✅ Bind atlas binding=27 sur globalSet + mirror ring
- ✅ Application dans pbr.frag.vk.glsl : atténue IBL irradiance + specular
- ⏳ V1 TODO : .glsli générique pour Layered/Toon réutilisable

### Phase Planar Reflection ✅ (2026-05-23) ⭐ FIXÉ
NkPlanarReflectionSystem + reflets planaires complets sur sol mirror.

- ✅ **Auto-bake** : user enregistre plan, renderer fait passe miroir avant Geometry
- ✅ **Cross-API VK + GL** validé sur Demo10 (newport_loft HDRI)
- ✅ **4 root causes du bug fixées** (cf. `memory/nkrenderer_planar_reflection_bugs.md`) :
  1. UBO Camera mirror ring dédié (Option B) — overwrite résolu
  2. Un-mirror Y dans VS (worldPos/N/T) + recalc B = cross(N,T) — handedness
  3. MPC + VoxelAO bind sur `mGlobalSetMirrorRing` (Vulkan strict DescriptorSet)
  4. Skybox + PBR IBL : un-mirror sampling direction R

### Phase L — Post-process (largement livré)
- ✅ **Bloom Dual-Kawase 11-pass AAA cross-API** (Jorge Jimenez 2014,
  COD: Advanced Warfare) — 6 downsample + 5 upsample + tonemap 2-textures
- ✅ ACES filmic tonemap avec exposure/gamma/saturation/vignette
- ✅ Fullscreen triangle pattern moderne (gl_VertexIndex sans VBO)
- ✅ Push constant yFlipUV différentiel sub-passes/tonemap par backend
- ✅ Push constants stageFlags fix (NK_ALL_GRAPHICS, VUID-01796) — 2026-05-23
- ✅ **Color Grading LUT 3D** (2026-05-23) — 16³ identity par défaut, sampler3D
  au binding=3 du tonemap, push constant `lutStrength` + `lutSize` avec bias
  texel correct, blend mix(mapped, graded, strength). User upload custom LUT
  via TODO `NkRenderer::SetColorGradingLUT(data, size)`
- ⏳ **FXAA** : shaders externes + pipeline créés (PP_FXAA), wirage RenderGraph
  TODO (besoin split tonemap→mToneTex + FXAA→swapchain, ~30 min refactor)
- ✅ SSAO v0 stable (16 samples poisson, contact AO local) ; GTAO complet
  + voxel AO planifiés (cf. Phase H.5b/H.6 ci-dessous)
- ✅ **Auto-exposure V0** (2026-05-23) — tonemap sample uBloom center (proxy
  luma moyenne via Dual-Kawase upsample), adapte exposure vers
  `autoExposureKey=0.18` mid-gray. Push constant étendu 32→48 bytes.
  Limitations V0 : pas d'eye adaptation temporelle (V1 = compute reduction
  + SSBO double-buffer), précision moyenne (bloom threshold filtre les
  basses luminances).
- ✅ **NkRHI compute audit** (2026-05-23) — compute support OK cross-API
  VK+GL (cf. `memory/nkrhi_compute_support.md`). Déjà utilisé par NkML,
  NkAnimationSystem morph, NkComputeContext wrapper. Foundation prête pour
  Phase N GPU prefilter, auto-exposure V1, Voxel AO v1, Lumen-lite GI.
- ❌ DOF/bokeh, Motion blur, TAA, vignette/grain chromatic, Lens flares
  — non implémentés

### Phase N — IBL pipeline (partielle CPU, GPU à venir)
- ✅ Phase N v0 : `LoadFromHDR(.hdr)` via NkImage existant + convolution CPU
  IBL irradiance + prefilter (Reinhard tonemap)
- ✅ Phase N v0.5 : Background HDR skybox visible (fullscreen triangle
  + sample cubemap)
- ✅ Phase N v1 : Cubemap dédié skybox `mSkyEnvCube` (RGBA32F sans Reinhard)
  au binding=26 — preserve HDR brut > 1.0
- ✅ Phase I : PBR specular IBL via tSkyEnvCube pour roughness ≤ 0.5
  (mirror) → métalliques recevent bloom HDR
- ❌ Compute shader prefilter GPU (remplace CPU 0.5-2s → <50ms)
- ❌ Env light probes / reflection probes par zone

### Phase F — Multi-backend (partielle)
- ✅ Vulkan + OpenGL testés sur toutes les démos
- ✅ NkShaderConverter VK→GL/HLSL/MSL via SPIRV-Cross
- ⏳ DX11/DX12 partiellement testés (NkRHI implémenté, démos pas validées)
- ⏳ Metal partiellement implémenté (NkRHI compile, runtime macOS pas testé)
- ❌ Software backend stub uniquement

---

## 🔄 En cours / TODO immédiat

### Phase D.4.2 — NkVSM v2 (extensions futures)
- **ClearRect API au RHI** : caching per-tile au lieu d'all-or-nothing
- **Dynamic offsets UBO** pour ObjectUBO : 1 buffer + per-draw dynamic offset,
  scale à 10k+ draws sans alloc descriptor sets supplémentaires
- **Shadow override Layered/Toon/Anime** : ajouter `shadowOverrides` au
  ObjectUBO de chaque shader (pour l'instant only PBR honore les overrides)
- **Alpha-tested shadow** : shader Shadow avec sampling alpha texture pour
  foliage/grilles (utilise `castShadowAlphaTest` actuellement reserved)
- **LOD tile size** adaptatif : tile petit pour lights loin/dim, gros pour proches
- **Page-based VSM réel** UE5 (long terme, gros refactor 16k² atlas virtuel)

### Phase H.6 v1 — Voxel AO précision
- `.glsli` générique pour Layered/Toon/Anime (pas dupliquer le code)
- GPU bake voxel grid (CPU bake actuel = 1s sur startup)
- Densité voxel runtime adaptative (64³ → 128³ selon scene)
- Multi-bounce GI light injection (style Lumen lite)

### Phase H.5b — GTAO complet (papier Activision 2016)
Amélioration incrémentale au screen-space (alternative voxel) :
- Vraie reconstruction view-space depuis depth + invProj
- Cosine-weighted horizon integration analytique
- 8-16 directions de référence
- Cross-bilateral blur avec edge-stopping depth
- Multiplie IBL dans le PBR shader (pas juste post)

### Phase H.5c — Opacity-aware AO/shadows (conditionnel)
Pour les sols/objets transparents, propagation partielle de l'AO/shadow.
4 approches techniques notées dans la mémoire.

### Phase E — Materials 2D + lumière 2D + ombres 2D
- ⏳ Phase E v0 partielle : `DrawSpriteGlow` API stable mais effet glow non
  fonctionnel (DrawSprite fallback)
- À refactor en v1 : pipeline override par batch + conflit bindings
  Render2D vs Overlay

### Phase L — Finition post-process (TODO restants)
- **FXAA wirage RenderGraph** : pipeline créé, manque split tonemap→mToneTex
  + nouvelle pass FXAA→swapchain (~30 min refactor RenderGraph)
- **Auto-exposure** : adaptation luminance moyenne → exposure adapté
  via mipmap chain HDR (1x1 fetch) OU compute reduction (~1-2h)
- **API SetColorGradingLUT(data, size)** : permettre upload custom .cube/.3dl
  LUT cinema. Identity LUT fonctionne déjà comme placeholder
- **TAA** (Temporal AA) : remplacer FXAA par TAA moderne UE5-style.
  Jittered proj + velocity buffer + history texture + neighborhood clamp.
  ~4-5h, gros impact visuel "next gen"
- **DOF/bokeh** : profondeur de champ avec cercle de confusion
- **Motion blur** : object + camera, vélocité buffer
- **Vignette/grain/chromatic/Lens flares** : effets de lens

### Compute infrastructure (NkRHI audit prioritaire)
Avant Phase N GPU prefilter / auto-exposure GPU / Lumen GI : valider que
NkRHI a un compute path solide cross-API. Vulkan + GL ont compute, DX11
limité, DX12+Metal OK. Plan :
1. Audit `NkIDevice::DispatchCompute()`, `vkCmdDispatch` wrapper, GL shader
   storage barriers, etc. (~30 min)
2. Mini démo compute : "double values in buffer" pour valider end-to-end
3. Premier use case : auto-exposure compute reduction (lit HDR mip 0,
   reduce parallèle → 1 float luma écrit dans UBO)
4. Phase N v2 : compute prefilter IBL (~3h, replace CPU 1-2s par <50ms)

### Phase N — IBL pipeline GPU
- Compute shader equirect→cubemap (remplace CPU)
- Compute shader irradiance convolution GPU
- Compute shader prefilter par mip GPU
- Env light probes (sources multiples + blend par zone)
- Reflection probes par pièce/zone (cubemap localisé)

### Bugs/quirks connus
- **FPS chute Vulkan Debug** : 500→100 fps en ~2s sans interaction
  observée 2026-05-16. Probable Vulkan validation layers + UBO writes
  + descriptor updates intensifs en Debug. À vérifier en Release.
- **Self-shadowing artifacts** sur certains objets : bias actuel 0.003
  (NkVSMConfig.shadowBias). Live-tunable via `[` `]` dans Demo3D HUD.
  Si artefact persiste, monter à 0.005-0.01.

---

## ❌ Restant priorité 2 — Qualité visuelle/perf

### Phase H — Texture pipeline
- ✅ Loader PNG/JPG/TGA/HDR via NkImage (existant)
- ❌ Loader EXR (OpenEXR)
- ❌ Mipmap generation auto
- ❌ Compression BC1-7 (desktop) + ASTC + ETC2 (mobile)
- ❌ Texture streaming (LOD-mip selon distance)
- ❌ Hot-reload des textures
- ❌ Atlasing pour batching

### Phase M — Forward+ / Deferred
- Forward+ : compute light culling tile-based (>32 lumières)
- Ou Deferred : GBuffer + light pass (beaucoup de petites lights)
- Bench scène 100+ lights

---

## ❌ Priorité 3 — Animation & VFX

### Phase I (animation, ≠ Phase I IBL mirror) — Skeletal animation full
- Bone hierarchies + skin matrices SSBO
- Playback : linear, Hermite, cubic, additive
- Blend trees + state machines
- IK : FABRIK, CCD, two-bone
- Morph targets / blend shapes
- Retargeting

### Phase J — VFX particles
- GPU compute particle system
- Mesh particles, ribbon trails, decals
- Beam emitters, force fields, vector fields
- Event triggers (collision, lifetime)

---

## ❌ Priorité 4 — Avancé

- **Phase K** — Volumétrique : fog, god rays, clouds raymarched, volume textures, SSS amélioré
- **Phase O** — Caméras avancées : multi-cam (split-screen, PiP), cinema, VR/stéréoscopique
- **Phase P** — Scene graph + culling : hierarchy complète (interfaces D.5 prêtes), frustum culling, HiZ occlusion, LOD auto, instancing
- **Phase Q** — Editor integration : gizmos translate/rotate/scale, selection outline, stats graph, profiler frame
- **Phase R** — Raytracing hardware : Vulkan KHR_ray_tracing + DXR, RT shadows/reflections/GI, hybride rasterization+RT
- **Phase S** — GPU-driven : indirect rendering, bindless, mesh shaders, GPU culling, virtual textures (megatexture style id Tech)

---

## Minimum viable UE5-like

État actuel = **~80% du minimum viable** (NkVSM v0 + v1 cascade fade + caching
+ normal bias + per-material override + planar reflection complete ajoutent
~10% par rapport à l'estimation précédente de 70%). Restant pour MVP :
- **Phase H.6 v1 voxel AO précision** (gpu bake + .glsli partagé)
- **Phase L finition** (FXAA + auto-exposure + color grading)
- **Phase N GPU** (compute prefilter pour boot rapide)
- **Phase E v1** (Materials 2D fonctionnels)
- **Phase F finition** (DX/Metal validation)
- **Phase D.4.2** (NkVSM v2 : ClearRect API + dynamic offsets UBO + shader overrides étendus)

Au-delà : Phase H texture pipeline + Phase M Forward+ + Phase I animation
+ Phase J VFX = renderer **complet** AAA. K/O/P/Q/R/S = spécialisations
selon usage cible (jeu real-time vs cinema vs editor vs VR).
