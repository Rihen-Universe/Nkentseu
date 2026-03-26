#pragma once
// =============================================================================
// NkMesh.h — Structures de maillage 3D.
//
//  NkStaticMesh   — Maillage immuable uploadé une fois sur le GPU.
//  NkDynamicMesh  — Maillage mis à jour chaque frame (vertex buffer CPU→GPU).
//
// Vertex layout standard (voir NkVertex3D) :
//   location 0 : position  (vec3)
//   location 1 : normal    (vec3)
//   location 2 : tangent   (vec4 — w = signe du bitangent)
//   location 3 : uv        (vec2)
//   location 4 : color     (vec4, default blanc)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {

    // =========================================================================
    // NkVertex3D — Vertex standard du renderer 3D
    // =========================================================================
    struct NkVertex3D {
        NkVec3  position  = {};
        NkVec3  normal    = {0,1,0};
        NkVec4  tangent   = {1,0,0,1};   // w = ±1 (orientation bitangent)
        NkVec2  uv        = {};
        NkColor4f color   = {1,1,1,1};
    };

    // Layout standard (partagé par static & dynamic mesh)
    inline NkVertexLayout NkVertex3DLayout() noexcept {
        NkVertexLayout vl;
        vl.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  0,  "POSITION")
          .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT,  12, "NORMAL")
          .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 24, "TANGENT")
          .AddAttribute(3, 0, NkGPUFormat::NK_RG32_FLOAT,   40, "TEXCOORD")
          .AddAttribute(4, 0, NkGPUFormat::NK_RGBA32_FLOAT, 48, "COLOR")
          .AddBinding(0, sizeof(NkVertex3D));
        return vl;
    }

    // =========================================================================
    // NkMeshData — données CPU d'un maillage
    // =========================================================================
    struct NkMeshData {
        NkVector<NkVertex3D> vertices;
        NkVector<uint32>     indices;
        NkPrimitiveTopology  topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;

        bool IsValid() const noexcept { return !vertices.IsEmpty(); }
        uint32 VertexCount() const noexcept { return vertices.Size(); }
        uint32 IndexCount()  const noexcept { return indices.Size(); }
    };

    // =========================================================================
    // NkMeshAABB — boîte englobante axis-aligned
    // =========================================================================
    struct NkMeshAABB {
        NkVec3 min = { 1e30f,  1e30f,  1e30f};
        NkVec3 max = {-1e30f, -1e30f, -1e30f};

        void Expand(const NkVec3& p) noexcept {
            min.x = math::NkMin(min.x, p.x); min.y = math::NkMin(min.y, p.y); min.z = math::NkMin(min.z, p.z);
            max.x = math::NkMax(max.x, p.x); max.y = math::NkMax(max.y, p.y); max.z = math::NkMax(max.z, p.z);
        }
        NkVec3 Center() const noexcept { return {(min.x+max.x)*0.5f,(min.y+max.y)*0.5f,(min.z+max.z)*0.5f}; }
        NkVec3 Extent() const noexcept { return {(max.x-min.x)*0.5f,(max.y-min.y)*0.5f,(max.z-min.z)*0.5f}; }
    };

    // =========================================================================
    // NkStaticMesh — maillage immuable (GPU immutable buffers)
    // =========================================================================
    class NKRENDERER_API NkStaticMesh {
        public:
            NkStaticMesh()  = default;
            ~NkStaticMesh() { Destroy(); }

            NkStaticMesh(const NkStaticMesh&)            = delete;
            NkStaticMesh& operator=(const NkStaticMesh&) = delete;

            bool Init(NkIDevice* device, const NkMeshData& data) noexcept;
            void Destroy() noexcept;

            // Binding avant draw
            void Bind(NkICommandBuffer* cmd) const noexcept;

            // Draw sans pipeline ni material — à appeler après Bind()
            void Draw(NkICommandBuffer* cmd) const noexcept;

            bool IsValid() const noexcept { return mIsValid; }

            const NkMeshAABB& GetAABB()   const noexcept { return mAABB; }
            uint32 GetVertexCount()        const noexcept { return mVertexCount; }
            uint32 GetIndexCount()         const noexcept { return mIndexCount; }
            NkPrimitiveTopology Topology() const noexcept { return mTopology; }

        private:
            NkIDevice*          mDevice      = nullptr;
            NkBufferHandle      mVBO;
            NkBufferHandle      mIBO;
            uint32              mVertexCount = 0;
            uint32              mIndexCount  = 0;
            NkPrimitiveTopology mTopology    = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            NkMeshAABB          mAABB;
            bool                mIsValid     = false;
    };

    // =========================================================================
    // NkDynamicMesh — maillage mis à jour chaque frame
    // =========================================================================
    class NKRENDERER_API NkDynamicMesh {
        public:
            NkDynamicMesh()  = default;
            ~NkDynamicMesh() { Destroy(); }

            NkDynamicMesh(const NkDynamicMesh&)            = delete;
            NkDynamicMesh& operator=(const NkDynamicMesh&) = delete;

            /// Alloue les buffers GPU pour maxVertices / maxIndices
            bool Init(NkIDevice* device,
                    uint32 maxVertices, uint32 maxIndices,
                    NkPrimitiveTopology topology = NkPrimitiveTopology::NK_TRIANGLE_LIST) noexcept;
            void Destroy() noexcept;

            /// Met à jour les données CPU puis les upload au GPU (appelé avant Bind/Draw)
            bool Update(const NkMeshData& data) noexcept;

            void Bind(NkICommandBuffer* cmd) const noexcept;
            void Draw(NkICommandBuffer* cmd) const noexcept;

            bool IsValid() const noexcept { return mIsValid; }
            uint32 GetCurrentIndexCount()  const noexcept { return mCurrentIndexCount; }
            uint32 GetCurrentVertexCount() const noexcept { return mCurrentVertexCount; }

        private:
            NkIDevice*          mDevice             = nullptr;
            NkBufferHandle      mVBO;
            NkBufferHandle      mIBO;
            uint32              mMaxVertices        = 0;
            uint32              mMaxIndices         = 0;
            uint32              mCurrentVertexCount = 0;
            uint32              mCurrentIndexCount  = 0;
            NkPrimitiveTopology mTopology           = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            bool                mIsValid            = false;
    };

    // =========================================================================
    // NkPrimitiveMeshBuilder — utilitaires de génération procédurale
    // =========================================================================
    struct NKRENDERER_API NkPrimitiveMeshBuilder {
        static NkMeshData MakeCube(float32 size = 1.f) noexcept;
        static NkMeshData MakePlane(float32 width = 1.f, float32 depth = 1.f,
                                    uint32 subdivisionsX = 1,
                                    uint32 subdivisionsZ = 1) noexcept;
        static NkMeshData MakeSphere(float32 radius  = 0.5f,
                                     uint32 rings    = 16,
                                     uint32 segments = 32) noexcept;
        static NkMeshData MakeCylinder(float32 radius = 0.5f, float32 height = 1.f,
                                       uint32 segments = 32) noexcept;
        static NkMeshData MakeCone(float32 radius = 0.5f, float32 height = 1.f,
                                   uint32 segments = 32) noexcept;
        static NkMeshData MakeCapsule(float32 radius = 0.5f, float32 height = 1.f,
                                      uint32 rings = 8, uint32 segments = 32) noexcept;
        static NkMeshData MakeQuad(float32 width = 1.f, float32 height = 1.f) noexcept;
    };

} // namespace nkentseu
