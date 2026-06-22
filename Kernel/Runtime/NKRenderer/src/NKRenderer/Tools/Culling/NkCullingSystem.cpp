// =============================================================================
// NkCullingSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkCullingSystem.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKMemory/NkAllocator.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        // (NkFrustum est defini inline dans Core/NkRendererTypes.h —
        //  FromViewProj / TestAABB / TestSphere / TestPoint y sont implementees.)

        // =========================================================================
        // NkCullingSystem
        // =========================================================================

        NkCullingSystem::~NkCullingSystem() {
            Shutdown();
        }

        bool NkCullingSystem::Init(const NkCullingConfig& cfg) {
            mCfg = cfg;
            mOctree = memory::NkGetDefaultAllocator().New<OctreeType>(cfg.worldX, cfg.worldY, cfg.worldZ,
                                    cfg.worldW, cfg.worldH, cfg.worldD);
            mReady  = true;
            return true;
        }

        void NkCullingSystem::Shutdown() {
            if (!mReady) return;
            memory::NkGetDefaultAllocator().Delete(mOctree);
            mOctree = nullptr;
            mRegistry.Clear();
            mResults.Clear();
            mLODs.Clear();
            mReady = false;
        }

        void NkCullingSystem::Register(uint64 id, const NkAABB& aabb,
                                        float32 maxDist, bool alwaysVisible) {
            NkCullable obj;
            obj.id           = id;
            obj.aabb         = aabb;
            obj.maxDrawDist  = maxDist;
            obj.alwaysVisible= alwaysVisible;
            mRegistry.Insert(id, obj);

            if (mOctree) {
                NkVec3f c = aabb.Center();
                mOctree->Insert(obj, c.x, c.y, c.z);
            }
        }

        void NkCullingSystem::Unregister(uint64 id) {
            mRegistry.Erase(id);
            mResults.Erase(id);
            mLODs.Erase(id);
            // Octree rebuild from registry
            if (mOctree) {
                mOctree->Clear();
                for (auto& kv : mRegistry) {
                    NkVec3f c = kv.Second.aabb.Center();
                    mOctree->Insert(kv.Second, c.x, c.y, c.z);
                }
            }
        }

        void NkCullingSystem::UpdateAABB(uint64 id, const NkAABB& newAABB) {
            auto* obj = mRegistry.Find(id);
            if (!obj) return;
            obj->aabb = newAABB;
            // Rebuild octree
            if (mOctree) {
                mOctree->Clear();
                for (auto& kv : mRegistry) {
                    NkVec3f c = kv.Second.aabb.Center();
                    mOctree->Insert(kv.Second, c.x, c.y, c.z);
                }
            }
        }

        void NkCullingSystem::BeginFrame(const NkCamera3D& cam, const NkMat4f& viewProj) {
            if (!mReady) return;
            mCamPos  = cam.GetPosition();
            mFrustum = NkFrustum::FromViewProj(viewProj);

            mResults.Clear();
            mLODs.Clear();
            mStatTotal   = (uint32)mRegistry.Size();
            mStatVisible = 0;
            mStatCulled  = 0;

            for (auto& kv : mRegistry) {
                NkCullResult r = EvaluateOne(kv.Second);
                mResults.Insert(kv.First, r);

                NkVec3f c    = kv.Second.aabb.Center();
                NkVec3f diff = { c.x - mCamPos.x, c.y - mCamPos.y, c.z - mCamPos.z };
                float32 dist = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
                mLODs.Insert(kv.First, ComputeLOD(dist));

                if (r == NkCullResult::NK_VISIBLE) ++mStatVisible;
                else                               ++mStatCulled;
            }
        }

        NkCullResult NkCullingSystem::TestDrawCall(uint64 id, int32* outLOD) const {
            auto* r = mResults.Find(id);
            if (!r) return NkCullResult::NK_CULLED_FRUSTUM;
            if (outLOD) {
                auto* lod = mLODs.Find(id);
                *outLOD = lod ? *lod : 0;
            }
            return *r;
        }

        void NkCullingSystem::QueryVisible(NkVector<uint64>& outIds) const {
            outIds.Clear();
            for (auto& kv : mResults) {
                if (kv.Second == NkCullResult::NK_VISIBLE)
                    outIds.PushBack(kv.First);
            }
        }

        uint32 NkCullingSystem::FilterDrawCalls(NkDrawCall3D* dcs, uint32 count) const {
            uint32 write = 0;
            for (uint32 i = 0; i < count; ++i) {
                uint64 id = dcs[i].aabb.Center().x != 0 ? 0 : 0; // placeholder — use mesh handle
                // In practice, use an id passed via sortKey or external mapping
                // For now, do frustum test inline
                if (mFrustum.TestAABB(dcs[i].aabb) && dcs[i].visible) {
                    dcs[write++] = dcs[i];
                }
            }
            return write;
        }

        void NkCullingSystem::DrawDebugOctree(NkOverlayRenderer* /*overlay*/) const {
            // Stub: iterate octree and draw each node bounds via overlay->DrawWireBox
        }

        NkCullResult NkCullingSystem::EvaluateOne(const NkCullable& obj) const {
            if (obj.alwaysVisible) return NkCullResult::NK_VISIBLE;

            // Distance culling
            if (mCfg.distanceCulling && obj.maxDrawDist > 0.f) {
                NkVec3f c    = obj.aabb.Center();
                NkVec3f diff = { c.x - mCamPos.x, c.y - mCamPos.y, c.z - mCamPos.z };
                float32 dist = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
                if (dist > obj.maxDrawDist) return NkCullResult::NK_CULLED_DISTANCE;
                if (dist < obj.minDrawDist) return NkCullResult::NK_CULLED_DISTANCE;
            }

            // Frustum culling
            if (mCfg.frustumCulling) {
                if (!mFrustum.TestAABB(obj.aabb))
                    return NkCullResult::NK_CULLED_FRUSTUM;
            }

            return NkCullResult::NK_VISIBLE;
        }

        int32 NkCullingSystem::ComputeLOD(float32 dist) const {
            for (int32 i = 3; i >= 0; --i) {
                if (dist >= mCfg.lodDistances[i]) return i;
            }
            return 0;
        }

    } // namespace renderer
} // namespace nkentseu
