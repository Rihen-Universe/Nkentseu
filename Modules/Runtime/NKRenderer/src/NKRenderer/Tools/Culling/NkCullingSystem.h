#pragma once
// =============================================================================
// NkCullingSystem.h  — NKRenderer v4.0  (Tools/Culling/)
//
// Culling multi-technique : Frustum, Occlusion, Distance, Back-face.
// Utilise NkOctree<NkCullable> (NKContainers/Specialized) pour l'accélération.
//
// Responsabilités :
//   - Frustum culling : rejeter objets hors du frustum caméra
//   - Occlusion culling : rejeter objets derrière d'autres (HZB)
//   - Distance culling  : LOD + fade + cull par distance
//   - Insertion/MAJ dans l'octree spatial
//   - Statistiques : draw calls soumis vs culled
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKContainers/Specialized/NkOctree.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Résultat de visibilité pour un draw call
    // =========================================================================
    enum class NkCullResult : uint8 {
        NK_VISIBLE   = 0,
        NK_CULLED_FRUSTUM,
        NK_CULLED_OCCLUSION,
        NK_CULLED_DISTANCE,
        NK_CULLED_BACKFACE,
    };

    // =========================================================================
    // Objet enregistré dans le système de culling
    // =========================================================================
    struct NkCullable {
        uint64  id          = 0;
        NkAABB  aabb;
        float32 maxDrawDist = 1000.f;
        float32 minDrawDist = 0.f;
        bool    alwaysVisible = false;
    };

    // =========================================================================
    // Frustum 6 plans (view-space ou world-space)
    // =========================================================================
    struct NkFrustum {
        NkVec4f planes[6]; // left, right, bottom, top, near, far (normale + d)

        static NkFrustum FromViewProj(const NkMat4f& vp);

        bool TestAABB  (const NkAABB& b)    const noexcept;
        bool TestSphere(NkVec3f c, float32 r) const noexcept;
        bool TestPoint (NkVec3f p)           const noexcept;
    };

    // =========================================================================
    // Configuration du culling
    // =========================================================================
    struct NkCullingConfig {
        bool    frustumCulling   = true;
        bool    occlusionCulling = false;  // HZB — nécessite passe depth
        bool    distanceCulling  = true;
        float32 maxDistance      = 1000.f;
        float32 lodDistances[4]  = { 0.f, 50.f, 150.f, 400.f };
        float32 worldX = -5000.f, worldY = -500.f, worldZ = -5000.f;
        float32 worldW = 10000.f, worldH = 1000.f, worldD = 10000.f;
    };

    // =========================================================================
    // NkCullingSystem
    // =========================================================================
    class NkCullingSystem {
    public:
        NkCullingSystem() = default;
        ~NkCullingSystem();

        bool Init(const NkCullingConfig& cfg = {});
        void Shutdown();

        // ── Enregistrement objets ─────────────────────────────────────────────
        void Register  (uint64 id, const NkAABB& aabb,
                         float32 maxDist = 1000.f, bool alwaysVisible = false);
        void Unregister(uint64 id);
        void UpdateAABB(uint64 id, const NkAABB& newAABB);

        // ── Calcul de visibilité ──────────────────────────────────────────────
        void BeginFrame(const NkCamera3D& cam, const NkMat4f& viewProj);

        NkCullResult TestDrawCall(uint64 id, int32* outLOD = nullptr) const;

        void   QueryVisible  (NkVector<uint64>& outIds) const;
        uint32 FilterDrawCalls(NkDrawCall3D* dcs, uint32 count) const;

        // ── Stats ─────────────────────────────────────────────────────────────
        uint32 GetLastFrameTotal()   const { return mStatTotal; }
        uint32 GetLastFrameVisible() const { return mStatVisible; }
        uint32 GetLastFrameCulled()  const { return mStatCulled; }

        void DrawDebugOctree(class NkOverlayRenderer* overlay) const;

    private:
        using OctreeType = NkOctree<NkCullable>;

        NkCullingConfig                  mCfg;
        OctreeType*                      mOctree  = nullptr;
        NkFrustum                        mFrustum;
        NkVec3f                          mCamPos  = {};
        bool                             mReady   = false;

        NkHashMap<uint64, NkCullResult>  mResults;
        NkHashMap<uint64, int32>         mLODs;
        NkHashMap<uint64, NkCullable>    mRegistry;

        mutable uint32 mStatTotal   = 0;
        mutable uint32 mStatVisible = 0;
        mutable uint32 mStatCulled  = 0;

        NkCullResult EvaluateOne(const NkCullable& obj) const;
        int32        ComputeLOD (float32 dist)           const;
    };

} // namespace renderer
} // namespace nkentseu
