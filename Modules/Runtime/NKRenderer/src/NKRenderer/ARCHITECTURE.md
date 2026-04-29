# NKRenderer v4.0 — Architecture complète

## Vue d'ensemble

NKRenderer est un système de rendu **haut niveau** construit au-dessus de NKRHI.
Il vise les mêmes domaines qu'Unreal Engine ou UPBGE : film d'animation, jeu vidéo,
temps réel, archiviz, modélisation 3D, PV3DE (Patient Virtuel 3D Émotif).

---

## Règles architecturales fondamentales

| Règle | Description |
|-------|-------------|
| **Séparation stricte** | NKRenderer ne connaît PAS : NkScene, ECS, NkApplication |
| **Seul lien plateforme** | `NkSurfaceDesc` (depuis NKWindow/Core/NkSurface.h) passé à `NkRenderer::Create()` |
| **Resize** | L'application écoute `NkGraphicsContextResizeEvent` → appelle `renderer->OnResize(w,h)` |
| **Handles distincts** | Handles RHI dans `nkentseu::` — Handles Renderer dans `nkentseu::renderer::` |
| **Bibliothèques optionnelles** | NKFont, NKImage, NKUI sont des backends par défaut, remplaçables via callbacks |

---

## Structure des fichiers

```
NKRenderer/
├── NkRenderer.h                   ← Façade pure (interface abstraite)
├── CMakeLists.txt                 ← Build system
│
├── Core/
│   ├── NkRendererTypes.h          ← Handles, vertex layouts, AABB, lights, draw calls, stats
│   ├── NkRendererConfig.h         ← Configuration + 7 presets (ForGame/ForFilm/ForArchviz/...)
│   ├── NkCamera.h/.cpp            ← Camera3D (frustum culling, UBO GPU) + Camera2D
│   ├── NkRenderGraph.h/.cpp       ← Ordonnancement passes GPU, barrières auto, DOT debug
│   ├── NkRendererImpl.h/.cpp      ← Implémentation concrète, possède tous les sous-systèmes
│   └── NkTextureLibrary.h/.cpp    ← Cache textures, NKImage backend, BRDF LUT, built-ins
│
├── Shader/
│   ├── NkShaderBackend.h/.cpp     ← GL/VK/DX11/DX12/MSL/NkSL — compile + documentation diffs
│   └── NkShaderLibrary.h/.cpp     ← Cache programmes, hot-reload polling
│
├── Mesh/
│   └── NkMeshSystem.h/.cpp        ← Sous-meshes, LOD, primitives built-in, VertexLayout
│
├── Materials/
│   └── NkMaterialSystem.h/.cpp    ← Template+Instance, 20+ modèles PBR/NPR/Debug/Custom
│
├── Tools/
│   ├── Render2D/                  ← Batching, sprites, 9-slice, shapes, gradients, bezier
│   ├── Render3D/                  ← PBR, frustum culling, instancing, skinning, debug gizmos
│   ├── Text/                      ← NkTextRenderer (NKFont+custom backend, SDF, extrusion 3D)
│   ├── Shadow/                    ← CSM directionnel, PCF/PCSS, atlas multi-cascade
│   ├── PostProcess/               ← SSAO, Bloom Karis, ACES tonemap, FXAA, TAA, SSR, DOF
│   ├── Overlay/                   ← HUD debug, stats, show-texture
│   ├── Offscreen/                 ← RTT, readback CPU, resize, multi-target
│   ├── VFX/                       ← Particules CPU/GPU, trails ribbon, decals projetés
│   ├── Animation/                 ← Skinning GPU, onion skinning, squelette debug, morph targets
│   └── Simulation/                ← NkSimulationRenderer — PV3DE, foule, blend shapes FACS
│
└── Shaders/
    ├── PBR/
    │   ├── NkSL/pbr.nksl          ← Source maître NkSL (transpile vers tous backends)
    │   ├── GL/pbr.vert + pbr.frag ← GLSL 4.60 OpenGL (IBL, 32 lumières, shadows)
    │   └── VK/  DX11/  DX12/  MSL/ (à compléter par backend)
    ├── Toon/GL/                   ← Stepped shadows, outline, rim light
    ├── Shadow/GL/                 ← Depth-only CSM avec slope-scale bias
    ├── PostProcess/GL/            ← fullscreen.vert, tonemap_aces, bloom_down, fxaa, ssao
    ├── Particles/GL/              ← particle.vert + particle.geom (billboard expansion)
    └── Debug/GL/                  ← sprite2d.vert + sprite2d.frag
```

---

## Cycle de vie

```cpp
// 1. Créer via preset
auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_OPENGL, 1920, 1080);
NkRenderer* renderer = NkRenderer::Create(device, surfaceDesc, cfg);

// 2. Boucle de rendu
while (running) {
    renderer->BeginFrame();
      // 3D
      r3d->BeginScene(ctx);
      r3d->Submit(drawCall);
      // 2D
      r2d->Begin(cmd, w, h);
      r2d->DrawSprite(...);
      r2d->End();
      // Text
      txt->DrawText(...);
    renderer->EndFrame();
    renderer->Present();
}

// 3. Resize (depuis NkGraphicsContextResizeEvent)
renderer->OnResize(newW, newH);

// 4. Shutdown
NkRenderer::Destroy(renderer);
```

