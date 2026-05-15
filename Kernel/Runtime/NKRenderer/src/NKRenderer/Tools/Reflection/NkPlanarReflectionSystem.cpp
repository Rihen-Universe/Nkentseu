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
                if (p.active) p.rt.Shutdown();
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
            return p.rt.Init(mDevice, mTexLib, rtDesc);
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
            logger.Info("[NkPlanarReflectionSystem] Registered plane idx={0} ({1}x{2} hdr={3})\n",
                        h.idx, desc.rtWidth, desc.rtHeight, desc.hdr ? 1 : 0);
            return h;
        }

        void NkPlanarReflectionSystem::Unregister(NkPlanarReflectionHandle handle) {
            if (!handle.IsValid() || handle.idx >= mPlanes.Size()) return;
            auto& p = mPlanes[handle.idx];
            if (p.active) { p.rt.Shutdown(); p.active = false; }
        }

        void NkPlanarReflectionSystem::Clear() {
            for (auto& p : mPlanes) if (p.active) p.rt.Shutdown();
            mPlanes.Clear();
        }

        void NkPlanarReflectionSystem::RenderReflections(NkICommandBuffer* cmd, NkRender3D* r3d) {
            if (!r3d || !r3d->IsInScene()) return;
            if (mPlanes.Empty()) return;

            // Reflection matrix pour un plan defini par N et un point P :
            //   M = I - 2*N*N^T  (sur la partie 3x3)
            //   et translation -2*(N.P)*N pour faire passer le plan par P.
            // Construite en column-major (NkMat4f).
            const NkSceneContext& ctx = r3d->GetSceneContext();
            const NkVec3f camPos = ctx.camera.GetPosition();

            for (auto& p : mPlanes) {
                if (!p.active) continue;

                const NkVec3f N = p.desc.normal.Normalized();
                const float32 d = -N.Dot(p.desc.point);
                // Sol-miroir physique unidirectionnel : on capture uniquement
                // les objets cote +N (au-dessus du plan pour Y=0 / N=+Y). Le
                // sol shader discard la face arriere -> vu de dessous on voit
                // directement les objets sans interception du sol.
                const NkVec3f Neff = N;
                const float32 deff = d;
                (void)camPos;  // plus utilise depuis qu'on n'inverse plus Neff

                // Matrice de reflexion (col-major) :
                //   col 0 = (1-2*Nx*Nx, -2*Ny*Nx,   -2*Nz*Nx,   0)
                //   col 1 = (-2*Nx*Ny,   1-2*Ny*Ny, -2*Nz*Ny,   0)
                //   col 2 = (-2*Nx*Nz,  -2*Ny*Nz,   1-2*Nz*Nz,  0)
                //   col 3 = (-2*deff*Nx, -2*deff*Ny, -2*deff*Nz, 1)
                NkMat4f mirror = NkMat4f::Identity();
                mirror[0][0] = 1.f - 2.f * Neff.x * Neff.x;
                mirror[0][1] =      -2.f * Neff.y * Neff.x;
                mirror[0][2] =      -2.f * Neff.z * Neff.x;
                mirror[1][0] =      -2.f * Neff.x * Neff.y;
                mirror[1][1] = 1.f - 2.f * Neff.y * Neff.y;
                mirror[1][2] =      -2.f * Neff.z * Neff.y;
                mirror[2][0] =      -2.f * Neff.x * Neff.z;
                mirror[2][1] =      -2.f * Neff.y * Neff.z;
                mirror[2][2] = 1.f - 2.f * Neff.z * Neff.z;
                mirror[3][0] = -2.f * deff * Neff.x;
                mirror[3][1] = -2.f * deff * Neff.y;
                mirror[3][2] = -2.f * deff * Neff.z;

                const NkMat4f mirrorViewProj = ctx.camera.GetViewProj() * mirror;

                // Met a jour le UBO Camera pour que le sol sample le bon RT (le shader
                // ReflFloor lit uCam.mirrorViewProj pour calculer reflUV).
                r3d->SetMirrorViewProj(mirrorViewProj);

                // Clip plane (Neff, deff) : ne capture que les drawcalls cote +Neff
                // (objets reellement situes du cote de la camera). Le sol lui-meme
                // (sur le plan, side=0) est filtre par "side <= 0" -> skip.
                const NkVec4f clipPlane{Neff.x, Neff.y, Neff.z, deff};

                // Capture + flush dans le RT
                p.rt.BeginRender(cmd);
                r3d->FlushIntoRT(cmd, p.rt.GetRenderPass(), mirror, mirrorViewProj, clipPlane);
                p.rt.EndRender(cmd);

                // Lie le RT au material cible (slot "albedo").
                if (mMatSys && p.desc.targetMaterial.IsValid()) {
                    auto* inst = mMatSys->GetInstance(p.desc.targetMaterial);
                    if (inst) {
                        // Note : on poke directement via le param "albedo" du material
                        // car le shader ReflFloor recoit le RT comme albedo sampler.
                        // SetTexture marquera l'instance dirty -> rebind set=2.
                        inst->SetTexture(NkString("albedo"), p.rt.GetColorHandle());
                    }
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu
