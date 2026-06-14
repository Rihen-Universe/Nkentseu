#include "pch.h"
// =============================================================================
// NkVoxelVolume.cpp  — NKRenderer v5.0  (Tools/Voxel/)
// Creation reelle des storage images 3D (NK_UNORDERED_ACCESS) + import graph.
// =============================================================================
#include "NKRenderer/Tools/Voxel/NkVoxelVolume.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRenderer/Core/NkRenderGraph.h"

namespace nkentseu {
    namespace renderer {

        NkVoxelVolume::~NkVoxelVolume() noexcept { Shutdown(); }

        bool NkVoxelVolume::Init(NkIDevice* device, const NkVoxelConfig& cfg) noexcept {
            mDevice = device;
            mCfg    = cfg;
            mReady  = CreateTargets();
            return mReady;
        }

        void NkVoxelVolume::Shutdown() noexcept {
            DestroyTargets();
            mReady  = false;
            mDevice = nullptr;
        }

        void NkVoxelVolume::Clear(NkICommandBuffer* cmd) noexcept {
            (void)cmd;
            // TODO(Voxel): clear via petit kernel "clear" (Dispatch3D) ou clear device,
            //   density -> 0 (vide), color -> 0. Penser au UAVBarrier apres.
        }

        bool NkVoxelVolume::CreateTargets() noexcept {
            if (!mDevice || mCfg.dimX == 0 || mCfg.dimY == 0 || mCfg.dimZ == 0) return false;

            // Tex3D() positionne deja bindFlags = SHADER_RESOURCE | UNORDERED_ACCESS.
            NkTextureDesc dd = NkTextureDesc::Tex3D(mCfg.dimX, mCfg.dimY, mCfg.dimZ, mCfg.formats.density);
            mDensity = mDevice->CreateTexture(dd);

            NkTextureDesc dm = NkTextureDesc::Tex3D(mCfg.dimX, mCfg.dimY, mCfg.dimZ, mCfg.formats.material);
            mMaterial = mDevice->CreateTexture(dm);

            bool ok = mDensity.IsValid() && mMaterial.IsValid();

            if (mCfg.enableColor) {
                NkTextureDesc dc = NkTextureDesc::Tex3D(mCfg.dimX, mCfg.dimY, mCfg.dimZ, mCfg.formats.color);
                mColor = mDevice->CreateTexture(dc);
                ok = ok && mColor.IsValid();
            }
            return ok;
        }

        void NkVoxelVolume::DestroyTargets() noexcept {
            if (mDevice) {
                if (mDensity.IsValid())  mDevice->DestroyTexture(mDensity);
                if (mMaterial.IsValid()) mDevice->DestroyTexture(mMaterial);
                if (mColor.IsValid())    mDevice->DestroyTexture(mColor);
            }
            mDensity = mMaterial = mColor = NkTextureHandle{};
            mResDensity = mResMaterial = mResColor = 0;
        }

        void NkVoxelVolume::ImportToGraph(NkRenderGraph* graph) noexcept {
            if (!graph) return;
            mResDensity  = graph->ImportTexture("Voxel_Density",  mDensity,
                                                NkResourceState::NK_UNORDERED_ACCESS);
            mResMaterial = graph->ImportTexture("Voxel_Material", mMaterial,
                                                NkResourceState::NK_UNORDERED_ACCESS);
            if (mColor.IsValid())
                mResColor = graph->ImportTexture("Voxel_Color", mColor,
                                                 NkResourceState::NK_UNORDERED_ACCESS);
        }

    } // namespace renderer
} // namespace nkentseu
