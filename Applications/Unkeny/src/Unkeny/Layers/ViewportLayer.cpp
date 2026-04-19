#include "ViewportLayer.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/src/NKRenderer/Scene/NkRenderSystem.h"

namespace nkentseu {
    namespace Unkeny {

        ViewportLayer::ViewportLayer(const NkString& name,
                                     NkIDevice* device,
                                     NkICommandBuffer* cmd) noexcept
            : Layer(name), mDevice(device), mCmd(cmd) {}

        ViewportLayer::~ViewportLayer() { DestroyFBO(); }

        // =====================================================================
        void ViewportLayer::OnAttach() {
            nk_uint32 w = mDevice ? mDevice->GetSwapchainWidth()  : 1280;
            nk_uint32 h = mDevice ? mDevice->GetSwapchainHeight() : 720;
            CreateFBO(w, h);
            logger.Infof("[ViewportLayer] Attaché {}x{}\n", w, h);
        }

        void ViewportLayer::OnDetach() {
            if (mDevice) mDevice->WaitIdle();
            DestroyFBO();
            logger.Infof("[ViewportLayer] Détaché\n");
        }

        // =====================================================================
        void ViewportLayer::OnUpdate(float dt) {
            if (!mCamera) return;

            // Mettre à jour l'aspect ratio de la caméra si le FBO a changé
            if (mVpHeight > 0)
                mCamera->aspect = (float32)mVpWidth / (float32)mVpHeight;

            // Mettre à jour la caméra avec l'état souris du viewport
            mCamera->OnMouseMove(mMouse.dx, mMouse.dy,
                                  mMouse.altDown, mMouse.rightDown);
            if (mMouse.scroll != 0.f) {
                mCamera->OnMouseScroll(mMouse.scroll);
                mMouse.scroll = 0.f;
            }
            mCamera->Update(dt, mMouse.isHovered);

            // Mettre à jour les gizmos
            if (mGizmos && mWorld && mSel && mVpWidth > 0 && mVpHeight > 0) {
                mGizmos->Update(*mWorld, *mSel, *mCamera,
                                mMouse.x, mMouse.y,
                                (float32)mVpWidth, (float32)mVpHeight,
                                mMouse.leftDown && mMouse.isHovered);
            }

            // Reset des deltas
            mMouse.dx = 0; mMouse.dy = 0;
        }

        // =====================================================================
        void ViewportLayer::OnRender() {
            if (!mDevice || !mCmd) return;
            if (!mRenderPass.IsValid() || !mFramebuffer.IsValid()) return;
            if (mVpWidth == 0 || mVpHeight == 0) return;

            const NkRect2D area{0, 0,
                static_cast<nk_int32>(mVpWidth),
                static_cast<nk_int32>(mVpHeight)};

            mCmd->Reset();
            mCmd->Begin();

            if (mCmd->BeginRenderPass(mRenderPass, mFramebuffer, area)) {
                NkViewport vp{0.f, 0.f,
                    (nk_float32)mVpWidth, (nk_float32)mVpHeight, 0.f, 1.f};
                mCmd->SetViewport(vp);
                mCmd->SetScissor(area);

                // Rendre la scène ECS
                RenderScene();

                // Rendre les gizmos par-dessus
                RenderGizmos();

                mCmd->EndRenderPass();
            }

            mCmd->End();
        }

        // =====================================================================
        void ViewportLayer::RenderScene() noexcept {
            if (!mWorld || !mCamera) return;

            // Injecter les matrices caméra dans NkRenderScene
            mRenderScene.Clear();
            mRenderScene.viewMatrix     = mCamera->viewMatrix;
            mRenderScene.projMatrix     = mCamera->projMatrix;
            mRenderScene.viewProjMatrix = mCamera->viewProjMatrix;
            mRenderScene.cameraPosition = mCamera->position;

            // NkRenderSystem peuple mRenderScene depuis le monde ECS
            // (en production, NkRenderSystem tourne dans le NkScheduler)
            // Ici on l'invoque directement pour la Phase 3
            renderer::NkRenderSystem renderSys(&mRenderScene);
            // renderSys.Execute(*mWorld, 0.f); // TODO : quand NkScheduler est connecté

            // TODO Phase 3+ : NkRender3D::BeginScene(mRenderScene)
            //                  NkRender3D::Flush(mCmd)
        }

