#pragma once
// =============================================================================
// NkMeshCache.h
// Gestion des buffers GPU pour les meshes : vertex, index, skinning.
//
// Le NkMeshCache gère un gros buffer GPU en mémoire persistante et y alloue
// des plages pour chaque mesh (sub-allocation). Cela réduit les binding changes
// et améliore les performances (moins de draw calls à la soumission).
//
// Formats de vertex pré-définis :
//   NkVertexStatic   — position + normal + tangent + uv0 + uv1 (vertex complet)
//   NkVertexSkinned  — NkVertexStatic + joints[4] + weights[4]
//   NkVertexSimple   — position + uv (pour sprites 3D, terrain simple)
//   NkVertexDebug    — position + color (pour debug draw)
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKMath/NkVec.h"

namespace nkentseu {

    // =============================================================================
    // Vertex layouts standards
    // =============================================================================
    struct NkVertexStatic {
        math::NkVec3 position;   // 12
        math::NkVec3 normal;     // 12
        math::NkVec4 tangent;    // 16  (w = bitangent sign)
        math::NkVec2 uv0;        // 8
        math::NkVec2 uv1;        // 8
        // Total : 56 bytes

        static NkVertexLayout GetLayout();
    };

    struct NkVertexSkinned {
        math::NkVec3 position;
        math::NkVec3 normal;
        math::NkVec4 tangent;
        math::NkVec2 uv0;
        math::NkVec2 uv1;
        uint8        joints[4];  // indices dans le tableau de bones
        float32      weights[4]; // poids (somme = 1.0)
        // Total : 72 bytes

        static NkVertexLayout GetLayout();
    };

    struct NkVertexSimple {
        math::NkVec3 position;
        math::NkVec2 uv;
        // Total : 20 bytes

        static NkVertexLayout GetLayout();
    };

    struct NkVertexDebug {
        math::NkVec3 position;
        uint32       color;    // RGBA packed
        // Total : 16 bytes

        static NkVertexLayout GetLayout();
    };

    // =============================================================================
    // Identifiant de mesh GPU
    // =============================================================================
    using NkMeshId = uint64;
    static constexpr NkMeshId NK_INVALID_MESH = 0;

    // =============================================================================
    // AABB simplifié (pour frustum culling)
    // =============================================================================
    struct NkAABB {
        math::NkVec3 min = { -0.5f, -0.5f, -0.5f };
        math::NkVec3 max = {  0.5f,  0.5f,  0.5f };

        math::NkVec3 Center() const {
            return { (min.x+max.x)*0.5f, (min.y+max.y)*0.5f, (min.z+max.z)*0.5f };
        }
        math::NkVec3 Extents() const {
            return { (max.x-min.x)*0.5f, (max.y-min.y)*0.5f, (max.z-min.z)*0.5f };
        }
        float32 Radius() const {
            auto e = Extents();
            return sqrtf(e.x*e.x + e.y*e.y + e.z*e.z);
        }
    };

    // =============================================================================
    // Informations sur un mesh GPU
    // =============================================================================
    struct NkMeshInfo {
        NkMeshId        id         = NK_INVALID_MESH;
        NkBufferHandle  vertexBuffer;
        NkBufferHandle  indexBuffer;
        uint32          vertexCount= 0;
        uint32          indexCount = 0;
        uint32          vertexStride= 0;
        uint64          vertexOffset= 0;
        uint64          indexOffset = 0;
        NkIndexFormat   indexFormat = NkIndexFormat::NK_UINT32;
        NkPrimitiveTopology topology= NkPrimitiveTopology::NK_TRIANGLE_LIST;
        NkAABB          bounds;
        bool            hasSkin    = false;
        bool            isValid()  const { return vertexBuffer.IsValid(); }
    };

    // =============================================================================
    // Descripteur de création de mesh
    // =============================================================================
    struct NkMeshDesc {
        const void*       vertices      = nullptr;
        uint32            vertexCount   = 0;
        uint32            vertexStride  = 0;
        const void*       indices       = nullptr;
        uint32            indexCount    = 0;
        NkIndexFormat     indexFormat   = NkIndexFormat::NK_UINT32;
        NkPrimitiveTopology topology    = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        NkAABB            bounds;
        const char*       debugName     = nullptr;
    };

