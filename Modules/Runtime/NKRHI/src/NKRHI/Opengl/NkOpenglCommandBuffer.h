#pragma once
// =============================================================================
// NkOpenGLCommandBuffer.h
// Command buffer OpenGL : enregistre les commandes en mémoire CPU,
// les rejoue sur le thread GL via Execute().
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKCore/NkTraits.h"
#include <cstring>

#ifndef NK_NO_GLAD2
#  include <glad/gl.h>
#endif

namespace nkentseu {

class NkOpenGLDevice;

class NkOpenGLCommandBuffer final : public NkICommandBuffer {
public:
    explicit NkOpenGLCommandBuffer(NkOpenGLDevice* dev, NkCommandBufferType type)
        : mDev(dev), mType(type) {}
    ~NkOpenGLCommandBuffer() override = default;

    // ── Cycle de vie ─────────────────────────────────────────────────────────
    bool Begin()  override { mCmds.Clear(); mRecording=true; return true; }
    void End()    override { mRecording=false; }
    void Reset()  override { mCmds.Clear(); mRecording=false; }
    bool IsValid()                  const override { return mDev!=nullptr; }
    NkCommandBufferType GetType()   const override { return mType; }

    // ── Exécution (appelé par NkOpenGLDevice::Submit) ───────────────────────────
    void Execute(NkOpenGLDevice* dev);

    // =========================================================================
    // Render Pass
    // =========================================================================
    bool BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb,
                          const NkRect2D& area) override {
        if (!mRecording || !fb.IsValid() || !rp.IsValid() || area.width <= 0 || area.height <= 0) return false;
        Push([=]{ GL_BeginRenderPass(rp,fb,area); });
        return true;
    }
    void EndRenderPass() override {
        Push([]{ /* GL : rien à faire — FBO reste actif jusqu'au prochain bind */ });
    }

    // =========================================================================
    // Viewport & Scissor
    // =========================================================================
    void SetViewport(const NkViewport& vp) override {
        Push([vp]{
            glViewport((GLint)vp.x,(GLint)vp.y,(GLsizei)vp.width,(GLsizei)vp.height);
            glDepthRangef(vp.minDepth,vp.maxDepth);
        });
    }
    void SetViewports(const NkViewport* vps, uint32 n) override {
        NkVector<NkViewport> v = CopyArray(vps, n);
        Push([v = traits::NkMove(v)] {
    #if defined(NK_OPENGL_ES)
            if (!v.empty()) {
                glViewport((GLint)v[0].x, (GLint)v[0].y, (GLsizei)v[0].width, (GLsizei)v[0].height);
                glDepthRangef(v[0].minDepth, v[0].maxDepth);
            }
    #else
            for (uint32 i = 0; i < (uint32)v.size(); i++) {
                GLfloat vp[] = {(float)v[i].x, (float)v[i].y, v[i].width, v[i].height};
                glViewportIndexedfv(i, vp);
            }
    #endif
        });
    }
    void SetScissor(const NkRect2D& r) override {
        Push([r]{
            glScissor(r.x,r.y,(GLsizei)r.width,(GLsizei)r.height);
            glEnable(GL_SCISSOR_TEST);
        });
    }
    void SetScissors(const NkRect2D* rects, uint32 n) override {
        NkVector<NkRect2D> v = CopyArray(rects, n);
        Push([v = traits::NkMove(v)] {
    #if defined(NK_OPENGL_ES)
            if (!v.empty()) {
                glScissor(v[0].x, v[0].y, (GLsizei)v[0].width, (GLsizei)v[0].height);
            }
    #else
            for (uint32 i = 0; i < (uint32)v.size(); i++)
                glScissorIndexed(i, v[i].x, v[i].y, (GLsizei)v[i].width, (GLsizei)v[i].height);
    #endif
        });
    }

    // Clear dynamique — stocké comme état du CB, lu dans GL_BeginRenderPass
    void SetClearColor(float r, float g, float b, float a = 1.f) override {
        Push([this, r, g, b, a]{ mClearR=r; mClearG=g; mClearB=b; mClearA=a; });
    }
    void SetClearDepth(float depth = 1.f, uint32 stencil = 0) override {
        Push([this, depth, stencil]{ mClearDepth=depth; mClearStencil=stencil; });
    }

