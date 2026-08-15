# NKRenderer — Roadmap

État actuel (2026-07-22) : Phases A → G + G.ext M.1..M.5 + M.8 livrées ; VSM v2
(overrides + ombres alpha-testées **4 backends**), morph targets v1+skinnés,
animation avancée v1+v2, streaming réel v1+v2, deferred v1+v2 (GL/VK validés)
livrés — voir « Reste à faire priorisé » ;
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

## 🦴 L'ANIMATION A QUITTÉ CE MODULE (2026-08-14)

`Tools/Animation` **ne contient plus le système d'animation**. En application du
bloc de décision « SUBSTRATS ANIMATION ET COMPORTEMENT » (`CLAUDE.md` du
répertoire parent), **5 076 des 5 568 lignes** en sont sorties. Il reste
**492 lignes, et rien que ce qui dessine** :

| Reste ici | rôle |
|---|---|
| `Tools/Animation/NkAnimationSystem.{h,cpp}` (345 l.) | **façade de rendu** : téléversement des matrices de skinning, soumission des meshes skinnés, pelure d'oignon, compute de morph targets, debug-draw du squelette |
| `Tools/Animation/NkPoseDebugDraw.{h,cpp}` (147 l.) | dessine COM, polygone de support, fil d'aplomb |

Où le reste est parti :

| | module | espace de noms | volume |
|---|---|---|---|
| clips, keyframes, échantillonnage, player, blend 1D/2D, HFSM, reciblage, éditeur de pose-clés, motion path | `Kernel/Runtime/NKAnimation` | `nkentseu::anim` | **3 456 l.** |
| masse/COM, équilibre, contacts, correction de pose et de clip | `Kernel/Runtime/NKAnimPhysics` | `nkentseu::animphys` | **1 621 l.** |

**NKRenderer est désormais CONSOMMATEUR de ces deux modules**, et son code le dit :
dans ses signatures, tout ce qui est préfixé `anim::` ou `animphys::` vient d'un
substrat. Une frontière qu'on lit vaut mieux qu'une frontière qu'on documente.

**Motif, mesuré** : sur les 5 568 lignes d'origine, **438 seulement — 7,9 %**
touchaient au rendu. Sept en-têtes sur onze n'incluaient que Foundation et
écrivaient eux-mêmes leur indépendance (« Pure Foundation : AUCUN GPU, headless »).
Conséquence supprimée : NkAnima, PV3DE et NKScena devaient tirer **tout** le
renderer pour animer, et une application 2D ne pouvait pas animer du tout.

⚠️ **`BakeFromGLTF` n'est plus une méthode de `NkAnimationClip`.** C'était le seul
lien entre le modèle d'animation et le chargeur glTF, et il suffisait à retenir
toute l'animation ici. C'est désormais une **fonction libre**, du côté qui connaît
le format : `renderer::BakeClipFromGLTF(data, animIdx, fps, out)` dans
`Mesh/NkGLTFAnimBake.{h,cpp}`. Pas d'interface, pas de virtuel — même motif que la
suppression de la seconde structure demi-arête le 2026-07-26.

⚠️ **`Tools/IK/NkIKSystem` n'a PAS déménagé** et reste LA référence IK du dépôt.
Son énumération `NkIKSolver` a été renommée **`NkIKMethod`** : elle désigne une
*méthode* de résolution, et le nom devenait dangereux depuis que `Noge::NkIKSolver`
existe pour désigner une *classe* d'adaptation.

📍 **`Tools/Director/NkRoleContext` (555 l.) attend encore ici**, pour la même
raison que l'animation y attendait : c'est le périmètre de **NKBehavior**, qui
n'existe pas. Ne pas le déplacer vers rien.

---

## 🥽 NOTE DE COORDINATION — chantier NKXR / stéréo (agent XR, 2026-08-10)

Le chantier XR (`Kernel/Runtime/NKXR`, `XR_MISSION_IA.md`, branche de travail
worktree `Nkentseu-xr`) a livré son étage 0 (validé par Rihen le 2026-08-10) :
la démo `NKXRDemo` rend déjà la stéréo côte à côte **sans toucher aux passes**
— un `NkRenderer` complet PAR ŒIL en offscreen partagé (patron NK3DModeler),
c'est la « V1 deux passes vers deux cibles » prévue par la mission. L'étage 1
demande maintenant DE la coordination, d'où cette note. Interlocuteur : agent
NKXR ; rien ici ne sera fait sans elle.