    // =============================================================================
    // Formes géométriques pré-construites
    // =============================================================================
    enum class NkPrimitive : uint8 {
        NK_QUAD,           // quad plat 1x1 en XY
        NK_CUBE,           // cube 1x1x1 centré
        NK_SPHERE,         // sphère UV subdivisée
        NK_ICOSPHERE,      // sphère icosaèdre (mieux répartie)
        NK_CYLINDER,       // cylindre
        NK_CONE,
        NK_CAPSULE,
        NK_PLANE,          // grille subdivisiable
        NK_FRUSTUM,        // frustum pour debug
        NK_SCREEN_QUAD,    // quad NDC pour les passes full-screen
    };

    // =============================================================================
    // NkMeshCache
    // =============================================================================
    class NkMeshCache {
        public:
            explicit NkMeshCache(NkIDevice* device, uint64 maxVtxBytes, uint64 maxIdxBytes);
            ~NkMeshCache();

            bool Initialize();
            void Shutdown();

            // ── Upload de géométrie ───────────────────────────────────────────────────
            NkMeshId Upload(const NkMeshDesc& desc);
            NkMeshId UploadStatic(const NkVertexStatic* vtx, uint32 vtxCount,
                                const uint32* idx, uint32 idxCount,
                                const NkAABB& bounds, const char* name = nullptr);
            NkMeshId UploadSkinned(const NkVertexSkinned* vtx, uint32 vtxCount,
                                    const uint32* idx, uint32 idxCount,
                                    const NkAABB& bounds, const char* name = nullptr);

            // ── Mise à jour dynamique ─────────────────────────────────────────────────
            // Pour les meshes qui changent chaque frame (terrain LOD, particles, etc.)
            bool     UpdateVertices(NkMeshId id, const void* vtx, uint32 vtxCount,
                                    uint32 offsetVertex = 0);
            bool     UpdateIndices(NkMeshId id, const void* idx, uint32 idxCount,
                                    uint32 offsetIndex = 0);

            // ── Primitives pré-construites ────────────────────────────────────────────
            NkMeshId GetPrimitive(NkPrimitive prim, uint32 subdivisions = 16);

            // ── Accès ─────────────────────────────────────────────────────────────────
            const NkMeshInfo* Get(NkMeshId id) const;
            bool              IsValid(NkMeshId id) const;
            void              Release(NkMeshId id);

            // ── Bind ─────────────────────────────────────────────────────────────────
            // Lie les buffers pour un DrawCall (shortcut pour NkICommandBuffer)
            void BindForDraw(NkICommandBuffer* cmd, NkMeshId id) const;

            // ── Statistiques ──────────────────────────────────────────────────────────
            uint32 GetMeshCount()     const;
            uint64 GetUsedVtxBytes()  const;
            uint64 GetUsedIdxBytes()  const;

        private:
            NkIDevice*    mDevice;
            uint64        mMaxVtxBytes, mMaxIdxBytes;
            uint64        mNextId = 1;

            // Pool de buffers GPU (big buffer + sub-allocation)
            NkBufferHandle mVertexPool;
            NkBufferHandle mIndexPool;
            uint64         mVtxHead = 0;
            uint64         mIdxHead = 0;

            NkUnorderedMap<NkMeshId, NkMeshInfo>  mMeshes;
            NkUnorderedMap<uint32, NkMeshId>      mPrimitiveCache;

            NkMeshId BuildPrimitive(NkPrimitive prim, uint32 subdivisions);
            NkMeshId AllocId();

            // Builders géométriques
            static void BuildQuad       (NkVector<NkVertexStatic>&, NkVector<uint32>&, NkAABB&);
            static void BuildCube       (NkVector<NkVertexStatic>&, NkVector<uint32>&, NkAABB&);
            static void BuildSphere     (NkVector<NkVertexStatic>&, NkVector<uint32>&, NkAABB&, uint32 stacks, uint32 slices);
            static void BuildIcosphere  (NkVector<NkVertexStatic>&, NkVector<uint32>&, NkAABB&, uint32 subdivisions);
            static void BuildCylinder   (NkVector<NkVertexStatic>&, NkVector<uint32>&, NkAABB&, uint32 slices);
            static void BuildPlane      (NkVector<NkVertexStatic>&, NkVector<uint32>&, NkAABB&, uint32 sub);
            static void BuildScreenQuad (NkVector<NkVertexSimple>&, NkVector<uint32>&);
    };

} // namespace nkentseu