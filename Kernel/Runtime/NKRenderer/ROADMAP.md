# NKRenderer — Roadmap

État actuel (2026-07-12) : Phases A → G + G.ext M.1..M.5 + M.8 livrées ;
viewport d'édition (gizmo + 6 view modes + edit mode mesh) livré ; convolutions
IBL sur GPU compute (Phase N v1) livrées (DX11 = CPU par défaut). Pipeline
post-process avec **Bloom Dual-Kawase 11-pass AAA cross-API** + ACES tonemap.
HDR IBL avec cubemap dédié skybox (RGBA32F brut). PBR avec mirror via
tSkyEnvCube pour roughness ~0. SSAO v0 + Voxel AO v0 stable. **Planar
Reflection bugs RÉSOLUS** (2026-05-23). **NkVirtualShadowMaps v0 livré**
(2026-05-23) : multi-lights DIR + SPOT + POINT avec atlas dynamique skyline.
10 démos couvrant tous les matériaux et features.

Cross-API validé sur **Vulkan + OpenGL + DX11 + DX12** (parité atteinte 2026-06-24,
voir « Multi-backend » plus bas). Metal + Software restent à valider.

---

## 🔎 Audit d'implémentation (2026-06-24) — état réel code vs roadmap

**Cœur forward RÉELLEMENT implémenté et fonctionnel** : Render3D, RenderGraph, Shadow
(NkVirtualShadowMaps cascades), Environment/IBL CPU, Planar Reflection, VoxelAO,
Materials/Shader, Render2D, Text, Overlay, Offscreen, VFX (CPU), Animation (+skinning GPU
câblé). C'est ce qui tourne sur les 11 démos et les 4 backends GPU.

**Partiels (cœur, trou identifié)** :
- PostProcess : tonemap ACES + bloom OK ; **FXAA DÉJÀ câblé au RenderGraph** (split
  tonemap→`ToneLDR` puis passe `FXAA_Final`→swapchain, flag `cfg.postProcess.fxaa`, shader
  `PP_FXAA/NkSL/pp_fxaa.nksl` FXAA 3.11 ; validé exécutant sur OpenGL 2026-06-25 — la mention
  « non câblé » était périmée). Reste : LUT 3D dégradé en dummy 1×1 sur OpenGL.
- Animation : tracks/blend réels ; **skinning GPU RÉEL sur GL/VK/DX11/DX12** —
  shader `Skin/NkSL/skin.vert.nksl` (source NkSL unique, bones UBO set=1 binding=4
  depuis le fix collision LightsUBO 2026-07-03), `EnsureSkinPipeline`+`FlushSkinned`.
  La note « DX12 bloqué SSBO » était PÉRIMÉE : réparé 2026-06-27 (bones→TEXCOORD2/3)
  puis migration NkSL. **morph targets = stub** (`ApplyMorphTargets` return base,
  re-vérifié 2026-07-12) ; pas de state machine / blend tree / retargeting.
- Render2D : **`DrawSpriteGlow` = stub** (fallback DrawSprite).
- Mesh : **loader glTF 2.0 LIVRÉ** — `NkGLTFLoader.{h,cpp}` from-scratch zero-STL.
  `.gltf` (JSON + .bin externe + data URI base64) et `.glb` (chunks JSON/BIN). Attributs
  POSITION/NORMAL/TANGENT/TEXCOORD_0/1/COLOR_0 ; indices u8/u16/u32→u32 ; normales calculées
  si absentes ; AABB global + par-submesh ; un NkSubMesh par primitive. **MATÉRIAUX PBR
  LIVRÉS (2026-06-25)** : `NkGLTFMaterial`/`NkGLTFImage` (baseColor/metallicRoughness/normal/
  emissive/occlusion, décodage data URI/externe/.glb via NKImage) + pont `NkGLTFMaterialBridge`
  → `NkMaterialInstance(DefaultPBR())` (API publique). **SKINNING LIVRÉ (2026-06-25)** :
  JOINTS_0/WEIGHTS_0 + `skins`/inverseBind + hiérarchie `nodes` + `animations` (LINEAR/STEP/
  CUBICSPLINE, slerp) + `EvaluateGLTFPose(t)`. Câblé dans `NkMeshSystem::Import`. Démos :
  `renderdemo --demo=12` (rubber_duck texturé) + `--demo=13` (Khronos SimpleSkin animé).
  Validé `gltftest` (rubber_duck 5676 v / 33216 i). **DIFFÉRÉ** : morph targets, cameras/lights
  glTF, KHR extensions, sparse accessors, ombres du mesh skinné (pose de repos), DX12 skin.