---

## Presets disponibles

| Preset | Pipeline | Qualité | Post-FX activés |
|--------|----------|---------|-----------------|
| `ForGame()` | Forward+ | High | Bloom, SSAO, FXAA, ACES |
| `ForFilm()` | Deferred | Cinematic | Tout : DOF, MotionBlur, TAA, SSR, PCSS |
| `ForArchviz()` | Deferred | Ultra | HBAO, SSR, TAA, Color Grading |
| `ForMobile()` | Forward | Mobile | Aucun — performance max |
| `For2D()` | Forward | Low | Aucun — rendu 2D pur |
| `ForEditor()` | Forward+ | High | FXAA, debug overlay |
| `ForOffscreen()` | Deferred | Cinematic | Export image haute qualité |

---

## Modèles de matériaux

### Réaliste
`PBR_METALLIC` `PBR_SPECULAR` `ARCHIVIZ` `SKIN (SSS)` `HAIR (Kajiya-Kay)`
`GLASS` `CLOTH` `CAR_PAINT` `FOLIAGE` `WATER` `TERRAIN` `EMISSIVE` `VOLUME`

### Stylisé / NPR
`TOON` `TOON_INK (manga)` `ANIME` `WATERCOLOR` `SKETCH` `PIXEL_ART` `FLAT` `UPBGE_EEVEE`

### Debug / Utilitaire
`UNLIT` `DEBUG_NORMALS` `DEBUG_UV` `WIREFRAME` `DEBUG_DEPTH` `DEBUG_AO`

### Custom
`CUSTOM` — shader NkSL fourni par l'utilisateur, transpilé vers tous les backends

---

## NkSL — Shader Language

NkSL est un GLSL-like avec extensions multi-backend :

```nksl
@uniform 0 CameraUBO { mat4 viewProj; vec4 camPos; float time; }
@texture(3) tAlbedo;

@vertex {
    @in vec3 aPos : 0;  @out vec3 vPos : 0;
    void main() {
        @target GL  { gl_Position = viewProj * vec4(aPos, 1.0); }
        @target VK  { gl_Position = viewProj * vec4(aPos, 1.0); gl_Position.y *= -1; }
        @target DX11{ SV_Position = mul(viewProj, float4(aPos, 1.0)); }
    }
}
@fragment {
    @in vec3 vPos : 0;  @out vec4 fragColor : 0;
    void main() { fragColor = texture(tAlbedo, vPos.xy); }
}
```

Transpile automatiquement vers : `GL 4.60` · `Vulkan GLSL → SPIR-V` · `HLSL SM5` · `HLSL SM6` · `MSL 2.x`

---

## Intégration NKWindow / NKEvent

```cpp
// NKRenderer n'inclut JAMAIS NKWindow ni NKEvent directement.
// La seule surface d'interface est NkSurfaceDesc.

// Exemple d'intégration applicative :
auto& evSys = NkEventSystem::GetInstance();
evSys.Subscribe<NkGraphicsContextResizeEvent>([&](const auto& e) {
    renderer->OnResize(e.width, e.height);
});

// NKFont : chargement police avec backend default
NkFontHandle font = renderer->GetTextRenderer()->LoadFont("Roboto.ttf", 20.f);
// NKFont custom backend (FreeType, DirectWrite...) :
renderer->GetTextRenderer()->LoadFontCustom("Roboto.ttf", 20.f, myCustomLoader);

// NKImage : chargeur custom (stb_image, WIC, etc.) :
renderer->GetTextures()->SetCustomLoader(myLoadFn, myFreeFn, userData);
```

---

## Futures couches (architecture cible)

```
Applications (jeu · film · archiviz · modelisation · PV3DE)
      ↓
NkScene / ECS         ← module futur (entités, composants, scene graph)
      ↓
NKRenderer v4         ← CE MODULE
      ↓
NKRHI                 ← couche RHI bas niveau (NkIDevice, NkICommandBuffer)
      ↓
GPU (OpenGL · Vulkan · DX11 · DX12 · Metal · Software)
```

---

## Démos incluses

| # | Nom | Fonctionnalités démontrées |
|---|-----|--------------------------|
| 01 | Basic3D | PBR, IBL, ombres CSM, lumières multiples |
| 02 | Game2D | Sprites, tilemaps, texte SDF, UI 9-slice, formes |
| 03 | Film | DOF, motion blur, SSR, color grading, PCSS |
| 04 | Archviz | HBAO, SSR, matériaux archiviz, verre |
| 05 | Animation | Skinning GPU, onion skinning, squelette debug |
| 06 | VFX | Particules feu/fumée/étincelles, trail ribbon, decal |
| 07 | Offscreen | Minimap RTT, portail, rendu multi-target |
| 08 | PV3DE | Patient Virtuel 3D Émotif, blend shapes FACS, larmes |
| 09 | MultiViewport | Éditeur 4 vues (top/front/side/persp) |
| 10 | NkSL | Shader custom NkSL wave + iridescent |
