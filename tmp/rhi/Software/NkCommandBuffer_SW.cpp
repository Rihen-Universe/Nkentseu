// =============================================================================
// NkCommandBuffer_SW.cpp — Replay des commandes sur le rasteriseur CPU
// =============================================================================
#include "NkCommandBuffer_SW.h"
#include "NkRHI_Device_SW.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {

// État d'exécution local (non persistant entre frames)
struct NkSWExecState {
    NkSWTexture*   colorTarget  = nullptr;
    NkSWTexture*   depthTarget  = nullptr;
    NkSWPipeline*  pipeline     = nullptr;
    NkSWShader*    shader       = nullptr;
    NkSWBuffer*    vertexBufs[8]= {};
    uint64         vertexOffsets[8]={};
    NkSWBuffer*    indexBuf     = nullptr;
    uint64         indexOffset  = 0;
    bool           indexUint32  = true;
    NkViewport     viewport     = {};
    NkRect2D       scissor      = {};
    uint8_t        pushData[256]= {};
    NkSWDescSet*   descSet      = nullptr;
    NkSWRasterizer raster;
};

NkCommandBuffer_SW::NkCommandBuffer_SW(NkDevice_SW* dev, NkCommandBufferType type)
    : mDev(dev), mType(type) {}

void NkCommandBuffer_SW::Execute(NkDevice_SW* dev) {
    for (auto& cmd : mCommands) cmd(dev);
}

// =============================================================================
// Render Pass
// =============================================================================
void NkCommandBuffer_SW::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                          NkFramebufferHandle fb,
                                          const NkRect2D& area) {
    uint64 fbId = fb.id;
    NkRect2D a  = area;
    Push([fbId, a](NkDevice_SW* dev) {
        auto* fbo = dev->GetFBO(fbId);
        if (!fbo) return;
        auto* color = dev->GetTex(fbo->colorId);
        auto* depth = dev->GetTex(fbo->depthId);
        // Clear color
        if (color && !color->mips.empty()) {
            std::fill(color->mips[0].begin(), color->mips[0].end(), 0);
        }
        // Clear depth à 1.0
        if (depth && !depth->mips.empty()) {
            float one = 1.0f;
            for (uint32 i = 0; i < depth->Width() * depth->Height(); i++)
                memcpy(depth->mips[0].data() + i*4, &one, 4);
        }
    });
}
void NkCommandBuffer_SW::EndRenderPass() { Push([](NkDevice_SW*) {}); }

// =============================================================================
// Viewport / Scissor — stockés dans l'état, pas d'effet direct sur SW
// =============================================================================
void NkCommandBuffer_SW::SetViewport(const NkViewport& vp) {
    NkViewport v = vp;
    Push([v](NkDevice_SW*) { /* stocké dans NkSWExecState si multi-pass */ (void)v; });
}
void NkCommandBuffer_SW::SetViewports(const NkViewport* vps, uint32) { SetViewport(vps[0]); }
void NkCommandBuffer_SW::SetScissor(const NkRect2D& r) {
    NkRect2D sc = r;
    Push([sc](NkDevice_SW*) { (void)sc; });
}
void NkCommandBuffer_SW::SetScissors(const NkRect2D* r, uint32) { SetScissor(r[0]); }

// =============================================================================
// Pipeline
// =============================================================================
void NkCommandBuffer_SW::BindGraphicsPipeline(NkPipelineHandle p) {
    uint64 pid = p.id;
    Push([pid](NkDevice_SW* dev) {
        // L'état sera appliqué au moment du draw
        // On mémorise dans une variable statique thread_local
        static thread_local uint64 sPipeId = 0;
        sPipeId = pid;
    });
}
void NkCommandBuffer_SW::BindComputePipeline(NkPipelineHandle p) {
    BindGraphicsPipeline(p);
}

// =============================================================================
// Descriptor Set
// =============================================================================
void NkCommandBuffer_SW::BindDescriptorSet(NkDescSetHandle set, uint32, uint32*, uint32) {
    uint64 sid = set.id;
    Push([sid](NkDevice_SW* dev) {
        static thread_local uint64 sDescId = 0;
        sDescId = sid;
    });
}