        void ViewportLayer::RenderGizmos() noexcept {
            if (!mGizmos) return;
            // TODO Phase 3+ : NkDebugRenderer::Submit(mGizmos->drawLines, mCmd)
            // Pour l'instant les lignes sont disponibles dans mGizmos->drawLines
        }

        // =====================================================================
        void ViewportLayer::ResizeFBO(nk_uint32 w, nk_uint32 h) noexcept {
            if (w == mVpWidth && h == mVpHeight) return;
            if (w == 0 || h == 0) return;
            if (mDevice) mDevice->WaitIdle();
            DestroyFBO();
            CreateFBO(w, h);
        }

        // =====================================================================
        bool ViewportLayer::CreateFBO(nk_uint32 w, nk_uint32 h) noexcept {
            if (!mDevice || w == 0 || h == 0) return false;
            mVpWidth = w; mVpHeight = h;

            // Texture couleur
            {
                NkTextureDesc td = NkTextureDesc::Tex2D(w, h, NkGPUFormat::NK_RGBA8_UNORM);
                td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE | NkBindFlags::NK_RENDER_TARGET;
                td.debugName = "ViewportColor";
                mColorTarget = mDevice->CreateTexture(td);
                if (!mColorTarget.IsValid()) return false;
            }
            // Texture profondeur
            {
                NkTextureDesc td = NkTextureDesc::Tex2D(w, h, NkGPUFormat::NK_D24_UNORM_S8_UINT);
                td.bindFlags = NkBindFlags::NK_DEPTH_STENCIL;
                td.debugName = "ViewportDepth";
                mDepthTarget = mDevice->CreateTexture(td);
                if (!mDepthTarget.IsValid()) return false;
            }
            // RenderPass
            {
                NkRenderPassDesc rpd;
                rpd.colorAttachments.PushBack({
                    NkGPUFormat::NK_RGBA8_UNORM, NkLoadOp::NK_CLEAR, NkStoreOp::NK_STORE});
                rpd.depthAttachment = {
                    NkGPUFormat::NK_D24_UNORM_S8_UINT, NkLoadOp::NK_CLEAR, NkStoreOp::NK_DONT_CARE};
                rpd.clearColor = {0.13f, 0.13f, 0.13f, 1.f};
                rpd.clearDepth = 1.f;
                mRenderPass = mDevice->CreateRenderPass(rpd);
                if (!mRenderPass.IsValid()) return false;
            }
            // Framebuffer
            {
                NkFramebufferDesc fbd;
                fbd.renderPass = mRenderPass;
                fbd.width      = w; fbd.height = h;
                fbd.colorTargets.PushBack(mColorTarget);
                fbd.depthTarget = mDepthTarget;
                mFramebuffer = mDevice->CreateFramebuffer(fbd);
                if (!mFramebuffer.IsValid()) return false;
            }

            logger.Infof("[ViewportLayer] FBO {}x{}\n", w, h);
            return true;
        }

        void ViewportLayer::DestroyFBO() noexcept {
            if (!mDevice) return;
            if (mFramebuffer.IsValid())  mDevice->DestroyFramebuffer(mFramebuffer);
            if (mRenderPass.IsValid())   mDevice->DestroyRenderPass(mRenderPass);
            if (mDepthTarget.IsValid())  mDevice->DestroyTexture(mDepthTarget);
            if (mColorTarget.IsValid())  mDevice->DestroyTexture(mColorTarget);
            mFramebuffer = {}; mRenderPass = {};
            mDepthTarget = {}; mColorTarget = {};
        }

    } // namespace Unkeny
} // namespace nkentseu
