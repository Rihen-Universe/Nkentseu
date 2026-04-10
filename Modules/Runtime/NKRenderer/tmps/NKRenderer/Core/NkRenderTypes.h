#pragma once
// =============================================================================
// NkRenderTypes.h
// Types fondamentaux du NKRenderer — partagés par tous les sous-systèmes.
//
// Contenu :
//   - NkRendererHandle<Tag>       Identifiants opaques fortement typés
//   - NkColor4f               Couleur RGBA float + utilités
//   - NkVertex3D / NkVertex2D Vertex interleaved avec layout RHI
//   - NkRendererResult        Codes de retour
//   - NkPrimType2D            Primitives 2D
//   - NkMeshTopology          Topologies de mesh
//   - NkProjectionSpace       Convention NDC (Vulkan/DX vs OpenGL)
//   - NkRenderMode            Modes de visualisation
//   - NkBlendMode             Modes de transparence
//   - NkShadingModel          Modèles d'éclairage / matériaux
//   - NkRenderTargetDesc      Description d'une surface de rendu offscreen
//   - NkRendererStats         Statistiques de frame
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

        // =====================================================================
        // NkRendererHandle<Tag>
        // Identifiant opaque fortement typé — chaque ressource renderer a son
        // propre type d'ID, incompatible avec les handles RHI bas-niveau.
        // =====================================================================
        template<typename Tag>
        struct NkRendererHandle {
            uint64 id = 0;
            bool   IsValid()    const noexcept { return id != 0; }
            bool   operator==(const NkRendererHandle& o) const noexcept { return id == o.id; }
            bool   operator!=(const NkRendererHandle& o) const noexcept { return id != o.id; }
            static NkRendererHandle Null() noexcept { return {0}; }
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

        using NkTexture2DID    = NkRendererHandle<TagTexture2D>;
        using NkTextureCubeID  = NkRendererHandle<TagTextureCube>;
        using NkTexture3DID    = NkRendererHandle<TagTexture3D>;
        using NkMaterialID     = NkRendererHandle<TagMaterial>;
        using NkMeshID         = NkRendererHandle<TagMesh>;
        using NkShaderAssetID  = NkRendererHandle<TagShaderAsset>;
        using NkRenderTargetID = NkRendererHandle<TagRenderTarget>;
        using NkFontID         = NkRendererHandle<TagFont>;
        using NkSpriteID       = NkRendererHandle<TagSprite>;
        using NkParticleID     = NkRendererHandle<TagParticle>;

        // =====================================================================
        // NkColor4f
        // Couleur RGBA linéaire [0, 1]. Stockage float32 x4.
        // =====================================================================
        struct NkColor4f {
            float r = 1.f, g = 1.f, b = 1.f, a = 1.f;

            NkColor4f() = default;
            constexpr NkColor4f(float r, float g, float b, float a = 1.f)
                : r(r), g(g), b(b), a(a) {}

            // ── Couleurs prédéfinies ──────────────────────────────────────────
            static constexpr NkColor4f White()       { return {1,1,1,1}; }
            static constexpr NkColor4f Black()       { return {0,0,0,1}; }
            static constexpr NkColor4f Red()         { return {1,0,0,1}; }
            static constexpr NkColor4f Green()       { return {0,1,0,1}; }
            static constexpr NkColor4f Blue()        { return {0,0,1,1}; }
            static constexpr NkColor4f Yellow()      { return {1,1,0,1}; }
            static constexpr NkColor4f Cyan()        { return {0,1,1,1}; }
            static constexpr NkColor4f Magenta()     { return {1,0,1,1}; }
            static constexpr NkColor4f Orange()      { return {1,0.5f,0,1}; }
            static constexpr NkColor4f Purple()      { return {0.5f,0,1,1}; }
            static constexpr NkColor4f Gray()        { return {0.5f,0.5f,0.5f,1}; }
            static constexpr NkColor4f Transparent() { return {0,0,0,0}; }

            // ── Conversion depuis entier ARGB hex (0xAARRGGBB) ────────────────
            static NkColor4f FromHex(uint32 hex) noexcept {
                return {
                    ((hex >> 16) & 0xFF) / 255.f,
                    ((hex >>  8) & 0xFF) / 255.f,
                    ( hex        & 0xFF) / 255.f,
                    ((hex >> 24) & 0xFF) / 255.f
                };
            }

            // ── Conversion sRGB ──────────────────────────────────────────────
            // sRGB → linear (approximation gamma 2.2)
            static float SRGBToLinear(float c) noexcept {
                return (c <= 0.04045f) ? (c / 12.92f) : ((c + 0.055f) / 1.055f * (c + 0.055f) / 1.055f);
            }
            static float LinearToSRGB(float c) noexcept {
                c = (c < 0.f) ? 0.f : (c > 1.f ? 1.f : c);
                return (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * c * 0.416667f - 0.055f);
            }
            NkColor4f ToLinear()  const noexcept { return {SRGBToLinear(r), SRGBToLinear(g), SRGBToLinear(b), a}; }
            NkColor4f ToSRGB()    const noexcept { return {LinearToSRGB(r), LinearToSRGB(g), LinearToSRGB(b), a}; }

            // ── Opérateurs ───────────────────────────────────────────────────
            NkColor4f operator*(float s)             const noexcept { return {r*s, g*s, b*s, a*s}; }
            NkColor4f operator*(const NkColor4f& o)  const noexcept { return {r*o.r, g*o.g, b*o.b, a*o.a}; }
            NkColor4f operator+(const NkColor4f& o)  const noexcept { return {r+o.r, g+o.g, b+o.b, a+o.a}; }
            NkColor4f operator-(const NkColor4f& o)  const noexcept { return {r-o.r, g-o.g, b-o.b, a-o.a}; }
            bool      operator==(const NkColor4f& o) const noexcept { return r==o.r&&g==o.g&&b==o.b&&a==o.a; }
            bool      operator!=(const NkColor4f& o) const noexcept { return !(*this==o); }

            static NkColor4f Lerp(const NkColor4f& a, const NkColor4f& b, float t) noexcept {
                return a + (b - a) * t;
            }

            // ── Conversion vers NkVec4f ─────────────────────────────────────
            NkVec4f ToVec4() const noexcept { return {r, g, b, a}; }
        };

        // =====================================================================
        // NkVertex3D
        // Vertex 3D interleaved universel — sous-systèmes 3D utilisent un sous-ensemble.
        // Layout GPU std140-compatible, toujours envoyé en entier au RHI.
        // =====================================================================
        struct NkVertex3D {
            NkVec3f   position  = {0,0,0};
            NkVec3f   normal    = {0,1,0};
            NkVec3f   tangent   = {1,0,0};
            NkVec3f   bitangent = {0,0,1};
            NkVec2f   uv0       = {0,0};       // Canal UV primaire
            NkVec2f   uv1       = {0,0};       // Canal UV secondaire (lightmap)
            NkColor4f color     = {1,1,1,1};
            NkVec4f   joints    = {0,0,0,0};   // Skeletal: indices de joints
            NkVec4f   weights   = {1,0,0,0};   // Skeletal: poids de blend

            static NkVertexLayout GetLayout() {
                NkVertexLayout l;
                uint32 off = 0;
                l.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,    off, "POSITION",     0); off += 12;
                l.AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT,    off, "NORMAL",       0); off += 12;
                l.AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT,    off, "TANGENT",      0); off += 12;
                l.AddAttribute(3, 0, NkGPUFormat::NK_RGB32_FLOAT,    off, "BITANGENT",    0); off += 12;
                l.AddAttribute(4, 0, NkGPUFormat::NK_RG32_FLOAT,     off, "TEXCOORD",     0); off += 8;
                l.AddAttribute(5, 0, NkGPUFormat::NK_RG32_FLOAT,     off, "TEXCOORD",     1); off += 8;
                l.AddAttribute(6, 0, NkGPUFormat::NK_RGBA32_FLOAT,   off, "COLOR",        0); off += 16;
                l.AddAttribute(7, 0, NkGPUFormat::NK_RGBA32_FLOAT,   off, "BLENDINDICES", 0); off += 16;
                l.AddAttribute(8, 0, NkGPUFormat::NK_RGBA32_FLOAT,   off, "BLENDWEIGHT",  0); off += 16;
                l.AddBinding(0, sizeof(NkVertex3D));
                return l;
            }
        };

        // =====================================================================
        // NkVertex2D
        // Vertex 2D interleaved pour le renderer 2D (sprites, UI, texte).
        // =====================================================================
        struct NkVertex2D {
            NkVec2f   position = {0,0};
            NkVec2f   uv       = {0,0};
            NkColor4f color    = {1,1,1,1};

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

        // =====================================================================
        // NkRendererResult — codes de retour uniformes
        // =====================================================================
        enum class NkRendererResult : uint8 {
            NK_OK = 0,
            NK_NOT_INITIALIZED,
            NK_INVALID_PARAM,
            NK_OUT_OF_MEMORY,
            NK_GPU_ERROR,
            NK_ALREADY_EXISTS,
            
            NK_ERROR_INVALID_DEVICE,
            NK_ERROR_INVALID_HANDLE,
            NK_ERROR_COMPILE_FAILED,
            NK_ERROR_OUT_OF_MEMORY,
            NK_ERROR_NOT_SUPPORTED,
            NK_ERROR_IO,

            NK_NOT_FOUND,
            NK_ERROR_UNKNOWN,
        };

        inline bool NkRenderOk(NkRendererResult r) noexcept {
            return r == NkRendererResult::NK_OK;
        }

        // =====================================================================
        // NkPrimType2D — formes géométriques 2D
        // =====================================================================
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

        // =====================================================================
        // NkMeshTopology — topologie GPU
        // =====================================================================
        enum class NkMeshTopology : uint32 {
            NK_TRIANGLES,
            NK_TRIANGLE_STRIP,
            NK_LINES,
            NK_LINE_STRIP,
            NK_POINTS,
            NK_QUADS,
            NK_PATCHES
        };

        // =====================================================================
        // NkProjectionSpace — convention de l'espace NDC
        //   NK_NDC_ZERO_ONE  → Vulkan / DX  (z ∈ [0, 1])
        //   NK_NDC_NEG_ONE   → OpenGL       (z ∈ [-1, 1])
        // =====================================================================
        enum class NkProjectionSpace : uint8 {
            NK_NDC_ZERO_ONE,
            NK_NDC_NEG_ONE,
        };

        // =====================================================================
        // NkRenderMode — mode de visualisation (solide, wireframe, debug, etc.)
        // =====================================================================
        enum class NkRenderMode : uint32 {
            NK_SOLID,              // Rendu PBR / Phong normal
            NK_WIREFRAME,          // Fil de fer
            NK_POINTS,             // Nuage de points
            NK_NORMALS,            // Visualiser les normales
            NK_UV,                 // Visualiser les canaux UV
            NK_VERTEX_COLOR,       // Visualiser les couleurs vertex
            NK_ALBEDO,             // Albedo brut sans éclairage
            NK_DEPTH,              // Depth buffer
            NK_AMBIENT_OCCLUSION,  // Occlusion ambiante
            NK_ROUGHNESS,          // Canal roughness
            NK_METALLIC,           // Canal metallic
            NK_EMISSIVE,           // Canal émission
            NK_SOLID_FLAT,         // Éclairage plat (Blender-like)
            NK_MATERIAL,           // Matcap preview
            NK_RENDERED,           // Rendu complet IBL
        };

        // =====================================================================
        // NkBlendMode — mode de transparence / composition
        // =====================================================================
        enum class NkBlendMode : uint32 {
            NK_OPAQUE,
            NK_MASKED,        // Alpha test (cutout)
            NK_TRANSLUCENT,   // Alpha blend standard
            NK_ADDITIVE,
            NK_MULTIPLY,
            NK_SCREEN,
            NK_PREMULTIPLIED
        };

        // =====================================================================
        // NkShadingModel — modèle d'éclairage du matériau (UE5-like)
        // =====================================================================
        enum class NkShadingModel : uint32 {
            // ── PBR physique ─────────────────────────────────────────────────
            NK_DEFAULT_LIT,           // PBR Metallic/Roughness (UE5)
            NK_SUBSURFACE,            // SSS — peau, cire, végétation
            NK_CLEAR_COAT,            // Vernis bicouche — voitures, bois laqué
            NK_TWO_SIDED_FOLIAGE,     // Feuillage double-face
            NK_HAIR,                  // Cheveux (Marschner)
            NK_CLOTH,                 // Tissu (Ashikhmin-Shirley)
            NK_EYE,                   // Yeux (cornée + iris + sclérotique)
            NK_SINGLE_LAYER_WATER,    // Eau de surface (réfraction, caustique)
            NK_THIN_TRANSLUCENT,      // Verre fin / papier
            // ── Non photoréaliste ─────────────────────────────────────────────
            NK_UNLIT,                 // Pas d'éclairage
            NK_TOON,                  // Cel shading
            // ── Film / Modélisation ──────────────────────────────────────────
            NK_PBR_SPEC_GLOSS,        // PBR Specular/Glossiness
            NK_PRINCIPLED,            // Disney BSDF complet
            NK_VOLUME_BSDF,           // Volumes (fumée, feu, SSS complexe)
            NK_GLASS_BSDF,            // Verre physique (Fresnel, IoR)
            NK_METAL_BSDF,            // Métal conducteur complexe
            NK_PHONG,                 // Phong classique (legacy)
        };

        // =====================================================================
        // NkRenderTargetDesc — description d'une surface de rendu offscreen.
        // Utilisée par NkRenderTarget, G-Buffer, shadow maps, post-process.
        // =====================================================================
        struct NkRenderTargetDesc {
            uint32      width       = 1280;
            uint32      height      = 720;
            NkGPUFormat colorFormat = NkGPUFormat::NK_RGBA16_FLOAT;
            NkGPUFormat depthFormat = NkGPUFormat::NK_D32_FLOAT;
            uint32      samples     = 1;
            bool        hasMips     = false;
            const char* debugName   = nullptr;
        };

        // =====================================================================
        // NkRendererStats — statistiques de rendu par frame (profiling, HUD)
        // =====================================================================
        struct NkRendererStats {
            uint32 drawCalls       = 0;
            uint32 triangles       = 0;
            uint32 vertices        = 0;
            uint32 instances       = 0;
            uint32 pipelineChanges = 0;
            uint32 materialChanges = 0;
            uint32 textureBinds    = 0;
            uint32 shadowCasters   = 0;
            uint64 gpuMemoryBytes  = 0;
            float  cpuTimeMs       = 0.f;
            float  gpuTimeMs       = 0.f;

            void Reset() noexcept { *this = {}; }
        };

    } // namespace renderer
} // namespace nkentseu
