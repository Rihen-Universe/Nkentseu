// =============================================================================
// NkOffscreenTarget.cpp — Rendu hors-écran (minimap, portails, captures)
// =============================================================================
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        bool NkOffscreenTarget::Init(NkIDevice* device, NkResourceManager* resources,
                                       const NkOffscreenDesc& desc)
        {
            if (!device || !resources) return false;
            mDevice    = device;
            mResources = resources;
            mDesc      = desc;

            return AllocTargets();
        }

        void NkOffscreenTarget::Shutdown() {
            FreeTargets();
            mDevice = nullptr;
        }

        bool NkOffscreenTarget::AllocTargets() {
            // Créer le color buffer HDR
            char dbgC[64]; snprintf(dbgC, 64, "%s_Color", mDesc.name ? mDesc.name : "Offscreen");
            mColorTex = mResources->CreateRenderTarget(
                mDesc.width, mDesc.height, mDesc.colorFormat, mDesc.msaa, dbgC);
            if (!mColorTex.IsValid()) return false;

            // Créer le depth buffer
            if (mDesc.hasDepth) {
                char dbgD[64]; snprintf(dbgD, 64, "%s_Depth", mDesc.name ? mDesc.name : "Offscreen");
                mDepthTex = mResources->CreateDepthTexture(
                    mDesc.width, mDesc.height, NkPixelFormat::NK_D32F, mDesc.msaa, dbgD);
            }

            // Créer le render target combiné
            NkRenderTargetDesc rtDesc{};
            rtDesc.width  = mDesc.width;
            rtDesc.height = mDesc.height;
            rtDesc.AddColor(mColorTex);
            if (mDesc.hasDepth && mDepthTex.IsValid())
                rtDesc.SetDepth(mDepthTex);
            rtDesc.debugName = mDesc.name;
            mRenderTarget = mResources->CreateRenderTarget(rtDesc);

            // Si résolution de MSAA demandée, créer la texture résolue
            if (mDesc.msaa != NkMSAA::NK_1X && mDesc.resolveAfterRender) {
                char dbgR[64]; snprintf(dbgR, 64, "%s_Resolved", mDesc.name ? mDesc.name : "Offscreen");
                mResolvedTex = mResources->CreateRenderTarget(
                    mDesc.width, mDesc.height, mDesc.colorFormat, NkMSAA::NK_1X, dbgR);
            }

            return mRenderTarget.IsValid();
        }

        void NkOffscreenTarget::FreeTargets() {
            if (mRenderTarget.IsValid()) {
                mResources->ReleaseRenderTarget(mRenderTarget);
                mRenderTarget = {};
            }
            if (mColorTex.IsValid()) {
                mResources->ReleaseTexture(mColorTex);
                mColorTex = {};
            }
            if (mDepthTex.IsValid()) {
                mResources->ReleaseTexture(mDepthTex);
                mDepthTex = {};
            }
            if (mResolvedTex.IsValid()) {
                mResources->ReleaseTexture(mResolvedTex);
                mResolvedTex = {};
            }
        }

        // =============================================================================
        void NkOffscreenTarget::BeginCapture(NkICommandBuffer* cmd,
                                               bool clearColor, NkColorF clearCol,
                                               bool clearDepth)
        {
            if (!cmd || !mRenderTarget.IsValid()) return;
            mCmd = cmd;
            NkClearFlags flags = NkClearFlags::NK_NONE;
            if (clearColor) flags |= NkClearFlags::NK_COLOR;
            if (clearDepth && mDesc.hasDepth) flags |= NkClearFlags::NK_DEPTH;
            cmd->BeginRenderTarget(mRenderTarget, flags, clearCol, 1.f, 0);
            cmd->SetViewport({0.f,0.f,(float32)mDesc.width,(float32)mDesc.height});
            cmd->SetScissor ({0,0,(int32)mDesc.width,(int32)mDesc.height});
        }

        void NkOffscreenTarget::EndCapture() {
            if (!mCmd) return;
            mCmd->EndRenderTarget();
            // Résoudre MSAA si nécessaire
            if (mDesc.msaa != NkMSAA::NK_1X && mDesc.resolveAfterRender
                && mResolvedTex.IsValid() && mColorTex.IsValid())
            {
                mCmd->ResolveTexture(mColorTex, mResolvedTex);
            }
            mCmd = nullptr;
        }

        // =============================================================================
        void NkOffscreenTarget::Resize(uint32 w, uint32 h) {
            if (w == mDesc.width && h == mDesc.height) return;
            mDesc.width  = w;
            mDesc.height = h;
            FreeTargets();
            AllocTargets();
        }

        NkTextureHandle NkOffscreenTarget::GetResult() const {
            // Retourner la texture résolue si MSAA, sinon la couleur brute
            if (mDesc.msaa != NkMSAA::NK_1X && mDesc.resolveAfterRender
                && mResolvedTex.IsValid())
                return mResolvedTex;
            return mColorTex;
        }

        bool NkOffscreenTarget::ReadbackPixels(void* outData, uint32 rowPitch) const {
            if (!mColorTex.IsValid() || !outData || !mDevice) return false;
            NkTextureHandle th; th.id = mResources->GetTextureRHIId(mColorTex);
            return mDevice->ReadbackTexture(th, outData, rowPitch);
        }

    } // namespace renderer
} // namespace nkentseu
