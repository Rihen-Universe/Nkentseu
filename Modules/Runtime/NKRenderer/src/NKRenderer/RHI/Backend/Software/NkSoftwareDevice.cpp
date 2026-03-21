// =============================================================================
// NkSoftwareDevice.cpp — v4 final
// Corrections :
//   - NkLog2 → NkLog(x)/NkLog(2.f) (BUG 9)
//   - Present() via NkSoftwareContext (sans NkRhiContextView)
//   - CreateShader lit cpuVertFn/cpuFragFn/cpuCompFn comme const void*
// =============================================================================
#include "NkSoftwareDevice.h"
#include "NkSoftwareCommandBuffer.h"
#include "NkSoftwareSwapchain.h"
#include "NKRenderer/Context/Graphics/Software/NkSoftwareContext.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include <cstring>
#include <cstdio>

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

#define NK_SW_LOG(...) logger_src.Infof("[NkSoftwareDevice] " __VA_ARGS__)

namespace nkentseu {
using threading::NkScopedLock;
using namespace math;

// =============================================================================
NkSoftwareDevice::~NkSoftwareDevice() { if (mIsValid) Shutdown(); }

bool NkSoftwareDevice::Initialize(NkIGraphicsContext* ctx) {
    mCtx = ctx;

    mWidth  = 512u;
    mHeight = 512u;
    if (ctx) {
        const auto info = ctx->GetInfo();
        if (info.windowWidth  > 0) mWidth  = info.windowWidth;
        if (info.windowHeight > 0) mHeight = info.windowHeight;

        if (ctx->GetApi() == NkGraphicsApi::NK_API_SOFTWARE) {
            auto* swData = static_cast<NkSoftwareContextData*>(ctx->GetNativeContextData());
            if (swData) {
            if (swData->width  > 0) mWidth  = swData->width;
            if (swData->height > 0) mHeight = swData->height;
            }
        }
    }
    if (mWidth  == 0) mWidth  = 512u;
    if (mHeight == 0) mHeight = 512u;

    CreateSwapchainObjects();

    mCaps.computeShaders       = true;
    mCaps.drawIndirect         = false;
    mCaps.multiViewport        = false;
    mCaps.independentBlend     = true;
    mCaps.maxTextureDim2D      = 4096;
    mCaps.maxColorAttachments  = 4;
    mCaps.maxVertexAttributes  = 16;
    mCaps.maxPushConstantBytes = 256;
    mCaps.vramBytes            = 0;

    mIsValid = true;
    NK_SW_LOG("Initialisé (%u×%u)\n", mWidth, mHeight);
    return true;
}

void NkSoftwareDevice::CreateSwapchainObjects() {
    NkTextureDesc cd;
    cd.format    = NkGpuFormat::NK_RGBA8_UNORM;
    cd.width     = mWidth; cd.height = mHeight; cd.mipLevels = 1;
    cd.bindFlags = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
    cd.debugName = "SW_Backbuffer";
    auto colorH  = CreateTexture(cd);

    NkTextureDesc dd;
    dd.format    = NkGpuFormat::NK_D32_FLOAT;
    dd.width     = mWidth; dd.height = mHeight; dd.mipLevels = 1;
    dd.bindFlags = NkBindFlags::NK_DEPTH_STENCIL;
    dd.debugName = "SW_Depthbuffer";
    auto depthH  = CreateTexture(dd);

    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_RGBA8_UNORM))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP = CreateRenderPass(rpd);

    NkFramebufferDesc fbd;
    fbd.renderPass = mSwapchainRP;
    fbd.colorAttachments.PushBack(colorH);
    fbd.depthAttachment = depthH;
    fbd.width = mWidth; fbd.height = mHeight;
    mSwapchainFB = CreateFramebuffer(fbd);
}

