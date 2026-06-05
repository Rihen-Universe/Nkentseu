#include "pch.h"
// =============================================================================
// NkPixolBuffer.cpp  — NKRenderer v5.0  (Tools/PixolSculpt/)
// Creation reelle des storage images 2D (NK_UNORDERED_ACCESS) + import graph.
// =============================================================================
#include "NKRenderer/Tools/PixolSculpt/NkPixolBuffer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRenderer/Core/NkRenderGraph.h"

namespace nkentseu {
    namespace renderer {

        // Cree une storage image 2D (lecture/ecriture compute).
        static NkTextureHandle MakeStorage2D(NkIDevice* dev, uint32 w, uint32 h, NkGPUFormat fmt) {
            NkTextureDesc d;
            d.type      = NkTextureType::NK_TEX2D;
            d.format    = fmt;
            d.width     = w;
            d.height    = h;
            d.depth     = 1;
            d.bindFlags = NkBindFlags::NK_SHADER_RESOURCE | NkBindFlags::NK_UNORDERED_ACCESS;
            return dev->CreateTexture(d);
        }

        NkPixolBuffer::~NkPixolBuffer() noexcept { Shutdown(); }

        bool NkPixolBuffer::Init(NkIDevice* device, uint32 width, uint32 height,
                                 const NkPixolSculptConfig& cfg) noexcept {
            mDevice = device;
            mCfg    = cfg;
            mWidth  = width;
            mHeight = height;
            mReady  = CreateTargets();
            return mReady;
        }

        void NkPixolBuffer::Shutdown() noexcept {
            DestroyTargets();
            mReady  = false;
            mDevice = nullptr;
            mWidth = mHeight = 0;
        }

        bool NkPixolBuffer::Resize(uint32 width, uint32 height) noexcept {
            if (width == mWidth && height == mHeight) return mReady;
            DestroyTargets();
            mWidth  = width;
            mHeight = height;
            mReady  = CreateTargets();
            return mReady;
        }

        void NkPixolBuffer::Clear(NkICommandBuffer* cmd) noexcept {
            (void)cmd;
            // TODO(PixolSculpt): clear via petit kernel (depth -> sentinelle, mask -> 0).
        }

        bool NkPixolBuffer::CreateTargets() noexcept {
            if (!mDevice || mWidth == 0 || mHeight == 0) return false;
            mDepth    = MakeStorage2D(mDevice, mWidth, mHeight, mCfg.formats.depth);
            mNormal   = MakeStorage2D(mDevice, mWidth, mHeight, mCfg.formats.normal);
            mMaterial = MakeStorage2D(mDevice, mWidth, mHeight, mCfg.formats.material);
            bool ok = mDepth.IsValid() && mNormal.IsValid() && mMaterial.IsValid();
            if (mCfg.enableColor) {
                mColor = MakeStorage2D(mDevice, mWidth, mHeight, mCfg.formats.color);
                ok = ok && mColor.IsValid();
            }
            if (mCfg.enableMask) {
                mMask = MakeStorage2D(mDevice, mWidth, mHeight, mCfg.formats.mask);
                ok = ok && mMask.IsValid();
            }
            return ok;
        }

        void NkPixolBuffer::DestroyTargets() noexcept {
            if (mDevice) {
                if (mDepth.IsValid())    mDevice->DestroyTexture(mDepth);
                if (mNormal.IsValid())   mDevice->DestroyTexture(mNormal);
                if (mMaterial.IsValid()) mDevice->DestroyTexture(mMaterial);
                if (mColor.IsValid())    mDevice->DestroyTexture(mColor);
                if (mMask.IsValid())     mDevice->DestroyTexture(mMask);
            }
            mDepth = mNormal = mMaterial = mColor = mMask = NkTextureHandle{};
            mResDepth = mResNormal = mResColor = mResMask = 0;
        }

        void NkPixolBuffer::ImportToGraph(NkRenderGraph* graph) noexcept {
            if (!graph) return;
            mResDepth  = graph->ImportTexture("Pixol_Depth",  mDepth,
                                              NkResourceState::NK_UNORDERED_ACCESS);
            mResNormal = graph->ImportTexture("Pixol_Normal", mNormal,
                                              NkResourceState::NK_UNORDERED_ACCESS);
            if (mColor.IsValid())
                mResColor = graph->ImportTexture("Pixol_Color", mColor,
                                                 NkResourceState::NK_UNORDERED_ACCESS);
            if (mMask.IsValid())
                mResMask = graph->ImportTexture("Pixol_Mask", mMask,
                                                NkResourceState::NK_UNORDERED_ACCESS);
        }

    } // namespace renderer
} // namespace nkentseu
