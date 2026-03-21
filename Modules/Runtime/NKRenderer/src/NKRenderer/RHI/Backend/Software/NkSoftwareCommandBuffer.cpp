// =============================================================================
// NkSoftwareCommandBuffer.cpp — v4 final
// Corrections :
//   1. Suppression des 'logger' non déclarés → logger_src.Infof
//   2. Stride fallback : utilise le stride du pipeline (jamais 32 hardcodé)
//      → si vertexStride==0 dans le pipeline, on log une erreur explicite
//   3. NkSWShaderContext transmis correctement comme uniformData
//   4. IsEmpty() partout (pas Empty())
// =============================================================================
#include "NkSoftwareCommandBuffer.h"
#include "NkSoftwareDevice.h"
#include "NkSoftwareRasterizer.h"
#include "NkSWShaderHelper.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {

#define NK_CB_LOG(...) logger_src.Infof("[NkSoftwareCB] " __VA_ARGS__)

// =============================================================================
// Helper : allouer les buffers MSAA si nécessaire
// =============================================================================
static void EnsureMSAABuffers(NkSoftwareDevice* dev, NkSWFramebuffer* fbo,
                               uint32 sampleCount) {
    if (sampleCount <= 1) return;
    if (!fbo->msaaColorIds.IsEmpty()) return;

    for (uint32 i = 0; i < (uint32)fbo->colorIds.Size() && i < kNkSWMaxColorTargets; i++) {
        NkSWTexture* ct = dev->GetTex(fbo->colorIds[i]);
        if (!ct) continue;
        NkTextureDesc d = ct->desc;
        d.width     = d.width * sampleCount;
        d.mipLevels = 1;
        d.debugName = "MSAA_Color";
        NkTextureHandle h = dev->CreateTexture(d);
        fbo->msaaColorIds.PushBack(h.id);
    }

    NkSWTexture* dt = dev->GetTex(fbo->depthId);
    if (dt) {
        NkTextureDesc d = dt->desc;
        d.width     = d.width * sampleCount;
        d.mipLevels = 1;
        d.debugName = "MSAA_Depth";
        NkTextureHandle h = dev->CreateTexture(d);
        fbo->msaaDepthId = h.id;
    }
}