**Ce que l'étage 1 touche (minimal, hors fichiers verrouillés) :**
1. `Core/NkRendererTypes.h` + `Core/NkCamera.{h,cpp}` — **frustum décentré** :
   champs `useFovAsym + fovLeft/Right/Up/Down` (radians signés, convention
   XrFovf) dans `NkCamera3DData`, branche dédiée dans `RebuildImpl` avec la
   MÊME convention que le chemin symétrique (colonne-majeure, profondeur
   [-1,1], `w = -z_vue`) → pour un FOV symétrique, matrice IDENTIQUE au pixel
   près (c'est le critère de non-régression). Le culling suit tout seul : les
   plans de frustum sont extraits de `viewProj` (Gribb-Hartmann). AUCUNE passe
   modifiée. La projection de référence côté NKXR : `NkXrProjectionFromFov`
   (`NKXR/NkXrTypes.h`), testée aux bords du clip (self-test 66/66).
2. Rien d'autre à ce stade. Le partage d'un seul graphe pour deux vues (une
   seule shadow map, un seul culling — aujourd'hui tout est ×2) et le
   **multiview Vulkan** sont des chantiers ULTÉRIEURS qui toucheront le
   RenderGraph et les passes : ils feront l'objet d'une NOUVELLE note et d'un
   accord explicite AVANT toute modification.

**Exigence de Rihen (2026-08-10) — ANTI-ALIASING performant en XR** : en
casque, l'aliasing scintille à chaque micro-mouvement de tête ; le FXAA actuel
ne suffira pas à terme.

### ⚠️ Constat 2026-08-12 (agent NKXR) — `NkRendererConfig::msaaSamples` est un champ MORT
`grep` sur tout `Kernel/Runtime/NKRenderer/src` : le champ est **déclaré et
jamais lu**. Le MSAA n'existe donc PAS dans NKRenderer — le brancher n'est pas
un réglage mais un chantier réel : cibles multi-échantillonnées (couleur ET
profondeur), attachement de résolution dans les passes qui écrivent
`mainColor`, post-process qui lit l'image résolue, et le tout sur 4 backends.
**Rien n'a été touché** ; c'est signalé pour que le champ ne trompe personne
(un `msaaSamples = 4` dans une config donne aujourd'hui un silence, pas du
MSAA — le pire des retours).

**Ce que l'agent XR fait en attendant, sans toucher à NKRenderer** :
supersampling par la résolution (`NK_XR_RENDER_SCALE`, mesuré : 1872×1886 par
œil tient 68-71 i/s sur Quest 2 + 3070 Laptop) et essai du **TAA existant, un
historique par œil** — c'est gratuit dans l'architecture XR actuelle puisque
chaque œil a SON renderer, donc son propre historique : le piège classique du
TAA stéréo (historique partagé → fantômes) ne peut pas se produire ici.

**Quand le MSAA se fera**, l'ordre proposé : (1) `NkOffscreenDesc.samples` +
cibles MSAA + résolution dans la passe Geometry ; (2) vérifier les passes qui
lisent `mainColor`/`mainDepth` (SSAO, planar, bloom) ; (3) exposer via
`NkRendererConfig::msaaSamples` — enfin vivant. Interlocuteur : agent NKXR.

---

## ⚠️ Trois pièges du RenderGraph et des passes plein écran (mesurés, 2026-07-30)

Découverts en finissant le TAA. Les trois produisaient une image **d'apparence
plausible** : aucun ne se voit sans mesurer.

1. **Une passe SANS attachement ne fait PAS transitionner les transients qu'elle
   lit.** Le graph ne pose la barrière `COLOR_ATTACHMENT → SHADER_READ` que pour
   une passe déclarant un vrai attachement ; `Reads(id)` seul ne suffit pas →
   l'échantillonnage renvoie du **noir**, silencieusement. Ça semble marcher tant
   qu'une AUTRE passe fait la transition à votre place (la passe `AutoExposure`
   lit `mainColor` sans attachement et fonctionne, mais seulement parce que
   `PostProcess` le relit juste après AVEC un attachement). **Règle** : toute
   passe qui échantillonne un transient doit déclarer une vraie cible ; si sa
   sortie est persistante, en faire un transient DU GRAPH plutôt qu'une cible
   possédée par le sous-système.
2. **Le cache de framebuffers est indexé par NOM DE PASSE, et le graph persiste
   entre frames** → une passe ne peut pas changer de cible d'une frame à l'autre.
   Un ping-pong se fait donc avec des cibles FIXES et une passe de copie, pas en
   alternant l'attachement d'une même passe.
3. **`yFlipUV` (échantillonnage) et `ndcYSign` (reconstruction écran) sont deux
   quantités distinctes** qui divergent par backend : jamais les faire partager
   un slot de push-constant. Et le `yFlipUV` à utiliser dépend de la TEXTURE
   LUE : copier la convention d'une passe qui lit *autre chose* (ici le deferred
   lighting au lieu du FXAA, qui lit le même `ToneLDR`) donne une image
   retournée. Une copie entre deux cibles off-screen doit PRÉSERVER
   l'orientation — ce n'est pas la convention du blit vers l'écran.

