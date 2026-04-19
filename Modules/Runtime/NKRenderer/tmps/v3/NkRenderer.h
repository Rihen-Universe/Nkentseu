#pragma once
// =============================================================================
// NkRendererModule.h
// En-tête d'inclusion unique pour tout le système NkRenderer.
//
// Usage :
//   #include "NkRenderer/NkRendererModule.h"
//   using namespace nkentseu;
//
// Le système NkRenderer est la couche de rendu haut-niveau au-dessus du NKRHI.
// Il fournit :
//
//   ┌─────────────────────────────────────────────────────────────────────────┐
//   │  NkRenderer                                                             │
//   │  ├── NkTextureLibrary   — chargement PNG/HDR/KTX, render targets        │
//   │  ├── NkMeshCache        — primitives GPU, upload de géométrie           │
//   │  ├── NkMaterialSystem   — PBR, Toon, Archviz, Anime, Watercolor...      │
//   │  ├── NkRenderGraph      — ordonnancement des passes + barrières auto    │
//   │  ├── NkRender2D         — sprites, shapes, tilemap, texte               │
//   │  ├── NkRender3D         — meshes, lumières, ombres CSM, instancing      │
//   │  ├── NkPostProcessStack — SSAO, Bloom, DOF, FXAA, Tonemap, LUT...       │
//   │  ├── NkOverlayRenderer  — debug, gizmos, stats, HUD                    │
//   │  └── NkOffscreenTarget  — RTT, minimap, portals, shadow maps           │
//   └─────────────────────────────────────────────────────────────────────────┘
//
// Types de rendu supportés :
//   Réaliste    : PBR Métallique, PBR Spéculaire, Archviz, Skin, Hair, Glass
//   Stylisé     : Toon, Anime, Manga (Toon+Ink), Watercolor, Sketch, Pixel Art
//   Spéciaux    : Terrain multi-layer, Water FFT, Volume, Emissive, Car Paint
//   Debug       : Unlit, Wireframe, Normals, UV
//
// Pipelines de rendu :
//   Forward         — mobile/transparent, simple
//   Deferred        — many lights, opaque
//   Forward+        — clustered, best for mixed scenes (recommandé)
//   Tiled Deferred  — GPU uniforme
// =============================================================================

// ── Core ──────────────────────────────────────────────────────────────────────
#include "NKRenderer/Core/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkRendererImpl.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Core/NkMeshCache.h"
#include "NKRenderer/Core/NkRenderGraph.h"

// ── Materials ─────────────────────────────────────────────────────────────────
#include "NKRenderer/Materials/NkMaterialSystem.h"

// ── Tools : 2D ────────────────────────────────────────────────────────────────
#include "NKRenderer/Tools/Render2D/NkRender2D.h"

// ── Tools : 3D ────────────────────────────────────────────────────────────────
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

// ── Tools : Offscreen ─────────────────────────────────────────────────────────
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

// ── Tools : Post-process ──────────────────────────────────────────────────────
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"

// ── Tools : Overlay ───────────────────────────────────────────────────────────
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

// =============================================================================
// Résumé des préréglages disponibles
// =============================================================================
//
//  NkRendererConfig::ForGame(api, w, h)
//    → Forward+, ombres 2048, bloom, SSAO, FXAA, ACES
//
//  NkRendererConfig::ForFilm(api, w, h)
//    → Deferred, ombres 4096 soft, DOF bokeh, motion blur, TAA, HBAO, LUT
//
//  NkRendererConfig::ForArchviz(api, w, h)
//    → Deferred Ultra, SSR, HBAO, HDRI sky, color grading, no bloom
//
//  NkRendererConfig::ForMobile(api, w, h)
//    → Forward simple, pas d'ombre, pas de post-process
//
//  NkRendererConfig::For2D(api, w, h)
//    → Forward léger, sprites batch, pas de shadow/SSAO/DOF
//
// =============================================================================
// Exemple de démarrage minimal
// =============================================================================
//
//  // 1. Créer le renderer
//  auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_API_VULKAN, 1920, 1080);
//  NkRenderer* renderer = NkRenderer::Create(device, cfg);
//
//  // 2. Charger assets
//  auto* tex  = renderer->GetTextures();
//  auto* mats = renderer->GetMaterials();
//  auto* mesh = renderer->GetMeshCache();
//
//  NkTexId   albedo   = tex->Load("my_albedo.png");
//  NkMatId   mat      = mats->CreatePBR(albedo);
//  NkMeshId  sphere   = mesh->GetPrimitive(NkPrimitive::NK_ICOSPHERE);
//
//  // 3. Boucle de rendu
//  while (running) {
//      renderer->BeginFrame();
//
//      auto* r3d = renderer->GetRender3D();
//      r3d->SetCamera(camera);
//      r3d->SetSun({-0.3f,-1.f,-0.5f}, {1,0.95f,0.8f}, 5.f);
//      r3d->DrawMesh(sphere, NkTransform3D::Identity(), mat);
//
//      renderer->EndFrame();
//  }
//
//  // 4. Nettoyage
//  NkRenderer::Destroy(renderer);
//
// =============================================================================

