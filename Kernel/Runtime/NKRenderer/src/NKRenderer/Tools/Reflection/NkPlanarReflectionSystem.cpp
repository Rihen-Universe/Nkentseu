// =============================================================================
// NkPlanarReflectionSystem.cpp  — NKRenderer Tools/Reflection
// =============================================================================
#include "NkPlanarReflectionSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        bool NkPlanarReflectionSystem::Init(NkIDevice* dev, NkTextureLibrary* texLib,
                                            NkMaterialSystem* matSys) {
            mDevice = dev;
            mTexLib = texLib;
            mMatSys = matSys;
            return mDevice != nullptr;
        }

        void NkPlanarReflectionSystem::Shutdown() {
            for (auto& p : mPlanes) {
                if (p.active) {
                    p.rtPos.Shutdown();
                    p.rtNeg.Shutdown();
                }
            }
            mPlanes.Clear();
            mDevice = nullptr;
            mTexLib = nullptr;
            mMatSys = nullptr;
        }

        bool NkPlanarReflectionSystem::InitPlaneRT(Plane& p) {
            NkRenderTargetDesc rtDesc;
            rtDesc.width  = (p.desc.rtWidth  > 0) ? p.desc.rtWidth  : 512;
            rtDesc.height = (p.desc.rtHeight > 0) ? p.desc.rtHeight : 256;
            rtDesc.hdr    = p.desc.hdr;
            rtDesc.depth  = true;
            rtDesc.name   = p.desc.debugName.Empty() ? NkString("PlanarReflection") : p.desc.debugName;
            if (!p.rtPos.Init(mDevice, mTexLib, rtDesc)) return false;

            // 2eme RT pour la face arriere si twoSided ou faceMode == BOTH.
            const bool needsBack = p.desc.twoSided
                                || p.desc.faceMode == NkPlanarFaceMode::BOTH
                                || p.desc.faceMode == NkPlanarFaceMode::BACK_ONLY;
            if (needsBack) {
                NkRenderTargetDesc rtDescNeg = rtDesc;
                rtDescNeg.name = rtDesc.name + NkString("_Back");
                if (!p.rtNeg.Init(mDevice, mTexLib, rtDescNeg)) return false;
            }
            return true;
        }

        NkPlanarReflectionHandle NkPlanarReflectionSystem::Register(const NkPlanarReflectionDesc& desc) {
            Plane p;
            p.desc = desc;
            if (!InitPlaneRT(p)) {
                logger.Warnf("[NkPlanarReflectionSystem] RT init failed (w=%u h=%u)\n",
                             desc.rtWidth, desc.rtHeight);
                return {};
            }
            p.active = true;
            mPlanes.PushBack(p);
            NkPlanarReflectionHandle h;
            h.idx = (uint32)mPlanes.Size() - 1u;
            const bool needsBack = p.desc.twoSided
                                || p.desc.faceMode == NkPlanarFaceMode::BOTH
                                || p.desc.faceMode == NkPlanarFaceMode::BACK_ONLY;
            logger.Info("[NkPlanarReflectionSystem] Registered plane idx={0} ({1}x{2} hdr={3} twoSided={4})\n",
                        h.idx, desc.rtWidth, desc.rtHeight, desc.hdr ? 1 : 0,
                        needsBack ? 1 : 0);
            return h;
        }

        void NkPlanarReflectionSystem::Unregister(NkPlanarReflectionHandle handle) {
            if (!handle.IsValid() || handle.idx >= mPlanes.Size()) return;
            auto& p = mPlanes[handle.idx];
            if (p.active) {
                p.rtPos.Shutdown();
                p.rtNeg.Shutdown();
                p.active = false;
            }
        }

        void NkPlanarReflectionSystem::Clear() {
            for (auto& p : mPlanes) if (p.active) {
                p.rtPos.Shutdown();
                p.rtNeg.Shutdown();
            }
            mPlanes.Clear();
        }

        // Construit la matrice de reflexion 4x4 pour le plan (N, d).
        //   M = I - 2*N*N^T (3x3) + translation -2*d*N en colonne 3.
        // NkMat4f est column-major : m[col][row].
        static NkMat4f BuildMirrorMatrix(const NkVec3f& N, float32 d) {
            NkMat4f m = NkMat4f::Identity();
            m[0][0] = 1.f - 2.f * N.x * N.x;
            m[0][1] =      -2.f * N.y * N.x;
            m[0][2] =      -2.f * N.z * N.x;
            m[1][0] =      -2.f * N.x * N.y;
            m[1][1] = 1.f - 2.f * N.y * N.y;
            m[1][2] =      -2.f * N.z * N.y;
            m[2][0] =      -2.f * N.x * N.z;
            m[2][1] =      -2.f * N.y * N.z;
            m[2][2] = 1.f - 2.f * N.z * N.z;
            m[3][0] = -2.f * d * N.x;
            m[3][1] = -2.f * d * N.y;
            m[3][2] = -2.f * d * N.z;
            return m;
        }

        void NkPlanarReflectionSystem::RenderReflections(NkICommandBuffer* cmd, NkRender3D* r3d) {
            if (!r3d || !r3d->IsInScene()) return;
            if (mPlanes.Empty()) return;

            const NkSceneContext& ctx = r3d->GetSceneContext();

            for (auto& p : mPlanes) {
                if (!p.active) continue;

                const NkVec3f N = p.desc.normal.Normalized();
                const float32 d = -N.Dot(p.desc.point);

                const bool needsBack = p.desc.twoSided
                                    || p.desc.faceMode == NkPlanarFaceMode::BOTH
                                    || p.desc.faceMode == NkPlanarFaceMode::BACK_ONLY;

                // ── Passe RT+ (face avant : objets cote +N) ──────────────────
                {
                    const NkMat4f mirror         = BuildMirrorMatrix(N, d);
                    const NkMat4f mirrorViewProj = ctx.camera.GetViewProj() * mirror;
                    const NkVec4f clipPlane{N.x, N.y, N.z, d};

                    r3d->SetMirrorViewProj(mirrorViewProj);
                    p.rtPos.BeginRender(cmd);
                    r3d->FlushIntoRT(cmd, p.rtPos.GetRenderPass(), mirror, mirrorViewProj, clipPlane);
                    p.rtPos.EndRender(cmd);
                }

                // ── Passe RT- (face arriere : objets cote -N, si twoSided) ───
                if (needsBack && p.rtNeg.IsValid()) {
                    const NkVec3f Nneg = N * -1.f;
                    const float32 dneg = -d;
                    const NkMat4f mirrorBack    = BuildMirrorMatrix(Nneg, dneg);
                    const NkMat4f mirrorVPBack  = ctx.camera.GetViewProj() * mirrorBack;
                    const NkVec4f clipPlaneBack{Nneg.x, Nneg.y, Nneg.z, dneg};

                    p.rtNeg.BeginRender(cmd);
                    r3d->FlushIntoRT(cmd, p.rtNeg.GetRenderPass(), mirrorBack, mirrorVPBack, clipPlaneBack);
                    p.rtNeg.EndRender(cmd);
                }

                // ── Bind RTs au material cible ───────────────────────────────
                // slot "albedo" (binding 3)   = RT+
                // slot "normal" (binding 4)   = RT- (reuse slot inutilise pour
                //                                   le sol miroir qui n'a pas
                //                                   de normal map)
                if (mMatSys && p.desc.targetMaterial.IsValid()) {
                    auto* inst = mMatSys->GetInstance(p.desc.targetMaterial);
                    if (inst) {
                        inst->SetTexture(NkString("albedo"), p.rtPos.GetColorHandle());
                        if (needsBack && p.rtNeg.IsValid())
                            inst->SetTexture(NkString("normal"), p.rtNeg.GetColorHandle());
                        // Pousse le faceMode dans le UBO PBR de l'instance.
                        // Le shader ReflFloor lit uFloor.reflFloorFaceMode pour
                        // decider du sample (front/back/both) et du discard.
                        inst->SetReflFloorFaceMode((int32)p.desc.faceMode);
                    }
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu
