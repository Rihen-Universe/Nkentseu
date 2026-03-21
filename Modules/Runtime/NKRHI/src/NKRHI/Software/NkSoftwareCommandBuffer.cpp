// =============================================================================
// NkSoftwareCommandBuffer.cpp — Replay des commandes sur le rasteriseur CPU
// =============================================================================
#include "NkSoftwareCommandBuffer.h"
#include "NkSoftwareDevice.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkTraits.h"
#include <cstring>

namespace nkentseu {

#define NK_SW_CMD_LOG(...) logger_src.Infof("[NkRHI_SW] " __VA_ARGS__)

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
    uint8        pushData[256]= {};
    NkSWDescSet*   descSet      = nullptr;
    NkSWRasterizer raster;
};

NkSoftwareCommandBuffer::NkSoftwareCommandBuffer(NkSoftwareDevice* dev, NkCommandBufferType type)
    : mDev(dev), mType(type) {}

void NkSoftwareCommandBuffer::Execute(NkSoftwareDevice* dev) {
    for (auto& cmd : mCommands) cmd(dev);
}

// =============================================================================
// Render Pass
// =============================================================================
void NkSoftwareCommandBuffer::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                           NkFramebufferHandle fb,
                                           const NkRect2D& area) {
    mCurrentFramebufferId = fb.id;
    uint64 fbId = fb.id;
    NkRect2D a  = area;
    Push([fbId, a](NkSoftwareDevice* dev) {
        auto* fbo = dev->GetFBO(fbId);
        if (!fbo) return;
        auto* color = dev->GetTex(fbo->colorId);
        auto* depth = dev->GetTex(fbo->depthId);
        // Clear color
        if (color && !color->mips.Empty()) {
            NkVector<uint8>& colorMip = color->mips[0];
            for (usize i = 0; i < colorMip.Size(); ++i) colorMip[i] = 0;
        }
        // Clear depth à 1.0
        if (depth && !depth->mips.Empty()) {
            float32 one = 1.0f;
            for (uint32 i = 0; i < depth->Width() * depth->Height(); i++)
                memcpy(depth->mips[0].Data() + i*4, &one, 4);
        }
    });
}
void NkSoftwareCommandBuffer::EndRenderPass() {
    mCurrentFramebufferId = 0;
    Push([](NkSoftwareDevice*) {});
}

// =============================================================================
// Viewport / Scissor — stockés dans l'état, pas d'effet direct sur SW
// =============================================================================
void NkSoftwareCommandBuffer::SetViewport(const NkViewport& vp) {
    NkViewport v = vp;
    Push([v](NkSoftwareDevice*) { /* stocké dans NkSWExecState si multi-pass */ (void)v; });
}
void NkSoftwareCommandBuffer::SetViewports(const NkViewport* vps, uint32 n) {
    if (!vps || n == 0) return;
    SetViewport(vps[0]);
}
void NkSoftwareCommandBuffer::SetScissor(const NkRect2D& r) {
    NkRect2D sc = r;
    Push([sc](NkSoftwareDevice*) { (void)sc; });
}
void NkSoftwareCommandBuffer::SetScissors(const NkRect2D* r, uint32 n) {
    if (!r || n == 0) return;
    SetScissor(r[0]);
}

// =============================================================================
// Pipeline
// =============================================================================
void NkSoftwareCommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
    mBoundPipelineId = p.id;
}
void NkSoftwareCommandBuffer::BindComputePipeline(NkPipelineHandle p) {
    BindGraphicsPipeline(p);
}

// =============================================================================
// Descriptor Set
// =============================================================================
void NkSoftwareCommandBuffer::BindDescriptorSet(NkDescSetHandle set, uint32, uint32*, uint32) {
    mBoundDescSetId = set.id;
}

void NkSoftwareCommandBuffer::PushConstants(NkShaderStage, uint32, uint32 size, const void* data) {
    NkVector<uint8> buf;
    buf.Resize(size);
    if (data && size > 0) memcpy(buf.Data(), data, size);
    Push([b=traits::NkMove(buf)](NkSoftwareDevice*) { (void)b; });
}

