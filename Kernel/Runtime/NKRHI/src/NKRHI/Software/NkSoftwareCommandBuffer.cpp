// =============================================================================
// NkSoftwareCommandBuffer.cpp — Replay des commandes sur le rasteriseur CPU
// =============================================================================
#include "NkSoftwareCommandBuffer.h"
#include "NkSoftwareDevice.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkTraits.h"
#include "NkSWFastPath.h"
#include <cstring>

namespace nkentseu {

    #define NK_SW_CMD_LOG(...) logger_src.Infof("[NkRHI_SW] " __VA_ARGS__)
    thread_local static NkVector<NkVertexSoftware> sTempVerts;

    // Clear couleur ultra-rapide avec memset SIMD
    static void FastClearColor(uint8* pixels, uint32 w, uint32 h,
                                uint8 r, uint8 g, uint8 b, uint8 a) {
        const uint32 px = nkentseu::sw_detail::PackNative(r, g, b, a);
    #ifdef NK_SW_AVX2
        const __m256i vpx = _mm256_set1_epi32((int)px);
        uint8* p = pixels;
        usize n = (usize)w * h;
        while (n >= 8) {
            _mm256_storeu_si256((__m256i*)p, vpx);
            p += 32; n -= 8;
        }
        uint32* p32 = (uint32*)p;
        while (n-- > 0) *p32++ = px;
    #elif defined(NK_SW_SSE2)
        const __m128i vpx = _mm_set1_epi32((int)px);
        uint8* p = pixels;
        usize n = (usize)w * h;
        while (n >= 4) {
            _mm_storeu_si128((__m128i*)p, vpx);
            p += 16; n -= 4;
        }
        uint32* p32 = (uint32*)p;
        while (n-- > 0) *p32++ = px;
    #else
        // Fallback : remplir le premier pixel et copier par doublements
        uint32* p32 = (uint32*)pixels;
        const usize total = (usize)w * h;
        p32[0] = px;
        for (usize i = 1; i < total; i *= 2)
            memcpy(p32 + i, p32, (i * 2 > total ? total - i : i) * 4);
    #endif
    }

    // ── Helpers clip rect ────────────────────────────────────────────────────────
    static void ComputeClipRect(NkSoftwareDevice* dev, uint64 fbId,
                                const NkRect2D& sc, bool scEn,
                                int32& cx0, int32& cy0, int32& cx1, int32& cy1)
    {
        uint32 W=dev->GetSwapchainWidth(), H=dev->GetSwapchainHeight();
        if (fbId) {
            if (auto* fbo=dev->GetFBO(fbId)) {
                if (auto* ct=fbo->colorId?dev->GetTex(fbo->colorId):nullptr)
                    {W=ct->Width();H=ct->Height();}
                else if (auto* dt=fbo->depthId?dev->GetTex(fbo->depthId):nullptr)
                    {W=dt->Width();H=dt->Height();}
            }
        }
        cx0=0; cy0=0; cx1=(int32)W; cy1=(int32)H;
        if (scEn&&sc.width>0&&sc.height>0) {
            cx0=sc.x; cy0=sc.y; cx1=sc.x+sc.width; cy1=sc.y+sc.height;
            if (cx0<0)cx0=0; if(cy0<0)cy0=0;
            if(cx1>(int32)W)cx1=(int32)W;
            if(cy1>(int32)H)cy1=(int32)H;
        }
    }
    
    // Clear depth rapide (1.0f = 0x3F800000)
    static void FastClearDepth(float32* depthBuf, uint32 w, uint32 h) {
        // 1.0f → bits = 0x3F800000 → répétable comme uint32
        const uint32 one = 0x3F800000u;
        uint32* p = (uint32*)depthBuf;
        const usize total = (usize)w * h;
    #ifdef NK_SW_AVX2
        const __m256i v1 = _mm256_set1_epi32((int)one);
        usize i = 0;
        for (; i+8 <= total; i+=8)
            _mm256_storeu_si256((__m256i*)(p+i), v1);
        for (; i < total; ++i) p[i] = one;
    #elif defined(NK_SW_SSE2)
        const __m128i v1 = _mm_set1_epi32((int)one);
        usize i = 0;
        for (; i+4 <= total; i+=4)
            _mm_storeu_si128((__m128i*)(p+i), v1);
        for (; i < total; ++i) p[i] = one;
    #else
        p[0] = one;
        for (usize i = 1; i < total; i *= 2)
            memcpy(p+i, p, (i*2>total?total-i:i)*4);
    #endif
    }

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

