#pragma once
// =============================================================================
// NkEditorRHIRenderer.h — impl NKRHI de NkIEditorRenderer (UI sur NKRHI/NKRenderer).
//
// Pilote le frame au niveau DEVICE (modèle démo Base04 NkUIDemo) : device +
// swapchain + render pass backbuffer, et rend les draw-lists NKGui via
// NkGuiRHIBackend. C'est le backend de l'app d'animation / moteur (PAS NKCanvas).
//
// Expose GetDevice() / GetBackend() pour que le viewport 3D (NkRenderer offscreen)
// PARTAGE ce device et publie sa texture via NkGuiRHIBackend::RegisterTexture.
// =============================================================================

#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKGui/NkGuiRHIBackend.h"            // Integrations/NKGui
#include "NKWindow/NKWindow.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKLogger/NkLog.h"

namespace nkanima {

    using namespace nkentseu;

    class NkEditorRHIRenderer final : public editorkit::NkIEditorRenderer {
        public:
            bool Init(NkWindow& window, editorkit::NkEditorGfxApi api) override {
                NkDeviceInitInfo di;
                switch (api) {
                    case editorkit::NkEditorGfxApi::OpenGL:   di.api = NkGraphicsApi::NK_GFX_API_OPENGL;   break;
                    case editorkit::NkEditorGfxApi::Vulkan:   di.api = NkGraphicsApi::NK_GFX_API_VULKAN;   break;
                    case editorkit::NkEditorGfxApi::DX11:     di.api = NkGraphicsApi::NK_GFX_API_DX11;     break;
                    case editorkit::NkEditorGfxApi::DX12:     di.api = NkGraphicsApi::NK_GFX_API_DX12;     break;
                    case editorkit::NkEditorGfxApi::Software: di.api = NkGraphicsApi::NK_GFX_API_SOFTWARE; break;
                    default:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                        di.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
                        di.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
                        break;
                }
                di.surface = window.GetSurfaceDesc();
                di.width   = window.GetSize().width;
                di.height  = window.GetSize().height;
                di.context.swapchainFormat = NkSwapchainFormat::NK_SWAPCHAIN_BGRA8_UNORM;

                mDev = NkDeviceFactory::CreateWithFallback(
                    di, { di.api, NkGraphicsApi::NK_GFX_API_VULKAN, NkGraphicsApi::NK_GFX_API_OPENGL });
                if (!mDev || !mDev->IsValid()) { logger.Errorf("[NkEditorRHI] device echec\n"); return false; }

                mCmd = mDev->CreateCommandBuffer();
                if (!mCmd) { logger.Errorf("[NkEditorRHI] command buffer echec\n"); return false; }

                if (!mBackend.Init(mDev, mDev->GetSwapchainRenderPass(), mDev->GetApi())) {
                    logger.Errorf("[NkEditorRHI] backend NKGui echec\n"); return false;
                }
                logger.Info("[NkEditorRHI] pret : API={0}\n", NkGraphicsApiName(mDev->GetApi()));
                return true;
            }

            void Shutdown() override {
                if (mDev) mDev->WaitIdle();
                mBackend.Destroy();
                if (mDev) { NkDeviceFactory::Destroy(mDev); mDev = nullptr; }
                mCmd = nullptr;
            }
            bool IsValid() const override { return mDev && mDev->IsValid(); }

            math::NkVec2u Size() const override {
                if (!mDev) return { 0, 0 };
                return { mDev->GetSwapchainWidth(), mDev->GetSwapchainHeight() };
            }
            void OnResize(uint32 w, uint32 h) override { if (mDev) mDev->OnResize(w, h); }

            void BeginFrame() override {
                mFrameOk = false;
                if (!mDev || !mCmd) return;
                if (!mDev->BeginFrame(mFrame)) return;
                const uint32 w = mDev->GetSwapchainWidth(), h = mDev->GetSwapchainHeight();
                if (w == 0 || h == 0) { mDev->EndFrame(mFrame); return; }

                mCmd->Reset();
                if (!mCmd->Begin()) { mDev->EndFrame(mFrame); return; }

                // Hook viewport 3D (C2) : rendu offscreen sur le MÊME cmd, APRÈS Begin()
                // (sinon Reset() efface ses commandes) et AVANT la passe backbuffer
                // (ses BeginCapture/EndCapture ouvrent leurs propres passes ; pas de
                // render pass imbriquée avec celle du backbuffer).
                if (mPreUI) mPreUI(mCmd, mPreUIUser);

                if (!mCmd->BeginRenderPass(mDev->GetSwapchainRenderPass(),
                                           mDev->GetSwapchainFramebuffer(),
                                           NkRect2D{ 0, 0, (int32)w, (int32)h })) {
                    mCmd->End(); mDev->EndFrame(mFrame); return;
                }
                mCmd->SetViewport(NkViewport(0.f, 0.f, (float32)w, (float32)h));
                mCmd->SetScissor(NkRect2D(0u, 0u, w, h));
                mFrameOk = true;
            }

            void SubmitDrawList(const nkgui::NkGuiDrawList& dl, uint32 fbW, uint32 fbH) override {
                if (mFrameOk) mBackend.Submit(mCmd, dl, fbW, fbH);
            }

            void EndFrame() override {
                if (!mDev) return;
                if (mFrameOk) {
                    mCmd->EndRenderPass();
                    mCmd->End();
                    mDev->SubmitAndPresent(mCmd);
                }
                mDev->EndFrame(mFrame);
                mFrameOk = false;
            }

            bool UploadFontGray8(uint32 texId, const uint8* px, int32 w, int32 h) override {
                return mBackend.UploadTextureGray8(texId, px, w, h);
            }
            bool UploadImageRGBA(uint32 texId, const uint8* px, int32 w, int32 h) override {
                return mBackend.UploadTextureRGBA8(texId, px, w, h);
            }

            // ── Partage pour le viewport 3D (C2) ─────────────────────────────────
            NkIDevice*               GetDevice()  noexcept { return mDev; }
            nkgui::NkGuiRHIBackend&  GetBackend() noexcept { return mBackend; }
            // Callback appelé en début de frame (device frame ouvert, AVANT la passe
            // backbuffer) : le viewport 3D y rend sa scène dans son offscreen.
            using PreUIFn = void(*)(NkICommandBuffer* cmd, void* user);
            void SetPreUI(PreUIFn fn, void* user) noexcept { mPreUI = fn; mPreUIUser = user; }

        private:
            NkIDevice*              mDev = nullptr;
            NkICommandBuffer*       mCmd = nullptr;
            nkgui::NkGuiRHIBackend  mBackend;
            NkFrameContext          mFrame{};
            bool                    mFrameOk = false;
            PreUIFn                 mPreUI = nullptr;
            void*                   mPreUIUser = nullptr;
    };

} // namespace nkanima
