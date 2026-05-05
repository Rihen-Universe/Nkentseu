// =============================================================================
// NkRHI_Device_GL_Internal.cpp
// Fonctions "friend" partagées entre NkOpenGLDevice et NkOpenGLCommandBuffer.
// Accès aux internals du device sans exposer les maps dans les headers.
// =============================================================================
#include "NkOpenglDevice.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

// =============================================================================
// Accès aux objets GL — appelés depuis NkOpenGLCommandBuffer
// =============================================================================

GLuint NkOpenglGetBufferID(NkOpenGLDevice* dev, uint64 id) {
    auto* buffer = dev->mBuffers.Find(id);
    return buffer ? buffer->id : 0;
}

GLuint NkOpenglGetTextureID(NkOpenGLDevice* dev, uint64 id) {
    auto* texture = dev->mTextures.Find(id);
    return texture ? texture->id : 0;
}

GLuint NkOpenglGetFBOID(NkOpenGLDevice* dev, uint64 id) {
    auto* fbo = dev->mFramebuffers.Find(id);
    return fbo ? fbo->id : 0;
}

GLuint NkOpenglGetSamplerID(NkOpenGLDevice* dev, uint64 id) {
    auto* sampler = dev->mSamplers.Find(id);
    return sampler ? sampler->id : 0;
}

GLuint NkOpenglGetProgramID(NkOpenGLDevice* dev, uint64 id) {
    auto* pipeline = dev->mPipelines.Find(id);
    return pipeline ? pipeline->program : 0;
}

GLuint NkOpenglGetVAOID(NkOpenGLDevice* dev, uint64 id) {
    auto* pipeline = dev->mPipelines.Find(id);
    return pipeline ? pipeline->vao : 0;
}

GLenum NkOpenglGetPrimitive(NkOpenGLDevice* dev, uint64 id) {
    auto* pipeline = dev->mPipelines.Find(id);
    if (!pipeline) return GL_TRIANGLES;
    switch (pipeline->gfxDesc.topology) {
        case NkPrimitiveTopology::NK_TRIANGLE_LIST:  return GL_TRIANGLES;
        case NkPrimitiveTopology::NK_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
        case NkPrimitiveTopology::NK_TRIANGLE_FAN:   return GL_TRIANGLE_FAN;
        case NkPrimitiveTopology::NK_LINE_LIST:      return GL_LINES;
        case NkPrimitiveTopology::NK_LINE_STRIP:     return GL_LINE_STRIP;
        case NkPrimitiveTopology::NK_POINT_LIST:     return GL_POINTS;
        case NkPrimitiveTopology::NK_PATCH_LIST:     return GL_PATCHES;
        default:                                 return GL_TRIANGLES;
    }
}

bool NkOpenglIsCompute(NkOpenGLDevice* dev, uint64 id) {
    auto* pipeline = dev->mPipelines.Find(id);
    return pipeline && pipeline->isCompute;
}

uint32 NkOpenglGetVertexStride(NkOpenGLDevice* dev, uint64 pipelineId, uint32 binding) {
    auto* pipeline = dev->mPipelines.Find(pipelineId);
    if (!pipeline) return 0;
    for (uint32 i = 0; i < pipeline->vertexLayout.bindings.Size(); ++i) {
        const auto& b = pipeline->vertexLayout.bindings[i];
        if (b.binding == binding) return b.stride;
    }
    return 0;
}

// =============================================================================
// Applique le render state (rasterizer, depth/stencil, blend) depuis le pipeline
// =============================================================================
void NkOpenglApplyRenderState(NkOpenGLDevice* dev, uint64 pipelineId) {
    auto* pipeline = dev->mPipelines.Find(pipelineId);
    if (!pipeline || pipeline->isCompute) return;

    auto& d  = pipeline->gfxDesc;
    auto& rs = d.rasterizer;
    auto& ds = d.depthStencil;
    auto& bl = d.blend;

    // ── Rasterizer ────────────────────────────────────────────────────────────
#if defined(NK_OPENGL_ES)
    // OpenGL ES ne supporte pas glPolygonMode. Le mode fill est le seul disponible.
    // On ignore simplement les autres modes.
    (void)rs.fillMode;
#else
    switch (rs.fillMode) {
        case NkFillMode::NK_SOLID:      glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);  break;
        case NkFillMode::NK_WIREFRAME:  glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);  break;
        case NkFillMode::NK_POINT:      glPolygonMode(GL_FRONT_AND_BACK,GL_POINT); break;
    }