void NkCommandBuffer_SW::PushConstants(NkShaderStage, uint32, uint32 size, const void* data) {
    std::vector<uint8_t> buf(size);
    if (data) memcpy(buf.data(), data, size);
    Push([b=std::move(buf)](NkDevice_SW*) { (void)b; });
}

// =============================================================================
// Vertex / Index
// =============================================================================
void NkCommandBuffer_SW::BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 off) {
    uint64 bid = buf.id;
    Push([binding, bid, off](NkDevice_SW* dev) {
        static thread_local uint64 sVBIds[8]={};
        static thread_local uint64 sVBOffsets[8]={};
        sVBIds[binding] = bid;
        sVBOffsets[binding] = off;
    });
}
void NkCommandBuffer_SW::BindVertexBuffers(uint32 first, const NkBufferHandle* bufs,
                                            const uint64* offs, uint32 n) {
    for (uint32 i=0;i<n;i++) BindVertexBuffer(first+i, bufs[i], offs[i]);
}
void NkCommandBuffer_SW::BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) {
    uint64 bid = buf.id; bool u32 = (fmt == NkIndexFormat::Uint32);
    Push([bid, u32, off](NkDevice_SW* dev) {
        static thread_local uint64 sIBId=0, sIBOff=0;
        static thread_local bool   sIB32=true;
        sIBId = bid; sIBOff = off; sIB32 = u32;
    });
}

// =============================================================================
// Draw — cœur du rasteriseur
// =============================================================================
static NkSWVertex TransformVertex(const uint8_t* vdata, uint32 vidx, uint32 stride,
                                   NkSWShader* shader, const void* uniforms) {
    if (shader && shader->vertFn)
        return shader->vertFn(vdata + vidx * stride, vidx, uniforms);
    // Fallback : interpréter les 3 premiers floats comme position, 4 suivants comme couleur
    NkSWVertex v;
    if (stride >= 12) {
        memcpy(&v.position.x, vdata + vidx*stride,      4);
        memcpy(&v.position.y, vdata + vidx*stride + 4,  4);
        memcpy(&v.position.z, vdata + vidx*stride + 8,  4);
        v.position.w = 1.f;
    }
    if (stride >= 28) {
        memcpy(&v.color.r, vdata + vidx*stride + 12, 4);
        memcpy(&v.color.g, vdata + vidx*stride + 16, 4);
        memcpy(&v.color.b, vdata + vidx*stride + 20, 4);
        memcpy(&v.color.a, vdata + vidx*stride + 24, 4);
    } else {
        v.color = {1,1,1,1};
    }
    if (stride >= 36) {
        memcpy(&v.uv.x, vdata + vidx*stride + 28, 4);
        memcpy(&v.uv.y, vdata + vidx*stride + 32, 4);
    }
    return v;
}

void NkCommandBuffer_SW::Draw(uint32 vtx, uint32 inst, uint32 firstVtx, uint32 firstInst) {
    Push([vtx, inst, firstVtx, firstInst](NkDevice_SW* dev) {
        // Récupérer l'état thread_local (simplification single-thread)
        static thread_local uint64 sPipeId=0, sDescId=0;
        static thread_local uint64 sVBIds[8]={}, sVBOffsets[8]={};
        static thread_local uint64 sFBId=0;

        auto* pipe = dev->GetPipe(sPipeId);
        auto* sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
        auto* vb   = dev->GetBuf(sVBIds[0]);
        if (!vb) return;

        // Trouver le framebuffer courant (dernier BeginRenderPass)
        // Simplification : utiliser le swapchain
        auto* fbo = dev->GetFBO(dev->GetSwapchainFramebuffer().id);
        if (!fbo) return;
        auto* color = dev->GetTex(fbo->colorId);
        auto* depth = dev->GetTex(fbo->depthId);

        uint32 stride = pipe ? pipe->vertexStride : 32;
        const uint8_t* vdata = vb->data.data() + sVBOffsets[0];

        NkSWRasterizer raster;
        NkSWRasterState rs;
        rs.colorTarget = color;
        rs.depthTarget = depth;
        rs.pipeline    = pipe;
        rs.shader      = sh;
        rs.vertexData  = vdata;
        rs.vertexStride= stride;
        raster.SetState(rs);

        for (uint32 i = 0; i < inst; i++) {
            std::vector<NkSWVertex> verts(vtx);
            for (uint32 j = 0; j < vtx; j++)
                verts[j] = TransformVertex(vdata, firstVtx + j, stride, sh, nullptr);
            raster.DrawTriangles(verts.data(), vtx);
        }
    });
}