**⚠️ Couche « v4.0 » ORPHELINE — compile mais JAMAIS instanciée ni exposée par `NkRenderer`/
`NkRendererImpl`** (le renderer ne les utilise pas ; code à finir/brancher, pas à réécrire) :
- **Deferred** (`Passes/Deferred/NkDeferredPass`) : G-buffer 5 RT réel + barrières, **mais
  passe de lighting absente** (« tiled dispatch would be done here ») + non branché → renderer
  reste 100 % forward.
- **Streaming** : files/LRU/budget codés, **aucune E/S réelle** (`FinalizeLoad` ne charge rien,
  `ComputePriority` return 1.0) → simulateur de comptabilité mémoire.
- **IK** : rigs/chaînes OK, FABRIK a sa boucle mais sur positions placeholder {0,0,0}, ne
  lit/écrit jamais les bones → non fonctionnel ; TwoBone/CCD/Spline = squelettes.
- **Culling** : octree + frustum **réels** mais orphelins (jamais branchés au pipeline).
- **Denoiser** (OIDN/NRD `return false`), **AIRendering** (IssueCopy vide), **Voxel-sculpt**,
  **PixolSculpt** : partiels/stubs, orphelins.

**Reste à faire priorisé (re-vérifié à l'audit 2026-07-12)** :
1) ~~Culling frustum de base~~ **précision d'audit + complément 2026-07-12** : le frustum cull
   caméra était DÉJÀ actif pour l'opaque (`Submit` → `NkCamera3D::IsAABBVisible`, casters
   d'ombre collectés AVANT le cull) ; ajouté le **cull par batch des INSTANCIÉS** au Flush
   (2 chemins GPU/fallback, pas en passe miroir, passe shadow intacte) + **`GetCullStats()`**
   (opaque soumis/cullés + batchs instanciés cullés). Ce qui reste VRAIMENT orphelin =
   `NkCullingSystem` (octree/occlusion HZB/distance/LOD — v2, nécessite un mode retained).
   Limite connue : le miroir reflète la liste cullée par la caméra PRINCIPALE (un objet
   derrière la caméra manque du reflet). 2) **VSM v2 bornés** : shadowOverrides Layered/Toon/
   Anime (absents des .nksl, vérifié) + alpha-tested shadow. 3) **Finitions Phase L/E petites** :
   API `SetColorGradingLUT` (n'existe PAS — seulement des commentaires, vérifié) + vraie LUT 3D
   sur GL (dummy 1×1 confirmé NkPostProcessStack.cpp:296) + `DrawSpriteGlow` (fallback confirmé).
   4) Morph targets (stub confirmé ; nécessite AUSSI l'import morph glTF, différé). 5) Streaming
   réel (`FinalizeLoad` ne charge rien — « In a real impl » dans le code ; simulateur de budget).
   6) Deferred lighting pass + branchement (gros ; jamais instancié par le renderer). 7) IK
   renderer (orphelin — NB : NkAnima a son propre IK validé, celui du renderer est redondant à
   requalifier/supprimer). 8) Animation avancée (state machines/blend trees). 9) Metal + Software.
   10) Phase T.1 bake (nouveau chantier) ; T.2 graphe matériaux = ATTEND la coordination NKGraph.

