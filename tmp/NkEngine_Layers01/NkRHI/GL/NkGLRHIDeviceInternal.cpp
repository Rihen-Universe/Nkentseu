// =============================================================================
// NkRHI_Device_GL_Internal.cpp
// Fonctions "friend" partagées entre NkDevice_GL et NkCommandBuffer_GL.
// Accès aux internals du device sans exposer les maps dans les headers.
// =============================================================================
#include "NkRHI_Device_GL.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// Accès aux objets GL — appelés depuis NkCommandBuffer_GL
// =============================================================================

GLuint NkOpenglGetBufferID(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mBuffers.find(id);
    return it != dev->mBuffers.end() ? it->second.id : 0;
}

GLuint NkOpenglGetTextureID(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mTextures.find(id);
    return it != dev->mTextures.end() ? it->second.id : 0;
}

GLuint NkOpenglGetFBOID(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mFramebuffers.find(id);
    return it != dev->mFramebuffers.end() ? it->second.id : 0;
}

GLuint NkOpenglGetSamplerID(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mSamplers.find(id);
    return it != dev->mSamplers.end() ? it->second.id : 0;
}

GLuint NkOpenglGetProgramID(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mPipelines.find(id);
    return it != dev->mPipelines.end() ? it->second.program : 0;
}

GLuint NkOpenglGetVAOID(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mPipelines.find(id);
    return it != dev->mPipelines.end() ? it->second.vao : 0;
}

GLenum NkOpenglGetPrimitive(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mPipelines.find(id);
    if (it == dev->mPipelines.end()) return GL_TRIANGLES;
    switch (it->second.gfxDesc.topology) {
        case NkPrimitiveTopology::TriangleList:  return GL_TRIANGLES;
        case NkPrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case NkPrimitiveTopology::TriangleFan:   return GL_TRIANGLE_FAN;
        case NkPrimitiveTopology::LineList:      return GL_LINES;
        case NkPrimitiveTopology::LineStrip:     return GL_LINE_STRIP;
        case NkPrimitiveTopology::PointList:     return GL_POINTS;
        case NkPrimitiveTopology::PatchList:     return GL_PATCHES;
        default:                                 return GL_TRIANGLES;
    }
}

bool NkOpenglIsCompute(NkDevice_GL* dev, uint64 id) {
    auto it = dev->mPipelines.find(id);
    return it != dev->mPipelines.end() && it->second.isCompute;
}

// =============================================================================
// Applique le render state (rasterizer, depth/stencil, blend) depuis le pipeline
// =============================================================================
void NkOpenglApplyRenderState(NkDevice_GL* dev, uint64 pipelineId) {
    auto it = dev->mPipelines.find(pipelineId);
    if (it == dev->mPipelines.end() || it->second.isCompute) return;

    auto& d  = it->second.gfxDesc;
    auto& rs = d.rasterizer;
    auto& ds = d.depthStencil;
    auto& bl = d.blend;

    // ── Rasterizer ────────────────────────────────────────────────────────────
    switch (rs.fillMode) {
        case NkFillMode::Solid:      glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);  break;
        case NkFillMode::Wireframe:  glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);  break;
        case NkFillMode::Point:      glPolygonMode(GL_FRONT_AND_BACK,GL_POINT); break;
    }
    if (rs.cullMode == NkCullMode::None) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(rs.cullMode==NkCullMode::Back ? GL_BACK : GL_FRONT);
    }
    glFrontFace(rs.frontFace==NkFrontFace::CCW ? GL_CCW : GL_CW);

    if (rs.depthClip) glDisable(GL_DEPTH_CLAMP);
    else              glEnable(GL_DEPTH_CLAMP);

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
            case NkCompareOp::Never:        return GL_NEVER;
            case NkCompareOp::Less:         return GL_LESS;
            case NkCompareOp::Equal:        return GL_EQUAL;
            case NkCompareOp::LessEqual:    return GL_LEQUAL;
            case NkCompareOp::Greater:      return GL_GREATER;
            case NkCompareOp::NotEqual:     return GL_NOTEQUAL;
            case NkCompareOp::GreaterEqual: return GL_GEQUAL;
            case NkCompareOp::Always:       return GL_ALWAYS;
            default:                        return GL_LEQUAL;
        }
    };
    auto toGLSt = [](NkStencilOp op) -> GLenum {
        switch(op) {
            case NkStencilOp::Keep:      return GL_KEEP;
            case NkStencilOp::Zero:      return GL_ZERO;
            case NkStencilOp::Replace:   return GL_REPLACE;
            case NkStencilOp::IncrClamp: return GL_INCR;
            case NkStencilOp::DecrClamp: return GL_DECR;
            case NkStencilOp::Invert:    return GL_INVERT;
            case NkStencilOp::IncrWrap:  return GL_INCR_WRAP;
            case NkStencilOp::DecrWrap:  return GL_DECR_WRAP;
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
            case NkBlendFactor::Zero:                  return GL_ZERO;
            case NkBlendFactor::One:                   return GL_ONE;
            case NkBlendFactor::SrcColor:              return GL_SRC_COLOR;
            case NkBlendFactor::OneMinusSrcColor:      return GL_ONE_MINUS_SRC_COLOR;
            case NkBlendFactor::DstColor:              return GL_DST_COLOR;
            case NkBlendFactor::OneMinusDstColor:      return GL_ONE_MINUS_DST_COLOR;
            case NkBlendFactor::SrcAlpha:              return GL_SRC_ALPHA;
            case NkBlendFactor::OneMinusSrcAlpha:      return GL_ONE_MINUS_SRC_ALPHA;
            case NkBlendFactor::DstAlpha:              return GL_DST_ALPHA;
            case NkBlendFactor::OneMinusDstAlpha:      return GL_ONE_MINUS_DST_ALPHA;
            case NkBlendFactor::SrcAlphaSaturate:      return GL_SRC_ALPHA_SATURATE;
            default:                                   return GL_ONE;
        }
    };
    auto toGLBO = [](NkBlendOp op) -> GLenum {
        switch(op) {
            case NkBlendOp::Add:    return GL_FUNC_ADD;
            case NkBlendOp::Sub:    return GL_FUNC_SUBTRACT;
            case NkBlendOp::RevSub: return GL_FUNC_REVERSE_SUBTRACT;
            case NkBlendOp::Min:    return GL_MIN;
            case NkBlendOp::Max:    return GL_MAX;
            default:                return GL_FUNC_ADD;
        }
    };

    if (bl.alphaToCoverage) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else                    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    for (uint32 i=0; i<bl.attachmentCount; i++) {
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
    glBlendColor(bl.blendConstants[0],bl.blendConstants[1],
                 bl.blendConstants[2],bl.blendConstants[3]);

    // ── Patch size (tessellation) ─────────────────────────────────────────────
    if (d.topology==NkPrimitiveTopology::PatchList)
        glPatchParameteri(GL_PATCH_VERTICES,(GLint)d.patchControlPoints);
}

// =============================================================================
// Applique un descriptor set
// =============================================================================
void NkOpenglApplyDescSet(NkDevice_GL* dev, uint64 setId,
                        const std::vector<uint32>& dynOff) {
    auto sit = dev->mDescSets.find(setId);
    if (sit == dev->mDescSets.end()) return;
    auto& ds = sit->second;
    dev->ApplyDescriptors(ds);
    (void)dynOff; // TODO: appliquer les offsets dynamiques
}

} // namespace nkentseu
