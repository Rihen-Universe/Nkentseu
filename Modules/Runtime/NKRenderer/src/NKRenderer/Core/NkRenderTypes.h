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

// =============================================================================
// Primitive types 2D
// =============================================================================
enum class NkPrimType2D : uint32 {
    Point, Line, Triangle, Quad, Circle, Ellipse, Polygon, RoundedRect, Arc
};

// =============================================================================
// Topology
// =============================================================================
enum class NkMeshTopology : uint32 {
    Triangles, TriangleStrip, Lines, LineStrip, Points, Quads, Patches
};

// =============================================================================
// Viewport/Camera
// =============================================================================
struct NkCamera {
    NkVec3f  position   = {0, 0, 5};
    NkVec3f  target     = {0, 0, 0};
    NkVec3f  up         = {0, 1, 0};
    float    fovDeg     = 60.f;
    float    nearPlane  = 0.01f;
    float    farPlane   = 1000.f;
    float    orthoSize  = 5.f;
    bool     isOrtho    = false;

    NkMat4f GetView() const {
        return NkMat4f::LookAt(position, target, up);
    }
    NkMat4f GetProjection(float aspect) const {
        if (isOrtho)
            return NkMat4f::Orthogonal(
                NkVec2f(-orthoSize * aspect, -orthoSize),
                NkVec2f( orthoSize * aspect,  orthoSize),
                nearPlane, farPlane, true);
        return NkMat4f::Perspective(NkAngle(fovDeg), aspect, nearPlane, farPlane);
    }
};

// =============================================================================
// Éclairage
// =============================================================================
enum class NkLightType : uint32 {
    Directional,
    Point,
    Spot,
    Area,
    Sky     // IBL / HDRI ambient
};

struct NkLight {
    NkLightType type       = NkLightType::Directional;
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
    Solid,           // Rendu normal PBR/Phong
    Wireframe,       // Fil de fer
    Points,          // Nuage de points
    Normals,         // Visualiser les normales
    UV,              // Visualiser les UVs
    VertexColor,     // Visualiser les couleurs vertex
    Albedo,          // Albedo brut sans éclairage
    Depth,           // Depth buffer
    AmbientOcclusion,
    Roughness,
    Metallic,
    Emissive,
    // Modes Blender-like
    SolidFlat,       // Solid avec éclairage plat
    Material,        // Matcap preview
    Rendered,        // Rendu complet avec IBL
};

// =============================================================================
// Blend mode
// =============================================================================
enum class NkBlendMode : uint32 {
    Opaque,
    Masked,          // Alpha test (cutout)
    Translucent,     // Alpha blend
    Additive,
    Multiply,
    Screen,
    PreMultiplied,
};

// =============================================================================
// Shading model (pour les matériaux)
// =============================================================================
enum class NkShadingModel : uint32 {
    // Jeux vidéo (PBR)
    DefaultLit,           // PBR Metallic/Roughness (UE-like)
    Subsurface,           // SSS pour la peau / végétation
    ClearCoat,            // Vernis (voitures)
    TwoSidedFoliage,      // Feuillage
    Hair,                 // Cheveux (Marschner)
    Cloth,                // Tissu (Ashikhmin)
    Eye,                  // Yeux
    SingleLayerWater,     // Eau
    ThinTranslucent,      // Verre fin
    // Non-photoréaliste
    Unlit,                // Pas d'éclairage
    Toon,                 // Cel shading
    // Application modélisation / film
    PBR_Spec_Gloss,       // PBR Specular/Glossiness (UPBGE, Blender Principled)
    Principled,           // Disney BSDF Principled
    VolumeBSDF,           // Volumes (fumée, feu, SSS complexe)
    GlassBSDF,            // Verre physique
    MetalBSDF,            // Métal conducteur
    Phong,                // Phong classique (legacy)
};

} // namespace renderer
} // namespace nkentseu
