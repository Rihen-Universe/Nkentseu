// =============================================================================
// NkIKSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkIKSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKMemory/NkAllocator.h"
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

        void NkIKRig::SetWorldPose(const NkMat4f* worldMats, uint32 count) {
            mBoneMatrices.Clear();
            if (!worldMats || count == 0) return;
            mBoneMatrices.Reserve(count);
            for (uint32 i = 0; i < count; ++i) mBoneMatrices.PushBack(worldMats[i]);
        }

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
            for (auto& kv : mRigs) memory::NkGetDefaultAllocator().Delete(kv.Second);
            mRigs.Clear();
            mReady = false;
        }

        NkIKRig* NkIKSystem::CreateRig(uint64 skeletonId) {
            auto* existing = mRigs.Find(skeletonId);
            if (existing) return *existing;
            NkIKRig* rig = memory::NkGetDefaultAllocator().New<NkIKRig>(skeletonId);
            mRigs.Insert(skeletonId, rig);
            return rig;
        }

        void NkIKSystem::DestroyRig(NkIKRig*& rig) {
            if (!rig) return;
            mRigs.Erase(rig->GetSkeletonId());
            memory::NkGetDefaultAllocator().Delete(rig);
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

        // ── Helpers partagés (positions monde + write-back bind-fidèle b+) ────
        // Construit les positions MONDE de la chaîne depuis bones[boneIdx] et déduit
        // les longueurs de segment manquantes. Retourne n.
        static uint32 BuildChainPositions(NkVector<NkIKBone>& descBones,
                                          const NkVector<NkMat4f>& bones,
                                          NkVector<NkVec3f>& pos) {
            uint32 n = (uint32)descBones.Size();
            pos.Clear();
            for (uint32 i = 0; i < n; ++i) {
                uint32 bi = descBones[i].boneIdx;
                if (bi < bones.Size()) { const NkMat4f& m = bones[bi];
                    pos.PushBack({m.position.x, m.position.y, m.position.z}); }
                else pos.PushBack({0,0,0});
            }
            for (uint32 i = 1; i < n; ++i)
                if (descBones[i].length <= 1e-6f) {
                    NkVec3f d = {pos[i].x-pos[i-1].x, pos[i].y-pos[i-1].y, pos[i].z-pos[i-1].z};
                    descBones[i].length = sqrtf(d.x*d.x+d.y*d.y+d.z*d.z);
                }
            return n;
        }

        // Réécrit les matrices MONDE des os = translation résolue + rotation
        // BIND-FIDÈLE : delta monde (bindDir->newDir) appliqué sur la rotation bind
        // locale de l'os (bones[bi] sans translation) -> préserve le twist d'origine.
        // En bind pose (dir inchangée) le delta = identité -> matrice bind exacte.
        // L'effecteur (sans segment fils) suit le delta de son parent.
        static void WriteBackBindFidele(const NkVector<NkIKBone>& descBones, NkVector<NkMat4f>& bones,
                                        const NkVector<NkVec3f>& pos,
                                        const NkVector<NkVec3f>& bindPos) {
            uint32 n = (uint32)descBones.Size();
            NkQuatf lastDelta;  // identité
            for (uint32 i = 0; i < n; ++i) {
                uint32 bi = descBones[i].boneIdx;
                if (bi >= bones.Size()) continue;
                NkMat4f bindRot = bones[bi];
                bindRot.m30 = 0.f; bindRot.m31 = 0.f; bindRot.m32 = 0.f; bindRot.m33 = 1.f;
                NkQuatf delta = lastDelta;
                if (i + 1 < n) {
                    NkVec3f bd = { bindPos[i+1].x-bindPos[i].x, bindPos[i+1].y-bindPos[i].y, bindPos[i+1].z-bindPos[i].z };
                    NkVec3f nd = { pos[i+1].x-pos[i].x, pos[i+1].y-pos[i].y, pos[i+1].z-pos[i].z };
                    float32 bl = sqrtf(bd.x*bd.x+bd.y*bd.y+bd.z*bd.z);
                    float32 nl = sqrtf(nd.x*nd.x+nd.y*nd.y+nd.z*nd.z);
                    if (bl > 1e-6f && nl > 1e-6f) {
                        bd.x/=bl; bd.y/=bl; bd.z/=bl; nd.x/=nl; nd.y/=nl; nd.z/=nl;
                        delta = NkQuatf(bd, nd); lastDelta = delta;
                    }
                }
                bones[bi] = NkMat4f::Translate(pos[i]) * delta.ToMat4() * bindRot;
            }
        }

        void NkIKSystem::SolveChain_TwoBone(NkIKRig::ChainEntry& chain,
                                            NkVector<NkMat4f>& bones) {
            // Two-bone analytique (bras / jambe) : résout les 3 premiers joints
            // (racine, milieu, effecteur) par loi des cosinus + pôle, puis write-back
            // bind-fidèle. Les joints au-delà (chaîne >3) suivent le dernier segment.
            if (chain.desc.bones.Size() < 2) return;
            NkVector<NkVec3f> pos;
            uint32 n = BuildChainPositions(chain.desc.bones, bones, pos);
            NkVector<NkVec3f> bindPos = pos;

            const uint32 iMid = 1u;
            const uint32 iEnd = (n >= 3) ? 2u : 1u;
            NkVec3f root   = pos[0];
            NkVec3f target = chain.target.position;
            float32 la = chain.desc.bones[iMid].length;            // root -> mid
            float32 lb = (n >= 3) ? chain.desc.bones[iEnd].length : 0.f; // mid -> end

            NkVec3f toT = { target.x-root.x, target.y-root.y, target.z-root.z };
            float32 dist = sqrtf(toT.x*toT.x+toT.y*toT.y+toT.z*toT.z);
            if (dist < 1e-6f || la < 1e-6f) { WriteBackBindFidele(chain.desc.bones, bones, pos, bindPos); return; }
            NkVec3f dirT = { toT.x/dist, toT.y/dist, toT.z/dist };

            float32 reach = la + lb;
            float32 d = dist;
            if (d > reach) d = reach;
            if (d < fabsf(la - lb)) d = fabsf(la - lb) + 1e-5f;

            // Projection racine->milieu sur dirT (loi des cosinus) + hauteur perpend.
            float32 a = (lb > 1e-6f) ? (la*la - lb*lb + d*d) / (2.f*d) : la;
            float32 h2 = la*la - a*a;
            float32 h  = (h2 > 0.f) ? sqrtf(h2) : 0.f;

            // Direction du coude : pôle si fourni, sinon direction bind du milieu.
            NkVec3f ref = (chain.target.usePole) ? chain.target.poleVector
                : NkVec3f{ bindPos[iMid].x-bindPos[0].x, bindPos[iMid].y-bindPos[0].y, bindPos[iMid].z-bindPos[0].z };
            float32 rl = sqrtf(ref.x*ref.x+ref.y*ref.y+ref.z*ref.z);
            if (rl < 1e-6f) ref = {0,1,0}; else { ref.x/=rl; ref.y/=rl; ref.z/=rl; }
            float32 dp = ref.x*dirT.x + ref.y*dirT.y + ref.z*dirT.z;
            NkVec3f perp = { ref.x-dirT.x*dp, ref.y-dirT.y*dp, ref.z-dirT.z*dp };
            float32 pl = sqrtf(perp.x*perp.x+perp.y*perp.y+perp.z*perp.z);
            if (pl < 1e-6f) perp = {0,1,0}; else { perp.x/=pl; perp.y/=pl; perp.z/=pl; }

            pos[iMid] = { root.x + dirT.x*a + perp.x*h,
                          root.y + dirT.y*a + perp.y*h,
                          root.z + dirT.z*a + perp.z*h };
            pos[iEnd] = { root.x + dirT.x*d, root.y + dirT.y*d, root.z + dirT.z*d };

            // Joints supplémentaires (chaîne > 3) : prolongent le dernier segment.
            for (uint32 i = iEnd + 1; i < n; ++i) {
                NkVec3f pv = { pos[i-1].x-pos[i-2].x, pos[i-1].y-pos[i-2].y, pos[i-1].z-pos[i-2].z };
                float32 vl = sqrtf(pv.x*pv.x+pv.y*pv.y+pv.z*pv.z);
                if (vl > 1e-6f) { pv.x/=vl; pv.y/=vl; pv.z/=vl; }
                float32 len = chain.desc.bones[i].length;
                pos[i] = { pos[i-1].x+pv.x*len, pos[i-1].y+pv.y*len, pos[i-1].z+pv.z*len };
            }
            WriteBackBindFidele(chain.desc.bones, bones, pos, bindPos);
        }

        void NkIKSystem::SolveChain_CCD(NkIKRig::ChainEntry& chain,
                                        NkVector<NkMat4f>& bones) {
            // Cyclic Coordinate Descent : à chaque passe (effecteur -> racine), on
            // fait pivoter le sous-bras [i+1..n-1] autour du joint i pour aligner
            // l'effecteur sur la cible. Rotations rigides -> longueurs préservées.
            if (chain.desc.bones.Empty()) return;
            NkVector<NkVec3f> pos;
            uint32 n = BuildChainPositions(chain.desc.bones, bones, pos);
            NkVector<NkVec3f> bindPos = pos;
            if (n < 2) { WriteBackBindFidele(chain.desc.bones, bones, pos, bindPos); return; }

            NkVec3f target = chain.target.position;
            uint32  iters  = chain.desc.maxIterations;
            for (uint32 iter = 0; iter < iters; ++iter) {
                for (int32 i = (int32)n - 2; i >= 0; --i) {
                    NkVec3f P  = pos[(uint32)i];
                    NkVec3f E  = pos[n-1];
                    NkVec3f aE = { E.x-P.x, E.y-P.y, E.z-P.z };
                    NkVec3f aT = { target.x-P.x, target.y-P.y, target.z-P.z };
                    float32 le = sqrtf(aE.x*aE.x+aE.y*aE.y+aE.z*aE.z);
                    float32 lt = sqrtf(aT.x*aT.x+aT.y*aT.y+aT.z*aT.z);
                    if (le < 1e-6f || lt < 1e-6f) continue;
                    aE.x/=le; aE.y/=le; aE.z/=le; aT.x/=lt; aT.y/=lt; aT.z/=lt;
                    NkQuatf q(aE, aT);  // rotation minimale aE -> aT
                    for (uint32 j = (uint32)i + 1; j < n; ++j) {
                        NkVec3f rel = { pos[j].x-P.x, pos[j].y-P.y, pos[j].z-P.z };
                        NkVec3f rr  = q * rel;
                        pos[j] = { P.x+rr.x, P.y+rr.y, P.z+rr.z };
                    }
                }
                NkVec3f d = { pos[n-1].x-target.x, pos[n-1].y-target.y, pos[n-1].z-target.z };
                if (sqrtf(d.x*d.x+d.y*d.y+d.z*d.z) < chain.desc.tolerance) break;
            }
            WriteBackBindFidele(chain.desc.bones, bones, pos, bindPos);
        }

        void NkIKSystem::SolveChain_FABRIK(NkIKRig::ChainEntry& chain,
                                            NkVector<NkMat4f>& bones) {
            if (chain.desc.bones.Empty()) return;
            NkVec3f target = chain.target.position;
            uint32  iters  = chain.desc.maxIterations;

            // Positions MONDE courantes (pose fournie par SetWorldPose) + longueurs.
            NkVector<NkVec3f> pos;
            uint32 n = BuildChainPositions(chain.desc.bones, bones, pos);
            NkVec3f root = pos[0];                 // racine MONDE (ancrage fixe)
            NkVector<NkVec3f> bindPos = pos;        // positions BIND pour le write-back (b+)

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
            WriteBackBindFidele(chain.desc.bones, bones, pos, bindPos);
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
