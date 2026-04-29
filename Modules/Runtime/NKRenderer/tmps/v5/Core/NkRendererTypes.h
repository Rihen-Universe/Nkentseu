#pragma once
// =============================================================================
// NkRendererTypes.h  — NKRenderer v4.0
// Source de vérité unique pour tous les types du module NKRenderer.
//
// Règles absolues :
//   • Namespace nkentseu::renderer — jamais nkentseu:: directement
//   • Aucune dépendance vers NkScene, ECS, NKWindow, NKEvent
//   • Les handles RHI (NKRHI) restent dans nkentseu::
//   • Les handles Renderer restent dans nkentseu::renderer::
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    using namespace math;

    // =========================================================================
    // Handles opaques typés — 64-bit, 0 = invalide
    // =========================================================================
    template<typename Tag>
    struct NkRendHandle {
        uint64 id = 0;
        bool IsValid() const noexcept { return id != 0; }
        bool operator==(const NkRendHandle& o) const noexcept { return id == o.id; }
        bool operator!=(const NkRendHandle& o) const noexcept { return id != o.id; }
        static NkRendHandle Null() noexcept { return {0}; }
    };

    struct TagRTexture  {};
    struct TagRMesh     {};
    struct TagRShader   {};
    struct TagRMaterial {};
    struct TagRMatInst  {};
    struct TagRFont     {};
    struct TagRTarget   {};

    using NkTexHandle      = NkRendHandle<TagRTexture>;
    using NkMeshHandle     = NkRendHandle<TagRMesh>;
    using NkShaderHandle   = NkRendHandle<TagRShader>;
    using NkMatHandle      = NkRendHandle<TagRMaterial>;
    using NkMatInstHandle  = NkRendHandle<TagRMatInst>;
    using NkFontHandle     = NkRendHandle<TagRFont>;
    using NkTargetHandle   = NkRendHandle<TagRTarget>;

    // =========================================================================
    // Résultat d'opération
    // =========================================================================
    enum class NkRResult : uint32 {
        NK_OK = 0,
        NK_ERR_INVALID_DEVICE,
        NK_ERR_INVALID_HANDLE,
        NK_ERR_COMPILE_FAILED,
        NK_ERR_OUT_OF_MEMORY,
        NK_ERR_NOT_SUPPORTED,
        NK_ERR_IO,
        NK_ERR_UNKNOWN,
    };
    inline bool NkROk(NkRResult r) noexcept { return r == NkRResult::NK_OK; }

    // =========================================================================
    // Blend modes
    // =========================================================================
    enum class NkBlendMode : uint8 {
        NK_OPAQUE    = 0,
        NK_ALPHA     = 1,
        NK_ADDITIVE  = 2,
        NK_MULTIPLY  = 3,
        NK_PREMULT   = 4,
    };

    // =========================================================================
    // Vertex layouts standards
    // =========================================================================
    struct NkVertex2D {
        NkVec2f  pos;
        NkVec2f  uv;
        uint32   color;   // RGBA8 packed
    };

    struct NkVertex3D {
        NkVec3f  pos;
        NkVec3f  normal;
        NkVec3f  tangent;
        NkVec2f  uv;
        NkVec2f  uv2;
        uint32   color;
    };

    struct NkVertexSkinned {
        NkVec3f  pos;
        NkVec3f  normal;
        NkVec3f  tangent;
        NkVec2f  uv;
        uint32   color;
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

    // =========================================================================
    // AABB
    // =========================================================================
    struct NkAABB {
        NkVec3f min = {-0.5f,-0.5f,-0.5f};
        NkVec3f max = { 0.5f, 0.5f, 0.5f};

        NkVec3f Center()   const noexcept { return {(min.x+max.x)*0.5f,(min.y+max.y)*0.5f,(min.z+max.z)*0.5f}; }
        NkVec3f Extents()  const noexcept { return {(max.x-min.x)*0.5f,(max.y-min.y)*0.5f,(max.z-min.z)*0.5f}; }

        void Expand(NkVec3f p) noexcept {
            if(p.x<min.x) min.x=p.x; if(p.y<min.y) min.y=p.y; if(p.z<min.z) min.z=p.z;
            if(p.x>max.x) max.x=p.x; if(p.y>max.y) max.y=p.y; if(p.z>max.z) max.z=p.z;
        }
        void Merge(const NkAABB& o) noexcept { Expand(o.min); Expand(o.max); }

        bool Contains(NkVec3f p) const noexcept {
            return p.x>=min.x&&p.x<=max.x&&p.y>=min.y&&p.y<=max.y&&p.z>=min.z&&p.z<=max.z;
        }

        static NkAABB Empty() noexcept { return {{ 1e9f, 1e9f, 1e9f},{-1e9f,-1e9f,-1e9f}}; }
        static NkAABB Unit()  noexcept { return {}; }
    };

    // =========================================================================
    // Lumières
    // =========================================================================
    enum class NkLightType : uint8 {
        NK_DIRECTIONAL = 0,
        NK_POINT,
        NK_SPOT,
        NK_AREA,
    };

    struct NkLightDesc {
        NkLightType type          = NkLightType::NK_DIRECTIONAL;
        NkVec3f     direction     = {0,-1,0};
        NkVec3f     position      = {0,0,0};
        NkVec3f     color         = {1,1,1};
        float32     intensity     = 1.f;
        float32     range         = 10.f;
        float32     innerAngle    = 25.f;
        float32     outerAngle    = 35.f;
        float32     areaWidth     = 1.f;
        float32     areaHeight    = 1.f;
        bool        castShadow    = true;
    };

    // =========================================================================
    // Caméras
    // =========================================================================
    struct NkCamera3DData {
        NkVec3f position  = {0,0,5};
        NkVec3f target    = {0,0,0};
        NkVec3f up        = {0,1,0};
        float32 fovY      = 65.f;
        float32 aspect    = 16.f/9.f;
        float32 nearPlane = 0.1f;
        float32 farPlane  = 1000.f;
        bool    ortho     = false;
        float32 orthoSize = 10.f;
    };

    struct NkCamera2DData {
        NkVec2f center   = {0,0};
        float32 zoom     = 1.f;
        float32 rotation = 0.f;
        uint32  width    = 1280;
        uint32  height   = 720;
    };

    // =========================================================================
    // Draw calls
    // =========================================================================
    struct NkDrawCall3D {
        NkMeshHandle    mesh;
        NkMatInstHandle material;
        NkMat4f         transform;
        NkVec3f         tint         = {1,1,1};
        float32         alpha        = 1.f;
        NkAABB          aabb;
        bool            castShadow   = true;
        bool            visible      = true;
        uint32          subMeshIdx   = 0xFFFFFFFFu;
        uint32          sortKey      = 0;
    };

    struct NkDrawCallInstanced {
        NkMeshHandle        mesh;
        NkMatInstHandle     material;
        NkVector<NkMat4f>   transforms;
        NkVector<NkVec3f>   tints;
    };

    struct NkDrawCallSkinned {
        NkMeshHandle     mesh;
        NkMatInstHandle  material;
        NkMat4f          transform;
        NkVector<NkMat4f> boneMatrices;
        NkVec3f          tint  = {1,1,1};
        float32          alpha = 1.f;
        bool             castShadow = true;
    };

    // =========================================================================
    // Stats par frame
    // =========================================================================
    struct NkRendererStats {
        uint32  drawCalls      = 0;
        uint32  triangles      = 0;
        uint32  vertices       = 0;
        uint32  textureBinds   = 0;
        uint32  shaderSwitches = 0;
        uint32  batchCount     = 0;
        float32 gpuTimeMs      = 0.f;
        float32 cpuTimeMs      = 0.f;

        void Reset() noexcept { *this = {}; }
    };

} // namespace renderer
} // namespace nkentseu