// =============================================================================
// Vertex / Index
// =============================================================================
void NkSoftwareCommandBuffer::BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 off) {
    if (binding >= 8) return;
    mBoundVertexBufferIds[binding] = buf.id;
    mBoundVertexOffsets[binding] = off;
}
void NkSoftwareCommandBuffer::BindVertexBuffers(uint32 first, const NkBufferHandle* bufs,
                                            const uint64* offs, uint32 n) {
    for (uint32 i=0;i<n;i++) BindVertexBuffer(first+i, bufs[i], offs[i]);
}
void NkSoftwareCommandBuffer::BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) {
    mBoundIndexBufferId = buf.id;
    mBoundIndexOffset = off;
    mBoundIndexUint32 = (fmt == NkIndexFormat::NK_UINT32);
}

// =============================================================================
// Draw — cœur du rasteriseur
// =============================================================================
static NkSWVertex TransformVertex(const uint8* vdata, uint32 vidx, uint32 stride,
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

static const void* ResolveUniformData(NkSoftwareDevice* dev, uint64 descSetId) {
    if (!dev || descSetId == 0) return nullptr;
    auto* ds = dev->GetDescSet(descSetId);
    if (!ds) return nullptr;

    // Use the first bound uniform buffer as shader constant data.
    for (const auto& b : ds->bindings) {
        if (b.type != NkDescriptorType::NK_UNIFORM_BUFFER &&
            b.type != NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC) {
            continue;
        }
        if (b.bufId == 0) continue;
        auto* ub = dev->GetBuf(b.bufId);
        if (!ub || ub->data.Empty()) continue;
        return ub->data.Data();
    }
    return nullptr;
}

void NkSoftwareCommandBuffer::Draw(uint32 vtx, uint32 inst, uint32 firstVtx, uint32 firstInst) {
    const uint64 pipelineId = mBoundPipelineId;
    const uint64 descSetId = mBoundDescSetId;
    const uint64 vbId = mBoundVertexBufferIds[0];
    const uint64 vbOffset = mBoundVertexOffsets[0];
    const uint64 fbId = mCurrentFramebufferId;
    Push([vtx, inst, firstVtx, firstInst, pipelineId, descSetId, vbId, vbOffset, fbId](NkSoftwareDevice* dev) {
        auto* pipe = dev->GetPipe(pipelineId);
        auto* sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
        auto* vb   = dev->GetBuf(vbId);
        if (!vb) return;

        auto* fbo = dev->GetFBO(fbId);
        if (!fbo) fbo = dev->GetFBO(dev->GetSwapchainFramebuffer().id);
        if (!fbo) return;
        auto* color = dev->GetTex(fbo->colorId);
        auto* depth = dev->GetTex(fbo->depthId);
        const void* uniformData = ResolveUniformData(dev, descSetId);

        uint32 stride = pipe ? pipe->vertexStride : 32;
        const uint8* vdata = vb->data.Data() + vbOffset;

        NkSWRasterizer raster;
        NkSWRasterState rs;
        rs.colorTarget = color;
        rs.depthTarget = depth;
        rs.pipeline    = pipe;
        rs.shader      = sh;
        rs.uniformData = uniformData;
        rs.vertexData  = vdata;
        rs.vertexStride= stride;
        raster.SetState(rs);

        for (uint32 i = 0; i < inst; i++) {
            NkVector<NkSWVertex> verts(vtx);
            for (uint32 j = 0; j < vtx; j++)
                verts[j] = TransformVertex(vdata, firstVtx + j, stride, sh, uniformData);
            raster.DrawTriangles(verts.Data(), vtx);
        }
    });
}
void NkSoftwareCommandBuffer::DrawIndexed(uint32 idx, uint32 inst, uint32 firstIdx,
                                      int32 vtxOff, uint32 firstInst) {
    const uint64 pipelineId = mBoundPipelineId;
    const uint64 descSetId = mBoundDescSetId;
    const uint64 vbId = mBoundVertexBufferIds[0];
    const uint64 vbOffset = mBoundVertexOffsets[0];
    const uint64 ibId = mBoundIndexBufferId;
    const uint64 ibOffset = mBoundIndexOffset;
    const bool indexUint32 = mBoundIndexUint32;
    const uint64 fbId = mCurrentFramebufferId;
    Push([idx, inst, firstIdx, vtxOff, firstInst,
          pipelineId, descSetId, vbId, vbOffset, ibId, ibOffset, indexUint32, fbId](NkSoftwareDevice* dev) {
        auto* pipe = dev->GetPipe(pipelineId);
        auto* sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
        auto* vb   = dev->GetBuf(vbId);
        auto* ib   = dev->GetBuf(ibId);
        if (!vb || !ib) return;

        auto* fbo = dev->GetFBO(fbId);
        if (!fbo) fbo = dev->GetFBO(dev->GetSwapchainFramebuffer().id);
        if (!fbo) return;
        auto* color = dev->GetTex(fbo->colorId);
        auto* depth = dev->GetTex(fbo->depthId);
        const void* uniformData = ResolveUniformData(dev, descSetId);

        uint32 stride  = pipe ? pipe->vertexStride : 32;
        const uint8* vdata = vb->data.Data() + vbOffset;
        const uint8* idata = ib->data.Data() + ibOffset;

        NkSWRasterizer raster;
        NkSWRasterState rs;
        rs.colorTarget = color;
        rs.depthTarget = depth;
        rs.pipeline = pipe;
        rs.shader = sh;
        rs.uniformData = uniformData;
        raster.SetState(rs);

        for (uint32 i = 0; i < inst; i++) {
            NkVector<NkSWVertex> verts(idx);
            for (uint32 j = 0; j < idx; j++) {
                uint32 vi;
                if (indexUint32) vi = ((const uint32*)idata)[firstIdx + j] + vtxOff;
                else             vi = ((const uint16_t*)idata)[firstIdx + j] + vtxOff;
                verts[j] = TransformVertex(vdata, vi, stride, sh, uniformData);
            }
            raster.DrawTriangles(verts.Data(), idx);
        }
    });
}
void NkSoftwareCommandBuffer::DrawIndirect(NkBufferHandle, uint64, uint32, uint32) {
    Push([](NkSoftwareDevice*) { NK_SW_CMD_LOG("DrawIndirect non supporte\n"); });
}
void NkSoftwareCommandBuffer::DrawIndexedIndirect(NkBufferHandle, uint64, uint32, uint32) {
    Push([](NkSoftwareDevice*) { NK_SW_CMD_LOG("DrawIndexedIndirect non supporte\n"); });
}

// =============================================================================
// Compute
// =============================================================================
void NkSoftwareCommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    const uint64 pipelineId = mBoundPipelineId;
    Push([gx, gy, gz, pipelineId](NkSoftwareDevice* dev) {
        auto* pipe = dev->GetPipe(pipelineId);
        if (!pipe || !pipe->isCompute) return;
        auto* sh = dev->GetShader(pipe->shaderId);
        if (!sh || !sh->computeFn) return;
        sh->computeFn(gx, gy, gz, nullptr);
    });
}
void NkSoftwareCommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    uint64 bid = buf.id;
    Push([bid, off](NkSoftwareDevice* dev) {
        auto* b = dev->GetBuf(bid);
        if (!b || b->data.Size() < off + 12) return;
        const uint32* args = (const uint32*)(b->data.Data() + off);
        // Pas d'accès à dispatch ici, juste une indication
        NK_SW_CMD_LOG("DispatchIndirect(%u,%u,%u)\n", args[0], args[1], args[2]);
    });
}