## 🧭 Éditeur / Viewport (chantier 2026-07, cap « famille d'éditeurs »)

Socle d'un viewport d'édition façon Blender (testbed `renderdemo --demo=2`, futur socle
éditeur partagé). Détail + plan : mémoire `project_editor_gizmo_20260704` /
`project_editor_viewmodes_meshedit_plan`.

- ✅ **Gizmo 3D réutilisable** `NkGizmo3D` (`Core/NkGizmo.h`, header-only, découplé de
  NKEvent/NkRender3D) — translate/rotate/scale/combiné, poignées axe(1)/plan(2)/uniforme,
  orientation Global/Local/Normal, multi-sélection (pivot barycentre OU origines
  individuelles en Local), **snapping Ctrl** (pas configurables) + **verrou d'axe X/Y/Z**.
  Overlay via **nouvelle option moteur `NkRender3D::DrawDebugLine(..., overlay=true)`**
  (pipeline debug-line **depth-OFF** `mLinePipelineNoDepth`). Contrôleurs caméra réutilisables
  `NkOrbitCameraController3D` / `NkFlyCameraController3D`. Grille infinie `SetInfiniteGridEnabled`.
- ✅ **Modes d'affichage LIVRÉS (2026-07-05)** — touche Z cycle **6 modes**
  RENDERED / SOLID (matcap) / WIREFRAME / NORMAL / UV / AO ; wireframe via variante
  `pipelineWire` par template matériau (`NkMaterialSystem::SetWireframe`, rasterizer
  natif GL/VK/DX) ; uniforme `viewMode` + `matcapId` dans le CameraUBO ; **5 matcaps**
  (touche M : Studio/Clay/Metal/Toon procéduraux + Chrome texture, binding 28 global).
  ⚠️ Piège : le bloc CameraUBO doit rester IDENTIQUE entre pbr.vert et pbr.frag .nksl.
  Reste optionnel : mode DEPTH ; demande future Rihen = matcap par OBJET en RENDERED.
- ✅ **Edit Mode mesh LIVRÉ côté démo (2026-07-05, testbed Demo3D)** — TAB objet/édition,
  sélection **vertex/arête/face** (1/2/3, combinables Shift), pick rayon Möller-Trumbore,
  déplacement via `NkGizmo3D` (groupe au centroïde), **extrude (E) / delete (X) / merge (M) /
  create face (F)**, recalcul normales, X-ray Alt+Z, **batch GPU persistant**
  (`SetEditOverlayLines/Tris/Points`, ~145 FPS sphère dense), persistance par objet.
  Moteur : `NkMeshSystem` cache CPU (`keepCPU`) + `UpdateVertices(Range)` ;
  `NkRender3D::DrawDebugTriangle` ; purge debug-lines O(n). **`NkEditMesh` half-edge
  (Mesh/NkEditMesh.{h,cpp}) Phase 1a** compile — RESTE : quadify + câblage éditeur (1b),
  ops topo n-gon (2), import (3) — cf. mémoire `project_editmesh_halfedge_plan`.

## ✅ Livré

### Capture & enregistrement vidéo ✅ (2026-07-12) — pipeline complet
- ✅ **Readback GL réparé** (NKRHI `MapBuffer` : PERSISTENT/COHERENT illégaux
  sur storage mutable → 1282 ; flags par usage READ/WRITE) — capture sur
  GL **et** DX11 validée (images identiques), flip Y GL dans
  `NkOffscreenTarget::ReadbackPixels`.
- ✅ **`NkFrameCapture`** (Tools/Offscreen) : capture ASYNCHRONE — ring de
  staging buffers + fences (`Submit(signalFence)` + `IsFenceSignaled`),
  `EnqueueCopy` non bloquant (ring plein = frame sautée, jamais de stall),
  `Poll` non bloquant livrant RGBA8 top-down (flip GL auto) → consommable
  par un thread encodeur/tutoriel/réseau. Zéro `WaitIdle` en régime.
