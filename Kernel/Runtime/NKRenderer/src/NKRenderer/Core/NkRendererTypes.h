#pragma once
// =============================================================================
// NkRendererTypes.h  — NKRenderer v5.0  (Core/)
//
// Types fondamentaux du module renderer.
//
// Conventions :
//   - namespace nkentseu::renderer  (pas nkentseu:: directement)
//   - handles RHI (NKRHI)  : nkentseu::NkBufferHandle, NkTextureHandle, ...
//   - handles renderer     : nkentseu::renderer::NkTexHandle, NkMeshHandle, ...
//     -> Wrappers haut-niveau au-dessus du RHI : ajoutent reference-counting,
//        nommage, cycle de vie unifie (la TextureLibrary mappe NkTexHandle
//        vers NkTextureHandle interne).
//
// Layout vertex :
//   - NkVertex2D / NkVertex3D / NkVertexSkinned / NkVertexParticle / NkVertexDebug
//   - tous packs dans des structs POD avec offsets stables (pas d'union)
//
// Layout GPU (UBO/SSBO) :
//   - structs alignes 16 octets (std140 / std430 / DX12-compatible)
//   - prefixe "NkXxxGPU" pour les structs miroirs GPU
//
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        // =====================================================================
        // SECTION A — Handles renderer (haut niveau)
        // =====================================================================
        template<typename Tag>
        struct NkRendHandle {
            uint64 id = 0;
            constexpr bool IsValid()                         const noexcept { return id != 0; }
            constexpr bool operator==(const NkRendHandle& o) const noexcept { return id == o.id; }
            constexpr bool operator!=(const NkRendHandle& o) const noexcept { return id != o.id; }
            static constexpr NkRendHandle Null()                   noexcept { return {0}; }
        };

        struct TagRTexture  {};
        struct TagRMesh     {};
        struct TagRShader   {};
        struct TagRMaterial {};
        struct TagRMatInst  {};
        struct TagRFont     {};
        struct TagRTarget   {};
        struct TagREnvMap   {};   // env cubemap (HDR -> radiance)
        struct TagRIBL      {};   // jeu prefiltre (irradiance + specular + BRDF)
        struct TagRSkeleton {};
        struct TagRAnimClip {};

        using NkTexHandle      = NkRendHandle<TagRTexture>;
        using NkMeshHandle     = NkRendHandle<TagRMesh>;
        using NkShaderHandle   = NkRendHandle<TagRShader>;
        using NkMatHandle      = NkRendHandle<TagRMaterial>;
        using NkMatInstHandle  = NkRendHandle<TagRMatInst>;
        using NkFontHandle     = NkRendHandle<TagRFont>;
        using NkTargetHandle   = NkRendHandle<TagRTarget>;
        using NkEnvMapHandle   = NkRendHandle<TagREnvMap>;
        using NkIBLHandle      = NkRendHandle<TagRIBL>;
        using NkSkeletonHandle = NkRendHandle<TagRSkeleton>;
        using NkAnimClipHandle = NkRendHandle<TagRAnimClip>;

        // =====================================================================
        // SECTION B — Blend / Render queue
        // =====================================================================
        enum class NkBlendMode : uint8 {
            NK_OPAQUE    = 0,
            NK_ALPHA     = 1,    // src.a * src + (1-src.a) * dst
            NK_ADDITIVE  = 2,    // src + dst
            NK_MULTIPLY  = 3,    // src * dst
            NK_PREMULT   = 4,    // src + (1-src.a) * dst
            NK_SCREEN    = 5,    // 1 - (1-src) * (1-dst)
        };

        // Render queue — controle le tri et l'ordre de submission.
        // Utilise par Render3D (tri opaque/transparent), Materials (template),
        // Culling (filtrage), VFX (overlay).
        enum class NkRenderQueue : uint8 {
            NK_BACKGROUND  = 0,     // skybox, env (premier)
            NK_OPAQUE      = 100,   // PBR opaque (front-to-back)
            NK_ALPHA_TEST  = 150,   // alpha cutout / foliage
            NK_TRANSPARENT = 200,   // glass, particles (back-to-front)
            NK_OVERLAY     = 250,   // UI, debug (dernier)
        };

        // ViewMode — debug renderer (consomme par NkRender3D & overlay).
        enum class NkViewMode : uint8 {
            NK_SOLID   = 0,
            NK_WIREFRAME,
            NK_NORMALS,
            NK_UV,
            NK_DEPTH,
            NK_AO,
            NK_UNLIT,
        };

        // =====================================================================
        // SECTION C — Vertex layouts
        // =====================================================================
        struct NkVertex2D {
            NkVec2f  pos;
            NkVec2f  uv;
            uint32   color;     // RGBA8 packed
        };
        static_assert(sizeof(NkVertex2D) == 20, "NkVertex2D layout");

        struct NkVertex3D {
            NkVec3f  pos;
            NkVec3f  normal;
            NkVec3f  tangent;   // .w (signe bitangent) en vec4 si besoin
            NkVec2f  uv;
            NkVec2f  uv2;
            uint32   color;
        };

        struct NkVertexSkinned : NkVertex3D {
            uint8    boneIdx[4]    = {0,0,0,0};
            float32  boneWeight[4] = {1.f,0.f,0.f,0.f};
        };

        struct NkVertexDebug {
            NkVec3f  pos;
            uint32   color;
        };

        struct NkVertexParticle {
            NkVec3f  pos;
            NkVec2f  uv;
            uint32   color;
            float32  size;
            float32  rotation;
        };

        // =====================================================================
        // SECTION D — Geometrie (AABB, sphere, plan, frustum)
        // =====================================================================
        struct NkAABB {
            NkVec3f min = { 1e30f,  1e30f,  1e30f};
            NkVec3f max = {-1e30f, -1e30f, -1e30f};

            constexpr NkVec3f Center()  const noexcept { return {(min.x+max.x)*0.5f,(min.y+max.y)*0.5f,(min.z+max.z)*0.5f}; }
            constexpr NkVec3f Extents() const noexcept { return {(max.x-min.x)*0.5f,(max.y-min.y)*0.5f,(max.z-min.z)*0.5f}; }
            constexpr NkVec3f Size()    const noexcept { return {(max.x-min.x),     (max.y-min.y),     (max.z-min.z)};     }

            void Expand(NkVec3f p) noexcept {
                if(p.x<min.x) min.x=p.x; if(p.y<min.y) min.y=p.y; if(p.z<min.z) min.z=p.z;
                if(p.x>max.x) max.x=p.x; if(p.y>max.y) max.y=p.y; if(p.z>max.z) max.z=p.z;
            }
            void Merge(const NkAABB& o) noexcept { Expand(o.min); Expand(o.max); }

            bool Contains(NkVec3f p) const noexcept {
                return p.x>=min.x && p.x<=max.x
                    && p.y>=min.y && p.y<=max.y
                    && p.z>=min.z && p.z<=max.z;
            }

            // Transforme l'AABB par une matrice 4x4 et recalcule un AABB englobant.
            // Utile pour culling apres world-matrix.
            NkAABB Transformed(const NkMat4f& m) const noexcept {
                NkAABB out = NkAABB::Empty();
                NkVec3f c = Center();
                NkVec3f e = Extents();
                // 8 coins -> on prend min/max apres transform
                for (int i = 0; i < 8; i++) {
                    NkVec3f corner = {
                        c.x + ((i & 1) ? e.x : -e.x),
                        c.y + ((i & 2) ? e.y : -e.y),
                        c.z + ((i & 4) ? e.z : -e.z)
                    };
                    NkVec4f t = m * NkVec4f{corner.x, corner.y, corner.z, 1.f};
                    out.Expand({t.x, t.y, t.z});
                }
                return out;
            }

            static constexpr NkAABB Empty() noexcept { return {{ 1e30f, 1e30f, 1e30f},{-1e30f,-1e30f,-1e30f}}; }
            static constexpr NkAABB Unit()  noexcept { return {{-0.5f,-0.5f,-0.5f},{ 0.5f, 0.5f, 0.5f}}; }
        };

        struct NkSphere {
            NkVec3f center = {0,0,0};
            float32 radius = 0.f;
        };

        struct NkPlane {
            NkVec3f normal = {0,1,0};
            float32 d      = 0.f;        // d = -dot(normal, pointOnPlane)

            // Distance signee du point au plan
            constexpr float32 Distance(NkVec3f p) const noexcept {
                return normal.x*p.x + normal.y*p.y + normal.z*p.z + d;
            }
            // Normalise le plan (a appliquer apres extraction depuis viewProj)
            void Normalize() noexcept {
                float32 inv = 1.f / sqrtf(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
                normal.x *= inv; normal.y *= inv; normal.z *= inv; d *= inv;
            }
        };

        // =====================================================================
        // NkFrustum — 6 plans (left, right, bottom, top, near, far)
        // Plan = NkVec4f { normal.xyz, d }, point inside iff dot(n,p)+d >= 0.
        // Layout NkVec4f par plan = compatible upload UBO/SSBO direct (shader culling).
        // =====================================================================
        struct NkFrustum {
            NkVec4f planes[6];

            // Extraction Gribb-Hartmann depuis matrice viewProj.
            static inline NkFrustum FromViewProj(const NkMat4f& m) noexcept {
                NkFrustum f;
                f.planes[0] = { m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0] }; // L
                f.planes[1] = { m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0] }; // R
                f.planes[2] = { m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1] }; // B
                f.planes[3] = { m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1] }; // T
                f.planes[4] = { m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2] }; // N
                f.planes[5] = { m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2] }; // F
                for (int i = 0; i < 6; ++i) {
                    float32 len = sqrtf(f.planes[i].x*f.planes[i].x
                                      + f.planes[i].y*f.planes[i].y
                                      + f.planes[i].z*f.planes[i].z);
                    if (len > 1e-7f) {
                        f.planes[i].x /= len; f.planes[i].y /= len;
                        f.planes[i].z /= len; f.planes[i].w /= len;
                    }
                }
                return f;
            }

            // Test AABB (avec p-vertex, evite faux negatifs)
            inline bool TestAABB(const NkAABB& b) const noexcept {
                for (int i = 0; i < 6; ++i) {
                    const NkVec4f& p = planes[i];
                    NkVec3f pv = {
                        p.x >= 0.f ? b.max.x : b.min.x,
                        p.y >= 0.f ? b.max.y : b.min.y,
                        p.z >= 0.f ? b.max.z : b.min.z
                    };
                    if (p.x*pv.x + p.y*pv.y + p.z*pv.z + p.w < 0.f) return false;
                }
                return true;
            }
            inline bool TestSphere(NkVec3f c, float32 r) const noexcept {
                for (int i = 0; i < 6; ++i) {
                    const NkVec4f& p = planes[i];
                    if (p.x*c.x + p.y*c.y + p.z*c.z + p.w < -r) return false;
                }
                return true;
            }
            inline bool TestPoint(NkVec3f pt) const noexcept {
                for (int i = 0; i < 6; ++i) {
                    const NkVec4f& p = planes[i];
                    if (p.x*pt.x + p.y*pt.y + p.z*pt.z + p.w < 0.f) return false;
                }
                return true;
            }
        };

        // =====================================================================
        // SECTION E — Lumieres (CPU-side)
        // =====================================================================
        enum class NkLightType : uint8 {
            NK_DIRECTIONAL = 0,
            NK_POINT       = 1,
            NK_SPOT        = 2,
            NK_AREA        = 3,
        };

        struct NkLightDesc {
            NkLightType type        = NkLightType::NK_DIRECTIONAL;
            NkVec3f     direction   = {0,-1,0};
            NkVec3f     position    = {0,0,0};
            NkVec3f     color       = {1,1,1};
            float32     intensity   = 1.f;
            float32     range       = 10.f;
            float32     innerAngle  = 25.f;     // degres (cone interieur — full bright)
            float32     outerAngle  = 35.f;     // degres (cone exterieur — fade to 0)
            float32     areaWidth   = 1.f;
            float32     areaHeight  = 1.f;
            bool        castShadow  = true;
        };

        // =====================================================================
        // SECTION F — Layout GPU des lumieres (Forward+ clustered)
        // =====================================================================
        // std140-aligne, 96 octets par light.
        struct NkLightGPU {
            NkVec4f position;     // .xyz = pos, .w = type (cast en uint dans le shader)
            NkVec4f direction;    // .xyz = dir,  .w = range
            NkVec4f color;        // .rgb = color * intensity, .a = inverse-square attenuation flag
            NkVec4f cone;         // .x = cosInner, .y = cosOuter, .z = areaW, .w = areaH
            NkVec4f shadow;       // .x = shadowMapIndex (-1 si none), .yzw = reserved
            NkMat4f shadowVP;     // matrice light-space (pour shadow sampling)
        };

        // Structure de cluster (Forward+) — un cluster = sub-frustum
        struct NkClusterAABB {
            NkVec4f minBounds;    // .xyz = min view-space, .w = padding
            NkVec4f maxBounds;
        };

        // Tableau de lights par cluster (compute fillera ce buffer)
        struct NkClusterLightList {
            uint32 count;
            uint32 indices[256];   // doit matcher cluster.maxLightsPerCluster
        };

        // =====================================================================
        // SECTION G — Camera GPU layout (UBO)
        // =====================================================================
        // std140-aligne, 256 octets. A jour 1 fois par frame, set=0 binding=0.
        struct NkCameraUBO {
            NkMat4f view;
            NkMat4f proj;
            NkMat4f viewProj;
            NkMat4f invView;
            NkMat4f invProj;
            NkMat4f invViewProj;
            NkVec4f position;         // .xyz = world pos, .w = time
            NkVec4f viewport;         // .xy = w,h ; .zw = invW, invH
            NkVec4f depthParams;      // .x = near, .y = far, .z = nearLin, .w = farLin
            NkVec4f frustumPlanes[6]; // pour culling shader-side
        };

        // =====================================================================
        // SECTION H — Camera CPU descriptors
        // =====================================================================
        struct NkCamera3DData {
            NkVec3f position  = {0,0,5};
            NkVec3f target    = {0,0,0};
            NkVec3f up        = {0,1,0};
            float32 fovY      = 65.f;       // degres
            float32 aspect    = 16.f/9.f;
            float32 nearPlane = 0.1f;
            float32 farPlane  = 1000.f;
            bool    ortho     = false;
            float32 orthoSize = 10.f;
        };

        struct NkCamera2DData {
            NkVec2f center   = {0,0};
            float32 zoom     = 1.f;
            float32 rotation = 0.f;         // degres
            uint32  width    = 1280;
            uint32  height   = 720;
        };

        // =====================================================================
        // SECTION I — Draw calls (3D)
        // =====================================================================
        struct NkDrawCall3D {
            NkMeshHandle    mesh;
            NkMatInstHandle material;
            NkMat4f         transform = NkMat4f::Identity();
            NkVec3f         tint      = {1,1,1};
            float32         alpha     = 1.f;
            NkAABB          aabb;                                  // world-space, pour culling
            bool            castShadow = true;
            bool            receiveShadow = true;
            bool            visible    = true;
            uint32          subMeshIdx = 0xFFFFFFFFu;              // -1 = all submeshes
            uint32          sortKey    = 0;                        // material*1000 + meshHash
            int32           lightLayerMask = -1;                   // -1 = lit by all lights
        };

        struct NkDrawCallInstanced {
            NkMeshHandle      mesh;
            NkMatInstHandle   material;
            NkVector<NkMat4f> transforms;
            NkVector<NkVec3f> tints;
            NkAABB            aabb;
        };

        struct NkDrawCallSkinned {
            NkMeshHandle      mesh;
            NkMatInstHandle   material;
            NkMat4f           transform = NkMat4f::Identity();
            NkVector<NkMat4f> boneMatrices;
            NkVec3f           tint  = {1,1,1};
            float32           alpha = 1.f;
            NkAABB            aabb;
            bool              castShadow = true;
        };

        // Indirect draw command (GPU-driven) — match GL_DRAW_ELEMENTS_INDIRECT_COMMAND
        struct NkDrawIndirectCmd {
            uint32 indexCount;
            uint32 instanceCount;
            uint32 firstIndex;
            int32  baseVertex;
            uint32 baseInstance;
        };

        // =====================================================================
        // SECTION J — Stats par frame
        // =====================================================================
        struct NkRendererStats {
            uint32  drawCalls       = 0;
            uint32  triangles       = 0;
            uint32  vertices        = 0;
            uint32  textureBinds    = 0;
            uint32  shaderSwitches  = 0;
            uint32  pipelineSwitches= 0;
            uint32  batchCount      = 0;
            uint32  culled          = 0;        // mesh culled by frustum
            uint32  lightsActive    = 0;        // lights affecting visible geometry
            uint32  shadowCasters   = 0;
            float32 gpuTimeMs       = 0.f;
            float32 cpuTimeMs       = 0.f;
            float32 cullTimeMs      = 0.f;
            float32 shadowTimeMs    = 0.f;
            float32 geomTimeMs      = 0.f;
            float32 postTimeMs      = 0.f;

            void Reset() noexcept { *this = {}; }
        };

        // (NkSceneContext est defini dans Core/NkCamera.h car il contient
        //  un NkCamera3D par valeur — il aurait introduit une dependance
        //  circulaire en etant ici. NkCamera.h est sa "place canonique"
        //  cote types-de-rendu-haut-niveau.)

    } // namespace renderer
} // namespace nkentseu