#pragma once
// =============================================================================
// NkRendererModule.h  v3.0
// Point d'entrée unique pour tout le système NkRenderer.
//
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                    NKRENDERER SYSTEM v3.0                               ║
// ║                                                                         ║
// ║  BACKENDS SHADER SUPPORTÉS :                                            ║
// ║  ─ OpenGL GLSL     (#version 460 core, binding sans set)                ║
// ║  ─ Vulkan GLSL     (#version 460, set+binding, push_constant)           ║
// ║  ─ HLSL DX11 SM5   (cbuffer, Texture2D/Sampler séparés)                ║
// ║  ─ HLSL DX12 SM6   (Root Signature, Root Constants, DXC, bindless)     ║
// ║  ─ Metal MSL 2.x   ([[buffer]],[[texture]],[[sampler]] annotations)    ║
// ║  ─ Software C++    (NkSL → lambdas CPU)                                 ║
// ║                                                                         ║
// ║  SOUS-MESHES :                                                          ║
// ║  ─ NkMesh contient 1..N NkSubMesh (range d'indices + matériau)         ║
// ║  ─ Import GLTF → sous-meshes automatiques par primitives                ║
// ║  ─ NkMeshBuilder::BeginSubMesh/EndSubMesh pour construction manuelle    ║
// ║  ─ LOD par sous-mesh                                                    ║
// ║  ─ Culling par sous-mesh (AABB individuel)                              ║
// ║                                                                         ║
// ║  DOMAINES CIBLES :                                                      ║
// ║  ─ Animation 2D/3D      ─ Design 2D/3D     ─ Simulation IA             ║
// ║  ─ Archiviz             ─ Jeux 2D/3D       ─ Moteur de jeu             ║
// ║  ─ VFX 2D/3D            ─ Film d'animation ─ Simulation auto           ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// =============================================================================

// ── Core ──────────────────────────────────────────────────────────────────────
#include "Core/NkRendererConfig.h"
#include "Core/NkRendererImpl.h"
#include "Core/NkTextureLibrary.h"

// ── Shader System ─────────────────────────────────────────────────────────────
#include "Shader/NkShaderBackend.h"
#include "Shader/NkShaderCompiler.h"
#include "Shader/NkShaderLibrary.h"

// ── Mesh System ───────────────────────────────────────────────────────────────
#include "Mesh/NkMeshSystem.h"

// ── Materials ─────────────────────────────────────────────────────────────────
#include "Materials/NkMaterialSystem.h"

// ── Tools ─────────────────────────────────────────────────────────────────────
#include "Tools/Render2D/NkRender2D.h"
#include "Tools/Render3D/NkRender3D.h"
#include "Tools/Offscreen/NkOffscreenTarget.h"
#include "Tools/PostProcess/NkPostProcessStack.h"
#include "Tools/Overlay/NkOverlayRenderer.h"
#include "Tools/Particles/NkParticleSystem.h"
#include "Tools/Animation/NkAnimationSystem.h"
#include "Tools/Simulation/NkSimulationRenderer.h"
#include "Tools/VFX/NkVFXSystem.h"