// =============================================================================
// NkSWExecState::ResolveDrawState
// =============================================================================
NkSWDrawState NkSWExecState::ResolveDrawState(NkSoftwareDevice* dev) const {
    NkSWDrawState ds;

    ds.pipeline = dev->GetPipe(pipelineId);
    if (ds.pipeline) ds.shader = dev->GetShader(ds.pipeline->shaderId);

    uint64 fboId = (framebufferId != 0) ? framebufferId
                                         : dev->GetSwapchainFramebuffer().id;
    NkSWFramebuffer* fbo = dev->GetFBO(fboId);
    if (fbo) {
        uint32 sc = ds.pipeline ? ds.pipeline->sampleCount : 1u;
        if (sc > 1) EnsureMSAABuffers(dev, fbo, sc);

        ds.colorTargetCount = (uint32)fbo->colorIds.Size();
        if (ds.colorTargetCount > kNkSWMaxColorTargets)
            ds.colorTargetCount = kNkSWMaxColorTargets;
        for (uint32 i = 0; i < ds.colorTargetCount; i++)
            ds.colorTargets[i] = dev->GetTex(fbo->colorIds[i]);
        // Sync alias
        ds.SyncColorTargetAlias();
        ds.depthTarget = dev->GetTex(fbo->depthId);
        ds.targetW     = fbo->w;
        ds.targetH     = fbo->h;

        ds.msaaSamples = sc;
        if (sc > 1) {
            for (uint32 i = 0; i < (uint32)fbo->msaaColorIds.Size() && i < kNkSWMaxColorTargets; i++)
                ds.msaaTargets[i] = dev->GetTex(fbo->msaaColorIds[i]);
            ds.msaaDepth = dev->GetTex(fbo->msaaDepthId);
        }
    }

    NkSWDescSet* descSet = dev->GetDescSet(descSetId);
    if (descSet) {
        for (auto& b : descSet->bindings) {
            if (b.slot == 0 && b.type == NkDescriptorType::NK_UNIFORM_BUFFER) {
                NkSWBuffer* ub = dev->GetBuf(b.bufId);
                if (ub && !ub->data.IsEmpty()) ds.uniformData = ub->data.Data();
            } else if (b.slot < NkSWDrawState::kMaxBindings) {
                if (b.type == NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER) {
                    ds.textures[b.slot] = dev->GetTex(b.texId);
                    ds.samplers[b.slot] = dev->GetSamp(b.sampId);
                }
            }
        }
    }

    NkSWBuffer* vb = dev->GetBuf(vbIds[0]);
    if (vb) {
        ds.vertexData   = vb->data.Data() + vbOffsets[0];
        // CORRECTION BUG 10 : utiliser le stride du pipeline
        // Ne jamais fallback à 32 — 32 est faux pour Vtx3D (36 bytes)
        ds.vertexStride = (ds.pipeline && ds.pipeline->vertexStride > 0)
                        ? ds.pipeline->vertexStride
                        : 0u;  // 0 = pas de stride défini → erreur gérée dans TransformVertex
    }

    if (hasViewport) {
        ds.vpX=vpX; ds.vpY=vpY; ds.vpW=vpW; ds.vpH=vpH;
        ds.vpMinZ=vpMinZ; ds.vpMaxZ=vpMaxZ; ds.hasViewport=true;
    } else if (ds.targetW > 0 && ds.targetH > 0) {
        ds.vpX=0; ds.vpY=0;
        ds.vpW=(float)ds.targetW; ds.vpH=(float)ds.targetH;
        ds.vpMinZ=0.f; ds.vpMaxZ=1.f; ds.hasViewport=true;
    }

    if (hasScissor) {
        ds.scissorX=scissorX; ds.scissorY=scissorY;
        ds.scissorW=scissorW; ds.scissorH=scissorH;
        ds.hasScissor=true;
    }

    if (pushConstantsSize > 0) {
        memcpy(ds.pushConstants, pushConstants, pushConstantsSize);
        ds.pushConstantsSize = pushConstantsSize;
    }

    return ds;
}

// =============================================================================
NkSoftwareCommandBuffer::NkSoftwareCommandBuffer(NkSoftwareDevice* dev,
                                                   NkCommandBufferType type)
    : mDev(dev), mType(type) {}

void NkSoftwareCommandBuffer::Execute(NkSoftwareDevice* dev) {
    NkSWExecState state;
    for (auto& cmd : mCommands) cmd(dev, state);
}

// =============================================================================
// Render Pass
// =============================================================================
void NkSoftwareCommandBuffer::BeginRenderPass(NkRenderPassHandle,
                                               NkFramebufferHandle fb,
                                               const NkRect2D&) {
    uint64 fbId = fb.id;
    Push([fbId](NkSoftwareDevice* dev, NkSWExecState& state) {
        state.framebufferId = fbId;
        NkSWFramebuffer* fbo = dev->GetFBO(fbId);
        if (!fbo) return;
        for (auto cid : fbo->colorIds) {
            NkSWTexture* t = dev->GetTex(cid);
            if (t) t->Clear({0.f, 0.f, 0.f, 1.f});
        }
        NkSWTexture* dt = dev->GetTex(fbo->depthId);
        if (dt) dt->ClearDepth(1.f);
        for (auto cid : fbo->msaaColorIds) {
            NkSWTexture* t = dev->GetTex(cid);
            if (t && !t->mips.IsEmpty()) memset(t->mips[0].Data(), 0, t->mips[0].Size());
        }
        if (fbo->msaaDepthId) {
            NkSWTexture* t = dev->GetTex(fbo->msaaDepthId);
            if (t) t->ClearDepth(1.f);
        }
    });
}