void NkSoftwareDevice::Shutdown() {
    NkScopedLock lock(mMutex);
    mBuffers.Clear(); mTextures.Clear(); mSamplers.Clear();
    mShaders.Clear(); mPipelines.Clear(); mRenderPasses.Clear();
    mFramebuffers.Clear(); mDescLayouts.Clear(); mDescSets.Clear();
    mFences.Clear(); mSemaphores.Clear();
    mIsValid = false;
    NK_SW_LOG("Shutdown\n");
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkSoftwareDevice::CreateBuffer(const NkBufferDesc& desc) {
    NkScopedLock lock(mMutex);
    NkSWBuffer b; b.desc = desc;
    b.data.Resize((usize)desc.sizeBytes, (uint8)0);
    if (desc.initialData)
        memcpy(b.data.Data(), desc.initialData, (size_t)desc.sizeBytes);
    uint64 hid = NextId(); mBuffers[hid] = traits::NkMove(b);
    NkBufferHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyBuffer(NkBufferHandle& h) {
    NkScopedLock lock(mMutex); mBuffers.Erase(h.id); h.id = 0;
}
bool NkSoftwareDevice::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    NkSWBuffer* b = mBuffers.Find(buf.id); if (!b) return false;
    if (off + sz > b->data.Size()) return false;
    memcpy(b->data.Data() + off, data, (size_t)sz); return true;
}
bool NkSoftwareDevice::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    return WriteBuffer(buf, data, sz, off);
}
bool NkSoftwareDevice::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
    NkSWBuffer* b = mBuffers.Find(buf.id); if (!b) return false;
    if (off + sz > b->data.Size()) return false;
    memcpy(out, b->data.Data() + off, (size_t)sz); return true;
}
NkMappedMemory NkSoftwareDevice::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    NkSWBuffer* b = mBuffers.Find(buf.id); if (!b) return {};
    uint64 mapSz = sz > 0 ? sz : b->desc.sizeBytes - off;
    b->mapped = true;
    return { b->data.Data() + off, mapSz };
}
void NkSoftwareDevice::UnmapBuffer(NkBufferHandle buf) {
    NkSWBuffer* b = mBuffers.Find(buf.id); if (b) b->mapped = false;
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkSoftwareDevice::CreateTexture(const NkTextureDesc& desc) {
    NkScopedLock lock(mMutex);
    NkSWTexture t; t.desc = desc;
    t.isRenderTarget = NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET) ||
                       NkHasFlag(desc.bindFlags, NkBindFlags::NK_DEPTH_STENCIL);
    uint32 bpp = NkSWBpp(desc.format);

    // CORRECTION BUG 9 : NkLog2 → NkLog(x)/NkLog(2)
    uint32 mipCount = desc.mipLevels == 0
        ? (uint32)(NkFloor(math::NkLog((float)NkMax(desc.width, desc.height)) / math::NkLog(2.f)) + 1)
        : desc.mipLevels;

    uint32 w = desc.width, h = desc.height;
    for (uint32 m = 0; m < mipCount; m++) {
        uint32 pixW = NkMax(1u, w), pixH = NkMax(1u, h);
        uint32 sz   = pixW * pixH * bpp;
        NkVector<uint8> mipData; mipData.Resize(sz, (uint8)0);
        if (desc.format == NkGpuFormat::NK_D32_FLOAT) {
            float one = 1.f;
            for (uint32 i = 0; i < pixW * pixH; i++) memcpy(mipData.Data() + i*4, &one, 4);
        }
        t.mips.PushBack(traits::NkMove(mipData));
        w >>= 1; h >>= 1;
    }

    if (desc.initialData && !t.mips.IsEmpty()) {
        uint32 rp    = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
        uint32 copyH = (uint32)NkMin((float32)desc.height,
                                      (float32)(t.mips[0].Size() / NkMax(1u, rp)));
        for (uint32 y = 0; y < copyH; y++) {
            uint32 dOff = y * desc.width * bpp;
            uint32 sOff = y * rp;
            uint32 cW   = NkMin(desc.width * bpp, (uint32)t.mips[0].Size() - dOff);
            if (cW > 0)
                memcpy(t.mips[0].Data() + dOff, (const uint8*)desc.initialData + sOff, cW);
        }
    }

    uint64 hid = NextId(); mTextures[hid] = traits::NkMove(t);
    NkTextureHandle hth; hth.id = hid; return hth;
}
void NkSoftwareDevice::DestroyTexture(NkTextureHandle& h) {
    NkScopedLock lock(mMutex); mTextures.Erase(h.id); h.id = 0;
}
bool NkSoftwareDevice::WriteTexture(NkTextureHandle t, const void* pixels, uint32 rowPitch) {
    NkSWTexture* tex = mTextures.Find(t.id); if (!tex || tex->mips.IsEmpty()) return false;
    uint32 bpp = tex->Bpp(), stride = rowPitch > 0 ? rowPitch : tex->Width() * bpp;
    for (uint32 y = 0; y < tex->Height(); y++) {
        uint32 dOff = y * tex->Width() * bpp;
        if (dOff + tex->Width() * bpp > tex->mips[0].Size()) break;
        memcpy(tex->mips[0].Data() + dOff, (const uint8*)pixels + y * stride, tex->Width() * bpp);
    }
    return true;
}
bool NkSoftwareDevice::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x, uint32 y, uint32, uint32 w, uint32 h, uint32,
    uint32 mip, uint32, uint32 rowPitch) {
    NkSWTexture* tex = mTextures.Find(t.id);
    if (!tex || mip >= (uint32)tex->mips.Size()) return false;
    uint32 bpp = tex->Bpp(), stride = rowPitch > 0 ? rowPitch : w * bpp;
    for (uint32 row = 0; row < h; row++) {
        uint32 dy = y + row; if (dy >= tex->Height(mip)) break;
        uint32 dOff = (dy * tex->Width(mip) + x) * bpp;
        uint32 cW   = NkMin(w * bpp, (tex->Width(mip) - x) * bpp);
        if (dOff + cW > tex->mips[mip].Size()) break;
        memcpy(tex->mips[mip].Data() + dOff, (const uint8*)pixels + row * stride, cW);
    }
    return true;
}
bool NkSoftwareDevice::GenerateMipmaps(NkTextureHandle t, NkFilter) {
    NkSWTexture* tex = mTextures.Find(t.id); if (!tex) return false;
    for (uint32 m = 1; m < (uint32)tex->mips.Size(); m++) {
        uint32 dw = tex->Width(m), dh = tex->Height(m);
        for (uint32 py = 0; py < dh; py++)
            for (uint32 px = 0; px < dw; px++) {
                NkVec4f c = tex->Sample((px+.5f)/(float)dw, (py+.5f)/(float)dh, m-1);
                tex->Write(px, py, c, m);
            }
    }
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkSoftwareDevice::CreateSampler(const NkSamplerDesc& d) {
    NkScopedLock lock(mMutex);
    NkSWSampler samp; samp.desc = d;
    uint64 hid = NextId(); mSamplers[hid] = samp;
    NkSamplerHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroySampler(NkSamplerHandle& h) {
    NkScopedLock lock(mMutex); mSamplers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Shaders — lit cpuVertFn/cpuFragFn depuis NkShaderStageDesc
// =============================================================================
NkShaderHandle NkSoftwareDevice::CreateShader(const NkShaderDesc& desc) {
    NkScopedLock lock(mMutex);
    NkSWShader sh;
    for (uint32 i = 0; i < (uint32)desc.stages.Size(); i++) {
        auto& s = desc.stages[i];
        if (s.stage == NkShaderStage::NK_VERTEX && s.cpuVertFn)
            sh.vertFn = *static_cast<const NkSWVertexFn*>(s.cpuVertFn);
        if (s.stage == NkShaderStage::NK_FRAGMENT && s.cpuFragFn)
            sh.fragFn = *static_cast<const NkSWFragmentFn*>(s.cpuFragFn);
        if (s.stage == NkShaderStage::NK_COMPUTE  && s.cpuCompFn) {
            sh.isCompute = true;
            sh.computeFn = *static_cast<const NkSWComputeFn*>(s.cpuCompFn);
        }
    }
    if (!sh.fragFn && !sh.isCompute) sh.SetDefaultPassthrough();
    uint64 hid = NextId(); mShaders[hid] = traits::NkMove(sh);
    NkShaderHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyShader(NkShaderHandle& h) {
    NkScopedLock lock(mMutex); mShaders.Erase(h.id); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkSoftwareDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    NkScopedLock lock(mMutex);
    NkSWPipeline p;
    p.shaderId   = d.shader.id;
    p.isCompute  = false;
    p.depthTest  = d.depthStencil.depthTestEnable;
    p.depthWrite = d.depthStencil.depthWriteEnable;
    p.depthOp    = d.depthStencil.depthCompareOp;
    p.cullMode   = d.rasterizer.cullMode;
    p.frontFace  = d.rasterizer.frontFace;
    p.topology   = d.topology;
    p.vertexStride = (d.vertexLayout.bindings.Size() > 0)
                   ? d.vertexLayout.bindings[0].stride : 0u;
    if (d.blend.attachments.Size() > 0) {
        p.blendEnable = d.blend.attachments[0].blendEnable;
        p.srcColor    = d.blend.attachments[0].srcColor;
        p.dstColor    = d.blend.attachments[0].dstColor;
        p.srcAlpha    = d.blend.attachments[0].srcAlpha;
        p.dstAlpha    = d.blend.attachments[0].dstAlpha;
    }
    for (auto& dl : d.descriptorSetLayouts) p.descLayoutIds.PushBack(dl.id);
    uint64 hid = NextId(); mPipelines[hid] = p;
    NkPipelineHandle h; h.id = hid; return h;
}
NkPipelineHandle NkSoftwareDevice::CreateComputePipeline(const NkComputePipelineDesc& d) {
    NkScopedLock lock(mMutex);
    NkSWPipeline p; p.shaderId = d.shader.id; p.isCompute = true;
    uint64 hid = NextId(); mPipelines[hid] = p;
    NkPipelineHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyPipeline(NkPipelineHandle& h) {
    NkScopedLock lock(mMutex); mPipelines.Erase(h.id); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle NkSoftwareDevice::CreateRenderPass(const NkRenderPassDesc& d) {
    NkScopedLock lock(mMutex);
    NkSWRenderPass rp; rp.desc = d;
    uint64 hid = NextId(); mRenderPasses[hid] = rp;
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyRenderPass(NkRenderPassHandle& h) {
    NkScopedLock lock(mMutex); mRenderPasses.Erase(h.id); h.id = 0;
}
NkFramebufferHandle NkSoftwareDevice::CreateFramebuffer(const NkFramebufferDesc& d) {
    NkScopedLock lock(mMutex);
    NkSWFramebuffer fb;
    for (auto& ca : d.colorAttachments) fb.colorIds.PushBack(ca.id);
    fb.depthId      = d.depthAttachment.id;
    fb.w            = d.width; fb.h = d.height;
    fb.renderPassId = d.renderPass.id;
    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyFramebuffer(NkFramebufferHandle& h) {
    NkScopedLock lock(mMutex); mFramebuffers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkSoftwareDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    NkScopedLock lock(mMutex);
    NkSWDescSetLayout dsl; dsl.desc = d;
    uint64 hid = NextId(); mDescLayouts[hid] = dsl;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id = 0;
}
NkDescSetHandle NkSoftwareDevice::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
    NkScopedLock lock(mMutex);
    NkSWDescSet ds; ds.layoutId = layoutHandle.id;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::FreeDescriptorSet(NkDescSetHandle& h) {
    NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id = 0;
}
void NkSoftwareDevice::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    NkScopedLock lock(mMutex);
    for (uint32 i = 0; i < n; i++) {
        auto& w  = writes[i];
        NkSWDescSet* ds = mDescSets.Find(w.set.id); if (!ds) continue;
        bool found = false;
        for (auto& e : ds->bindings) {
            if (e.slot == w.binding) {
                e.type = w.type; e.bufId = w.buffer.id;
                e.texId = w.texture.id; e.sampId = w.sampler.id;
                found = true; break;
            }
        }
        if (!found) {
            NkSWDescSet::Binding b;
            b.slot = w.binding; b.type = w.type;
            b.bufId = w.buffer.id; b.texId = w.texture.id; b.sampId = w.sampler.id;
            ds->bindings.PushBack(b);
        }
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkSoftwareDevice::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkSoftwareCommandBuffer(this, t);
}
void NkSoftwareDevice::DestroyCommandBuffer(NkICommandBuffer*& cb) {
    delete cb; cb = nullptr;
}

// =============================================================================
// Submit
// =============================================================================
void NkSoftwareDevice::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i = 0; i < n; i++) {
        auto* sw = dynamic_cast<NkSoftwareCommandBuffer*>(cbs[i]);
        if (sw) sw->Execute(this);
    }
    if (fence.IsValid()) {
        NkSWFence* f = mFences.Find(fence.id); if (f) f->signaled = true;
    }
}
void NkSoftwareDevice::SubmitAndPresent(NkICommandBuffer* cb) {
    NkICommandBuffer* cbs[] = { cb };
    Submit(cbs, 1, {});
    Present();
}

// =============================================================================
// Present
// =============================================================================
void NkSoftwareDevice::Present() {
    auto* swCtx = mCtx ? dynamic_cast<NkSoftwareContext*>(mCtx) : nullptr;
    if (!swCtx || !swCtx->IsValid()) return;

    NkSWFramebuffer* fbObj = mFramebuffers.Find(mSwapchainFB.id);
    if (!fbObj || fbObj->colorIds.IsEmpty()) return;
    NkSWTexture* tex = mTextures.Find(fbObj->colorIds[0]);
    if (!tex || tex->mips.IsEmpty()) return;

    NkSoftwareFramebuffer& back = swCtx->GetBackBuffer();
    if (back.width != mWidth || back.height != mHeight || back.pixels.Empty()) {
        back.Resize(mWidth, mHeight);
    }
    if (back.pixels.Empty() || back.stride == 0) return;

    uint32 copyW = NkMin(mWidth, back.width);
    uint32 copyH = NkMin(mHeight, back.height);
    uint32 bpp   = tex->Bpp();
    if (bpp == 4) {
        for (uint32 y = 0; y < copyH; y++)
            memcpy(back.pixels.Data() + (usize)y * back.stride,
                   tex->mips[0].Data() + (usize)y * mWidth * 4u,
                   (usize)copyW * 4u);
    } else {
        for (uint32 y = 0; y < copyH; y++) {
            const uint8* src = tex->mips[0].Data() + y * mWidth * bpp;
            uint8* dst = back.pixels.Data() + (usize)y * back.stride;
            for (uint32 x = 0; x < copyW; x++) {
                NkVec4f col = NkSWReadPixel(src + x * bpp, tex->desc.format);
                dst[x*4+0]=(uint8)(NkClamp(col.x,0.f,1.f)*255.f);
                dst[x*4+1]=(uint8)(NkClamp(col.y,0.f,1.f)*255.f);
                dst[x*4+2]=(uint8)(NkClamp(col.z,0.f,1.f)*255.f);
                dst[x*4+3]=255;
            }
        }
    }

    swCtx->EndFrame();
    swCtx->Present();
}

const uint8* NkSoftwareDevice::BackbufferPixels() const {
    const NkSWFramebuffer* fb = mFramebuffers.Find(mSwapchainFB.id);
    if (!fb || fb->colorIds.IsEmpty()) return nullptr;
    const NkSWTexture* tex = mTextures.Find(fb->colorIds[0]);
    if (!tex || tex->mips.IsEmpty()) return nullptr;
    return tex->mips[0].Data();
}

// =============================================================================
// Fences
// =============================================================================
NkFenceHandle NkSoftwareDevice::CreateFence(bool signaled) {
    NkSWFence f; f.signaled = signaled;
    uint64 hid = NextId(); mFences[hid] = f;
    NkFenceHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroyFence(NkFenceHandle& h) { mFences.Erase(h.id); h.id = 0; }
bool NkSoftwareDevice::WaitFence(NkFenceHandle f, uint64) {
    NkSWFence* fObj = mFences.Find(f.id); return fObj && fObj->signaled;
}
bool NkSoftwareDevice::IsFenceSignaled(NkFenceHandle f) {
    NkSWFence* fObj = mFences.Find(f.id); return fObj && fObj->signaled;
}
void NkSoftwareDevice::ResetFence(NkFenceHandle f) {
    NkSWFence* fObj = mFences.Find(f.id); if (fObj) fObj->signaled = false;
}

// =============================================================================
// Frame
// =============================================================================
void NkSoftwareDevice::BeginFrame(NkFrameContext& frame) {
    NkSWFramebuffer* fb = mFramebuffers.Find(mSwapchainFB.id);
    if (fb) {
        NkSWTexture* dt = mTextures.Find(fb->depthId);
        if (dt) dt->ClearDepth(1.f);
        for (auto cid : fb->colorIds) {
            NkSWTexture* ct = mTextures.Find(cid);
            if (ct) ct->Clear({0.f, 0.f, 0.f, 1.f});
        }
    }
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
}
void NkSoftwareDevice::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex + 1) % 1;
    ++mFrameNumber;
}
void NkSoftwareDevice::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0 || (w == mWidth && h == mHeight)) return;
    NkScopedLock lock(mMutex);
    mWidth = w; mHeight = h;
    if (mSwapchainFB.IsValid()) DestroyFramebuffer(mSwapchainFB);
    if (mSwapchainRP.IsValid()) DestroyRenderPass(mSwapchainRP);
    CreateSwapchainObjects();
    NK_SW_LOG("OnResize → %u×%u\n", w, h);
}

// =============================================================================
// Accesseurs internes
// =============================================================================
NkSWBuffer*      NkSoftwareDevice::GetBuf    (uint64 id) { return mBuffers.Find(id);      }
NkSWTexture*     NkSoftwareDevice::GetTex    (uint64 id) { return mTextures.Find(id);     }
NkSWSampler*     NkSoftwareDevice::GetSamp   (uint64 id) { return mSamplers.Find(id);     }
NkSWShader*      NkSoftwareDevice::GetShader (uint64 id) { return mShaders.Find(id);      }
NkSWPipeline*    NkSoftwareDevice::GetPipe   (uint64 id) { return mPipelines.Find(id);    }
NkSWDescSet*     NkSoftwareDevice::GetDescSet(uint64 id) { return mDescSets.Find(id);     }
NkSWFramebuffer* NkSoftwareDevice::GetFBO    (uint64 id) { return mFramebuffers.Find(id); }
NkSWRenderPass*  NkSoftwareDevice::GetRP     (uint64 id) { return mRenderPasses.Find(id); }

// =============================================================================
// Swapchain & Semaphores
// =============================================================================
NkISwapchain* NkSoftwareDevice::CreateSwapchain(NkIGraphicsContext* ctx,
                                                  const NkSwapchainDesc& desc) {
    auto* sc = new NkSoftwareSwapchain(this);
    if (!sc->Initialize(ctx, desc)) { delete sc; return nullptr; }
    return sc;
}
void NkSoftwareDevice::DestroySwapchain(NkISwapchain*& sc) {
    if (!sc) return; sc->Shutdown(); delete sc; sc = nullptr;
}
#ifdef CreateGpuSemaphore
#  undef CreateGpuSemaphore
#endif
NkSemaphoreHandle NkSoftwareDevice::CreateGpuSemaphore() {
    NkSWSemaphore sem; sem.signaled = false;
    uint64 hid = NextId(); mSemaphores[hid] = sem;
    NkSemaphoreHandle h; h.id = hid; return h;
}
void NkSoftwareDevice::DestroySemaphore(NkSemaphoreHandle& handle) {
    mSemaphores.Erase(handle.id); handle.id = 0;
}

} // namespace nkentseu