    // =========================================================================
    // Pipeline
    // =========================================================================
    void BindGraphicsPipeline(NkPipelineHandle p) override {
        mBoundPipeline=p;
        Push([this,p]{ GL_BindGraphicsPipeline(p); });
    }
    void BindComputePipeline(NkPipelineHandle p) override {
        mBoundPipeline=p;
        Push([this,p]{ GL_BindComputePipeline(p); });
    }
    void BindDescriptorSet(NkDescSetHandle set, uint32 idx,
                            uint32* dynOff, uint32 dynCount) override {
        NkVector<uint32> offs = CopyArray(dynOff, dynCount);
        Push([this,set,idx,offs=traits::NkMove(offs)]{
            GL_BindDescriptorSet(set,idx,offs);
        });
    }
    void PushConstants(NkShaderStage stages, uint32 offset,
                        uint32 size, const void* data) override {
        NkVector<uint8> buf = CopyBytes(data, size);
        Push([this,stages,offset,size,buf=traits::NkMove(buf)]{
            // GL : pas de push constants natives → on passe via uniform location
            // Le shader doit déclarer un uniform block nommé "PushConstants"
            // Implémentation avec glProgramUniform1iv / glUniformBlockBinding
            (void)stages; (void)offset; (void)size;
            GLint loc=glGetUniformLocation(mCurrentProgram,"_PushConstants");
            if (loc>=0) glUniform4fv(loc,(GLsizei)(buf.size()/16),(const GLfloat*)buf.Data());
        });
    }