void NkCommandBuffer_SW::DrawIndexed(uint32 idx, uint32 inst, uint32 firstIdx,
                                      int32 vtxOff, uint32 firstInst) {
    Push([idx, inst, firstIdx, vtxOff, firstInst](NkDevice_SW* dev) {
        static thread_local uint64 sPipeId=0;
        static thread_local uint64 sVBIds[8]={}, sVBOffsets[8]={};
        static thread_local uint64 sIBId=0, sIBOff=0;
        static thread_local bool   sIB32=true;

        auto* pipe = dev->GetPipe(sPipeId);
        auto* sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
        auto* vb   = dev->GetBuf(sVBIds[0]);
        auto* ib   = dev->GetBuf(sIBId);
        if (!vb || !ib) return;

        auto* fbo   = dev->GetFBO(dev->GetSwapchainFramebuffer().id);
        if (!fbo) return;
        auto* color = dev->GetTex(fbo->colorId);
        auto* depth = dev->GetTex(fbo->depthId);

        uint32 stride  = pipe ? pipe->vertexStride : 32;
        const uint8_t* vdata = vb->data.data() + sVBOffsets[0];
        const uint8_t* idata = ib->data.data() + sIBOff;

        NkSWRasterizer raster;
        NkSWRasterState rs;
        rs.colorTarget = color; rs.depthTarget = depth;
        rs.pipeline = pipe; rs.shader = sh;
        raster.SetState(rs);

        for (uint32 i = 0; i < inst; i++) {
            std::vector<NkSWVertex> verts(idx);
            for (uint32 j = 0; j < idx; j++) {
                uint32 vi;
                if (sIB32) vi = ((const uint32*)idata)[firstIdx + j] + vtxOff;
                else       vi = ((const uint16_t*)idata)[firstIdx + j] + vtxOff;
                verts[j] = TransformVertex(vdata, vi, stride, sh, nullptr);
            }
            raster.DrawTriangles(verts.data(), idx);
        }
    });
}

void NkCommandBuffer_SW::DrawIndirect(NkBufferHandle, uint64, uint32, uint32) {
    Push([](NkDevice_SW*) { printf("[NkRHI_SW] DrawIndirect non supporté\n"); });
}
void NkCommandBuffer_SW::DrawIndexedIndirect(NkBufferHandle, uint64, uint32, uint32) {
    Push([](NkDevice_SW*) { printf("[NkRHI_SW] DrawIndexedIndirect non supporté\n"); });
}

// =============================================================================
// Compute
// =============================================================================
void NkCommandBuffer_SW::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    Push([gx, gy, gz](NkDevice_SW* dev) {
        static thread_local uint64 sPipeId=0;
        auto* pipe = dev->GetPipe(sPipeId);
        if (!pipe || !pipe->isCompute) return;
        auto* sh = dev->GetShader(pipe->shaderId);
        if (!sh || !sh->computeFn) return;
        sh->computeFn(gx, gy, gz, nullptr);
    });
}
void NkCommandBuffer_SW::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    uint64 bid = buf.id;
    Push([bid, off](NkDevice_SW* dev) {
        auto* b = dev->GetBuf(bid);
        if (!b || b->data.size() < off + 12) return;
        const uint32* args = (const uint32*)(b->data.data() + off);
        // Pas d'accès à dispatch ici, juste une indication
        printf("[NkRHI_SW] DispatchIndirect(%u,%u,%u)\n", args[0], args[1], args[2]);
    });
}