Corollaire : **un effet temporel doit être désarmé à chaque rebuild du graph**
(`mTAAHasPrev = false`), car le rebuild recrée ses transients vierges — sinon
l'écran s'assombrit puis remonte sur ~20 frames à chaque resize, changement
d'option ou redirection de cible (capture / enregistrement).

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
   Anime (absents des .nksl, vérifié) + alpha-tested shadow — ✅ LIVRÉ 2026-07-12 (cf. TODOs V2).
   3) **Finitions Phase L/E petites** : ✅ `SetColorGradingLUT(rgba, size)` LIVRÉ
   (NkPostProcessStack, accessible via `GetPostProcess()`, recréation auto si taille change) ;
   ✅ vraie LUT 3D sur GL LIVRÉE (le dummy 1×1 datait du chemin SPIRV-Cross sampler3D —
   le tonemap NkSL natif marche : validé capture `NK_LUT_TEST=1` teal&orange, 99,4 % pixels
   gradés, zéro crash) ; ✅ `DrawSpriteGlow` RÉEL LIVRÉ + validé capture (halo
   radial visible, demo 9) — intégré au batching (batch dédié marqué glow →
   pipeline Glow2D + PC 96B au Flush, jamais fusionné ; buffers 1-quad v0
   retirés). AU PASSAGE, vrai bug GL 2D corrigé : le descriptor set UNIQUE
   partagé du Flush était écrasé au Submit (exécution différée → TOUS les
   batchs de la frame samplaient la DERNIÈRE texture bindée, ex. l'atlas de
   police du HUD à la place des textures des sprites ; UB sur VK aussi) →
   POOL de 256 sets per-batch, bindings partagés (lights/cookies/shadows/
   normal) répliqués via BindSharedTexture/BindSharedUBO.
   4) ~~Morph targets~~ ✅ **LIVRÉ v1 CPU (2026-07-13)** : import glTF (`primitives[].targets`
   deltas POSITION/NORMAL bakés world, `mesh.weights`, canaux anim `WEIGHTS` plats
   LINEAR/STEP/CUBICSPLINE-dégradé) + `EvaluateGLTFMorphWeights(t)` +
   `ApplyGLTFMorphCPU` (base + Σ w·delta, normales renormalisées) →
   `NkMeshSystem::UpdateVertices` (mesh `dynamic=true`). Asset de test généré
   `Resources/Models/MorphTest/morph_test.gltf` (cube→sphère + étirement Y, anim
   4 s) ; DemoGLTF applique automatiquement si le modèle a des morphs — validé
   capture GL (`NK_GLTF_MODEL=Resources/Models/MorphTest/morph_test.gltf --demo=12`).
   ✅ **Morphs sur meshes SKINNÉS (2026-07-13)** : `ApplyGLTFMorphCPUSkinned` (deltas
   appliqués sur `skinnedVertices` AVANT le skinning GPU, bones préservés, cœur commun
   template) + câblage DemoAnim ; asset test `SkinMorphTest/skinmorph_test.gltf`
   (colonne 2 os qui PLIE pendant qu'un morph la GONFLE, déphasés) — validé captures
   (bulge à t1, coude 70° sans bulge à t2 : les deux coexistent).
   Reste v2 : application GPU (compute). 5) ~~Streaming réel~~ ✅ **LIVRÉ v1
   (2026-07-13)** : `NkStreamingSystem` fait de VRAIES E/S — worker thread dédié
   (disque + décodage CPU : NkImage RGBA8 / loaders mesh gltf/glb/obj), upload GPU
   sur le thread de rendu au Update() (borné maxJobsPerFrame), handles réels
   (`GetTexture/GetMesh`), éviction LRU de vraies ressources (Release) + stream-out
   par distance, priorité 1/(1+dist), échecs sans retry-spam (`GetFailedCount`).
   Self-test `NK_STREAM_TEST=1` 4/4 (5 assets réels + 1 introuvable, budget 6 MiB
   serré → 3 évictions, handles cohérents, budget respecté). ✅ **V2 MIP
   STREAMING progressif (2026-07-13)** : le worker fabrique une version basse
   résolution (`lowResMax`, Resize bilinéaire) uploadée EN PREMIER (texture
   floue instantanée → zéro pop) ; la pleine résolution REMPLACE quand la
   caméra passe sous `refineDist = streamInDist × refineDistMult`
   (`TickRefines`, budget d'uploads partagé, swap de handle — l'appelant
   re-binde en surveillant `GetTexture`). Config ajustable runtime
   (`GetConfig()`). Démo `--demo=19` Stream : allée de panneaux, caméra libre
   (C/WASD), distances réglables live (1/2+Shift), fondu anti-pop.
   Reste v3 : LOD meshes, vraie chaîne de mips partagée (base-level GPU).
   6) ~~Deferred~~ ✅ **LIVRÉ v1 (2026-07-13)** : pipeline DIFFÉRÉ opt-in (`cfg.deferred`,
   `NK_DEFERRED=1` dans renderdemo) — passe `DeferredGeom` (MRT : RT0 albedo+metallic
   RGBA8, RT1 normal world+roughness 16F, RT2 emissive+AO 16F + depth partagée, UN
   pipeline pour tous les opaques) → passe `DeferredLight` fullscreen (G-buffer +
   LightsUBO + ombres atlas + IBL irradiance/prefilter/BRDF-LUT, worldPos reconstruit
   depuis la depth via invViewProj — NDC Y INVERSÉ car les VS 3D négatent Y en clip) →
   passe `ForwardRest` (skybox/instanciés/skins/grille/transparents/debug forward
   par-dessus, même depth). Validé capture GL demo 2 : 73 % pixels ≈ forward, ombres
   + panneau alpha-testé OK, **207 FPS vs 140 forward**. Diag `NK_DEFLIGHT_DEBUG=1/2/3`
   (N/worldPos/albedo). ⚠ FIX NKRHI GL au passage : `CreateFramebuffer` n'appelait
   JAMAIS glDrawBuffers → les MRT 1..N-1 étaient JETÉES (défaut GL = attachment 0 seul).
   **V2 en cours (2026-07-13)** : ✅ COOKIES portés (spot 2D + point cube — parité GL
   passée de 73 % à **91,7 %** vs forward, le X rouge du sol est là) ; ✅ conventions
   NDC/sampling PAR BACKEND dans le PC (GL : sample direct + ndcY=-1 ; VK : sample
   direct + ndcY=+1 ; DX : sample flippé + ndcY=-1) ; **MULTI-BACKEND
   (feu vert Rihen, 2026-07-13)** : la CAUSE RACINE de VK sombre + DX12 RT1..2
   mortes était le **blend à 1 seul attachement** sur un render pass à 3 cibles
   (VK exige attachmentCount == N ; DX12 laissait RT1..7 avec write mask 0) —
   fix : le pipeline G-buffer déclare 3 blends opaques (le RP VK et le PSO DX12
   savaient DÉJÀ faire du MRT). ✅ **GL = référence** (91,8 % parité, purger
   `cache/shaders` si le X rouge manque) ; ✅ **VULKAN VALIDÉ capture** (mêmes
   conventions que DX : sample flippé + ndcY négatif — l'essai « sample direct »
   donnait l'image inversée) ; ✅ **RAYONS PARASITES DX11/DX12 RÉSOLUS
   (2026-07-23, a6c71299)** : le signe NDC Y de la reconstruction worldPos
   était codé en dur (-1) alors que le VS flippe vUV sur DX → worldPos MIROITÉ
   verticalement → le cône du spot à cookie touchait les positions miroir
   (rayons en éventail) ; le shader consomme désormais pc.invResolution.x
   (ndcYSign : +1 DX, -1 GL/VK). **DEFERRED CORRECT SUR LES 4 BACKENDS**
   (captures : DX11+DX12 alignés sur GL, VK non régressé) ; ✅ **DX12 : DEVICE
   REMOVED RÉSOLU (2026-07-23, 269207c9)** — 4 causes racines (root constants 64 o
   débordés, release PSO immédiat → destruction différée, cache variantes
   NkUnorderedMap défaillant → NkVector, RowPitch readback) — détail dans
   « Bugs/quirks connus » ; le deferred DX12 tourne à 409 FPS avec capture
   réelle. Limites restantes : clearcoat/subsurface/velocity non portés,
   passe miroir non différée, boucle 32 lumières (tiled/clustered = v3), DX12 à
   capturer.
   Ancien plan :
   l'existant `Passes/Deferred/NkDeferredPass` = G-buffer 5 RT + buffers lumières, SANS
   shaders ni branchement. Briques : **(a)** shaders NkSL `DeferredGeom` (variante du PBR
   vert/frag écrivant en MRT : RT0 albedo+metallic RGBA8, RT1 normal+roughness RGBA16F,
   RT2 emissive+AO RGBA16F — velocity différée) ; **(b)** shader `DeferredLight` plein
   écran (fullscreen triangle, lit G-buffer + LightsUBO + ShadowSlots + IBL → accum HDR
   RGBA16F ; v1 = boucle 32 lumières par pixel, tiled/clustered = v2) ; **(c)** branchement
   `NkRendererImpl::RebuildRenderGraph` derrière `cfg.deferred` (défaut OFF) : pass Geometry
   MRT → pass Lighting → alimente la chaîne post existante (bloom/tonemap inchangés) ;
   reconstruction worldPos depuis depth (invViewProj). Prérequis vérifiés : le RenderGraph
   gère les MRT via SetColor(0..3), les transients RGBA16F existent (HDR path). 7) ~~IK renderer~~ ✅ **REQUALIFIÉ (2026-07-13)** :
   il n'existe qu'UN module IK (`Tools/IK/NkIKSystem`) et il N'EST PLUS orphelin — rendu
   fonctionnel par NkAnima M0 (3240b1ae : FABRIK/TwoBone/CCD sur positions réelles via
   `BindPose`/`EvaluateGLTFWorldJoints`, validé DemoIK + DemoIKChar). La note d'audit
   « placeholder {0,0,0} » datait d'avant M0. Rien à supprimer : c'est L'IK de NkAnima. 8) ~~Animation avancée~~ ✅ **LIVRÉ v1 (2026-07-13)** :
   **NkBlendTree1D** (N clips sur un axe paramétrique, blend BONE-LOCAL TRS-NLerp par os
   AVANT le FK — correct sur les rotations — + phases synchronisées via temps normalisé
   sur durée interpolée) + **NkAnimStateMachine** (états clip/blend-tree, transitions
   par paramètres bool/float + seuil, crossfade, any-state) + helper partagé
   `NkBlendLocalTRS`. Validé : Fox Survey/Walk/Run en fondu continu (captures — galop
   plein à param 1.94, mix cohérent à 0.57) + self-test SM 3/3 (`NK_ANIM_SMTEST=1`).
   Demo : `NK_SKIN_MODEL=Resources/Models/Fox/Fox.glb --demo=16`.
   ✅ **v2 COMPLÈTE (2026-07-13)** : **NkBlendTree2D** (N clips à des points 2D,
   pondération inverse-distance Shepard p=2 + hit exact, blend bone-local CUMULATIF
   avant FK, phases synchro durée pondérée) ; **SM crossfade BONE-LOCAL** (les états
   clip/tree exposent leur pose locale pré-FK via GetLocalPose/GetSkeletonClip —
   blend TRS par os puis UN FK ; fallback matriciel sinon) ; **événements de
   transition** (`SetTransitionCallback(from, to, finished)` au déclenchement et à
   la fin du fondu). Self-tests 5/5 (`NK_ANIM_SMTEST=1` : SM 3/3 + events 2+2 +
   blend2D mix/exact). 9) Metal + Software.
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
- ⚠️ **PLAFOND ENCODEUR MESURÉ** : le H.264 CPU soutient ~10 fps en 720p.
  Défaut NK_RECORD_FPS = 10. ✅ Côté NKMedia (commit 0bbaabcb) : file
  **bornée** (maxQueuedFrames=32, drop-newest) + stats
  `QueueDepth()/DroppedFrames()/EncodeFps()` — plus de saturation RAM
  possible ; renderdemo auto-régule (saute l'échantillon si file ≥ 24) et
  logue les stats à l'arrêt. Reste (NKMedia) : mode MJPEG pour cadence haute.
- ✅ **Toggle à chaud + zone (2026-07-12)** — renderdemo : **touche INSER** (ex-F9, conflit demo 2)
  démarre/arrête l'enregistrement en cours de session (noms auto
  `nk_record_NNN.mp4`) ; `NK_RECORD_RECT=x,y,w,h` n'enregistre qu'une ZONE
  (crop CPU au push, w/h alignés 2, clamp fenêtre, arrêt propre au resize).
  Côté moteur tout est activable/désactivable au runtime
  (`SetFinalColorTargetMirror` ↔ handle nul). Doc :
  `wiki/Runtime/NKRenderer/Capture.md` + README racine.