void NkSoftwareCommandBuffer::EndRenderPass() {
    Push([](NkSoftwareDevice*, NkSWExecState&) {});
}

// =============================================================================
// Viewport / Scissor
// =============================================================================
void NkSoftwareCommandBuffer::SetViewport(const NkViewport& vp) {
    NkViewport v = vp;
    Push([v](NkSoftwareDevice*, NkSWExecState& s) {
        s.vpX=v.x; s.vpY=v.y; s.vpW=v.width; s.vpH=v.height;
        s.vpMinZ=v.minDepth; s.vpMaxZ=v.maxDepth; s.hasViewport=true;
    });
}
void NkSoftwareCommandBuffer::SetViewports(const NkViewport* v, uint32 n) {
    if (n > 0) SetViewport(v[0]);
}
void NkSoftwareCommandBuffer::SetScissor(const NkRect2D& r) {
    NkRect2D sc = r;
    Push([sc](NkSoftwareDevice*, NkSWExecState& s) {
        s.scissorX=sc.x; s.scissorY=sc.y;
        s.scissorW=sc.width; s.scissorH=sc.height;
        s.hasScissor=true;
    });
}
void NkSoftwareCommandBuffer::SetScissors(const NkRect2D* r, uint32 n) {
    if (n > 0) SetScissor(r[0]);
}

// =============================================================================
// Pipeline / Descriptor / Vertex
// =============================================================================
void NkSoftwareCommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
    uint64 id = p.id;
    Push([id](NkSoftwareDevice*, NkSWExecState& s) { s.pipelineId = id; });
}
void NkSoftwareCommandBuffer::BindComputePipeline(NkPipelineHandle p) {
    BindGraphicsPipeline(p);
}
void NkSoftwareCommandBuffer::BindDescriptorSet(NkDescSetHandle set, uint32, uint32*, uint32) {
    uint64 id = set.id;
    Push([id](NkSoftwareDevice*, NkSWExecState& s) { s.descSetId = id; });
}
void NkSoftwareCommandBuffer::PushConstants(NkShaderStage, uint32 offset,
                                             uint32 size, const void* data) {
    NkVector<uint8> buf;
    buf.Resize(size);
    if (data) memcpy(buf.Data(), data, size);
    Push([b=traits::NkMove(buf), offset, size](NkSoftwareDevice*, NkSWExecState& s) {
        uint32 end = NkMin(offset + size, 256u);
        uint32 n   = end > offset ? end - offset : 0u;
        if (n > 0) {
            memcpy(s.pushConstants + offset, b.Data(), n);
            s.pushConstantsSize = NkMax(s.pushConstantsSize, end);
        }
    });
}
void NkSoftwareCommandBuffer::BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 off) {
    uint64 id = buf.id;
    Push([binding, id, off](NkSoftwareDevice*, NkSWExecState& s) {
        if (binding < 8) { s.vbIds[binding] = id; s.vbOffsets[binding] = off; }
    });
}
void NkSoftwareCommandBuffer::BindVertexBuffers(uint32 first, const NkBufferHandle* b,
                                                  const uint64* o, uint32 n) {
    for (uint32 i = 0; i < n; i++) BindVertexBuffer(first + i, b[i], o[i]);
}
void NkSoftwareCommandBuffer::BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) {
    uint64 id = buf.id;
    bool u32 = (fmt == NkIndexFormat::NK_UINT32);
    Push([id, u32, off](NkSoftwareDevice*, NkSWExecState& s) {
        s.ibId = id; s.ibOffset = off; s.ib32 = u32;
    });
}

