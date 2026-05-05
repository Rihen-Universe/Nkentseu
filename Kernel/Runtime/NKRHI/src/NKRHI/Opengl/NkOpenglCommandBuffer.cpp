// =============================================================================
// NkOpenGLCommandBuffer.cpp
// =============================================================================
#include "NkOpenglCommandBuffer.h"
#include "NkOpenglDevice.h"
#include <cstdio>

namespace nkentseu {

// Accès aux internals du device via amitié
extern GLuint   NkOpenglGetBufferID (NkOpenGLDevice* dev, uint64 id);
extern GLuint   NkOpenglGetTextureID(NkOpenGLDevice* dev, uint64 id);
extern GLuint   NkOpenglGetFBOID    (NkOpenGLDevice* dev, uint64 id);
extern GLuint   NkOpenglGetSamplerID(NkOpenGLDevice* dev, uint64 id);
extern void     NkOpenglApplyRenderState(NkOpenGLDevice* dev, uint64 pipelineId);
extern GLuint   NkOpenglGetProgramID(NkOpenGLDevice* dev, uint64 id);
extern GLuint   NkOpenglGetVAOID   (NkOpenGLDevice* dev, uint64 id);
extern GLenum   NkOpenglGetPrimitive(NkOpenGLDevice* dev, uint64 id);
extern uint32   NkOpenglGetVertexStride(NkOpenGLDevice* dev, uint64 pipelineId, uint32 binding);
extern bool     NkOpenglIsCompute   (NkOpenGLDevice* dev, uint64 id);
extern void     NkOpenglApplyDescSet(NkOpenGLDevice* dev, uint64 setId,
                                   const NkVector<uint32>& dynOff);

// =============================================================================
void NkOpenGLCommandBuffer::Execute(NkOpenGLDevice* dev) {
    mDev = dev;
    for (auto& cmd : mCmds) cmd();
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BeginRenderPass(NkRenderPassHandle rp,
                                             NkFramebufferHandle fb,
                                             const NkRect2D& area) {
    GLuint fboId = NkOpenglGetFBOID(mDev, fb.id);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    GLbitfield clearBits = 0;
    clearBits |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

    glViewport(area.x, area.y, (GLsizei)area.width, (GLsizei)area.height);

    glClearColor(mClearR, mClearG, mClearB, mClearA);
    glClearDepthf(mClearDepth);
    glClearStencil((GLint)mClearStencil);
    glClear(clearBits);

    (void)rp;
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindGraphicsPipeline(NkPipelineHandle p) {
    mBoundPipeline = p;
    mCurrentProgram = NkOpenglGetProgramID(mDev, p.id);
    mCurrentVAO     = NkOpenglGetVAOID   (mDev, p.id);
    mPrimitive      = NkOpenglGetPrimitive(mDev, p.id);
    mIsCompute      = false;

    glUseProgram(mCurrentProgram);
    glBindVertexArray(mCurrentVAO);
    NkOpenglApplyRenderState(mDev, p.id);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindComputePipeline(NkPipelineHandle p) {
    mCurrentProgram = NkOpenglGetProgramID(mDev, p.id);
    mIsCompute      = true;
    glUseProgram(mCurrentProgram);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindDescriptorSet(NkDescSetHandle set, uint32 idx,
                                               const NkVector<uint32>& dynOff) {
    (void)idx;
    NkOpenglApplyDescSet(mDev, set.id, dynOff);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindVertexBuffer(uint32 binding,
                                              NkBufferHandle buf, uint64 off) {
    GLuint bufId = NkOpenglGetBufferID(mDev, buf.id);
    uint32 stride = NkOpenglGetVertexStride(mDev, mBoundPipeline.id, binding);
    if (stride == 0) stride = 1;
#if defined(NK_OPENGL_ES)
    // Sur ES, on doit binder le buffer et configurer l'attribut manuellement
    glBindBuffer(GL_ARRAY_BUFFER, bufId);
    // Le stride et offset sont configurés via glVertexAttribPointer dans le pipeline
    // On doit répéter l'appel pour chaque attribut de ce binding
    // Simplification : on suppose que le VAO stocke déjà ces infos (ce qui n'est pas le cas en ES)
    // Pour une implémentation complète, il faudrait stocker les descriptions d'attributs.
    glBindVertexBuffer(binding, bufId, (GLintptr)off, (GLsizei)stride);
#else
    glBindVertexBuffer((GLuint)binding, bufId, (GLintptr)off, (GLsizei)stride);
#endif
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindIndexBuffer(NkBufferHandle buf,
                                             NkIndexFormat fmt, uint64 off) {
    mIndexFormat = fmt;
    mIndexOffset = off;
    GLuint bufId = NkOpenglGetBufferID(mDev, buf.id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufId);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindForIndirect(NkBufferHandle buf) {
    GLuint bufId = NkOpenglGetBufferID(mDev, buf.id);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, bufId);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                        const NkBufferCopyRegion& r) {
    GLuint s=NkOpenglGetBufferID(mDev,src.id);
    GLuint d=NkOpenglGetBufferID(mDev,dst.id);
#if defined(NK_OPENGL_ES)
    glBindBuffer(GL_COPY_READ_BUFFER, s);
    glBindBuffer(GL_COPY_WRITE_BUFFER, d);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                        (GLintptr)r.srcOffset, (GLintptr)r.dstOffset, (GLsizeiptr)r.size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
#else
    glCopyNamedBufferSubData(s,d,(GLintptr)r.srcOffset,(GLintptr)r.dstOffset,(GLsizeiptr)r.size);
#endif
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyBufferToTexture(NkBufferHandle src,
    NkTextureHandle dst, const NkBufferTextureCopyRegion& r) {
    GLuint bufId = NkOpenglGetBufferID (mDev, src.id);
    GLuint texId = NkOpenglGetTextureID(mDev, dst.id);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bufId);
#if defined(NK_OPENGL_ES)
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexSubImage2D(GL_TEXTURE_2D,(GLint)r.mipLevel,
        (GLint)r.x,(GLint)r.y,(GLsizei)r.width,(GLsizei)r.height,
        GL_RGBA, GL_UNSIGNED_BYTE, (const void*)r.bufferOffset);
    glBindTexture(GL_TEXTURE_2D, 0);
#else
    glTextureSubImage2D(texId,(GLint)r.mipLevel,
        (GLint)r.x,(GLint)r.y,(GLsizei)r.width,(GLsizei)r.height,
        GL_RGBA, GL_UNSIGNED_BYTE, (const void*)r.bufferOffset);
#endif
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyTextureToBuffer(NkTextureHandle src,
    NkBufferHandle dst, const NkBufferTextureCopyRegion& r) {
    GLuint texId = NkOpenglGetTextureID(mDev, src.id);
    GLuint bufId = NkOpenglGetBufferID (mDev, dst.id);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, bufId);
#if defined(NK_OPENGL_ES)
    // OpenGL ES n'a pas glGetTextureImage ; on utilise glReadPixels sur un FBO
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, (GLint)r.mipLevel);
    glReadPixels((GLint)r.x, (GLint)r.y, (GLsizei)r.width, (GLsizei)r.height,
                 GL_RGBA, GL_UNSIGNED_BYTE, (void*)r.bufferOffset);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
#else
    glGetTextureImage(texId,(GLint)r.mipLevel,GL_RGBA,GL_UNSIGNED_BYTE,
        (GLsizei)(r.width*r.height*4),(void*)r.bufferOffset);
#endif
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                         const NkTextureCopyRegion& r) {
    GLuint s=NkOpenglGetTextureID(mDev,src.id);
    GLuint d=NkOpenglGetTextureID(mDev,dst.id);
#if defined(NK_OPENGL_ES)
    // glCopyImageSubData n'est pas disponible en ES 3.0, utiliser FBO blit
    GLuint srcFBO=0, dstFBO=0;
    glGenFramebuffers(1,&srcFBO);
    glGenFramebuffers(1,&dstFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s, (GLint)r.srcMip);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d, (GLint)r.dstMip);
    glBlitFramebuffer((GLint)r.srcX, (GLint)r.srcY, (GLint)(r.srcX+r.width), (GLint)(r.srcY+r.height),
                      (GLint)r.dstX, (GLint)r.dstY, (GLint)(r.dstX+r.width), (GLint)(r.dstY+r.height),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1,&srcFBO);
    glDeleteFramebuffers(1,&dstFBO);
#else
    glCopyImageSubData(
        s,GL_TEXTURE_2D,(GLint)r.srcMip,(GLint)r.srcX,(GLint)r.srcY,(GLint)r.srcZ,
        d,GL_TEXTURE_2D,(GLint)r.dstMip,(GLint)r.dstX,(GLint)r.dstY,(GLint)r.dstZ,
        (GLsizei)r.width,(GLsizei)r.height,(GLsizei)r.depth);
#endif
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                                         const NkTextureCopyRegion& r, NkFilter filter) {
    GLuint srcFBO=0, dstFBO=0;
    glGenFramebuffers(1,&srcFBO);
    glGenFramebuffers(1,&dstFBO);

    GLuint srcId=NkOpenglGetTextureID(mDev,src.id);
    GLuint dstId=NkOpenglGetTextureID(mDev,dst.id);

#if defined(NK_OPENGL_ES)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcId, (GLint)r.srcMip);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstId, (GLint)r.dstMip);
    glBlitFramebuffer(
        (GLint)r.srcX,(GLint)r.srcY,(GLint)(r.srcX+r.width),(GLint)(r.srcY+r.height),
        (GLint)r.dstX,(GLint)r.dstY,(GLint)(r.dstX+r.width),(GLint)(r.dstY+r.height),
        GL_COLOR_BUFFER_BIT,
        filter==NkFilter::NK_NEAREST ? GL_NEAREST : GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
#else
    glNamedFramebufferTexture(srcFBO,GL_COLOR_ATTACHMENT0,srcId,(GLint)r.srcMip);
    glNamedFramebufferTexture(dstFBO,GL_COLOR_ATTACHMENT0,dstId,(GLint)r.dstMip);
    glBlitNamedFramebuffer(srcFBO,dstFBO,
        (GLint)r.srcX,(GLint)r.srcY,(GLint)(r.srcX+r.width),(GLint)(r.srcY+r.height),
        (GLint)r.dstX,(GLint)r.dstY,(GLint)(r.dstX+r.width),(GLint)(r.dstY+r.height),
        GL_COLOR_BUFFER_BIT,
        filter==NkFilter::NK_NEAREST ? GL_NEAREST : GL_LINEAR);
#endif

    glDeleteFramebuffers(1,&srcFBO);
    glDeleteFramebuffers(1,&dstFBO);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_GenerateMipmaps(NkTextureHandle tex) {
    GLuint id=NkOpenglGetTextureID(mDev,tex.id);
#if defined(NK_OPENGL_ES)
    glBindTexture(GL_TEXTURE_2D, id);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
#else
    glGenerateTextureMipmap(id);
#endif
}

} // namespace nkentseu