#endif

    if (rs.cullMode == NkCullMode::NK_NONE) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(rs.cullMode==NkCullMode::NK_BACK ? GL_BACK : GL_FRONT);
    }
    glFrontFace(rs.frontFace==NkFrontFace::NK_CCW ? GL_CCW : GL_CW);

#if defined(NK_OPENGL_ES)
    // GL_DEPTH_CLAMP n'existe pas en OpenGL ES. On ignore.
#else
    if (rs.depthClip) glDisable(GL_DEPTH_CLAMP);
    else              glEnable(GL_DEPTH_CLAMP);
#endif

    if (rs.scissorTest) glEnable(GL_SCISSOR_TEST);
    else                glDisable(GL_SCISSOR_TEST);

    if (rs.depthBiasConst!=0.f || rs.depthBiasSlope!=0.f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(rs.depthBiasSlope, rs.depthBiasConst);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    if (rs.multisampleEnable) glEnable(GL_MULTISAMPLE);
    else                      glDisable(GL_MULTISAMPLE);

    // ── Depth / Stencil ───────────────────────────────────────────────────────
    auto toGLCmp = [](NkCompareOp op) -> GLenum {
        switch(op) {
            case NkCompareOp::NK_NEVER:        return GL_NEVER;
            case NkCompareOp::NK_LESS:         return GL_LESS;
            case NkCompareOp::NK_EQUAL:        return GL_EQUAL;
            case NkCompareOp::NK_LESS_EQUAL:    return GL_LEQUAL;
            case NkCompareOp::NK_GREATER:      return GL_GREATER;
            case NkCompareOp::NK_NOT_EQUAL:     return GL_NOTEQUAL;
            case NkCompareOp::NK_GREATER_EQUAL: return GL_GEQUAL;
            case NkCompareOp::NK_ALWAYS:       return GL_ALWAYS;
            default:                        return GL_LEQUAL;
        }
    };
    auto toGLSt = [](NkStencilOp op) -> GLenum {
        switch(op) {
            case NkStencilOp::NK_KEEP:      return GL_KEEP;
            case NkStencilOp::NK_ZERO:      return GL_ZERO;
            case NkStencilOp::NK_REPLACE:   return GL_REPLACE;
            case NkStencilOp::NK_INCR_CLAMP: return GL_INCR;
            case NkStencilOp::NK_DECR_CLAMP: return GL_DECR;
            case NkStencilOp::NK_INVERT:    return GL_INVERT;
            case NkStencilOp::NK_INCR_WRAP:  return GL_INCR_WRAP;
            case NkStencilOp::NK_DECR_WRAP:  return GL_DECR_WRAP;
            default:                     return GL_KEEP;
        }
    };

    if (ds.depthTestEnable) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(toGLCmp(ds.depthCompareOp));
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(ds.depthWriteEnable ? GL_TRUE : GL_FALSE);

    if (ds.stencilEnable) {
        glEnable(GL_STENCIL_TEST);
        auto& f=ds.front; auto& b=ds.back;
        glStencilFuncSeparate(GL_FRONT,toGLCmp(f.compareOp),f.reference,f.compareMask);
        glStencilFuncSeparate(GL_BACK, toGLCmp(b.compareOp),b.reference,b.compareMask);
        glStencilOpSeparate(GL_FRONT,toGLSt(f.failOp),toGLSt(f.depthFailOp),toGLSt(f.passOp));
        glStencilOpSeparate(GL_BACK, toGLSt(b.failOp),toGLSt(b.depthFailOp),toGLSt(b.passOp));
        glStencilMaskSeparate(GL_FRONT,f.writeMask);
        glStencilMaskSeparate(GL_BACK, b.writeMask);
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    // ── Blend ─────────────────────────────────────────────────────────────────
    auto toGLBF = [](NkBlendFactor f) -> GLenum {
        switch(f) {
            case NkBlendFactor::NK_ZERO:                  return GL_ZERO;
            case NkBlendFactor::NK_ONE:                   return GL_ONE;
            case NkBlendFactor::NK_SRC_COLOR:              return GL_SRC_COLOR;
            case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:      return GL_ONE_MINUS_SRC_COLOR;
            case NkBlendFactor::NK_DST_COLOR:              return GL_DST_COLOR;
            case NkBlendFactor::NK_ONE_MINUS_DST_COLOR:      return GL_ONE_MINUS_DST_COLOR;
            case NkBlendFactor::NK_SRC_ALPHA:              return GL_SRC_ALPHA;
            case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:      return GL_ONE_MINUS_SRC_ALPHA;
            case NkBlendFactor::NK_DST_ALPHA:              return GL_DST_ALPHA;
            case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:      return GL_ONE_MINUS_DST_ALPHA;
            case NkBlendFactor::NK_SRC_ALPHA_SATURATE:      return GL_SRC_ALPHA_SATURATE;
            default:                                   return GL_ONE;
        }
    };
    auto toGLBO = [](NkBlendOp op) -> GLenum {
        switch(op) {
            case NkBlendOp::NK_ADD:    return GL_FUNC_ADD;
            case NkBlendOp::NK_SUB:    return GL_FUNC_SUBTRACT;
            case NkBlendOp::NK_REV_SUB: return GL_FUNC_REVERSE_SUBTRACT;
            case NkBlendOp::NK_MIN:    return GL_MIN;
            case NkBlendOp::NK_MAX:    return GL_MAX;
            default:                return GL_FUNC_ADD;
        }
    };

    if (bl.alphaToCoverage) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else                    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

#if defined(NK_OPENGL_ES)
    // OpenGL ES ne supporte pas glEnablei / glBlendFuncSeparatei.
    // On applique les réglages du premier attachment pour tous.
    if (bl.attachments.Size() > 0) {
        auto& a = bl.attachments[0];
        if (a.blendEnable) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(toGLBF(a.srcColor), toGLBF(a.dstColor),
                                toGLBF(a.srcAlpha), toGLBF(a.dstAlpha));
            glBlendEquationSeparate(toGLBO(a.colorOp), toGLBO(a.alphaOp));
        } else {
            glDisable(GL_BLEND);
        }
        glColorMask((a.colorWriteMask&1)?GL_TRUE:GL_FALSE,
                    (a.colorWriteMask&2)?GL_TRUE:GL_FALSE,
                    (a.colorWriteMask&4)?GL_TRUE:GL_FALSE,
                    (a.colorWriteMask&8)?GL_TRUE:GL_FALSE);
    }
#else
    for (uint32 i=0; i<bl.attachments.Size(); i++) {
        auto& a=bl.attachments[i];
        if (a.blendEnable) {
            glEnablei(GL_BLEND,i);
            glBlendFuncSeparatei(i,toGLBF(a.srcColor),toGLBF(a.dstColor),
                                    toGLBF(a.srcAlpha),toGLBF(a.dstAlpha));
            glBlendEquationSeparatei(i,toGLBO(a.colorOp),toGLBO(a.alphaOp));
        } else {
            glDisablei(GL_BLEND,i);
        }
        glColorMaski(i,
            (a.colorWriteMask&1)?GL_TRUE:GL_FALSE,
            (a.colorWriteMask&2)?GL_TRUE:GL_FALSE,
            (a.colorWriteMask&4)?GL_TRUE:GL_FALSE,
            (a.colorWriteMask&8)?GL_TRUE:GL_FALSE);
    }
#endif
    glBlendColor(bl.blendConstants[0],bl.blendConstants[1],
                 bl.blendConstants[2],bl.blendConstants[3]);

    // ── Patch size (tessellation) ─────────────────────────────────────────────
#if defined(NK_OPENGL_ES)
    // OpenGL ES ne supporte pas les tessellation shaders.
    (void)d;
#else
    if (d.topology==NkPrimitiveTopology::NK_PATCH_LIST)
        glPatchParameteri(GL_PATCH_VERTICES,(GLint)d.patchControlPoints);
#endif
}

// =============================================================================
// Applique un descriptor set
// =============================================================================
void NkOpenglApplyDescSet(NkOpenGLDevice* dev, uint64 setId,
                        const NkVector<uint32>& dynOff) {
    auto* ds = dev->mDescSets.Find(setId);
    if (!ds) return;
    dev->ApplyDescriptors(*ds);
    (void)dynOff; // TODO: appliquer les offsets dynamiques
}

} // namespace nkentseu