    // =========================================================================
    // Vertex / Index Buffers
    // =========================================================================
    void BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 offset) override {
        Push([this,binding,buf,offset]{ GL_BindVertexBuffer(binding,buf,offset); });
    }
    void BindVertexBuffers(uint32 first, const NkBufferHandle* bufs,
                            const uint64* offs, uint32 count) override {
        NkVector<NkBufferHandle> bv = CopyArray(bufs, count);
        NkVector<uint64> ov = CopyArray(offs, count);
        Push([this,first,bv=traits::NkMove(bv),ov=traits::NkMove(ov)]{
            for (uint32 i=0;i<(uint32)bv.size();i++)
                GL_BindVertexBuffer(first+i,bv[i],ov[i]);
        });
    }
    void BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 offset) override {
        Push([this,buf,fmt,offset]{ GL_BindIndexBuffer(buf,fmt,offset); });
    }

    // =========================================================================
    // Draw
    // =========================================================================
    void Draw(uint32 vertCnt, uint32 instCnt, uint32 firstVert, uint32 firstInst) override {
        Push([this, vertCnt, instCnt, firstVert, firstInst] {
            if (instCnt > 1) {
    #if defined(NK_OPENGL_ES)
                glDrawArraysInstanced(mPrimitive, firstVert, vertCnt, instCnt);
    #else
                glDrawArraysInstancedBaseInstance(mPrimitive, firstVert, vertCnt, instCnt, firstInst);
    #endif
            } else {
                glDrawArrays(mPrimitive, firstVert, vertCnt);
            }
        });
    }
    void DrawIndexed(uint32 idxCnt, uint32 instCnt, uint32 firstIdx,
                    int32 vtxOff, uint32 firstInst) override {
        Push([this, idxCnt, instCnt, firstIdx, vtxOff, firstInst] {
            GLenum idxFmt = mIndexFormat == NkIndexFormat::NK_UINT16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            uint64 byteOff = firstIdx * (mIndexFormat == NkIndexFormat::NK_UINT16 ? 2 : 4) + mIndexOffset;
            if (instCnt > 1) {
    #if defined(NK_OPENGL_ES)
                glDrawElementsInstanced(mPrimitive, idxCnt, idxFmt, (const void*)byteOff, instCnt);
    #else
                glDrawElementsInstancedBaseVertexBaseInstance(mPrimitive, idxCnt, idxFmt,
                    (const void*)byteOff, instCnt, vtxOff, firstInst);
    #endif
            } else {
                glDrawElementsBaseVertex(mPrimitive, idxCnt, idxFmt, (const void*)byteOff, vtxOff);
            }
        });
    }
    void DrawIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override {
        Push([this, buf, off, cnt, stride] {
            GL_BindForIndirect(buf);
    #if defined(NK_OPENGL_ES)
            for (uint32 i = 0; i < cnt; ++i) {
                glDrawArraysIndirect(mPrimitive, (const void*)(off + i * stride));
            }
    #else
            if (cnt > 1) glMultiDrawArraysIndirect(mPrimitive, (const void*)off, cnt, (GLsizei)stride);
            else         glDrawArraysIndirect(mPrimitive, (const void*)off);
    #endif
        });
    }
    void DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override {
        Push([this, buf, off, cnt, stride] {
            GL_BindForIndirect(buf);
            GLenum fmt = mIndexFormat == NkIndexFormat::NK_UINT16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    #if defined(NK_OPENGL_ES)
            for (uint32 i = 0; i < cnt; ++i) {
                glDrawElementsIndirect(mPrimitive, fmt, (const void*)(off + i * stride));
            }
    #else
            if (cnt > 1) glMultiDrawElementsIndirect(mPrimitive, fmt, (const void*)off, cnt, (GLsizei)stride);
            else         glDrawElementsIndirect(mPrimitive, fmt, (const void*)off);
    #endif
        });
    }

    // =========================================================================
    // Compute
    // =========================================================================
    void Dispatch(uint32 gx, uint32 gy, uint32 gz) override {
        Push([gx,gy,gz]{ glDispatchCompute(gx,gy,gz); });
    }
    void DispatchIndirect(NkBufferHandle buf, uint64 off) override {
        Push([this,buf,off]{
            GL_BindForIndirect(buf);
            glDispatchComputeIndirect((GLintptr)off);
        });
    }

    // =========================================================================
    // Copies
    // =========================================================================
    void CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                     const NkBufferCopyRegion& r) override {
        Push([this,src,dst,r]{ GL_CopyBuffer(src,dst,r); });
    }
    void CopyBufferToTexture(NkBufferHandle src, NkTextureHandle dst,
                              const NkBufferTextureCopyRegion& r) override {
        Push([this,src,dst,r]{ GL_CopyBufferToTexture(src,dst,r); });
    }
    void CopyTextureToBuffer(NkTextureHandle src, NkBufferHandle dst,
                              const NkBufferTextureCopyRegion& r) override {
        Push([this,src,dst,r]{ GL_CopyTextureToBuffer(src,dst,r); });
    }
    void CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                      const NkTextureCopyRegion& r) override {
        Push([this,src,dst,r]{ GL_CopyTexture(src,dst,r); });
    }
    void BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                      const NkTextureCopyRegion& r, NkFilter filter) override {
        Push([this,src,dst,r,filter]{ GL_BlitTexture(src,dst,r,filter); });
    }

    // =========================================================================
    // Barriers (GL : glMemoryBarrier)
    // =========================================================================
    void Barrier(const NkBufferBarrier* bb, uint32 bc,
                  const NkTextureBarrier* tb, uint32 tc) override {
        GLbitfield bits=0;
        for (uint32 i=0;i<bc;i++) {
            auto& b=bb[i];
            if (b.stateAfter==NkResourceState::NK_SHADER_READ||
                b.stateAfter==NkResourceState::NK_UNORDERED_ACCESS)
                bits|=GL_SHADER_STORAGE_BARRIER_BIT|GL_UNIFORM_BARRIER_BIT;
            if (b.stateAfter==NkResourceState::NK_INDIRECT_ARG)
                bits|=GL_COMMAND_BARRIER_BIT;
            if (b.stateAfter==NkResourceState::NK_TRANSFER_DST||
                b.stateAfter==NkResourceState::NK_TRANSFER_SRC)
                bits|=GL_BUFFER_UPDATE_BARRIER_BIT;
        }
        for (uint32 i=0;i<tc;i++) {
            auto& b=tb[i];
            if (b.stateAfter==NkResourceState::NK_SHADER_READ)
                bits|=GL_TEXTURE_FETCH_BARRIER_BIT;
            if (b.stateAfter==NkResourceState::NK_UNORDERED_ACCESS)
                bits|=GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
            if (b.stateAfter==NkResourceState::NK_RENDER_TARGET||
                b.stateAfter==NkResourceState::NK_DEPTH_WRITE)
                bits|=GL_FRAMEBUFFER_BARRIER_BIT;
            if (b.stateAfter==NkResourceState::NK_TRANSFER_DST)
                bits|=GL_TEXTURE_UPDATE_BARRIER_BIT;
        }
        if (bits==0) bits=GL_ALL_BARRIER_BITS;
        Push([bits]{ glMemoryBarrier(bits); });
    }

    // =========================================================================
    // Mip generation
    // =========================================================================
    void GenerateMipmaps(NkTextureHandle tex, NkFilter) override {
        Push([this,tex]{ GL_GenerateMipmaps(tex); });
    }

    // =========================================================================
    // Debug markers
    // =========================================================================
    void BeginDebugGroup(const char* name, float r, float g, float b) override {
        (void)r;
        (void)g;
        (void)b;
        NkVector<char> n = CopyCString(name);
        Push([n=traits::NkMove(n)]{
            const GLsizei textLen = n.Size() > 0 ? (GLsizei)(n.Size() - 1) : 0;
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, textLen, n.Size() > 0 ? n.Data() : "");
        });
    }
    void EndDebugGroup() override {
        Push([]{ glPopDebugGroup(); });
    }
    void InsertDebugLabel(const char* name) override {
        NkVector<char> n = CopyCString(name);
        Push([n=traits::NkMove(n)]{
            const GLsizei textLen = n.Size() > 0 ? (GLsizei)(n.Size() - 1) : 0;
            glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
                0, GL_DEBUG_SEVERITY_NOTIFICATION, textLen, n.Size() > 0 ? n.Data() : "");
        });
    }

    // =========================================================================
    // Timestamp
    // =========================================================================
    void WriteTimestamp(uint32 idx) override {
    #if defined(NK_OPENGL_ES)
        (void)idx;
        // OpenGL ES ne supporte pas les timestamp queries de cette manière.
    #else
        Push([idx] { glQueryCounter(idx, GL_TIMESTAMP); });
    #endif
    }