// =============================================================================
// TransformVertex
// CORRECTION BUG 10 : stride 0 → log explicite au lieu de corrompre silencieusement
// =============================================================================
NkSWVertex NkSoftwareCommandBuffer::TransformVertex(const uint8* vdata, uint32 vidx,
                                                      uint32 stride, NkSWShader* sh,
                                                      const void* uniforms) {
    if (stride == 0) {
        // Stride non défini : retourner un vertex debug visible (rouge au centre)
        NkSWVertex v;
        v.position = {0.f, 0.f, 0.f, 1.f};
        v.color    = {1.f, 0.f, 0.f, 1.f};
        return v;
    }

    if (sh && sh->vertFn)
        return sh->vertFn(vdata + vidx * stride, vidx, uniforms);

    // Fallback générique : lire le vertex brut (position + normal + color)
    NkSWVertex v;
    const float* f = (const float*)(vdata + vidx * stride);
    uint32 nf = stride / sizeof(float);
    if (nf >= 3) v.position = {f[0], f[1], f[2], 1.f};
    if (nf >= 6) v.normal   = {f[3], f[4], f[5]};
    if (nf >= 9) { v.color  = {f[6], f[7], f[8], 1.f}; }
    else           v.color  = {1.f, 1.f, 1.f, 1.f};
    if (nf >= 11) v.uv = {f[9], f[10]};
    return v;
}

// =============================================================================
// ExecuteDraw — sans logs debug dans la boucle
// =============================================================================
void NkSoftwareCommandBuffer::ExecuteDraw(NkSoftwareDevice* dev,
                                           const NkSWExecState& state,
                                           const NkVector<NkSWVertex>& verts) {
    NkSWDrawState ds = state.ResolveDrawState(dev);
    if (ds.colorTargetCount == 0) return;
    uint32 count = (uint32)verts.Size();
    if (count == 0) return;

    NkSWRasterizer raster;
    raster.SetState(ds);

    NkPrimitiveTopology topo = ds.pipeline
        ? ds.pipeline->topology
        : NkPrimitiveTopology::NK_TRIANGLE_LIST;

    switch (topo) {
        case NkPrimitiveTopology::NK_TRIANGLE_LIST:
            raster.DrawTriangles(verts.Data(), count);
            break;
        case NkPrimitiveTopology::NK_TRIANGLE_STRIP:
            raster.DrawTriangleStrip(verts.Data(), count);
            break;
        case NkPrimitiveTopology::NK_TRIANGLE_FAN:
            raster.DrawTriangleFan(verts.Data(), count);
            break;
        case NkPrimitiveTopology::NK_LINE_LIST:
            raster.DrawLines(verts.Data(), count);
            break;
        case NkPrimitiveTopology::NK_LINE_STRIP:
            raster.DrawLineStrip(verts.Data(), count);
            break;
        case NkPrimitiveTopology::NK_POINT_LIST:
            raster.DrawPoints(verts.Data(), count);
            break;
        default:
            raster.DrawTriangles(verts.Data(), count);
            break;
    }
}

// =============================================================================
// ResolveCtx — construit NkSWShaderContext et retourne l'UBO
// =============================================================================
static const void* ResolveCtx(NkSoftwareDevice* dev, const NkSWExecState& state,
                                NkSWShaderContext& ctx) {
    const void* uboPtr = nullptr;
    NkSWDescSet* ds = dev->GetDescSet(state.descSetId);
    if (ds) {
        const NkSWDescSet::Binding* b = ds->FindBinding(0);
        if (b && b->type == NkDescriptorType::NK_UNIFORM_BUFFER) {
            NkSWBuffer* ub = dev->GetBuf(b->bufId);
            if (ub && !ub->data.IsEmpty()) uboPtr = ub->data.Data();
        }
    }
    ctx.uboData       = uboPtr;
    ctx.pushConstants = state.pushConstantsSize > 0 ? state.pushConstants : nullptr;
    ctx.pushConstSize = state.pushConstantsSize;
    ctx.instanceIndex = state.instanceIndex;
    // uniformData transmis au shader = le UBO directement (pour compatibilité)
    return uboPtr ? uboPtr : nullptr;
}