// =============================================================================
// Copies
// =============================================================================
void NkSoftwareCommandBuffer::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                     const NkBufferCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkBufferCopyRegion reg=r;
    Push([s, d, reg](NkSoftwareDevice* dev) {
        auto* sb = dev->GetBuf(s), *db = dev->GetBuf(d);
        if (!sb || !db) return;
        const uint64 srcAvailable = sb->data.Size() > reg.srcOffset ? sb->data.Size() - reg.srcOffset : 0;
        const uint64 copySize = reg.size < srcAvailable ? reg.size : srcAvailable;
        memcpy(db->data.Data() + reg.dstOffset,
               sb->data.Data() + reg.srcOffset,
               (size_t)copySize);
    });
}
void NkSoftwareCommandBuffer::CopyBufferToTexture(NkBufferHandle src, NkTextureHandle dst,
                                              const NkBufferTextureCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkBufferTextureCopyRegion reg=r;
    Push([s, d, reg](NkSoftwareDevice* dev) {
        auto* sb = dev->GetBuf(s);
        auto* dt = dev->GetTex(d);
        if (!sb || !dt || dt->mips.Empty()) return;
        uint32 bpp = dt->Bpp();
        uint32 rp  = reg.bufferRowPitch > 0 ? reg.bufferRowPitch : reg.width * bpp;
        for (uint32 y = 0; y < reg.height; y++)
            memcpy(dt->mips[reg.mipLevel].Data() + (reg.y+y)*dt->Width(reg.mipLevel)*bpp + reg.x*bpp,
                   sb->data.Data() + reg.bufferOffset + y * rp,
                   reg.width * bpp);
    });
}
void NkSoftwareCommandBuffer::CopyTextureToBuffer(NkTextureHandle src, NkBufferHandle dst,
                                              const NkBufferTextureCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkBufferTextureCopyRegion reg=r;
    Push([s, d, reg](NkSoftwareDevice* dev) {
        auto* st = dev->GetTex(s);
        auto* db = dev->GetBuf(d);
        if (!st || !db || st->mips.Empty()) return;
        uint32 bpp = st->Bpp();
        uint32 rp  = reg.bufferRowPitch > 0 ? reg.bufferRowPitch : reg.width * bpp;
        for (uint32 y = 0; y < reg.height; y++)
            memcpy(db->data.Data() + reg.bufferOffset + y * rp,
                   st->mips[reg.mipLevel].Data() + (reg.y+y)*st->Width(reg.mipLevel)*bpp + reg.x*bpp,
                   reg.width * bpp);
    });
}
void NkSoftwareCommandBuffer::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                      const NkTextureCopyRegion& r) {
    uint64 s=src.id, d=dst.id; NkTextureCopyRegion reg=r;
    Push([s, d, reg](NkSoftwareDevice* dev) {
        auto* st = dev->GetTex(s), *dt = dev->GetTex(d);
        if (!st || !dt || st->mips.Empty() || dt->mips.Empty()) return;
        uint32 bpp = st->Bpp();
        for (uint32 y = 0; y < reg.height; y++)
            memcpy(dt->mips[reg.dstMip].Data() + (reg.dstY+y)*dt->Width(reg.dstMip)*bpp + reg.dstX*bpp,
                   st->mips[reg.srcMip].Data() + (reg.srcY+y)*st->Width(reg.srcMip)*bpp + reg.srcX*bpp,
                   reg.width * bpp);
    });
}
void NkSoftwareCommandBuffer::BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                                      const NkTextureCopyRegion& r, NkFilter f) {
    uint64 s=src.id, d=dst.id;
    Push([s, d](NkSoftwareDevice* dev) {
        auto* st = dev->GetTex(s), *dt = dev->GetTex(d);
        if (!st || !dt || st->mips.Empty() || dt->mips.Empty()) return;
        uint32 dw = dt->Width(), dh = dt->Height();
        for (uint32 y = 0; y < dh; y++)
            for (uint32 x = 0; x < dw; x++) {
                float32 u = (x + 0.5f) / dw, v = (y + 0.5f) / dh;
                dt->Write(x, y, st->Sample(u, v, 0));
            }
    });
}
void NkSoftwareCommandBuffer::GenerateMipmaps(NkTextureHandle tex, NkFilter f) {
    uint64 tid = tex.id;
    Push([tid](NkSoftwareDevice* dev) { dev->GenerateMipmaps({tid}, NkFilter::NK_LINEAR); });
}

// =============================================================================
// Debug
// =============================================================================
void NkSoftwareCommandBuffer::BeginDebugGroup(const char* name, float, float, float) {
    NK_SW_CMD_LOG(">>> %s\n", name ? name : "");
}
void NkSoftwareCommandBuffer::EndDebugGroup() {
    NK_SW_CMD_LOG("<<<\n");
}
void NkSoftwareCommandBuffer::InsertDebugLabel(const char* name) {
    NK_SW_CMD_LOG("--- %s\n", name ? name : "");
}

} // namespace nkentseu



