// =============================================================================
// NkOffscreenTarget.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkOffscreenTarget.h"
#include "../../Core/NkTextureLibrary.h"
#include <cstring>

namespace nkentseu {
namespace renderer {

    bool NkOffscreenTarget::Init(NkIDevice* d, NkTextureLibrary* t,
                                   const NkOffscreenDesc& desc) {
        mDevice = d; mTexLib = t; mDesc = desc;

        NkGPUFormat colorFmt = desc.hdr ? NkGPUFormat::NK_RGBA16F : desc.colorFmt;

        // Créer color target
        mColor = mTexLib->CreateRenderTarget(desc.width, desc.height,
                                               colorFmt, false, desc.readable,
                                               desc.name + "_Color");
        if (!mColor.IsValid()) return false;

        // Créer depth target si nécessaire
        if (desc.hasDepth) {
            mDepth = mTexLib->CreateRenderTarget(desc.width, desc.height,
                                                   desc.depthFmt, true, desc.readable,
                                                   desc.name + "_Depth");
        }

        // Framebuffer
        NkFramebufferDesc fbd;
        fbd.width         = desc.width;
        fbd.height        = desc.height;
        fbd.colorAttach   = mTexLib->GetRHIHandle(mColor);
        fbd.depthAttach   = desc.hasDepth ? mTexLib->GetRHIHandle(mDepth) : NkTextureHandle{};
        fbd.name          = desc.name;
        mFBO = mDevice->CreateFramebuffer(fbd);

        // Render pass
        NkRenderPassDesc rpd;
        rpd.colorFormat   = colorFmt;
        rpd.depthFormat   = desc.hasDepth ? desc.depthFmt : NkGPUFormat::NK_NONE;
        rpd.loadOp        = NkLoadOp::NK_CLEAR;
        rpd.storeOp       = NkStoreOp::NK_STORE;
        mRP = mDevice->CreateRenderPass(rpd);

        // Readback buffer (staging CPU) si demandé
        if (desc.readback) {
            NkBufferDesc bd;
            bd.size  = desc.width * desc.height * 4; // RGBA8
            bd.type  = NkBufferType::NK_READBACK;
            bd.usage = NkBufferUsage::NK_STAGING;
            mReadBuf = mDevice->CreateBuffer(bd);
        }

        mValid = true;
        return true;
    }

    void NkOffscreenTarget::Shutdown() {
        if (!mValid) return;
        if (mFBO.IsValid())     mDevice->DestroyFramebuffer(mFBO);
        if (mRP.IsValid())      mDevice->DestroyRenderPass(mRP);
        if (mReadBuf.IsValid()) mDevice->DestroyBuffer(mReadBuf);
        if (mColor.IsValid())   mTexLib->Release(mColor);
        if (mDepth.IsValid())   mTexLib->Release(mDepth);
        mValid = false;
    }

    void NkOffscreenTarget::BeginCapture(NkICommandBuffer* cmd,
                                           bool clearColor, NkVec4f cc, bool clearDepth) {
        if (!mValid || mCapturing) return;

        // Transition color → RENDER_TARGET
        cmd->TransitionTexture(mTexLib->GetRHIHandle(mColor),
                                NkResourceState::NK_SHADER_RESOURCE,
                                NkResourceState::NK_RENDER_TARGET);

        NkRenderPassBeginDesc rb;
        rb.framebuffer     = mFBO;
        rb.renderPass      = mRP;
        rb.clearColor      = clearColor;
        rb.clearColorValue = cc;
        rb.clearDepth      = clearDepth;
        rb.clearDepthValue = 1.0f;
        cmd->BeginRenderPass(rb);

        cmd->SetViewport(0, 0, mDesc.width, mDesc.height);
        cmd->SetScissor(0, 0, mDesc.width, mDesc.height);

        mCapturing = true;
    }

    void NkOffscreenTarget::EndCapture(NkICommandBuffer* cmd) {
        if (!mCapturing) return;
        cmd->EndRenderPass();

        // Transition color → SHADER_RESOURCE (pour l'utiliser comme texture)
        cmd->TransitionTexture(mTexLib->GetRHIHandle(mColor),
                                NkResourceState::NK_RENDER_TARGET,
                                NkResourceState::NK_SHADER_RESOURCE);
        mCapturing = false;
    }

    bool NkOffscreenTarget::ReadbackPixels(uint8* dst, uint32 rowPitch) {
        if (!mValid || !mReadBuf.IsValid() || !dst) return false;

        // Copy texture → staging buffer (bloquant)
        mDevice->CopyTextureToBuffer(mTexLib->GetRHIHandle(mColor), mReadBuf);
        mDevice->WaitIdle();

        // Map staging buffer → copier vers dst
        uint32 rp = (rowPitch > 0) ? rowPitch : mDesc.width * 4;
        void* mapped = mDevice->MapBuffer(mReadBuf);
        if (!mapped) return false;
        for (uint32 row = 0; row < mDesc.height; row++)
            memcpy(dst + row * rp,
                   (uint8*)mapped + row * mDesc.width * 4,
                   mDesc.width * 4);
        mDevice->UnmapBuffer(mReadBuf);
        return true;
    }

    bool NkOffscreenTarget::Resize(uint32 w, uint32 h) {
        if (w == mDesc.width && h == mDesc.height) return true;
        NkOffscreenDesc d = mDesc;
        d.width = w; d.height = h;
        Shutdown();
        return Init(mDevice, mTexLib, d);
    }

} // namespace renderer
} // namespace nkentseu