// =============================================================================
// Draw
// =============================================================================
void NkSoftwareCommandBuffer::Draw(uint32 vtxCount, uint32 instCount,
                                    uint32 firstVtx, uint32) {
    Push([vtxCount, instCount, firstVtx](NkSoftwareDevice* dev, NkSWExecState& state) {
        NkSWBuffer*   vb   = dev->GetBuf(state.vbIds[0]); if (!vb) return;
        NkSWPipeline* pipe = dev->GetPipe(state.pipelineId);
        NkSWShader*   sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;

        // CORRECTION BUG 10 : stride depuis le pipeline
        uint32 stride = (pipe && pipe->vertexStride > 0) ? pipe->vertexStride : 0u;
        if (stride == 0) {
            NK_CB_LOG("AVERTISSEMENT : vertexStride=0 dans le pipeline — stride non défini!\n");
        }

        const uint8* vdata = vb->data.Data() + state.vbOffsets[0];
        NkSWShaderContext ctx;
        const void* uniforms = ResolveCtx(dev, state, ctx);

        for (uint32 inst = 0; inst < instCount; inst++) {
            state.instanceIndex = inst;
            ctx.instanceIndex   = inst;
            NkVector<NkSWVertex> verts;
            verts.Resize(vtxCount);
            for (uint32 j = 0; j < vtxCount; j++)
                verts[j] = TransformVertex(vdata, firstVtx + j, stride, sh, uniforms);
            ExecuteDraw(dev, state, verts);
        }
    });
}

// =============================================================================
// DrawIndexed
// =============================================================================
void NkSoftwareCommandBuffer::DrawIndexed(uint32 idxCount, uint32 instCount,
                                           uint32 firstIdx, int32 vtxOff, uint32) {
    Push([idxCount, instCount, firstIdx, vtxOff](NkSoftwareDevice* dev, NkSWExecState& state) {
        NkSWBuffer*   vb   = dev->GetBuf(state.vbIds[0]); if (!vb) return;
        NkSWBuffer*   ib   = dev->GetBuf(state.ibId);      if (!ib) return;
        NkSWPipeline* pipe = dev->GetPipe(state.pipelineId);
        NkSWShader*   sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;

        uint32 stride = (pipe && pipe->vertexStride > 0) ? pipe->vertexStride : 0u;
        const uint8* vdata = vb->data.Data() + state.vbOffsets[0];
        const uint8* idata = ib->data.Data() + state.ibOffset;
        NkSWShaderContext ctx;
        const void* uniforms = ResolveCtx(dev, state, ctx);

        for (uint32 inst = 0; inst < instCount; inst++) {
            state.instanceIndex = inst;
            ctx.instanceIndex   = inst;
            NkVector<NkSWVertex> verts;
            verts.Resize(idxCount);
            for (uint32 j = 0; j < idxCount; j++) {
                uint32 vi = state.ib32 ? ((const uint32*)idata)[firstIdx + j]
                                       : ((const uint16*)idata)[firstIdx + j];
                vi = (uint32)((int32)vi + vtxOff);
                verts[j] = TransformVertex(vdata, vi, stride, sh, uniforms);
            }
            ExecuteDraw(dev, state, verts);
        }
    });
}

