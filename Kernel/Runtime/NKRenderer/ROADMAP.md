# NKRenderer — Roadmap

État actuel (mai 2026) : Phases A → D.3d livrées. PBR + IBL CPU + CSM 1-cascade
stable + soft shadows PCF Poisson + ring buffer UBO + post-process ACES tonemap.
Demo3D fonctionnelle (orbite, 16 sphères PBR colorées + cube doré + plane sol +
shadows soft + 2 lights point + 1 directional).

## En cours / pending immédiat

- **D.3d.1** PCSS contact-hardening (fix `tShadowMapRaw` qui retourne -1)
- **D.3c.1** CSM stabilization scenes ouvertes (cascade overlap, radius quantization)
- **E** Materials 2D + lumière 2D + ombres 2D
- **F** Test multi-backend (Vulkan, DX11, DX12, Metal, Software)

---

## Priorité 1 — Compléter les fondations

### Phase G — NkMaterialSystem (asset-based)
- `NkMaterialAsset` : shader + textures (albedo/normal/ORM/emissive) + params PBR
- `NkMaterialInstance` : override per-mesh (tint, parameters)
- Hot-reload `Resources/NKRenderer/Materials/*.mat` (JSON ou via NKReflection)
- Built-in : PBR, Toon/Anime, Glass, Skin, Hair, CarPaint, Cloth, Foliage, Volume,
  Water, Particles (16 dossiers déjà préparés)
- Wire `NkDrawCall3D::material` proprement (actuellement shortcut via
  metallic/roughness directs)

### Phase G.ext — Matériaux avancés style UE5 (M.1 → M.8)

> Sous-phases issues de la session 2026-05-11. S'appliquent après que Phase G de base
> soit opérationnelle (NkMaterialSystem + pipeline PBR/Toon/Anime/Unlit fonctionnels ✅).

- **M.1** ⭐ Material Layering — N couches superposées par objet avec masques (texture,
  vertex color). Shader généré à la volée. `NkLayeredMaterial` wrapper + UBO étendu
  `NkLayerParams[N]`. Inclut la 2ème passe outline (hull expansion).
- **M.2** Material Parameter Collections — `NkMaterialCollection` : pool de params
  nommés globaux (float, vec3…). Plusieurs matériaux y référent → un seul changement
  met à jour tous. UBO dédié set=0.
- **M.3** Blend par vertex color — R/G/B/A du vertex comme masques de blend entre
  2-4 matériaux. Dépend de M.1 pour le pipeline de blend.
- **M.4** Instances hiérarchiques — `NkMaterial::CreateInstance(parent)`, héritage +
  override sélectif via bitfield `mOverrides`. Propagation parent → enfants.
- **M.5** Material Functions — `.glsli` réutilisables dans les shaders custom
  (`NkFresnelFunc`, `NkToonRamp`, `NkPOM`…). Inclus via `#include`.
- **M.6** Vertex Paint runtime — `mesh->PaintVertex(idx, color)`, update partiel VBO.
  Dépend de NkDynamicMesh ou mécanisme de partial VBO update.
- **M.7** Decal Materials — pass post-opaque, projection OBB → world space → blend.
  **Bloqué** jusqu'à Phase M (deferred) ou ajout d'un mini G-Buffer depth+normal.
- **M.8** Multi-slot par sous-mesh — `NkDrawCall3D::materials[]` par slot de
  sous-mesh. NkRender3D itère les sous-meshes et bind le bon matériau à chaque
  DrawIndexed.

### Phase H — Pipeline assets textures
- Loader PNG/JPG/TGA/HDR/EXR (stb_image / OpenEXR)
- Mipmap generation
- Compression BC1-7 + ASTC + ETC2 (mobile)
- Texture streaming (LOD-mip selon distance)
- Hot-reload des textures
- Tableau atlasing pour batching

---

## Priorité 2 — Qualité visuelle 3D

### Phase L — Post-process avancé
- DOF avec bokeh
- Motion blur (object + camera)
- TAA (temporal AA + jittered projection)
- Color grading + LUT
- Vignette, grain, chromatic aberration
- Lens flares
- Bloom dual-Kawase complet (skeleton déjà dans NkPostConfig)

### Phase N — IBL pipeline GPU
- Compute shader prefilter (remplace le CPU D.2d)
- Loader HDR/EXR pour skyboxes réels
- Rendering skybox proprement
- Env light probes (plusieurs sources, blend par zone)
- Reflection probes (cubemap par pièce/zone)

### Phase M — Forward+ / Deferred
- Forward actuel = lights uniformes par drawcall
- Forward+ : compute light culling tile-based (>32 lumières)
- Ou Deferred : GBuffer + light pass (beaucoup de petites lights)
- Bench scène 100+ lights

---

## Priorité 3 — Animation & VFX

### Phase I — Skeletal animation full
- Bone hierarchies + skin matrices SSBO (squelette dans NkAnimationSystem)
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

## Priorité 4 — Avancé

- **Phase K** — Volumétrique : fog, god rays, clouds raymarched, volume textures, SSS amélioré
- **Phase O** — Caméras avancées : multi-cam (split-screen, PiP), cinema, VR/stéréoscopique
- **Phase P** — Scene graph + culling : hierarchy complète (interfaces D.5 prêtes), frustum culling, HiZ occlusion, LOD auto, instancing
- **Phase Q** — Editor integration : gizmos translate/rotate/scale, selection outline, stats graph, profiler frame
- **Phase R** — Raytracing hardware : Vulkan KHR_ray_tracing + DXR, RT shadows/reflections/GI, hybride rasterization+RT
- **Phase S** — GPU-driven : indirect rendering, bindless, mesh shaders, GPU culling, virtual textures (megatexture style id Tech)

---

## Minimum viable UE5-like

Après E/F : **G + H + L + N + I** suffisent pour un renderer complet. Le reste
(K/O/P/Q/R/S) devient progressivement spécialisé selon l'usage cible (jeu temps
réel vs cinema vs editor vs VR).