private:
    using Cmd = NkFunction<void()>;
    void Push(Cmd c) { mCmds.PushBack(traits::NkMove(c)); }

    template<typename T>
    static NkVector<T> CopyArray(const T* data, uint32 count) {
        NkVector<T> out;
        if (!data || count == 0) return out;
        out.Reserve(count);
        for (uint32 i = 0; i < count; ++i) out.PushBack(data[i]);
        return out;
    }

    static NkVector<uint8> CopyBytes(const void* data, uint32 size) {
        NkVector<uint8> out;
        if (!data || size == 0) return out;
        const uint8* bytes = static_cast<const uint8*>(data);
        out.Reserve(size);
        for (uint32 i = 0; i < size; ++i) out.PushBack(bytes[i]);
        return out;
    }

    static NkVector<char> CopyCString(const char* text) {
        NkVector<char> out;
        if (!text) {
            out.PushBack('\0');
            return out;
        }
        const usize len = std::strlen(text);
        out.Reserve(len + 1);
        for (usize i = 0; i < len; ++i) out.PushBack(text[i]);
        out.PushBack('\0');
        return out;
    }

    // ── Implémentations GL appelées depuis Execute ────────────────────────────
    void GL_BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb,
                             const NkRect2D& area);
    void GL_BindGraphicsPipeline(NkPipelineHandle p);
    void GL_BindComputePipeline (NkPipelineHandle p);
    void GL_BindDescriptorSet   (NkDescSetHandle set, uint32 idx,
                                  const NkVector<uint32>& dynOff);
    void GL_BindVertexBuffer    (uint32 binding, NkBufferHandle buf, uint64 off);
    void GL_BindIndexBuffer     (NkBufferHandle buf, NkIndexFormat fmt, uint64 off);
    void GL_BindForIndirect     (NkBufferHandle buf);
    void GL_CopyBuffer          (NkBufferHandle src, NkBufferHandle dst,
                                  const NkBufferCopyRegion& r);
    void GL_CopyBufferToTexture (NkBufferHandle src, NkTextureHandle dst,
                                  const NkBufferTextureCopyRegion& r);
    void GL_CopyTextureToBuffer (NkTextureHandle src, NkBufferHandle dst,
                                  const NkBufferTextureCopyRegion& r);
    void GL_CopyTexture         (NkTextureHandle src, NkTextureHandle dst,
                                  const NkTextureCopyRegion& r);
    void GL_BlitTexture         (NkTextureHandle src, NkTextureHandle dst,
                                  const NkTextureCopyRegion& r, NkFilter filter);
    void GL_GenerateMipmaps     (NkTextureHandle tex);

    NkOpenGLDevice*       mDev         = nullptr;
    NkCommandBufferType mType;
    NkVector<Cmd>   mCmds;
    bool               mRecording   = false;

    // Valeurs de clear dynamiques (SetClearColor / SetClearDepth)
    float   mClearR = 0.f, mClearG = 0.f, mClearB = 0.f, mClearA = 1.f;
    float   mClearDepth   = 1.f;
    uint32  mClearStencil = 0;

    // État courant (mis à jour par les commandes Bind*)
    NkPipelineHandle   mBoundPipeline;
    GLuint             mCurrentProgram  = 0;
    GLuint             mCurrentVAO      = 0;
    GLenum             mPrimitive       = GL_TRIANGLES;
    NkIndexFormat      mIndexFormat     = NkIndexFormat::NK_UINT16;
    uint64             mIndexOffset     = 0;
    bool               mIsCompute       = false;
};

} // namespace nkentseu