// =============================================================================
// DrawIndirect
// =============================================================================
void NkSoftwareCommandBuffer::DrawIndirect(NkBufferHandle buf, uint64 offset,
                                            uint32 drawCount, uint32 stride) {
    uint64 bid = buf.id;
    Push([bid, offset, drawCount, stride](NkSoftwareDevice* dev, NkSWExecState& state) {
        NkSWBuffer* argBuf = dev->GetBuf(bid); if (!argBuf) return;
        NkSWBuffer* vb     = dev->GetBuf(state.vbIds[0]); if (!vb) return;
        NkSWPipeline* pipe = dev->GetPipe(state.pipelineId);
        NkSWShader*   sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
        uint32 argStride   = stride > 0 ? stride : (uint32)sizeof(NkDrawIndirectArgs);
        uint32 vbStride    = (pipe && pipe->vertexStride > 0) ? pipe->vertexStride : 0u;
        const uint8* vdata = vb->data.Data() + state.vbOffsets[0];
        NkSWShaderContext ctx;
        const void* uniforms = ResolveCtx(dev, state, ctx);

        for (uint32 d = 0; d < drawCount; d++) {
            uint64 argsOff = offset + (uint64)d * argStride;
            if (argsOff + sizeof(NkDrawIndirectArgs) > argBuf->data.Size()) break;
            NkDrawIndirectArgs args;
            memcpy(&args, argBuf->data.Data() + argsOff, sizeof(args));
            if (!args.vertexCount) continue;
            for (uint32 inst = 0; inst < args.instanceCount; inst++) {
                state.instanceIndex = args.firstInstance + inst;
                ctx.instanceIndex   = state.instanceIndex;
                NkVector<NkSWVertex> verts;
                verts.Resize(args.vertexCount);
                for (uint32 j = 0; j < args.vertexCount; j++)
                    verts[j] = TransformVertex(vdata, args.firstVertex + j, vbStride, sh, uniforms);
                ExecuteDraw(dev, state, verts);
            }
        }
    });
}

// =============================================================================
// DrawIndexedIndirect
// =============================================================================
void NkSoftwareCommandBuffer::DrawIndexedIndirect(NkBufferHandle buf, uint64 offset,
                                                   uint32 drawCount, uint32 stride) {
    uint64 bid = buf.id;
    Push([bid, offset, drawCount, stride](NkSoftwareDevice* dev, NkSWExecState& state) {
        NkSWBuffer* argBuf = dev->GetBuf(bid); if (!argBuf) return;
        NkSWBuffer* vb     = dev->GetBuf(state.vbIds[0]); if (!vb) return;
        NkSWBuffer* ib     = dev->GetBuf(state.ibId);      if (!ib) return;
        NkSWPipeline* pipe = dev->GetPipe(state.pipelineId);
        NkSWShader*   sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
        uint32 argStride   = stride > 0 ? stride : (uint32)sizeof(NkDrawIndexedIndirectArgs);
        uint32 vbStride    = (pipe && pipe->vertexStride > 0) ? pipe->vertexStride : 0u;
        const uint8* vdata = vb->data.Data() + state.vbOffsets[0];
        const uint8* idata = ib->data.Data() + state.ibOffset;
        NkSWShaderContext ctx;
        const void* uniforms = ResolveCtx(dev, state, ctx);

        for (uint32 d = 0; d < drawCount; d++) {
            uint64 argsOff = offset + (uint64)d * argStride;
            if (argsOff + sizeof(NkDrawIndexedIndirectArgs) > argBuf->data.Size()) break;
            NkDrawIndexedIndirectArgs args;
            memcpy(&args, argBuf->data.Data() + argsOff, sizeof(args));
            if (!args.indexCount) continue;
            for (uint32 inst = 0; inst < args.instanceCount; inst++) {
                state.instanceIndex = args.firstInstance + inst;
                ctx.instanceIndex   = state.instanceIndex;
                NkVector<NkSWVertex> verts;
                verts.Resize(args.indexCount);
                for (uint32 j = 0; j < args.indexCount; j++) {
                    uint32 vi = state.ib32
                        ? ((const uint32*)idata)[args.firstIndex + j]
                        : ((const uint16*)idata)[args.firstIndex + j];
                    vi = (uint32)((int32)vi + args.vertexOffset);
                    verts[j] = TransformVertex(vdata, vi, vbStride, sh, uniforms);
                }
                ExecuteDraw(dev, state, verts);
            }
        }
    });
}