- ✅ **renderdemo `NK_CAPTURE=<frame>`** (PNG one-shot, validation headless
  des agents) et **`NK_RECORD=<out.mp4>`** (+`NK_RECORD_FPS`, défaut 30) :
  rendu → NkFrameCapture → `NkVideoRecorder` NKMedia (H.264, encodage
  threadé). **Prouvé bout-en-bout** : demo3 GL → mp4 h264 1280×720 30 fps
  6.4 s / 193 trames, lisible ffprobe/ffmpeg, contenu vérifié.
- ✅ **V2 « voir + enregistrer » (2026-07-12)** — finalement SANS toucher
  NKRHI : passe **MirrorPresent** dans le RenderGraph (blit plein-écran de la
  cible redirigée vers le vrai swapchain, shader `Blit/NkSL`, ~1 draw) via
  `SetFinalColorTargetMirror(target, true)`. La fenêtre reste vivante pendant
  l'enregistrement, rendu à pleine vitesse (HUD 144 FPS mesuré en record).
  2 pièges corrigés : descriptor set DÉDIÉ au blit (le set partagé avec FXAA
  était écrasé au Submit sur GL — exécution différée → FXAA lisait sa propre
  cible = image noire) ; flip Y écran par backend (DX/VK flip, GL direct).
  Protections : resize pendant record = arrêt propre ; drainage final borné.
- ⚠️ **PLAFOND ENCODEUR MESURÉ** : le H.264 CPU soutient ~10 fps en 720p
  (RAM plate) ; à 30 fps la file NON BORNÉE de NkVideoRecorder gonfle de
  ~100 Mo/s → machine saturée. Défaut NK_RECORD_FPS = 10. **À COORDONNER
  côté NKMedia** (module autre agent) : file bornée + politique de drop +
  stats de profondeur ; et/ou encodage MJPEG (moins cher) pour cadence haute.
- ✅ **Toggle à chaud + zone (2026-07-12)** — renderdemo : **touche F9**
  démarre/arrête l'enregistrement en cours de session (noms auto
  `nk_record_NNN.mp4`) ; `NK_RECORD_RECT=x,y,w,h` n'enregistre qu'une ZONE
  (crop CPU au push, w/h alignés 2, clamp fenêtre, arrêt propre au resize).
  Côté moteur tout est activable/désactivable au runtime
  (`SetFinalColorTargetMirror` ↔ handle nul). Doc :
  `wiki/Runtime/NKRenderer/Capture.md` + README racine.
- ⏳ V3 : résolution d'enregistrement ≠ fenêtre (`NK_RECORD_W/H`, rendu 4K
  natif pendant affichage 720p — l'offscreen est déjà indépendant) ; audio.

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

### Phase N — IBL pipeline
- ✅ Phase N v0 : `LoadFromHDR(.hdr)` via NkImage existant + convolution CPU
  IBL irradiance + prefilter (Reinhard tonemap)
- ✅ Phase N v0.5 : Background HDR skybox visible (fullscreen triangle
  + sample cubemap)
- ✅ Phase N v1 : Cubemap dédié skybox `mSkyEnvCube` (RGBA32F sans Reinhard)
  au binding=26 — preserve HDR brut > 1.0
- ✅ Phase I : PBR specular IBL via tSkyEnvCube pour roughness ≤ 0.5
  (mirror) → métalliques recevent bloom HDR
