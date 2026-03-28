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
    bool NkSoftwareCommandBuffer::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                            NkFramebufferHandle fb,
                                            const NkRect2D& area) {
        if (!mRecording || !fb.IsValid() || area.width <= 0 || area.height <= 0) return false;
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
        return true;
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
        mScissorRect = r;
        mScissorEnabled = true;
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
    static void UnpackRGBA8(uint32 packed, NkSWVec4& outColor) {
        outColor.r = static_cast<float32>((packed >> 0) & 0xFFu) / 255.f;
        outColor.g = static_cast<float32>((packed >> 8) & 0xFFu) / 255.f;
        outColor.b = static_cast<float32>((packed >> 16) & 0xFFu) / 255.f;
        outColor.a = static_cast<float32>((packed >> 24) & 0xFFu) / 255.f;
    }

    static NkSWVertex TransformVertex(const uint8* vdata, uint32 vidx, uint32 stride,
                                    NkSWShader* shader, const void* uniforms) {
        if (shader && shader->vertFn) {
            return shader->vertFn(vdata, vidx, uniforms);
        }

        NkSWVertex v{};
        const uint8* src = vdata + static_cast<uint64>(vidx) * stride;

        // UI fallback: float2 pos + float2 uv + uint32 packed color.
        if (stride == 20) {
            float32 px = 0.f;
            float32 py = 0.f;
            float32 u = 0.f;
            float32 vv = 0.f;
            uint32 packed = 0xFFFFFFFFu;
            memcpy(&px, src + 0, 4);
            memcpy(&py, src + 4, 4);
            memcpy(&u, src + 8, 4);
            memcpy(&vv, src + 12, 4);
            memcpy(&packed, src + 16, 4);

            float32 vpW = 0.f;
            float32 vpH = 0.f;
            if (uniforms) {
                const float32* viewport = static_cast<const float32*>(uniforms);
                vpW = viewport[0];
                vpH = viewport[1];
            }

            if (vpW > 1.f && vpH > 1.f) {
                const float32 ndcX = (px / vpW) * 2.f - 1.f;
                const float32 ndcY = 1.f - (py / vpH) * 2.f;
                v.position = {ndcX, ndcY, 0.f, 1.f};
            } else {
                v.position = {px, py, 0.f, 1.f};
            }
            v.uv = {u, vv};
            UnpackRGBA8(packed, v.color);
            return v;
        }

        // Generic fallback for non-UI pipelines.
        if (stride >= 12) {
            memcpy(&v.position.x, src + 0, 4);
            memcpy(&v.position.y, src + 4, 4);
            memcpy(&v.position.z, src + 8, 4);
            v.position.w = 1.f;
        }
        if (stride >= 28) {
            memcpy(&v.color.r, src + 12, 4);
            memcpy(&v.color.g, src + 16, 4);
            memcpy(&v.color.b, src + 20, 4);
            memcpy(&v.color.a, src + 24, 4);
        } else {
            v.color = {1, 1, 1, 1};
        }
        if (stride >= 36) {
            memcpy(&v.uv.x, src + 28, 4);
            memcpy(&v.uv.y, src + 32, 4);
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

    static const NkSWTexture* ResolveTextureData(NkSoftwareDevice* dev, uint64 descSetId) {
        if (!dev || descSetId == 0) return nullptr;
        auto* ds = dev->GetDescSet(descSetId);
        if (!ds) return nullptr;

        for (const auto& b : ds->bindings) {
            if (b.type != NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER &&
                b.type != NkDescriptorType::NK_SAMPLED_TEXTURE) {
                continue;
            }
            if (b.texId == 0) continue;
            return dev->GetTex(b.texId);
        }
        return nullptr;
    }

    void NkSoftwareCommandBuffer::Draw(uint32 vtx, uint32 inst, uint32 firstVtx, uint32 firstInst) {
        const uint64 pipelineId = mBoundPipelineId;
        const uint64 descSetId = mBoundDescSetId;
        const uint64 vbId = mBoundVertexBufferIds[0];
        const uint64 vbOffset = mBoundVertexOffsets[0];
        const uint64 fbId = mCurrentFramebufferId;
        const NkRect2D scissorRect = mScissorRect;
        const bool scissorEnabled = mScissorEnabled;
        Push([vtx, inst, firstVtx, firstInst, pipelineId, descSetId, vbId, vbOffset, fbId, scissorRect, scissorEnabled](NkSoftwareDevice* dev) {
            auto* pipe = dev->GetPipe(pipelineId);
            auto* sh   = pipe ? dev->GetShader(pipe->shaderId) : nullptr;
            auto* vb   = dev->GetBuf(vbId);
            if (!vb) return;

            auto* fbo = dev->GetFBO(fbId);
            if (!fbo) fbo = dev->GetFBO(dev->GetSwapchainFramebuffer().id);
            if (!fbo) return;
            auto* color = dev->GetTex(fbo->colorId);
            auto* depth = dev->GetTex(fbo->depthId);
            NkSWTexture* rasterTarget = color ? color : depth;
            if (!rasterTarget) return;
            const void* uniformData = ResolveUniformData(dev, descSetId);
            const NkSWTexture* textureData = ResolveTextureData(dev, descSetId);

            uint32 stride = pipe ? pipe->vertexStride : 32;
            if (stride == 0) {
                NK_SW_CMD_LOG("Draw ignore: stride=0 (pipeline=%llu)\n", (unsigned long long)pipelineId);
                return;
            }
            if (vbOffset >= vb->data.Size()) {
                NK_SW_CMD_LOG("Draw ignore: vbOffset out of range (offset=%llu size=%llu)\n",
                              (unsigned long long)vbOffset,
                              (unsigned long long)vb->data.Size());
                return;
            }

            const uint64 vertexCapacity = (vb->data.Size() - vbOffset) / stride;
            if ((uint64)firstVtx >= vertexCapacity) {
                NK_SW_CMD_LOG("Draw ignore: firstVtx out of range (first=%u cap=%llu)\n",
                              firstVtx, (unsigned long long)vertexCapacity);
                return;
            }

            uint64 safeVertexCount64 = vertexCapacity - (uint64)firstVtx;
            if (safeVertexCount64 > (uint64)vtx) safeVertexCount64 = (uint64)vtx;
            if (safeVertexCount64 == 0) return;
            const uint32 safeVertexCount = (uint32)safeVertexCount64;

            const uint8* vdata = vb->data.Data() + vbOffset;

            NkSWRasterizer raster;
            raster.width = rasterTarget->Width();
            raster.height = rasterTarget->Height();
            raster.ResetClipRect();
            if (scissorEnabled) {
                raster.SetClipRect(scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);
            }

            NkSWRasterState rs;
            rs.colorTarget = color;
            rs.depthTarget = depth;
            rs.pipeline    = pipe;
            rs.shader      = sh;
            rs.uniformData = uniformData;
            rs.texSampler  = textureData;
            rs.vertexData  = vdata;
            rs.vertexStride= stride;
            raster.SetState(rs);

            for (uint32 i = 0; i < inst; i++) {
                NkVector<NkSWVertex> verts(safeVertexCount);
                for (uint32 j = 0; j < safeVertexCount; j++)
                    verts[j] = TransformVertex(vdata, firstVtx + j, stride, sh, uniformData);
                raster.DrawTriangles(verts.Data(), safeVertexCount);
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
        const NkRect2D scissorRect = mScissorRect;
        const bool scissorEnabled = mScissorEnabled;
        Push([idx, inst, firstIdx, vtxOff, firstInst,
            pipelineId, descSetId, vbId, vbOffset, ibId, ibOffset, indexUint32, fbId, scissorRect, scissorEnabled](NkSoftwareDevice* dev) {
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
            NkSWTexture* rasterTarget = color ? color : depth;
            if (!rasterTarget) return;
            const void* uniformData = ResolveUniformData(dev, descSetId);
            const NkSWTexture* textureData = ResolveTextureData(dev, descSetId);

            uint32 stride  = pipe ? pipe->vertexStride : 32;
            if (stride == 0) {
                NK_SW_CMD_LOG("DrawIndexed ignore: stride=0 (pipeline=%llu)\n", (unsigned long long)pipelineId);
                return;
            }
            if (vbOffset >= vb->data.Size()) {
                NK_SW_CMD_LOG("DrawIndexed ignore: vbOffset out of range (offset=%llu size=%llu)\n",
                              (unsigned long long)vbOffset,
                              (unsigned long long)vb->data.Size());
                return;
            }
            const uint64 vertexCapacity = (vb->data.Size() - vbOffset) / stride;
            if (vertexCapacity == 0) {
                NK_SW_CMD_LOG("DrawIndexed ignore: vertexCapacity=0\n");
                return;
            }

            const uint64 indexStride = indexUint32 ? 4ull : 2ull;
            if (ibOffset >= ib->data.Size()) {
                NK_SW_CMD_LOG("DrawIndexed ignore: ibOffset out of range (offset=%llu size=%llu)\n",
                              (unsigned long long)ibOffset,
                              (unsigned long long)ib->data.Size());
                return;
            }
            const uint64 indexCapacity = (ib->data.Size() - ibOffset) / indexStride;
            if ((uint64)firstIdx >= indexCapacity) {
                NK_SW_CMD_LOG("DrawIndexed ignore: firstIdx out of range (first=%u cap=%llu)\n",
                              firstIdx, (unsigned long long)indexCapacity);
                return;
            }
            uint64 safeIndexCount64 = indexCapacity - (uint64)firstIdx;
            if (safeIndexCount64 > (uint64)idx) safeIndexCount64 = (uint64)idx;
            if (safeIndexCount64 == 0) return;
            const uint32 safeIndexCount = (uint32)safeIndexCount64;

            const uint8* vdata = vb->data.Data() + vbOffset;
            const uint8* idata = ib->data.Data() + ibOffset;

            NkSWRasterizer raster;
            raster.width = rasterTarget->Width();
            raster.height = rasterTarget->Height();
            raster.ResetClipRect();
            if (scissorEnabled) {
                raster.SetClipRect(scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);
            }

            NkSWRasterState rs;
            rs.colorTarget = color;
            rs.depthTarget = depth;
            rs.pipeline = pipe;
            rs.shader = sh;
            rs.uniformData = uniformData;
            rs.texSampler = textureData;
            raster.SetState(rs);

            for (uint32 i = 0; i < inst; i++) {
                NkVector<NkSWVertex> verts(safeIndexCount);
                for (uint32 j = 0; j < safeIndexCount; j++) {
                    uint32 rawIndex = indexUint32
                        ? ((const uint32*)idata)[firstIdx + j]
                        : (uint32)((const uint16_t*)idata)[firstIdx + j];
                    const int64 signedIndex = (int64)rawIndex + (int64)vtxOff;
                    if (signedIndex < 0 || (uint64)signedIndex >= vertexCapacity) {
                        NK_SW_CMD_LOG("DrawIndexed ignore: index out of range (raw=%u base=%d cap=%llu)\n",
                                      rawIndex, vtxOff, (unsigned long long)vertexCapacity);
                        return;
                    }
                    const uint32 vi = (uint32)signedIndex;
                    verts[j] = TransformVertex(vdata, vi, stride, sh, uniformData);
                }
                raster.DrawTriangles(verts.Data(), safeIndexCount);
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