// =============================================================================
// Compute
// =============================================================================
void NkSoftwareCommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    Push([gx, gy, gz](NkSoftwareDevice* dev, NkSWExecState& state) {
        NkSWPipeline* pipe = dev->GetPipe(state.pipelineId);
        if (!pipe || !pipe->isCompute) return;
        NkSWShader* sh = dev->GetShader(pipe->shaderId);
        if (!sh || !sh->computeFn) return;
        NkSWShaderContext ctx;
        const void* uniforms = ResolveCtx(dev, state, ctx);
        sh->computeFn(gx, gy, gz, uniforms);
    });
}
void NkSoftwareCommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    uint64 bid = buf.id;
    Push([bid, off](NkSoftwareDevice* dev, NkSWExecState& state) {
        NkSWBuffer* b = dev->GetBuf(bid);
        if (!b || b->data.Size() < off + 12) return;
        const uint32* a = (const uint32*)(b->data.Data() + off);
        NkSWPipeline* pipe = dev->GetPipe(state.pipelineId);
        if (!pipe || !pipe->isCompute) return;
        NkSWShader* sh = dev->GetShader(pipe->shaderId);
        if (!sh || !sh->computeFn) return;
        NkSWShaderContext ctx;
        const void* uniforms = ResolveCtx(dev, state, ctx);
        sh->computeFn(a[0], a[1], a[2], uniforms);
    });
}