- ✅ **Convolutions GPU compute (2026-07-12)** — `NkIBLCompute.{h,cpp}`
  (Tools/Environment) : kernels NkSL irradiance Lambert + prefilter GGX
  (chemin compute prouvé de NkTensorGpu : NkSL→SPIRV/GLSL/HLSL, SSBO in/out),
  branchés dans `LoadFromHDR` avec **fallback CPU automatique**. Mesuré
  (demo3, HDR 1k, prefilter 256²×6 mips) : **convolution 9-28 ms vs CPU
  79-99 ms** (compile kernels 10-260 ms one-shot par backend). Validation
  numérique `NK_IBL_VERIFY=1` : GL/VK/DX12 **maxDiff 5/255 sur 0.09 %** des
  octets (= trig float GPU). ⚠️ **DX11 : CPU par défaut** (maxDiff 175/255
  sur 0.8 % des texels, fxc cs_5_0 à investiguer ; `NK_IBL_GPU=1` force).
  NB : le cache disque IBL couvrait déjà les runs suivants ; le gain GPU =
  premier chargement + **swap de HDRI à chaud** (éditeur, T.5).
- ❌ Env light probes / reflection probes par zone

### Phase F — Multi-backend (DX au niveau VK)
- ✅ Vulkan + OpenGL testés sur toutes les démos
- ✅ NkShaderConverter VK→GL/HLSL/MSL via SPIRV-Cross + générateurs NkSL→HLSL DX11/DX12 directs
- ✅ **DX11 + DX12 validés à parité avec Vulkan** (2026-06-24, session marathon ~24 fixes RHI/
  shader/renderer). Bugs majeurs résolus : ring sampler overflow DX12 (>64 draws), matrice TBN
  transposée GLSL→HLSL (éclairage mort), mips matériau non générés DX12 (textures blanches),
  conventions Y DX (HDR/bloom/ombres/reflets), #820 clear-value (perf), cache DXIL (démarrage 6×).
  Détail dans mémoire `project_session_20260623_dx12_render_fixes`.
- ⏳ Metal partiellement implémenté (NkRHI compile, runtime macOS pas testé — besoin Mac)
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
*(Audit 2026-07-12 : la ROADMAP sous-vendait — le gros de la 2D éclairée est
LIVRÉ dans `NkRender2D`, seul le glow reste un stub.)*
- ✅ **Lumières 2D** : `SetLights2D` (point lights `kMaxLights2D` + ambient,
  UBO `lights[]` du shader Render2D)
- ✅ **Ombres 2D** : `SetShadowCasters2D` (cercles, E.5) +
  `SetShadowCastersAABB2D` (32 AABB murs/plateformes, E.7a)