// =============================================================================
// GUIDE DE DÉMARRAGE RAPIDE
// =============================================================================
//
// ── 1. Création du renderer ────────────────────────────────────────────────
//   auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_API_VULKAN, 1920, 1080);
//   NkRenderer* rend = NkRenderer::Create(device, cfg);
//
// ── 2. Shader custom (NkShaderBackendSources — supporte GL/VK/DX11/DX12/MSL)
//   NkShaderBackendSources src;
//   src.name        = "MyShader";
//   src.glslVertSrc = "...";   // OpenGL GLSL
//   src.vkVertSrc   = "...";   // Vulkan GLSL
//   src.dx11VertSrc = "...";   // HLSL DX11
//   src.dx12VertSrc = "...";   // HLSL DX12 SM6
//   src.mslVertSrc  = "...";   // Metal MSL
//   auto result = rend->GetShaderCompiler()->Compile(src); // auto-detect
//
// ── 3. Mesh avec sous-meshes ───────────────────────────────────────────────
//   NkVertexLayoutBuilder lb;
//   lb.AddPosition3F().AddNormal3F().AddUV2F();
//   NkMeshBuilder mb(lb.Build());
//
//   mb.BeginSubMesh("Body", matBody);
//   // ... vertices + indices ...
//   mb.EndSubMesh();
//
//   mb.BeginSubMesh("Glass", matGlass);
//   // ... vertices + indices ...
//   mb.EndSubMesh();
//
//   NkMeshId meshId = rend->GetMeshSystem()->Upload(mb, "MyMesh");
//   // Rendu par sous-mesh :
//   meshSys.BindMesh(cmd, meshId);
//   for(uint32 s=0; s<meshSys.GetSubMeshCount(meshId); ++s)
//       meshSys.DrawSubMesh(cmd, meshId, s);
//
// ── 4. Génération de code shader depuis layout ─────────────────────────────
//   NkString glsl = lb.GenerateGLSLInputs(460);      // OpenGL/Vulkan
//   NkString hlsl = lb.GenerateHLSLStruct(true);     // DX11/DX12
//   NkString msl  = lb.GenerateMSLStruct(true);      // Metal
//
// ── 5. Boucle de rendu ────────────────────────────────────────────────────
//   while (running) {
//       rend->BeginFrame();
//       rend->GetRender3D()->SetCamera(camera);
//       rend->GetRender3D()->DrawMesh(meshId, transform, matId);
//       rend->EndFrame();
//   }
//   NkRenderer::Destroy(rend);
//
// =============================================================================
// SHADER BACKENDS — DIFFÉRENCES CLÉS
// =============================================================================
//
//  OpenGL GLSL :
//    layout(binding=N, std140) uniform Block { ... };   // pas de set
//    layout(binding=N) uniform sampler2D tex;           // sampler global
//    out vec4 fragColor;  layout(location=0) out préférable pour MRT
//    sampler2DShadow pour PCF hardware (textureProj())
//
//  Vulkan GLSL :
//    layout(set=0, binding=0, std140) uniform Block { ... };  // set obligatoire
//    layout(push_constant) uniform PC { mat4 model; ... };    // push constants
//    layout(location=0) out — obligatoire sur tout
//    gl_Position.y : Vulkan Y vers le bas — utiliser NkViewport.flipY=true
//    NDC Z : [0,1] (pas [-1,1] comme OpenGL)
//
//  HLSL DX11 SM5 :
//    cbuffer Frame : register(b1) { float4x4 VP; }    // cbuffer par register
//    Texture2D tex : register(t0);                     // texture séparée du sampler
//    SamplerState s : register(s0);                    // sampler séparé
//    float4 PSMain(...) : SV_Target0                  // semantic sur le retour
//    mul(vec, mat) → vecteur-ligne × matrice           // attention à l'ordre!
//
//  HLSL DX12 SM6 :
//    [RootSignature("...")] sur les fonctions
//    ConstantBuffer<T> pc : register(b0) → root constants (push constants)
//    SM6.6 : ResourceDescriptorHeap[index] → bindless
//    DXC obligatoire (pas FXC)
//    SamplerComparisonState pour PCF natif
//
//  Metal MSL :
//    vertex V2F vmain(VIn v [[stage_in]],
//        constant Frame& f [[buffer(8)]]) { ... }
//    fragment float4 fmain(...,
//        texture2d<float> tex [[texture(0)]],
//        sampler s [[sampler(0)]]) { ... }
//    tex.sample(s, uv)            // pas de texture() comme GLSL
//    A * B (operator*)            // pas de mul() comme HLSL
//    float4x4 : column-major comme GLSL
//    NDC Z : [0,1] comme DX12
//
// =============================================================================
// SOUS-MESHES — DÉTAIL
// =============================================================================
//
//  NkSubMesh {
//    name         : nom du sous-mesh (ex: "Body", "Glass", "Hair")
//    firstIndex   : offset dans l'index buffer
//    indexCount   : nombre d'indices
//    vertexOffset : base vertex (pour DrawIndexed avec vertex offset)
//    material     : matériau par défaut (peut être overridé)
//    bounds       : AABB pour le culling individuel
//    visible      : activer/désactiver ce sous-mesh
//    castShadow   : cast shadow pour ce sous-mesh uniquement
//    lods[]       : niveaux de détail alternatifs
//  }
//
//  Usage courant (personnage) :
//    submesh[0] "Body"    → PBR métallique opaque
//    submesh[1] "Eyes"    → Unlit avec texture de pupilles
//    submesh[2] "Hair"    → double-sided + alpha blend
//    submesh[3] "Clothes" → PBR + alpha mask
//    submesh[4] "Glasses" → verre transparent
//
//  Usage terrain sectionné :
//    submesh[0..N] → zones de terrain avec matériaux différents
// =============================================================================