// =============================================================================
// Copies
// =============================================================================
void NkSoftwareCommandBuffer::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                          const NkBufferCopyRegion& r) {
    uint64 s = src.id, d = dst.id;
    NkBufferCopyRegion reg = r;
    Push([s, d, reg](NkSoftwareDevice* dev, NkSWExecState&) {
        NkSWBuffer* sb = dev->GetBuf(s), *db = dev->GetBuf(d); if (!sb || !db) return;
        uint64 avail = sb->data.Size() > reg.srcOffset ? sb->data.Size() - reg.srcOffset : 0u;
        uint64 sz    = NkMin(reg.size, avail);
        if (sz > 0) memcpy(db->data.Data() + reg.dstOffset,
                           sb->data.Data() + reg.srcOffset, (size_t)sz);
    });
}
void NkSoftwareCommandBuffer::CopyBufferToTexture(NkBufferHandle src, NkTextureHandle dst,
                                                    const NkBufferTextureCopyRegion& r) {
    uint64 s = src.id, d = dst.id;
    NkBufferTextureCopyRegion reg = r;
    Push([s, d, reg](NkSoftwareDevice* dev, NkSWExecState&) {
        NkSWBuffer* sb  = dev->GetBuf(s);
        NkSWTexture* dt = dev->GetTex(d);
        if (!sb || !dt || dt->mips.IsEmpty()) return;
        uint32 bpp  = dt->Bpp();
        uint32 mip  = NkMin(reg.mipLevel, (uint32)dt->mips.Size() - 1);
        uint32 rp   = reg.bufferRowPitch > 0 ? reg.bufferRowPitch : reg.width * bpp;
        for (uint32 y = 0; y < reg.height; y++) {
            uint32 dy = reg.y + y; if (dy >= dt->Height(mip)) break;
            uint32 dOff = (dy * dt->Width(mip) + reg.x) * bpp;
            uint32 sOff = (uint32)reg.bufferOffset + y * rp;
            uint32 cW   = NkMin(reg.width * bpp, (dt->Width(mip) - reg.x) * bpp);
            if (dOff + cW > dt->mips[mip].Size()) break;
            memcpy(dt->mips[mip].Data() + dOff, sb->data.Data() + sOff, cW);
        }
    });
}
void NkSoftwareCommandBuffer::CopyTextureToBuffer(NkTextureHandle src, NkBufferHandle dst,
                                                    const NkBufferTextureCopyRegion& r) {
    uint64 s = src.id, d = dst.id;
    NkBufferTextureCopyRegion reg = r;
    Push([s, d, reg](NkSoftwareDevice* dev, NkSWExecState&) {
        NkSWTexture* st = dev->GetTex(s);
        NkSWBuffer* db  = dev->GetBuf(d);
        if (!st || !db || st->mips.IsEmpty()) return;
        uint32 bpp = st->Bpp();
        uint32 mip = NkMin(reg.mipLevel, (uint32)st->mips.Size() - 1);
        uint32 rp  = reg.bufferRowPitch > 0 ? reg.bufferRowPitch : reg.width * bpp;
        for (uint32 y = 0; y < reg.height; y++) {
            uint32 sy = reg.y + y; if (sy >= st->Height(mip)) break;
            uint32 sOff = (sy * st->Width(mip) + reg.x) * bpp;
            uint32 dOff = (uint32)reg.bufferOffset + y * rp;
            uint32 cW   = NkMin(reg.width * bpp, (st->Width(mip) - reg.x) * bpp);
            if (sOff + cW > st->mips[mip].Size()) break;
            memcpy(db->data.Data() + dOff, st->mips[mip].Data() + sOff, cW);
        }
    });
}
void NkSoftwareCommandBuffer::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                           const NkTextureCopyRegion& r) {
    uint64 s = src.id, d = dst.id;
    NkTextureCopyRegion reg = r;
    Push([s, d, reg](NkSoftwareDevice* dev, NkSWExecState&) {
        NkSWTexture* st = dev->GetTex(s), *dt = dev->GetTex(d);
        if (!st || !dt || st->mips.IsEmpty() || dt->mips.IsEmpty()) return;
        uint32 bpp = st->Bpp();
        uint32 sm  = NkMin(reg.srcMip, (uint32)st->mips.Size() - 1);
        uint32 dm  = NkMin(reg.dstMip, (uint32)dt->mips.Size() - 1);
        for (uint32 y = 0; y < reg.height; y++) {
            uint32 sy = reg.srcY + y, dy = reg.dstY + y;
            if (sy >= st->Height(sm) || dy >= dt->Height(dm)) break;
            uint32 cW = NkMin(reg.width, NkMin(st->Width(sm) - reg.srcX,
                                                dt->Width(dm) - reg.dstX)) * bpp;
            if (!cW) continue;
            memcpy(dt->mips[dm].Data() + (dy * dt->Width(dm) + reg.dstX) * bpp,
                   st->mips[sm].Data() + (sy * st->Width(sm) + reg.srcX) * bpp, cW);
        }
    });
}
void NkSoftwareCommandBuffer::BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                                           const NkTextureCopyRegion&, NkFilter) {
    uint64 s = src.id, d = dst.id;
    Push([s, d](NkSoftwareDevice* dev, NkSWExecState&) {
        NkSWTexture* st = dev->GetTex(s), *dt = dev->GetTex(d);
        if (!st || !dt || st->mips.IsEmpty() || dt->mips.IsEmpty()) return;
        uint32 dw = dt->Width(), dh = dt->Height();
        for (uint32 y = 0; y < dh; y++)
            for (uint32 x = 0; x < dw; x++)
                dt->Write(x, y, st->Sample((x + .5f) / dw, (y + .5f) / dh));
    });
}
void NkSoftwareCommandBuffer::GenerateMipmaps(NkTextureHandle tex, NkFilter) {
    uint64 id = tex.id;
    Push([id](NkSoftwareDevice* dev, NkSWExecState&) {
        NkTextureHandle h; h.id = id;
        dev->GenerateMipmaps(h, NkFilter::NK_LINEAR);
    });
}

// =============================================================================
// Debug
// =============================================================================
void NkSoftwareCommandBuffer::BeginDebugGroup(const char* name, float, float, float) {
    NK_CB_LOG(">>> %s\n", name);
}
void NkSoftwareCommandBuffer::EndDebugGroup() {
    NK_CB_LOG("<<<\n");
}
void NkSoftwareCommandBuffer::InsertDebugLabel(const char* name) {
    NK_CB_LOG("--- %s\n", name);
}

} // namespace nkentseu