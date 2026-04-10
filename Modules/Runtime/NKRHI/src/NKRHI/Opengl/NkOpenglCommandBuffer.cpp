// =============================================================================
// NkOpenGLCommandBuffer.cpp
// =============================================================================
#include "NkOpenglCommandBuffer.h"
#include "NkOpenglDevice.h"
#include <cstdio>

namespace nkentseu {

// Accès aux internals du device via amitié (friend déclaré dans le .h du device)
// On utilise un cast + accès public aux maps via méthodes d'accès dédiées.
// Pour éviter le couplage fort, le CommandBuffer appelle des méthodes privées
// exposées via une interface "interne" — pattern standard dans bgfx/DiligentEngine.

// ── Accès aux objets GL depuis les IDs ───────────────────────────────────────
// Ces helpers sont inline-friend dans NkOpenGLDevice — ici on les déclare extern
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

    // Lire le render pass pour les clear values
    // (accès via NkOpenGLDevice::GetRenderPassDesc — méthode publique)
    // On simplifie : clear toujours avec glClear standard
    GLbitfield clearBits = 0;

    // On efface tous les attachments color + depth déclarés dans le RP
    // La vraie implémentation lirait le NkRenderPassDesc pour chaque attachment
    clearBits |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

    glViewport(area.x, area.y, (GLsizei)area.width, (GLsizei)area.height);

    // Clear avec les valeurs dynamiques (SetClearColor / SetClearDepth)
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
    (void)idx; // GL n'a pas de set index natif — on lie directement
    NkOpenglApplyDescSet(mDev, set.id, dynOff);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BindVertexBuffer(uint32 binding,
                                              NkBufferHandle buf, uint64 off) {
    GLuint bufId = NkOpenglGetBufferID(mDev, buf.id);
    // stride est stocké dans le VAO du pipeline courant
    // On passe stride=0 ici — le VAO a déjà le stride configuré
    uint32 stride = NkOpenglGetVertexStride(mDev, mBoundPipeline.id, binding);
    if (stride == 0) stride = 1;
    glBindVertexBuffer((GLuint)binding, bufId, (GLintptr)off, (GLsizei)stride);
    // Note : pour que ça marche, le VAO doit avoir été configuré avec
    // le stride via glVertexArrayBindingStride (DSA) dans CreateGraphicsPipeline.
    // C'est le cas dans NkOpenGLDevice::CreateGraphicsPipeline.
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
    glCopyNamedBufferSubData(s,d,(GLintptr)r.srcOffset,(GLintptr)r.dstOffset,(GLsizeiptr)r.size);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyBufferToTexture(NkBufferHandle src,
    NkTextureHandle dst, const NkBufferTextureCopyRegion& r) {
    GLuint bufId = NkOpenglGetBufferID (mDev, src.id);
    GLuint texId = NkOpenglGetTextureID(mDev, dst.id);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bufId);
    glTextureSubImage2D(texId,(GLint)r.mipLevel,
        (GLint)r.x,(GLint)r.y,(GLsizei)r.width,(GLsizei)r.height,
        GL_RGBA, GL_UNSIGNED_BYTE, (const void*)r.bufferOffset);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyTextureToBuffer(NkTextureHandle src,
    NkBufferHandle dst, const NkBufferTextureCopyRegion& r) {
    GLuint texId = NkOpenglGetTextureID(mDev, src.id);
    GLuint bufId = NkOpenglGetBufferID (mDev, dst.id);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, bufId);
    glGetTextureImage(texId,(GLint)r.mipLevel,GL_RGBA,GL_UNSIGNED_BYTE,
        (GLsizei)(r.width*r.height*4),(void*)r.bufferOffset);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                         const NkTextureCopyRegion& r) {
    GLuint s=NkOpenglGetTextureID(mDev,src.id);
    GLuint d=NkOpenglGetTextureID(mDev,dst.id);
    glCopyImageSubData(
        s,GL_TEXTURE_2D,(GLint)r.srcMip,(GLint)r.srcX,(GLint)r.srcY,(GLint)r.srcZ,
        d,GL_TEXTURE_2D,(GLint)r.dstMip,(GLint)r.dstX,(GLint)r.dstY,(GLint)r.dstZ,
        (GLsizei)r.width,(GLsizei)r.height,(GLsizei)r.depth);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                                         const NkTextureCopyRegion& r, NkFilter filter) {
    // GL blit via FBO temporaires
    GLuint srcFBO=0, dstFBO=0;
    glGenFramebuffers(1,&srcFBO);
    glGenFramebuffers(1,&dstFBO);

    GLuint srcId=NkOpenglGetTextureID(mDev,src.id);
    GLuint dstId=NkOpenglGetTextureID(mDev,dst.id);

    glNamedFramebufferTexture(srcFBO,GL_COLOR_ATTACHMENT0,srcId,(GLint)r.srcMip);
    glNamedFramebufferTexture(dstFBO,GL_COLOR_ATTACHMENT0,dstId,(GLint)r.dstMip);

    glBlitNamedFramebuffer(srcFBO,dstFBO,
        (GLint)r.srcX,(GLint)r.srcY,(GLint)(r.srcX+r.width),(GLint)(r.srcY+r.height),
        (GLint)r.dstX,(GLint)r.dstY,(GLint)(r.dstX+r.width),(GLint)(r.dstY+r.height),
        GL_COLOR_BUFFER_BIT,
        filter==NkFilter::NK_NEAREST ? GL_NEAREST : GL_LINEAR);

    glDeleteFramebuffers(1,&srcFBO);
    glDeleteFramebuffers(1,&dstFBO);
}

// =============================================================================
void NkOpenGLCommandBuffer::GL_GenerateMipmaps(NkTextureHandle tex) {
    GLuint id=NkOpenglGetTextureID(mDev,tex.id);
    glGenerateTextureMipmap(id);
}

// =============================================================================
// Fonctions externes implémentées dans NkRHI_Device_GL_Internal.cpp
// (séparation pour ne pas surcharger le .cpp principal)
// =============================================================================

} // namespace nkentseu