- ✅ **Layer masks lumière/shape** (E.7b : `light.layerMask & shape.layerMask`)
- ✅ **Normal maps 2D** (E.7c : binding 12, relief éclairé)
- ⏳ `DrawSpriteGlow` : API stable mais effet non fonctionnel (fallback
  DrawSprite — vérifié 2026-07-12). Refactor v1 : pipeline override par batch
  + conflit bindings Render2D vs Overlay

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
- ~~**Debug-draw invisible dans la vue principale quand un miroir est
  actif**~~ **CORRIGÉ 2026-07-12** : la passe miroir (rendue en premier)
  appelait `FlushDebug` qui décrémentait la vie des primitives one-frame
  et les purgeait — la vue principale n'avait plus rien (symptôme : cercle
  vert du matériau actif Demo4/5 visible SEULEMENT dans le reflet). Fix :
  les overlays debug/édition ne sont plus rendus dans la passe miroir
  (aides d'éditeur ≠ contenu de scène — un reflet ne les montre pas).
- **IBL GPU sur DX11** : convolutions compute désactivées par défaut
  (maxDiff 175/255 sur 0.8 % des texels vs CPU, fxc cs_5_0 — GL/VK/DX12
  propres à 5/255). `NK_IBL_GPU=1` pour reproduire/investiguer.
- **« Sous le plan plus clair qu'au-dessus » (demo3, rapport Rihen) —
  MESURÉ 2026-07-12** (capture DX11 + comparaison pixels, outil NK_CAPTURE) :
  le reflet n'est PAS plus lumineux (sphère réfléchie lum 174 vs directe 186) ;
  la vraie différence est un **voile gris désaturant** sur le reflet (canal B
  de la sphère : 2 direct → 58 reflété) = le mix du shader ReflFloor
  `color = mix(litBase, reflColor, reflStr)` injecte ~10-40 % de l'éclairage
  gris du sol par-dessus le reflet, + flou du RT de réflexion. La vue directe
  (sous le plan) est donc plus nette/saturée → perçue « plus claire ».
  En cause aussi : `reflStr = (1-roughness)*mix(0.9, 1.0, fresnel)` = miroir
  ~90 % à TOUT angle (non physique). **RÉSOLU EN OPTION UTILISATEUR
  (2026-07-12, demande Rihen)** : `NkPBRParams::reflBlend` +
  `NkMaterial(Instance)::SetReflFloorBlend(v)` — `-1` = Fresnel PHYSIQUE
  (4 % de face → 100 % rasant, style UE5) ; `[0..1]` = STYLISÉ avec intensité
  du voile litBase (1 = look historique par défaut, 0 = reflet pur). Propagé
  par l'héritage M.4. Validé par captures DX11 mesurées : mode défaut =
  non-régression pixel exacte ; reflet pur = canal B de la sphère réfléchie
  58 → 3 (= sphère directe) ; physique = reflet ~4 % de face. Demo4/demo3 :
  touche **P** cycle les modes + env `NK_REFL_MODEL=<0-3>`.
- **Readback OpenGL de NkOffscreenTarget cassé** (GLAD 1282
  glMapNamedBufferRange) — la capture NK_CAPTURE ne marche que sur DX11
  (vérifié pixel-perfect) ; fix côté NKRHI GL à coordonner (module partagé).

---

## ❌ Restant priorité 2 — Qualité visuelle/perf

### Phase H — Texture pipeline
- ✅ Loader PNG/JPG/TGA/HDR via NkImage (existant)
- ✅ **Loader EXR** (audit 2026-07-12 : `NkEXRCodec` livré dans NKImage —
  la démo materials charge d'ailleurs `piazza_bologni_1k.exr`)
- ✅ **Mipmap generation** (audit 2026-07-12 : `NkIDevice::GenerateMipmaps`
  au RHI, utilisé par la chaîne matériaux — cf. fix mips DX12 2026-06-23)
- ❌ Compression BC1-7 (desktop) + ASTC + ETC2 (mobile)
- ❌ Texture streaming (LOD-mip selon distance)
- ❌ Hot-reload des textures (les matériaux `.nkasset` l'ont, pas les textures)
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

## ❌ Phase T — Texturing & éclairage assistés (fusion corpus IA 2026-07-09)

Couches d'assistance au-dessus de l'existant (système de matériaux 16 familles,
IBL, NkVSM, hot-reload `.nkasset`) — **rien ne remplace, tout étend**. Principe :
génération éditable couche par couche, jamais un bitmap figé ; l'artiste reprend
la main à chaque étape. Inférence locale (NKAI/NKGen) privilégiée, API externe
optionnelle.

### T.1 — Bake automatique (fondation, AUCUNE IA, à faire en premier)
- ❌ Bake AO ray-based (réutilise NKRHI ; lié Phase H mipmaps/streaming)
- ❌ Bake curvature (pilote l'usure/dégradation procédurale)
- ❌ Bake thickness/SSS (peau, tissus translucides)
- ❌ Pipeline de bake batché (tous les assets d'une scène)

### T.2 — Graphe de matériaux (extension des templates existants)
- ❌ Les templates matériaux actuels deviennent des graphes pré-câblés
  navigables/éditables — **compatibilité ascendante garantie** (les `.nkasset`
  existants continuent de fonctionner)
- ❌ Nodes de blend (2 textures via masque procédural : bruit, gradient)
- ❌ Masques peints (entrée depuis la peinture 3D, cf. T.3)
- ❌ Nodes de variation procédurale (usure/salissure pilotées par curvature/AO de T.1)
- ❌ Compilation multi-backend via la chaîne shader existante (NkSL → GL/VK/DX)
- ❌ Presets génériques + presets signature (métal patiné doré, motifs Bamiléké,
  « tech-organique ») partagés entre projets
- ⚠️ Substrat de graphe = **NKGraph** (`Kernel/Runtime/NKGraph/ROADMAP.md`,
  décision 2026-07-09) : cœur agnostique partagé avec Blueprint (NKCode), VFX
  (Noge), procédural (AI), anim graphs (NkAnima) ; canvas d'édition dans
  NKEditorKit. Le graphe de matériaux est le **1er consommateur désigné**
  (P5 NKGraph) : il se construit AVEC le cœur, et compile vers NkSL (aucune
  évaluation de graphe au runtime)

### T.3 — Peinture de textures 3D (contrepoids manuel indispensable)
- ❌ Projection écran→UV temps réel, calques non destructifs
  (albedo/roughness/normal séparés), brosses classiques (dureté/opacité/flow)
- ❌ Modes de fusion + undo/redo par calque, export/import de calques inter-assets
- ❌ Stamps génératifs IA (zone + prompt → patch localisé), raccord automatique
  (palette/luminosité/fréquence de détail), bibliothèque de stamps

### T.4 — Génération de textures PBR
- ❌ Albedo depuis texte/référence → dérivation des autres maps (normal from
  height, roughness estimé) → plus tard génération multi-map native cohérente
- ❌ Tileabilité : détection des bords non tileables + correction auto
- ❌ Super-résolution : upscale cohérent cross-maps (albedo/normal/roughness en
  préservant leur relation physique)

### T.5 — Éclairage assisté
- ❌ GI light probes / irradiance volumes (= Phase N « env light probes » déjà
  listée ; prérequis SILENCIEUX de toute suggestion d'éclairage — un setup suggéré
  sur un rendu plat ne rendra jamais bien)
- ❌ Suggestion de setup depuis mood/référence : description → configuration
  structurée de lumières (type/position/couleur/intensité) traduite en lumières
  natives ; bibliothèque de setups classiques (three-point, clair-obscur,
  rim-light) en fallback
- ❌ Génération/calibration HDRI (import exposure/orientation via pipeline
  Phase N existant) + presets signature (jour/nuit/dramatique/doux + identité
  Afrofuturiste) avec variations proposées
- ❌ (R&D, jamais sur le chemin critique) relighting neuronal 2D pour previz
  rapide de mood sans re-render

**Ordre imposé** : T.1 (bake) → T.2/T.3 (éditabilité) → T.4 (génération) → T.5.
L'éditabilité AVANT la génération : une texture générée sans outil de retouche
fine est inutilisable en production stylisée.

---

## Minimum viable UE5-like

État actuel = **~80% du minimum viable** (NkVSM v0 + v1 cascade fade + caching
+ normal bias + per-material override + planar reflection complete ajoutent
~10% par rapport à l'estimation précédente de 70%). Restant pour MVP :
- **Phase H.6 v1 voxel AO précision** (gpu bake + .glsli partagé)
- **Phase L finition** (~~FXAA~~ ✅ + ~~auto-exposure~~ ✅ ; reste API
  SetColorGradingLUT + LUT 3D réelle sur GL)
- ~~**Phase N GPU** (compute prefilter)~~ ✅ 2026-07-12 (reste : GPU sur DX11)
- **Phase E v1** (Materials 2D fonctionnels)
- **Phase F finition** (DX/Metal validation)
- **Phase D.4.2** (NkVSM v2 : ClearRect API + dynamic offsets UBO + shader overrides étendus)

Au-delà : Phase H texture pipeline + Phase M Forward+ + Phase I animation
+ Phase J VFX = renderer **complet** AAA. K/O/P/Q/R/S = spécialisations
selon usage cible (jeu real-time vs cinema vs editor vs VR).
