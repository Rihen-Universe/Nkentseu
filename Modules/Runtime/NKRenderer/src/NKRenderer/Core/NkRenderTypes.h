#pragma once
// =============================================================================
// NkRenderTypes.h
// Types fondamentaux du NKRenderer — partagés par tous les sous-systèmes.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

#ifndef NKRENDERER_API
#define NKRENDERER_API
#endif

namespace nkentseu {
    namespace renderer {

        using namespace math;

        // =============================================================================
        // Identifiants opaques du renderer (séparés des handles RHI bas niveau)
        // =============================================================================
        template<typename Tag>
        struct NkRendererID {
            uint64 id = 0;
            bool IsValid() const { return id != 0; }
            bool operator==(const NkRendererID& o) const { return id == o.id; }
            bool operator!=(const NkRendererID& o) const { return id != o.id; }
            static NkRendererID Null() { return {0}; }
        };

        struct TagTexture2D    {};
        struct TagTextureCube  {};
        struct TagTexture3D    {};
        struct TagMaterial     {};
        struct TagMesh         {};
        struct TagShaderAsset  {};
        struct TagRenderTarget {};
        struct TagFont         {};
        struct TagSprite       {};
        struct TagParticle     {};

        using NkTexture2DID    = NkRendererID<TagTexture2D>;
        using NkTextureCubeID  = NkRendererID<TagTextureCube>;
        using NkTexture3DID    = NkRendererID<TagTexture3D>;
        using NkMaterialID     = NkRendererID<TagMaterial>;
        using NkMeshID         = NkRendererID<TagMesh>;
        using NkShaderAssetID  = NkRendererID<TagShaderAsset>;
        using NkRenderTargetID = NkRendererID<TagRenderTarget>;
        using NkFontID         = NkRendererID<TagFont>;
        using NkSpriteID       = NkRendererID<TagSprite>;

        // =============================================================================
        // Couleur RGBA float
        // =============================================================================
        struct NkColor4f {
            float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
            NkColor4f() = default;
            NkColor4f(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}
            static NkColor4f White()   { return {1,1,1,1}; }
            static NkColor4f Black()   { return {0,0,0,1}; }
            static NkColor4f Red()     { return {1,0,0,1}; }
            static NkColor4f Green()   { return {0,1,0,1}; }
            static NkColor4f Blue()    { return {0,0,1,1}; }
            static NkColor4f Yellow()  { return {1,1,0,1}; }
            static NkColor4f Cyan()    { return {0,1,1,1}; }
            static NkColor4f Magenta() { return {1,0,1,1}; }
            static NkColor4f Transparent() { return {0,0,0,0}; }
            static NkColor4f FromHex(uint32 hex) {
                return {
                    ((hex>>16)&0xFF)/255.f,
                    ((hex>>8)&0xFF)/255.f,
                    (hex&0xFF)/255.f,
                    ((hex>>24)&0xFF)/255.f
                };
            }
            NkColor4f operator*(float s) const { return {r*s,g*s,b*s,a*s}; }
            NkColor4f operator+(const NkColor4f& o) const { return {r+o.r,g+o.g,b+o.b,a+o.a}; }
        };

        // =============================================================================
        // Vertex universel du NKRenderer (interleaved, std layout)
        // Chaque sous-système (2D, 3D, Text, etc.) utilise un sous-ensemble
        // =============================================================================
        struct NkVertex3D {
            NkVec3f  position  = {0,0,0};
            NkVec3f  normal    = {0,1,0};
            NkVec3f  tangent   = {1,0,0};
            NkVec3f  bitangent = {0,0,1};
            NkVec2f  uv0       = {0,0};     // canal UV primaire
            NkVec2f  uv1       = {0,0};     // canal UV secondaire (lightmap)
            NkColor4f color    = {1,1,1,1};
            NkVec4f  joints    = {0,0,0,0}; // skeletal animation: joint indices
            NkVec4f  weights   = {1,0,0,0}; // skeletal animation: blend weights