- ✅ **V3 résolution d'export indépendante (2026-07-12)** : côté moteur
  `NkRenderer::SetRenderSizeOverride(w, h)` — rend TOUTE la 3D à la
  résolution demandée (RenderGraph/post-process/offscreen) SANS toucher le
  swapchain de la fenêtre (`ApplyRenderSize(touchDevice=false)`), la passe
  MirrorPresent fait le pont (viewport par-pass). renderdemo :
  `NK_RECORD_W/H` (ex. 3840×2160 natif pendant affichage 720p, alignés 2).
- ✅ **Finalisation MP4 asynchrone (2026-07-12)** : `recorder->End()` draine
  la file d'encodage (des secondes) — sur le thread de rendu ça FIGEAIT
  l'app au F9-stop. Fix renderdemo : l'affichage est restauré immédiatement,
  puis un `NkThread` dédié prend possession du recorder (heap NKMemory) et
  fait `End()+Delete` en fond. Pattern de référence documenté dans le wiki.
- ✅ **Qualité vidéo + anti-firefly (2026-07-12)** : `NK_RECORD_QP` (10..40,
  défaut 24 — plus bas = moins de blocs de compression) ; **Karis average**
  dans la 1re passe de bloom downsample (poids 1/(1+luma) par quad) — borne
  les fireflies spéculaires (métal roughness basse → lobe GGX en milliers)
  qui explosaient en rectangles violets géants dans les mips grossières
  (constaté en capture lossless DX11 + vidéo VK ; HDR source innocenté,
  max 22.25). Shaders : `PP_BloomDown/NkSL` + fallback VK synchronisé.
- ✅ **MJPEG + touche INSER (2026-07-12)** : `NK_RECORD_CODEC=mjpeg` +
  `NK_RECORD_MJPEG_Q` câblés sur l'API NKMedia 8c926507 (intra pur, 30-60 fps
  sans scintillement de blocs) ; toggle d'enregistrement déplacé **F9 → INSER**
  (F1-F12 toutes prises par les démos). Demo3D : panneau « feuillage »
  alpha-testé ajouté (disques troués, `SetCastShadowAlphaTest`) pour valider
  visuellement l'ombre trouée du pipeline Shadow_AlphaTest.