    NkSoftwareCommandBuffer::NkSoftwareCommandBuffer(NkSoftwareDevice* dev, NkCommandBufferType type) : mDev(dev), mType(type) {}

    void NkSoftwareCommandBuffer::Execute(NkSoftwareDevice* dev) {
        for (auto& cmd : mCommands) cmd(dev);
    }

    // =============================================================================
    // Render Pass
    // =============================================================================
    bool NkSoftwareCommandBuffer::BeginRenderPass(NkRenderPassHandle /*rp*/, NkFramebufferHandle fb, const NkRect2D& area) {
        if (!mRecording || !fb.IsValid() || area.width <= 0 || area.height <= 0) return false;

        mCurrentFramebufferId = fb.id;
        uint64 fbId = fb.id;
        NkRect2D a  = area;

        // Capturer les valeurs de clear dynamiques au moment de l'enregistrement
        uint8 cr = (uint8)(mClearR * 255.f);
        uint8 cg = (uint8)(mClearG * 255.f);
        uint8 cb = (uint8)(mClearB * 255.f);
        uint8 ca = (uint8)(mClearA * 255.f);
        float cdepth = mClearDepth;

        Push([fbId, a, cr, cg, cb, ca, cdepth](NkSoftwareDevice* dev) {
            auto* fbo = dev->GetFBO(fbId);
            if (!fbo) return;
            auto* color = dev->GetTex(fbo->colorId);
            auto* depth = dev->GetTex(fbo->depthId);

            // ── Clear couleur SIMD ────────────────────────────────────────────────────
            if (color && !color->mips.Empty()) {
                swfast::FastClearColor(color->mips[0].Data(), color->Width(), color->Height(), cr, cg, cb, ca);
            }

            // ── Clear depth ───────────────────────────────────────────────────────────
            if (depth && !depth->mips.Empty()) {
                swfast::FastClearDepth(reinterpret_cast<float32*>(depth->mips[0].Data()), depth->Width(), depth->Height());
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

    static NkVertexSoftware TransformVertex(const uint8* vdata, uint32 vidx, uint32 stride,
                                    NkSWShader* shader, const void* uniforms) {
        if (shader && shader->vertFn) {
            NkVertexSoftware vertex = {};
            vertex = shader->vertFn(vdata, vidx, uniforms);
            return vertex;
        }

        NkVertexSoftware v{};
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

    void NkSoftwareCommandBuffer::Draw(uint32 vtx, uint32 inst,
                                    uint32 firstVtx, uint32 /*firstInst*/) {
        const uint64   pipeId=mBoundPipelineId, descId=mBoundDescSetId;
        const uint64   vbId=mBoundVertexBufferIds[0], vbOff=mBoundVertexOffsets[0];
        const uint64   fbId=mCurrentFramebufferId;
        const NkRect2D sc=mScissorRect; const bool scEn=mScissorEnabled;
    
        Push([vtx,inst,firstVtx,pipeId,descId,vbId,vbOff,fbId,sc,scEn](NkSoftwareDevice* dev){
            int32 cx0,cy0,cx1,cy1;
            ComputeClipRect(dev,fbId,sc,scEn,cx0,cy0,cx1,cy1);
            swfast::ExecuteDrawFast(dev,pipeId,descId,vbId,vbOff,fbId, firstVtx,vtx,inst,cx0,cy0,cx1,cy1);
        });
    }

    void NkSoftwareCommandBuffer::DrawIndexed(uint32 idx, uint32 inst,
                                            uint32 firstIdx, int32 vtxOff,
                                            uint32 /*firstInst*/) {
        const uint64   pipeId=mBoundPipelineId, descId=mBoundDescSetId;
        const uint64   vbId=mBoundVertexBufferIds[0], vbOff=mBoundVertexOffsets[0];
        const uint64   ibId=mBoundIndexBufferId, ibOff=mBoundIndexOffset;
        const bool     ibU32=mBoundIndexUint32;
        const uint64   fbId=mCurrentFramebufferId;
        const NkRect2D sc=mScissorRect; const bool scEn=mScissorEnabled;
    
        Push([idx,inst,firstIdx,vtxOff,pipeId,descId,vbId,vbOff,
            ibId,ibOff,ibU32,fbId,sc,scEn](NkSoftwareDevice* dev){
            int32 cx0,cy0,cx1,cy1;
            ComputeClipRect(dev,fbId,sc,scEn,cx0,cy0,cx1,cy1);
            swfast::ExecuteDrawIndexedFast(dev,pipeId,descId,vbId,vbOff,
                                            ibId,ibOff,ibU32,fbId,
                                            firstIdx,idx,vtxOff,inst,
                                            cx0,cy0,cx1,cy1);
        });
    }

    void NkSoftwareCommandBuffer::DrawIndirect(NkBufferHandle indirectBuf, uint64 offset, uint32 drawCount, uint32 stride) {
        const uint64 pipelineId    = mBoundPipelineId;
        const uint64 descSetId     = mBoundDescSetId;
        const uint64 vbId          = mBoundVertexBufferIds[0];
        const uint64 vbOffset      = mBoundVertexOffsets[0];
        const uint64 fbId          = mCurrentFramebufferId;
        const NkRect2D scissorRect = mScissorRect;
        const bool   scissorEnabled = mScissorEnabled;
        const uint64 indirectBufId = indirectBuf.id;
        const uint32 drawStride    = (stride == 0) ? 16 : stride; // standard D3D12/Vulkan

        Push([pipelineId, descSetId, vbId, vbOffset, indirectBufId, offset, drawCount, drawStride,
            fbId, scissorRect, scissorEnabled](NkSoftwareDevice* dev) {

            // swfast::tl_vertPool.Reset();

            for (uint32 i = 0; i < drawCount; ++i) {
                uint64 cmdOffset = offset + i * drawStride;
                swfast::ExecuteDrawIndirectFast(
                    dev,
                    pipelineId, descSetId,
                    vbId, vbOffset,
                    indirectBufId, cmdOffset,
                    fbId,
                    drawStride,
                    scissorRect, scissorEnabled);
            }
        });
    }

    void NkSoftwareCommandBuffer::DrawIndexedIndirect(NkBufferHandle indirectBuf, uint64 offset, uint32 drawCount, uint32 stride) {
        const uint64 pipelineId    = mBoundPipelineId;
        const uint64 descSetId     = mBoundDescSetId;
        const uint64 vbId          = mBoundVertexBufferIds[0];
        const uint64 vbOffset      = mBoundVertexOffsets[0];
        const uint64 ibId          = mBoundIndexBufferId;
        const uint64 ibOffset      = mBoundIndexOffset;
        const bool   indexUint32   = mBoundIndexUint32;
        const uint64 fbId          = mCurrentFramebufferId;
        const NkRect2D scissorRect = mScissorRect;
        const bool   scissorEnabled = mScissorEnabled;
        const uint64 indirectBufId = indirectBuf.id;
        const uint32 drawStride    = (stride == 0) ? 20 : stride; // standard pour indexed

        Push([pipelineId, descSetId, vbId, vbOffset, ibId, ibOffset, indexUint32,
            indirectBufId, offset, drawCount, drawStride,
            fbId, scissorRect, scissorEnabled](NkSoftwareDevice* dev) {

            // swfast::tl_vertPool.Reset();

            for (uint32 i = 0; i < drawCount; ++i) {
                uint64 cmdOffset = offset + i * drawStride;
                swfast::ExecuteDrawIndexedIndirectFast(
                    dev,
                    pipelineId, descSetId,
                    vbId, vbOffset,
                    ibId, ibOffset, indexUint32,
                    indirectBufId, cmdOffset,
                    fbId,
                    drawStride,
                    scissorRect, scissorEnabled);
            }
        });
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

    void NkSoftwareCommandBuffer::BlitTexture(NkTextureHandle src, NkTextureHandle dst, const NkTextureCopyRegion& r, NkFilter f) {
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
