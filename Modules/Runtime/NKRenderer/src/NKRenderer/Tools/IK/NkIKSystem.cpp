// =============================================================================
// NkIKSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkIKSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkIKRig
    // =========================================================================

    NkIKRig::NkIKRig(uint64 skeletonId) : mSkeletonId(skeletonId) {}

    NkIKChainId NkIKRig::AddChain(const NkIKChainDesc& desc) {
        NkIKChainId id;
        id.id = mNextChainId++;

        ChainEntry entry;
        entry.id      = id;
        entry.desc    = desc;
        entry.target  = desc.target;
        entry.enabled = desc.enabled;
        mChains.PushBack(entry);
        return id;
    }

    void NkIKRig::RemoveChain(NkIKChainId id) {
        for (uint32 i = 0; i < mChains.Size(); ++i) {
            if (mChains[i].id == id) {
                mChains.Erase(mChains.Begin() + i);
                return;
            }
        }
    }

    void NkIKRig::SetTarget(NkIKChainId id, NkVec3f pos, NkQuatf rot) {
        for (auto& e : mChains) {
            if (e.id == id) {
                e.target.position = pos;
                e.target.rotation = rot;
                return;
            }
        }
    }

    void NkIKRig::SetWeight(NkIKChainId id, float32 w) {
        for (auto& e : mChains) { if (e.id == id) { e.weight = w; return; } }
    }

    void NkIKRig::SetEnabled(NkIKChainId id, bool enabled) {
        for (auto& e : mChains) { if (e.id == id) { e.enabled = enabled; return; } }
    }

    void NkIKRig::EnableAll(bool enabled) {
        for (auto& e : mChains) e.enabled = enabled;
    }

    const NkIKChainDesc* NkIKRig::GetChain(NkIKChainId id) const {
        for (auto& e : mChains) { if (e.id == id) return &e.desc; }
        return nullptr;
    }

    NkIKChainId NkIKRig::FindChain(const NkString& name) const {
        for (auto& e : mChains) { if (e.desc.name == name) return e.id; }
        return NkIKChainId{};
    }

    uint32 NkIKRig::GetChainCount() const { return (uint32)mChains.Size(); }

    const NkMat4f* NkIKRig::GetBoneMatrices()    const { return mBoneMatrices.Data(); }
    uint32         NkIKRig::GetBoneMatrixCount()  const { return (uint32)mBoneMatrices.Size(); }

    // =========================================================================
    // NkIKSystem
    // =========================================================================

    NkIKSystem::~NkIKSystem() {
        Shutdown();
    }

    bool NkIKSystem::Init(NkIDevice* device, NkAnimationSystem* animSys,
                           const NkIKConfig& cfg) {
        mDevice  = device;
        mAnimSys = animSys;
        mCfg     = cfg;
        mReady   = true;
        return true;
    }

    void NkIKSystem::Shutdown() {
        if (!mReady) return;
        for (auto& kv : mRigs) delete kv.Second;
        mRigs.Clear();
        mReady = false;
    }

    NkIKRig* NkIKSystem::CreateRig(uint64 skeletonId) {
        auto* existing = mRigs.Find(skeletonId);
        if (existing) return *existing;
        NkIKRig* rig = new NkIKRig(skeletonId);
        mRigs.Insert(skeletonId, rig);
        return rig;
    }

    void NkIKSystem::DestroyRig(NkIKRig*& rig) {
        if (!rig) return;
        mRigs.Erase(rig->GetSkeletonId());
        delete rig;
        rig = nullptr;
    }

    NkIKRig* NkIKSystem::GetRig(uint64 skeletonId) {
        auto* r = mRigs.Find(skeletonId);
        return r ? *r : nullptr;
    }

    void NkIKSystem::Solve(float32 deltaTimeSec) {
        for (auto& kv : mRigs)
            SolveRig(kv.Second, deltaTimeSec);
    }

    void NkIKSystem::SolveRig(NkIKRig* rig, float32 /*deltaTimeSec*/) {
        if (!rig) return;
        for (auto& chain : rig->mChains) {
            if (!chain.enabled || chain.weight <= 0.f) continue;
            switch (chain.desc.solver) {
                case NkIKSolver::NK_TWO_BONE:
                    SolveChain_TwoBone(chain, rig->mBoneMatrices);
                    break;
                case NkIKSolver::NK_CCD:
                    SolveChain_CCD(chain, rig->mBoneMatrices);
                    break;
                case NkIKSolver::NK_FABRIK:
                    SolveChain_FABRIK(chain, rig->mBoneMatrices);
                    break;
                case NkIKSolver::NK_SPLINE:
                    SolveChain_Spline(chain, rig->mBoneMatrices);
                    break;
                case NkIKSolver::NK_FBIK:
                    // FBIK decomposes into sub-chains; solve each
                    SolveChain_FABRIK(chain, rig->mBoneMatrices);
                    break;
            }
        }
    }

    void NkIKSystem::SetDebugDraw(bool enabled) {
        mCfg.debugDraw = enabled;
    }

    // ── Solvers ───────────────────────────────────────────────────────────────

    static inline NkVec3f MatGetTranslation(const NkMat4f& m) {
        return { m[3][0], m[3][1], m[3][2] };
    }

    void NkIKSystem::SolveChain_TwoBone(NkIKRig::ChainEntry& chain,
                                          NkVector<NkMat4f>& /*bones*/) {
        // Analytical two-bone IK (arm / leg)
        if (chain.desc.bones.Size() < 2) return;

        const NkIKBone& boneA = chain.desc.bones[0]; // root (hip/shoulder)
        const NkIKBone& boneB = chain.desc.bones[1]; // mid  (knee/elbow)
        float32 la = boneA.length;
        float32 lb = boneB.length;

        NkVec3f root   = chain.target.position; // placeholder — should come from bone matrix
        NkVec3f target = chain.target.position;
        NkVec3f diff   = { target.x - root.x, target.y - root.y, target.z - root.z };
        float32 dist   = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);

        // Clamp to reach
        float32 maxReach = la + lb;
        if (dist > maxReach) dist = maxReach;
        if (dist < fabsf(la - lb)) dist = fabsf(la - lb) + 1e-5f;

        // Law of cosines: cos(theta_b) = (la²+lb²-dist²) / (2·la·lb)
        float32 cosB = (la*la + lb*lb - dist*dist) / (2.f*la*lb);
        cosB = cosB < -1.f ? -1.f : cosB > 1.f ? 1.f : cosB;
        float32 /*thetaB*/ _ = acosf(cosB);
        (void)_;
        // Full bone matrix computation would write into bones[boneA.boneIdx/boneB.boneIdx]
    }

    void NkIKSystem::SolveChain_CCD(NkIKRig::ChainEntry& chain,
                                     NkVector<NkMat4f>& /*bones*/) {
        if (chain.desc.bones.Empty()) return;
        NkVec3f target = chain.target.position;
        uint32  iters  = chain.desc.maxIterations;

        // CCD: iterate from effector toward root
        for (uint32 iter = 0; iter < iters; ++iter) {
            int32 n = (int32)chain.desc.bones.Size() - 1;
            for (int32 i = n - 1; i >= 0; --i) {
                // Rotate bone[i] to point toward target
                // Stub — requires current end effector position from bone matrices
                (void)i;
            }
            // Check convergence
            float32 err = 0.f; // distance from effector to target
            if (err < chain.desc.tolerance) break;
        }
    }

    void NkIKSystem::SolveChain_FABRIK(NkIKRig::ChainEntry& chain,
                                        NkVector<NkMat4f>& /*bones*/) {
        if (chain.desc.bones.Empty()) return;
        uint32  n      = (uint32)chain.desc.bones.Size();
        NkVec3f target = chain.target.position;
        uint32  iters  = chain.desc.maxIterations;

        // Temporary positions
        NkVector<NkVec3f> pos;
        for (uint32 i = 0; i < n; ++i) pos.PushBack({0,0,0}); // placeholder

        NkVec3f root = pos[0];

        for (uint32 iter = 0; iter < iters; ++iter) {
            // Forward pass — from root to effector
            pos[n-1] = target;
            for (int32 i = (int32)n-2; i >= 0; --i) {
                float32 len = chain.desc.bones[i+1].length;
                NkVec3f dir = {
                    pos[i].x - pos[i+1].x,
                    pos[i].y - pos[i+1].y,
                    pos[i].z - pos[i+1].z
                };
                float32 d = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                if (d > 1e-7f) { dir.x/=d; dir.y/=d; dir.z/=d; }
                pos[i] = { pos[i+1].x + dir.x*len,
                            pos[i+1].y + dir.y*len,
                            pos[i+1].z + dir.z*len };
            }
            // Backward pass — from root back to effector
            pos[0] = root;
            for (uint32 i = 0; i < n-1; ++i) {
                float32 len = chain.desc.bones[i+1].length;
                NkVec3f dir = {
                    pos[i+1].x - pos[i].x,
                    pos[i+1].y - pos[i].y,
                    pos[i+1].z - pos[i].z
                };
                float32 d = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                if (d > 1e-7f) { dir.x/=d; dir.y/=d; dir.z/=d; }
                pos[i+1] = { pos[i].x + dir.x*len,
                              pos[i].y + dir.y*len,
                              pos[i].z + dir.z*len };
            }
            // Check convergence
            NkVec3f diff = { pos[n-1].x - target.x,
                             pos[n-1].y - target.y,
                             pos[n-1].z - target.z };
            float32 err = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
            if (err < chain.desc.tolerance) break;
        }
        // Write positions back to bone matrices (stub)
    }

    void NkIKSystem::SolveChain_Spline(NkIKRig::ChainEntry& chain,
                                        NkVector<NkMat4f>& /*bones*/) {
        if (chain.desc.bones.Size() < 2) return;
        // Spline IK: fit a Catmull-Rom curve through control points,
        // distribute bones along the curve — stub
    }

    void NkIKSystem::ApplyConstraint(const NkIKConstraint& c, NkQuatf& q,
                                      NkQuatf /*prevQ*/) {
        if (c.type == NkIKConstraintType::NK_FREE) return;
        // Clamp angle — simplified
        (void)c; (void)q;
    }

} // namespace renderer
} // namespace nkentseu