- ⏳ Reste : audio dans l'enregistrement.

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
- ✅ **Shadow override Layered/Toon/Anime** (2026-07-12) : ObjectUBO étendu
  (+shadowOverrides/+triplanarParams, identique dans les DEUX stages — le
  linker GL exige des déclarations de bloc identiques) dans toon/anime/
  layered .vert+.frag ; `SampleLightShadowEx(..., biasMul)` + garde
  receiveShadow (.x) câblés dans les 3 frags. LayeredV1 hors scope (ne
  sample pas d'ombres). C++ inchangé (ObjBlock déjà rempli pour tous).
- ✅ **Alpha-tested shadow** (foliage) (2026-07-12) : shaders
  `ShadowAlpha/NkSL` (VS pos+UV → FS sample tAlbedo, discard < 0.5) +
  pipeline `Shadow_AlphaTest` (layouts [global, object, GetInstanceLayout]
  → binde `matInst->GetDescSet()` tel quel) ; sélection par-caster dans
  RenderShadowPass quand `SetCastShadowAlphaTest(true)` (re-push PC après
  switch de PSO — DX12 invalide les root params). Piège résolu : le
  générateur GLSL injectait le flip Y NDC dès inputs+varyings → pragma
  commentaire **`@gl-no-flip-y`** (NkShaderBackend) pour les VS qui rendent
  dans l'atlas avec des varyings. **VALIDÉ visuellement (Rihen, 2026-07-12)** :
  panneau feuillage Demo3D → ombre à points (trous respectés) sur OpenGL.
  ⚠️ Piège corrigé au passage : `NkMaterial::Create(sys, "PBR")` échoue en
  silence (le template s'appelle `"Default_PBR"`) — préférer l'overload par
  TYPE (`NK_PBR_METALLIC`).
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

### Phase H.6 v1 — GI à UN REBOND ✅ (2026-07-30)
La grille de voxels porte désormais la **radiance réémise**, pas seulement
l'opacité : l'éclairage indirect vient de la géométrie réelle au lieu d'un
ambiant constant. Structure canonique du cone tracing (Crassin) —
`RGB` = radiance prémultipliée, `A` = opacité — donc **AO et GI partagent un
seul parcours de cônes** : l'AO ne coûte plus rien en plus.

- ✅ `NkVoxelOccluder::albedo` + `InjectLighting(lights)` / `InjectLightingIfDirty`
  (CPU) : éclairage direct par voxel × albédo, normale par **gradient d'opacité**,
  **visibilité par ray-march** dans la grille (c'est elle qui donne les ombres
  portées de l'indirect). `SetGIIntensity` applique le réglage à l'injection —
  aucun uniform ajouté au shader (registres DX contraints).
- ✅ `Include/NkVoxelAO.glsli` : `NkComputeVoxelGI()` (xyz = irradiance,
  w = AO) ; `NkComputeVoxelAO()` conservé pour Toon/Anime/Glass.
- ✅ Branché sur **Layered / LayeredV1** et sur **PBR** (`pbr.frag.nksl`).
- ✅ **MESURE (démo 8, mur rouge éclairé, aucune lumière rouge dans la scène)** :
  sur le flanc de la sphère qui fait face au mur, le rouge monte **1,9× plus vite
  que le bleu** (dR +45,6 contre dB +23,8 à intensité ×3), soit **+15,7 % de
  ratio R/B** ; à intensité 1, dR +18,7 contre dB +14,7. 58 % des pixels
  affectés. Le reste du gain est neutre : c'est le rebond du **sol clair**, qui
  domine légitimement celui du mur — comportement physique attendu, pas un défaut.
- ✅ Le mur de la scène de test est **réellement rendu** (cube sur l'AABB de
  l'occluder, bornes en **source unique** `kWallMin/kWallMax` pour qu'ils ne
  puissent pas diverger). Sans ce draw, la lumière rebondissait sur un mur
  invisible et l'effet semblait sortir de nulle part. ⚠️ Le mur est adossé en
  **+Z** : avec `yaw=0` la caméra orbite sur l'axe **X**, donc un mur en +X se
  planterait entre elle et la sphère.
  Non-régression : sans occluder injecté le terme est nul, rendu identique
  (démo 4 MAD 0,0014 pour un bruit run-à-run de 0,93). Debug + Release verts.
- 🔶 **DÉFAUT PRÉEXISTANT ISOLÉ — la texture voxel n'atteint pas le shader dans
  la démo 4** : ni le GI ni l'AO n'y produisent d'effet. Prouvé par deux mesures
  indépendantes : (1) un mur occluder massif (9064 voxels) ne change rien
  (MAD 0,0013) ; (2) la sonde `NK_GI_DEBUG_FILL=1` (grille entièrement saturée)
  ne change rien non plus (MAD 0,04), alors que la même sonde **sature l'écran
  en démo 8** (MAD 153). Le binding 27 ne remonte donc pas jusqu'au shader de
  cette démo — bug de binding antérieur au GI, à traiter à part.
- ⏳ Suite : injection en **compute** (l'interface et le shader ne bougeront pas),
  mips de la grille pour les cônes longue portée, bounds en uniform (aujourd'hui
  `NK_VOXEL_MIN/MAX_BOUNDS` est codé en dur et doit suivre `NkVoxelAOConfig`),
  et `giScale` synchronisé à la main entre CPU et `NK_VOXEL_GI_SCALE`.
- 🔧 Overrides : `NK_GI_TEST` (1 = scène de validation, 2 = témoin sans mur),
  `NK_GI_INTENSITY`, `NK_GI_DEBUG_FILL` (sonde de diagnostic).

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
- ✅ **Auto-exposure V1 — MESURE RÉELLE + ADAPTATION** (2026-07-30) : la V0
  échantillonnait **UN SEUL pixel** (le centre du RT de bloom) comme proxy du
  niveau de la scène → l'exposition était pilotée par ce qui se trouvait au
  milieu de l'écran, et le seuil de bloom écrasait les basses luminances.
  V1 : nouvelle passe `PP_AutoExposure` (`Resources/.../PP_AutoExposure/NkSL/`)
  qui calcule la **moyenne logarithmique** (moyenne géométrique, convention
  Reinhard) de la luminance sur **256 échantillons** de l'image HDR, pondérée
  vers le centre (métrage « center-weighted »), dans une cible **1×1 RGBA16F** ;
  **adaptation temporelle** façon accommodation de l'œil
  (`1 - exp(-dt·vitesse)`, indépendante du framerate) via **ping-pong de deux
  cibles 1×1 persistantes** (conservées à l'OnResize : sinon redimensionner
  provoquerait un flash). Le tonemap consomme la valeur au **binding 4**
  (PC 48→64 o, `p3 = (expMin, expMax)`).
  Config : `autoExposureSpeed/MinLuma/MaxLuma/MinExp/MaxExp` ; overrides de test
  `NK_AUTOEXP`, `NK_AUTOEXP_SPEED`, `NK_EXPOSURE`.
  Le shader n'a **aucune convention Y par backend** (la moyenne logarithmique
  est invariante à l'orientation) — contrairement au bloom et au tonemap.
  **MESURES (demo 2, luminance moyenne de la zone 3D, /255)** : référence
  exposition 1 sans auto = 98,1. Base **0,25** (sous-exposée) : 42,4 → **147,20**
  avec auto ; base **6** (surexposée, 81,7 % de pixels brûlés) : 204,9 →
  **147,17**. Soit **0,02 % d'écart entre deux bases distantes d'un facteur 24**
  = la boucle se ferme bien sur la mesure et non sur l'entrée.
  **Parité 4 backends** : GL 147,20 · Vulkan 147,19 · DX11 147,20 · DX12 147,19.
  Note d'usage : la cible 0,18 donne une image plus claire que l'exposition
  manuelle historique (147 vs 98) — baisser `autoExposureKey` vers ~0,10 pour
  retrouver le rendu d'avant.
  Reste V2 : réduction en **compute** (au lieu de 256 taps dans un fragment).
  ~~histogramme + percentiles~~ → **LIVRÉ le 2026-08-14** (`d226565f`), voir
  l'entrée ci-dessous : histogramme cumulé sur 16 seuils fixes en log-luminance,
  fenêtre [30ᵉ, 98ᵉ] percentile. *(Cette ligne annonçait encore le travail comme
  restant : corrigée le 15/08.)*
- ✅ **Exposition : la chaîne rendue cohérente** (2026-08-14 → 15) — quatre
  défauts distincts, tous mesurés, sur le trajet « ce qui est mesuré → ce qui
  est appliqué → ce qui est transmis » :
  1. l'exposition était appliquée **avant** l'ajout du bloom dans
     `pp_tonemap.frag.nksl` : aucune valeur d'exposition ne pouvait assombrir un
     halo (`d226565f`) ;
  2. `RunAutoExposure` mesurait `mainColor`, **avant** composition du bloom —
     elle ne voyait jamais le halo qu'elle amplifiait ensuite ;
  3. moyenne logarithmique → **histogramme de percentiles** [30ᵉ, 98ᵉ] : `log`
     diverge vers −∞ près de zéro, donc un décor sombre écrasait la mesure ;
  4. **le seuil de bloom s'ancre sur le blanc affiché** (`ca3f5fb8`) et non sur
     le HDR brut, via l'exposition résolue relevée par anneau (`3851e985`).
- ✅ **Exposition résolue : transmission réparée** (2026-08-15) — trois défauts
  liés, trouvés en mesurant le cas 4 (auto puis bloom, tout décocher, bloom
  seul) :
  1. `PumpExposureReadback` portait une branche « auto éteinte » **inatteignable**
     (son unique appelant, `RunAutoExposure`, retourne avant) : personne ne
     posait donc l'exposition manuelle. Réponse déplacée dans
     `ResolvedExposure()` — le point où la question est posée, seul valable hors
     frame, or le graphe se construit hors frame. Branche morte **retirée** ;
  2. `autoExposureStrength` manquait dans la liste de `SetPostConfig` qui salit
     le graphe : cocher l'auto ne créait aucune passe jusqu'au prochain
     redimensionnement. Seuil d'activation unifié en
     `NkPostConfig::kAutoExposureOn` + `AutoExposureRequested()` ;
  3. conséquence des deux : après extinction de l'auto, le seuil de bloom
     s'ancrait sur une exposition **héritée** — mesuré `bloomThr = 1,19` au lieu
     de `6,15` (0,85 × 7,24 / 5,15). Corrigé : `valid=1`, `aeExposure=1`.
  **Banc d'essai** : bloom rallumé sur les démos Sandbox cas 12 (DemoSkin) et 13
  (DemoIK), éteintes des mois durant à cause de ce défaut — plus aucun effet
  mesurable. Deux exécutions du même binaire diffèrent davantage (écart max 53)
  que bloom éteint contre allumé (49).
- ✅ **NkRHI compute audit** (2026-05-23) — compute support OK cross-API
  VK+GL (cf. `memory/nkrhi_compute_support.md`). Déjà utilisé par NkML,
  NkAnimationSystem morph, NkComputeContext wrapper. Foundation prête pour
  Phase N GPU prefilter, auto-exposure V1, Voxel AO v1, Lumen-lite GI.
- ✅ **TAA** (Temporal AA) livré V1 sur les 4 backends (2026-07-30) — détail et
  mesures dans « Phase L — Finition post-process » plus bas
- ❌ DOF/bokeh, Motion blur, vignette/grain chromatic, Lens flares
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
- ✅ **Shadow override Layered/Toon/Anime** (2026-07-12)
- ✅ **Alpha-tested shadow** (2026-07-12) : pipeline `ShadowAlpha` (VS+FS discard
  albedo < 0.5, set=2 universel des instances) branché sur `castShadowAlphaTest`.
  ✅ **Fix multi-backend (2026-07-22, 1ed4ea37)** — 2 causes racines DX :
  (1) POSITION (DX11+DX12) : les générateurs HLSL négatent Y du VS dès qu'il a
  inputs+varyings, mais ce VS rend dans l'atlas NON présenté → ombre déplacée ;
  pragma `@gl-no-flip-y` étendu aux 2 générateurs HLSL
  (`NkSLCompileOptions::disableAutoYFlip`, câblé NkShaderBackend).
  (2) TROUS absents (DX12) : `BuildGraphicsPSO` n'attachait le PS que si
  numRT>0 → en passe depth-only le FS discard ne tournait JAMAIS → ombre
  pleine ; PS attaché dès qu'il existe (discard-only légal avec 0 RTV).
  Validé capture DX11 = GL (position + trous) ; DX12 validé interactif.
- **LOD tile size** adaptatif : tile petit pour lights loin/dim, gros pour proches
- **Page-based VSM réel** UE5 (long terme, gros refactor 16k² atlas virtuel)

### Phase H.6 v1 — Voxel AO précision
- `.glsli` générique pour Layered/Toon/Anime (pas dupliquer le code)
- GPU bake voxel grid (CPU bake actuel = 1s sur startup)
- Densité voxel runtime adaptative (64³ → 128³ selon scene)
- 🔶 **GI à UN REBOND — v1 livrée côté moteur (2026-07-30), branchement PBR
  BLOQUÉ par un verrou.** La grille porte désormais, en plus de l'opacité, la
  **radiance réémise** par chaque voxel : structure canonique du voxel cone
  tracing (RGB = radiance prémultipliée, A = opacité) dans la texture RGBA8
  existante — donc **aucun binding ni format nouveau** (les registres DX sont
  déjà contraints, cf. [[project_dx_binding_model_plan]]).
  - `NkVoxelOccluder::albedo` + `InjectLighting(lights)` /
    `InjectLightingIfDirty(lights)` : pour chaque voxel occupé, éclairage direct
    (N·L par **gradient d'opacité** — une voxelisation AABB n'a pas de normale)
    × albédo / π, avec **visibilité par ray-march** dans la grille (c'est elle
    qui donne les ombres portées de l'indirect, donc son contraste).
  - `NkComputeVoxelGI()` : AO et GI dans **un seul** balayage de 4 cônes
    (accumulation front-to-back) — l'AO devient un produit dérivé, sans coût
    supplémentaire. `NkComputeVoxelAO()` conservé pour les matériaux qui ne
    veulent que l'AO.
  - Intensité appliquée **côté CPU à l'injection** (radiance nulle = GI éteint)
    plutôt qu'en uniform : zéro binding ajouté, et l'A/B de validation est exact.
  - **MESURE (demo 8, `NK_GI_TEST=1`, scène de color bleeding : mur ROUGE
    éclairé près de sphères)** : canal rouge moyen 20,95 → **24,34 (+16,2 %)**
    contre +5,9 % pour le bleu, alors qu'**aucune lumière rouge n'existe** dans
    la scène — l'écart R−B passe de −6,32 à −4,54. C'est le test canonique du
    GI : la teinte vient de la géométrie, pas d'un ambiant constant.
    A/B : `NK_GI_INTENSITY=0` vs `1`.
  - ⚠️ **BLOQUÉ** : le branchement dans le PBR (le matériau du gros des scènes)
    tombe dans `Resources/NKRenderer/Shaders/{Sel*,PBR}/**`, VERROUILLÉ par le
    chantier modélisation (`Engine/Noge/CONTINUATION.private.md`). Le GI est donc
    branché sur `Layered` et `LayeredV1` (libres) pour la validation. Pour
    l'activer sur le PBR une fois le verrou levé, dans `pbr.frag.nksl` remplacer
    `float voxAO = NkComputeVoxelAO(vWorldPos, N);` par
    `vec4 voxGI = NkComputeVoxelGI(vWorldPos, N); float voxAO = voxGI.w;`
    puis ajouter après le calcul de `amb` :
    `amb += kDi * albedo * voxGI.xyz * ao * uCam.iblStrength;`
    (`NkRender3D::BeginScene` étant lui aussi verrouillé, l'injection est
    déclenchée par l'APPLICATION — ce qui est de toute façon souhaitable tant
    qu'elle est en CPU : c'est la scène qui décide quand elle paie ce coût.)
  - Reste v2 : injection en **compute** (le calcul seul change, ni l'interface ni
    le shader), mips de la grille pour des cônes larges, bounds en uniform (ils
    sont encore en `#define` partagé entre CPU et shader), et plusieurs rebonds.

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
- ✅ `DrawSpriteGlow` **LIVRÉ (2026-07-12)** : batch dédié glow → pipeline
  Glow2D + PC au Flush ; au passage fix du descriptor set 2D partagé écrasé
  (pool 256 sets per-batch) — détail dans « Reste à faire priorisé » point 3

### Phase L — Finition post-process (TODO restants)
- **FXAA wirage RenderGraph** : pipeline créé, manque split tonemap→mToneTex
  + nouvelle pass FXAA→swapchain (~30 min refactor RenderGraph)
- ✅ **Auto-exposure** LIVRÉE V1 (2026-07-30) : mesure réelle (256 taps, moyenne
  logarithmique, cible 1×1) + adaptation temporelle, validée par mesures sur les
  4 backends — détail dans la section « Livré » Phase L ci-dessus
- ✅ **API SetColorGradingLUT(data, size)** LIVRÉE (2026-07-12) + vraie LUT 3D
  sur GL (validé capture `NK_LUT_TEST=1` teal&orange)
- ✅ **TAA** (Temporal AA) **LIVRÉ V1 sur les 4 backends** (2026-07-30) —
  opt-in (`postProcess.taa`, override `NK_TAA`), **exclusif du FXAA** (les
  enchaîner flouterait deux fois). Jitter sub-pixel Halton(2,3) 8 phases dans
  `NkRender3D` + reprojection par la profondeur + clamp de voisinage 3×3 contre
  le ghosting. Pas de velocity buffer (v2) : la reprojection n'est exacte que
  pour la géométrie statique, le clamp couvre les objets mobiles.
  **MESURES (demo 2, régime établi, zone 3D)** — indicateur d'**escalier**
  = part des pixels de bord au gradient purement axial, la signature de
  l'aliasing (référence → TAA) : GL 47,7 → **42,2 %** · Vulkan 46,5 → **41,7 %**
  · DX11 47,7 → **42,2 %** · DX12 47,6 → **42,4 %**, luminance préservée à
  ≤1,9 % près sur les 4. **L'AA vient bien de l'accumulation et non d'un flou** :
  le jitter SEUL (`NK_TAA_BLEND=0`) donne 53,7 %, soit PIRE que la référence.
  Trois défauts trouvés et corrigés par la mesure, tous silencieux (image
  d'apparence correcte) :
  1. une passe sans attachement ne fait pas transitionner les transients qu'elle
     lit → écran noir (cf. le piège en tête de ce fichier) ;
  2. `p0.y` servait à la fois de `yFlipUV` (VS) et de `ndcYSign` (FS), deux
     quantités qui divergent par backend → séparées en `p0.y` / `p1.x`
     (push-constant 80 → 96 o, toujours sous les 128 o Vulkan et DX12) ;
  3. la copie vers l'historique utilisait la convention du blit ÉCRAN au lieu de
     celle de la lecture off-screen → historique inversé sur DX, masqué par le
     clamp : luminance juste à 0,6 % près mais AUCUN antialiasing (escalier
     51,9 % au lieu de 42,2 %).
  Overrides de diagnostic : `NK_TAA_BLEND`, `NK_TAA_CLAMP=0` (le clamp masque
  les erreurs de reprojection), `NK_TAA_YFLIP` / `NK_TAA_NDCY` (conventions Y),
  `NK_TAA_DEBUG=1..4` (historique reprojeté / brut / décalage / profondeur),
  `NK_TAA_PREVLAG=N` (amplifie le mouvement inter-frame), `NK_TAA_PRESENT_HIST`.
  ⚠️ **`ndcYSign` NON validé par mesure** : à caméra fixe les deux signes
  s'annulent exactement (`ndcYSign² = 1` quand `reproj` = identité), donc aucune
  scène de démo actuelle ne les distingue. La valeur retenue suit le deferred
  lighting (validé par capture) ; à trancher sur une scène à caméra mobile via
  `NK_TAA_NDCY`.
  Reste V2 : velocity buffer (objets mobiles), variance clipping, mip bias.
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
- 🔄 **DETTE — six compensations d'un défaut désormais corrigé** (nommée le
  2026-08-15, retrait **non arbitré**). Le seuil de bloom s'appliquait sur le
  HDR brut : toute surface diffuse bien éclairée entrait dans le bright pass.
  Six fichiers ont compensé, sur des mois, **sans qu'aucun ne nomme la cause** :
  `Sandbox/src/Demo/main.cpp` cas 12 et 13 (`bloom = false`), `main.cpp:349` et
  `:368`, `DemoSkin.cpp:296` (soleil abaissé), `Demo11_FPSArena.cpp:492`
  (lumières retirées), `Demo3D.cpp:3011` (couleur choisie sous le seuil).
  Le banc d'essai du 15/08 montre que les cas 12 et 13 n'en ont **plus besoin**
  (aucun effet mesurable au-dessus du bruit inter-exécutions). Les quatre autres
  **n'ont pas été mesurés**. Ne pas retirer en bloc : chacun a été posé pour un
  symptôme qui lui est propre, et un seul de ces symptômes pourrait avoir une
  autre cause. Leçon associée : le commentaire du cas 12 décrivait exactement le
  défaut (« le bloom faisait BRILLER les textures saturées alors qu'elles ne
  reflètent pas la lumière ») des mois avant qu'il ne soit diagnostiqué —
  **une compensation documentée reste une cause non cherchée.**
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
- ~~**DX12 : DEVICE REMOVED 0x887A0001**~~ **RÉSOLU (2026-07-23, 269207c9)** —
  4 causes racines trouvées au debug layer : (1) root constants 64 o débordés
  par GridPC/Glow2D 96 o → root sig passée à 32 DWORDs (128 o) + clamp ;
  (2) release immédiat des PSO détruits en plein enregistrement → destruction
  DIFFÉRÉE générale datée par fence (pipelines/textures/buffers) ;
  (3) cache de variantes PSO `NkUnorderedMap<uint64,ComPtr>` imbriquée PERDAIT
  la valeur stockée (⚠ bug conteneur à isoler côté NKContainERS — map imbriquée
  déplacée + ComPtr) → variante PP_Tone reconstruite/relâchée CHAQUE frame →
  remplacée par `NkVector<NkPsoVariant>` ; (4) readback NK_CAPTURE :
  `CopyTextureToBuffer` passait RowPitch=0 au footprint (INVALID_CALL) → pitch
  serré aligné 256 + staging NkOffscreenTarget dimensionné/lu au pitch aligné.
  Validé : demo 2 DX12 zéro erreur, **capture NK_CAPTURE DX12 fonctionnelle**
  (pixels réels, ombre alpha-testée confirmée), deferred DX12 REND (409 FPS),
  non-régression GL/VK/DX11. PSO nommés (WKPDID) + env diag `NK_DX12_NODRAIN`.

---

## ❌ Restant priorité 2 — Qualité visuelle/perf

### Phase H — Texture pipeline
- ✅ Loader PNG/JPG/TGA/HDR via NkImage (existant)
- ✅ **Loader EXR** (audit 2026-07-12 : `NkEXRCodec` livré dans NKImage —
  la démo materials charge d'ailleurs `piazza_bologni_1k.exr`)
- ✅ **Mipmap generation** (audit 2026-07-12 : `NkIDevice::GenerateMipmaps`
  au RHI, utilisé par la chaîne matériaux — cf. fix mips DX12 2026-06-23)
- ❌ Compression BC1-7 (desktop) + ASTC + ETC2 (mobile)
- ✅ Texture streaming (LOD-mip selon distance) — **LIVRÉ 2026-07-13** via
  `NkStreamingSystem` v1+v2 (worker E/S réelles, low-res d'abord, raffinage par
  distance, démo `--demo=19`) — détail « Reste à faire priorisé » point 5
- ❌ Hot-reload des textures (les matériaux `.nkasset` l'ont, pas les textures)
- ❌ Atlasing pour batching

### Phase V — MONDES VOLUMINEUX : transposer les techniques prouvées en NKAI ❌

> **Décision de Rihen, 6 août 2026.** Les optimisations écrites pour faire tenir
> un modèle de langage de 7 milliards de paramètres dans 8 Go de VRAM
> (`Kernel/AI/NKInfer`, jalons QLoRA 4 et 5) **sont exactement celles qu'exigent
> les mondes 3D volumineux** — film d'animation, jeu, simulation. Ce ne sont pas
> des techniques voisines : ce sont les mêmes, appliquées à d'autres octets. À
> implémenter **dès que possible**, parce que tout ce qui vient après (scènes de
> production, mondes ouverts, plans de film) en dépend.

**Ce qui est DÉJÀ prouvé côté NKAI, et ce que ça donne ici :**

| Prouvé en NKAI (mesuré) | Transposition 3D | État |
|---|---|---|
| Poids **Q4_K/Q6_K résidents**, déquantifiés **dans le shader** — 36 Mo au lieu de 259, soit **7×** | Textures **BC1-7 / ASTC / ETC2** décompressées par l'échantillonneur ; positions, normales et UV **quantifiés** dans les tampons de sommets | ❌ (BC/ASTC déjà listés Phase H) |
| **`token_embd` jamais monté** : une ligne de 2 Ko lue au fichier par token, au lieu de 306 Mo en VRAM | **Streaming de géométrie** : ne réside que ce qui est visible ; le reste vit sur disque et arrive à la demande | 🔶 `NkStreamingSystem` le fait pour les **textures**, pas pour la géométrie |
| **Tuilage + mémoire partagée** dans le matmul : **×5** à M=256 (61 → 282 GFLOPS) | Tuilage du **culling** et du rendu par tuiles (clustered/tiled light culling, Phase M v3) — même raison, même gain | ❌ |
| **KV-cache** : ne jamais recalculer le passé | Caches de géométrie et d'ombres : ne pas recalculer ce qui n'a pas bougé | ✅ le principe existe (VSM, *dirty box* voxel) |

**À écrire (le vrai travail neuf) :**

- ❌ **Géométrie virtualisée** (façon Nanite) : niveaux de détail chargés selon
  la distance, groupes de triangles résidents à la demande. C'est le pendant
  exact du streaming de `token_embd` — on ne monte que ce qu'on regarde.
- ❌ **Atlas de textures virtuel** : un espace d'adressage de textures bien plus
  grand que la VRAM, dont seules les tuiles vues sont résidentes.
- ❌ **Hiérarchie spatiale** (BVH/octree de scène) pour décider quoi charger et
  quoi dessiner — le décideur dont dépendent les deux points précédents.
- ❌ **Quantification des attributs de sommets** au format GPU, sur le modèle des
  blocs Q4_K : lecture brute, décompression dans le shader.

**Nuance à ne pas perdre de vue** : en IA le goulot est la **bande passante
mémoire** (relire des gigaoctets de poids à chaque token) ; en 3D c'est plus
souvent le **nombre d'appels de dessin** et la **latence disque**. Les remèdes
se ressemblent, les priorités diffèrent — mesurer avant d'optimiser, comme le
jalon 4 l'a fait (avant/après à quatre tailles).

**Consommateurs visés** : Noge/Nogee (jeu, film d'animation, simulation) — voir
la section jumelle dans `Engine/Noge/ROADMAP.md`.

### Phase M — Forward+ / Deferred
- ✅ **Deferred v1+v2 LIVRÉ (2026-07-13)** : G-buffer MRT 3 RT + light pass
  fullscreen + ForwardRest, opt-in `cfg.deferred`/`NK_DEFERRED=1` — GL référence
  (91,8 % parité, 207 vs 140 FPS) + VULKAN validé capture ; DX11 fonctionnel
  (✅ rayons parasites du spot cookie RÉSOLUS 2026-07-23, a6c71299 — signe
  NDC Y par backend) ; DX12 ✅ device-removed RÉSOLU 2026-07-23 (269207c9) —
  **deferred v2 VALIDÉ SUR LES 4 BACKENDS GL/VK/DX11/DX12**, détail
  « Reste à faire priorisé » point 6 et « Bugs/quirks connus »
- ❌ v3 : tiled/clustered light culling (>32 lumières) + bench scène 100+ lights
- ❌ Forward+ (alternative tile-based si besoin)

---

## ❌ Priorité 3 — Animation & VFX

### Phase I (animation, ≠ Phase I IBL mirror) — Skeletal animation full
- ✅ Bone hierarchies + skin matrices (skinning GPU 4 backends, bones UBO)
- ✅ Playback : LINEAR/STEP/CUBICSPLINE glTF (additive à faire)
- ✅ Blend trees (1D + 2D Shepard) + state machines (crossfade bone-local +
  événements de transition) — **LIVRÉ v1+v2 2026-07-13**, points 8 du priorisé
- ✅ IK : FABRIK, CCD, two-bone (NkIKSystem, requalifié — c'est l'IK de NkAnima)
- ✅ Morph targets / blend shapes v1 CPU + skinnés (2026-07-13 ; v2 = GPU compute)
- ❌ Retargeting ; ❌ blend additif

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