            static NkVertexLayout GetLayout() {
                NkVertexLayout l;
                uint32 off = 0;
                l.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  off, "POSITION",  0); off += 12;
                l.AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT,  off, "NORMAL",    0); off += 12;
                l.AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT,  off, "TANGENT",   0); off += 12;
                l.AddAttribute(3, 0, NkGPUFormat::NK_RGB32_FLOAT,  off, "BITANGENT", 0); off += 12;
                l.AddAttribute(4, 0, NkGPUFormat::NK_RG32_FLOAT,   off, "TEXCOORD",  0); off += 8;
                l.AddAttribute(5, 0, NkGPUFormat::NK_RG32_FLOAT,   off, "TEXCOORD",  1); off += 8;
                l.AddAttribute(6, 0, NkGPUFormat::NK_RGBA32_FLOAT, off, "COLOR",     0); off += 16;
                l.AddAttribute(7, 0, NkGPUFormat::NK_RGBA32_FLOAT, off, "BLENDINDICES",0); off += 16;
                l.AddAttribute(8, 0, NkGPUFormat::NK_RGBA32_FLOAT, off, "BLENDWEIGHT",0); off += 16;
                l.AddBinding(0, sizeof(NkVertex3D));
                return l;
            }
        };

        struct NkVertex2D {
            NkVec2f  position = {0,0};
            NkVec2f  uv       = {0,0};
            NkColor4f color   = {1,1,1,1};

            static NkVertexLayout GetLayout() {
                NkVertexLayout l;
                uint32 off = 0;
                l.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,   off, "POSITION", 0); off += 8;
                l.AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   off, "TEXCOORD", 0); off += 8;
                l.AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, off, "COLOR",    0);
                l.AddBinding(0, sizeof(NkVertex2D));
                return l;
            }
        };

        // ── Résultat d'une opération renderer ─────────────────────────────────────
        enum class NkRendererResult : uint8 {
            NK_OK = 0,
            NK_NOT_INITIALIZED,
            NK_INVALID_PARAM,
            NK_OUT_OF_MEMORY,
            NK_GPU_ERROR,
        };

        inline bool NkRenderOk(NkRendererResult r) { return r == NkRendererResult::NK_OK; }

        // =============================================================================
        // Primitive types 2D
        // =============================================================================
        enum class NkPrimType2D : uint32 {
            NK_POINT,
            NK_LINE,
            NK_TRIANGLE,
            NK_QUAD,
            NK_CIRCLE,
            NK_ELLIPSE,
            NK_POLYGON,
            NK_ROUNDED_RECT,
            NK_ARC
        };

        // =============================================================================
        // Topology
        // =============================================================================
        enum class NkMeshTopology : uint32 {
            NK_TRIANGLES,
            NK_TRIANGLE_STRIP,
            NK_LINES,
            NK_LINE_STRIP,
            NK_POINTS,
            NK_QUADS,
            NK_PATCHES
        };

        // =============================================================================
        // Viewport/Camera
        // =============================================================================

        // ── Espace de projection ──────────────────────────────────────────────────
        enum class NkProjectionSpace : uint8 {
            NK_NDC_ZERO_ONE,  // Vulkan / DX (z ∈ [0,1])
            NK_NDC_NEG_ONE,   // OpenGL      (z ∈ [-1,1])
        };

        // =============================================================================
        // Éclairage
        // =============================================================================
        enum class NkLightType : uint32 {
            NK_DIRECTIONAL,
            NK_POINT,
            NK_SPOT,
            NK_AREA,
            NK_SKY     // IBL / HDRI ambient
        };

        struct NkLight {
            NkLightType type       = NkLightType::NK_DIRECTIONAL;
            NkVec3f     position   = {0, 10, 0};
            NkVec3f     direction  = {0,-1, 0};
            NkColor4f   color      = {1,1,1,1};
            float       intensity  = 1.f;
            float       range      = 10.f;         // Point/Spot
            float       innerCone  = 30.f;         // Spot inner (degrés)
            float       outerCone  = 45.f;         // Spot outer (degrés)
            bool        castShadow = true;
            uint32      shadowMapSize = 2048;
            // Area light
            NkVec2f     areaSize   = {1.f, 1.f};
        };

        // =============================================================================
        // Render Target Description
        // =============================================================================
        struct NkRenderTargetDesc {
            uint32      width       = 1280;
            uint32      height      = 720;
            NkGPUFormat colorFormat = NkGPUFormat::NK_RGBA16_FLOAT;
            NkGPUFormat depthFormat = NkGPUFormat::NK_D32_FLOAT;
            uint32      samples     = 1;
            bool        hasMips     = false;
            const char* debugName   = nullptr;
        };

        // =============================================================================
        // Statistiques de rendu par frame
        // =============================================================================
        struct NkRendererStats {
            uint32 drawCalls        = 0;
            uint32 triangles        = 0;
            uint32 vertices         = 0;
            uint32 instances        = 0;
            uint32 pipelineChanges  = 0;
            uint32 materialChanges  = 0;
            uint32 textureBinds     = 0;
            uint32 shadowCasters    = 0;
            uint64 gpuMemoryBytes   = 0;
            float  cpuTimeMs        = 0.f;
            float  gpuTimeMs        = 0.f;
        };

        // =============================================================================
        // Mode de rendu
        // =============================================================================
        enum class NkRenderMode : uint32 {
            NK_SOLID,             // Rendu normal PBR/Phong
            NK_WIREFRAME,         // Fil de fer
            NK_POINTS,            // Nuage de points
            NK_NORMALS,           // Visualiser les normales
            NK_UV,                // Visualiser les UVs
            NK_VERTEX_COLOR,      // Visualiser les couleurs vertex
            NK_ALBEDO,            // Albedo brut sans éclairage
            NK_DEPTH,             // Depth buffer
            NK_AMBIENT_OCCLUSION,
            NK_ROUGHNESS,
            NK_METALLIC,
            NK_EMISSIVE,
            // Modes Blender-like
            NK_SOLID_FLAT,        // Solid avec éclairage plat
            NK_MATERIAL,          // Matcap preview
            NK_RENDERED          // Rendu complet avec IBL
        };

        // =============================================================================
        // Blend mode
        // =============================================================================
        enum class NkBlendMode : uint32 {
            NK_OPAQUE,
            NK_MASKED,           // Alpha test (cutout)
            NK_TRANSLUCENT,      // Alpha blend
            NK_ADDITIVE,
            NK_MULTIPLY,
            NK_SCREEN,
            NK_PREMULTIPLIED
        };

        // =============================================================================
        // Shading model (pour les matériaux)
        // =============================================================================
        enum class NkShadingModel : uint32 {
            // Jeux vidéo (PBR)
            NK_DEFAULT_LIT,           // PBR Metallic/Roughness (UE-like)
            NK_SUBSURFACE,            // SSS pour la peau / végétation
            NK_CLEAR_COAT,            // Vernis (voitures)
            NK_TWO_SIDED_FOLIAGE,     // Feuillage
            NK_HAIR,                  // Cheveux (Marschner)
            NK_CLOTH,                 // Tissu (Ashikhmin)
            NK_EYE,                   // Yeux
            NK_SINGLE_LAYER_WATER,    // Eau
            NK_THIN_TRANSLUCENT,      // Verre fin
            // Non-photoréaliste
            NK_UNLIT,                 // Pas d'éclairage
            NK_TOON,                  // Cel shading
            // Application modélisation / film
            NK_PBR_SPEC_GLOSS,        // PBR Specular/Glossiness (UPBGE, Blender Principled)
            NK_PRINCIPLED,            // Disney BSDF Principled
            NK_VOLUME_BSDF,           // Volumes (fumée, feu, SSS complexe)
            NK_GLASS_BSDF,            // Verre physique
            NK_METAL_BSDF,            // Métal conducteur
            NK_PHONG                 // Phong classique (legacy)
        };

    } // namespace renderer
} // namespace nkentseu