// =============================================================================
// Copies
// =============================================================================
void NkCommandBuffer_SW::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                     const NkBufferCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkBufferCopyRegion reg=r;
    Push([s, d, reg](NkDevice_SW* dev) {
        auto* sb = dev->GetBuf(s), *db = dev->GetBuf(d);
        if (!sb || !db) return;
        memcpy(db->data.data() + reg.dstOffset,
               sb->data.data() + reg.srcOffset,
               (size_t)std::min(reg.size, (uint64)sb->data.size() - reg.srcOffset));
    });
}
void NkCommandBuffer_SW::CopyBufferToTexture(NkBufferHandle src, NkTextureHandle dst,
                                              const NkBufferTextureCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkBufferTextureCopyRegion reg=r;
    Push([s, d, reg](NkDevice_SW* dev) {
        auto* sb = dev->GetBuf(s), *dt = dev->GetTex(d);
        if (!sb || !dt || dt->mips.empty()) return;
        uint32 bpp = dt->Bpp();
        uint32 rp  = reg.bufferRowPitch > 0 ? reg.bufferRowPitch : reg.width * bpp;
        for (uint32 y = 0; y < reg.height; y++)
            memcpy(dt->mips[reg.mipLevel].data() + (reg.y+y)*dt->Width(reg.mipLevel)*bpp + reg.x*bpp,
                   sb->data.data() + reg.bufferOffset + y * rp,
                   reg.width * bpp);
    });
}
void NkCommandBuffer_SW::CopyTextureToBuffer(NkTextureHandle src, NkBufferHandle dst,
                                              const NkBufferTextureCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkBufferTextureCopyRegion reg=r;
    Push([s, d, reg](NkDevice_SW* dev) {
        auto* st = dev->GetTex(s), *db = dev->GetBuf(d);
        if (!st || !db || st->mips.empty()) return;
        uint32 bpp = st->Bpp();
        uint32 rp  = reg.bufferRowPitch > 0 ? reg.bufferRowPitch : reg.width * bpp;
        for (uint32 y = 0; y < reg.height; y++)
            memcpy(db->data.data() + reg.bufferOffset + y * rp,
                   st->mips[reg.mipLevel].data() + (reg.y+y)*st->Width(reg.mipLevel)*bpp + reg.x*bpp,
                   reg.width * bpp);
    });
}
void NkCommandBuffer_SW::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                      const NkTextureCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkTextureCopyRegion reg=r;
    Push([s, d, reg](NkDevice_SW* dev) {
        auto* st = dev->GetTex(s), *dt = dev->GetTex(d);
        if (!st || !dt || st->mips.empty() || dt->mips.empty()) return;
        uint32 bpp = st->Bpp();
        for (uint32 y = 0; y < reg.height; y++)
            memcpy(dt->mips[reg.dstMip].data() + (reg.dstY+y)*dt->Width(reg.dstMip)*bpp + reg.dstX*bpp,
                   st->mips[reg.srcMip].data() + (reg.srcY+y)*st->Width(reg.srcMip)*bpp + reg.srcX*bpp,
                   reg.width * bpp);
    });
}
void NkCommandBuffer_SW::BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                                      const NkTextureCopyRegion& r, NkFilter f) {
    uint64 s=src.id, d=dst.id;
    Push([s, d](NkDevice_SW* dev) {
        auto* st = dev->GetTex(s), *dt = dev->GetTex(d);
        if (!st || !dt || st->mips.empty() || dt->mips.empty()) return;
        uint32 dw = dt->Width(), dh = dt->Height();
        for (uint32 y = 0; y < dh; y++)
            for (uint32 x = 0; x < dw; x++) {
                float u = (x + 0.5f) / dw, v = (y + 0.5f) / dh;
                dt->Write(x, y, st->Sample(u, v, 0));
            }
    });
}
void NkCommandBuffer_SW::GenerateMipmaps(NkTextureHandle tex, NkFilter f) {
    uint64 tid = tex.id;
    Push([tid](NkDevice_SW* dev) { dev->GenerateMipmaps({tid}, NkFilter::Linear); });
}

// =============================================================================
// Debug
// =============================================================================
void NkCommandBuffer_SW::BeginDebugGroup(const char* name, float, float, float) {
    printf("[NkRHI_SW] >>> %s\n", name);
}
void NkCommandBuffer_SW::EndDebugGroup() {
    printf("[NkRHI_SW] <<<\n");
}
void NkCommandBuffer_SW::InsertDebugLabel(const char* name) {
    printf("[NkRHI_SW] --- %s\n", name);
}

} // namespace nkentseu
