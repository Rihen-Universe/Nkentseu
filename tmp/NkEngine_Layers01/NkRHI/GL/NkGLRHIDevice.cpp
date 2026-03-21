// =============================================================================
// NkRHI_Device_GL.cpp — Implémentation OpenGL 4.3+ du NkIDevice
// Utilise Direct State Access (GL 4.5+) avec fallback GL 4.3
// =============================================================================
#include "NkRHI_Device_GL.h"
#include "NkCommandBuffer_GL.h"
#include <cstdio>
#include <cstring>

#define NK_GL_LOG(...) printf("[NkRHI_GL] " __VA_ARGS__)
#define NK_GL_ERR(...) printf("[NkRHI_GL][ERR] " __VA_ARGS__)
#define NK_GL_CHECK() do { GLenum e=glGetError(); if(e!=GL_NO_ERROR) NK_GL_ERR("GL error 0x%X at %s:%d\n",e,__FILE__,__LINE__); } while(0)

namespace nkentseu {

NkDevice_GL::~NkDevice_GL() { if(mIsValid) Shutdown(); }

// =============================================================================
bool NkDevice_GL::Initialize(NkIGraphicsContext* ctx) {
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    // Vérifier GL 4.3 minimum (compute shaders)
    GLint major=0, minor=0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 4 || (major == 4 && minor < 3)) {
        NK_GL_ERR("OpenGL 4.3+ required (got %d.%d)\n", major, minor);
        return false;
    }

    mWidth  = ctx->GetInfo().windowWidth  > 0 ? ctx->GetInfo().windowWidth  : 1280;
    mHeight = ctx->GetInfo().windowHeight > 0 ? ctx->GetInfo().windowHeight : 720;

    QueryCaps();

    // Créer le render pass et framebuffer swapchain virtuels
    {
        NkRenderPassDesc rpd;
        rpd.AddColor(NkAttachmentDesc::Color(mSwapchainFormat))
           .SetDepth(NkAttachmentDesc::Depth());
        mSwapchainRP = CreateRenderPass(rpd);
        // FBO 0 = back buffer GL — on crée un handle factice
        uint64 fbId = NextId();
        mFramebuffers[fbId] = {0, mWidth, mHeight}; // GL FBO 0
        mSwapchainFB.id = fbId;
    }

    mIsValid = true;
    NK_GL_LOG("Initialized (GL %d.%d, %s)\n", major, minor,
              (const char*)glGetString(GL_RENDERER));
    return true;
}

void NkDevice_GL::Shutdown() {
    WaitIdle();
    // Détruire toutes les ressources restantes
    for (auto& [id,b] : mBuffers)   glDeleteBuffers(1,&b.id);
    for (auto& [id,t] : mTextures)  glDeleteTextures(1,&t.id);
    for (auto& [id,s] : mSamplers)  glDeleteSamplers(1,&s.id);
    for (auto& [id,sh]: mShaders)   glDeleteProgram(sh.program);
    for (auto& [id,p] : mPipelines) {
        if (p.vao) glDeleteVertexArrays(1,&p.vao);
    }
    for (auto& [id,f] : mFramebuffers) {
        if (f.id != 0) glDeleteFramebuffers(1,&f.id);
    }
    for (auto& [id,fn]: mFences) {
        if (fn.sync) glDeleteSync(fn.sync);
    }
    mBuffers.clear(); mTextures.clear(); mSamplers.clear();
    mShaders.clear(); mPipelines.clear(); mFramebuffers.clear();
    mRenderPasses.clear(); mDescLayouts.clear(); mDescSets.clear();
    mFences.clear();
    mIsValid = false;
    NK_GL_LOG("Shutdown\n");
}

void NkDevice_GL::QueryCaps() {
    GLint v=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,         &v); mCaps.maxTextureDim2D=(uint32)v;
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,      &v); mCaps.maxTextureDim3D=(uint32)v;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,&v); mCaps.maxTextureCubeSize=(uint32)v;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &v); mCaps.maxTextureArrayLayers=(uint32)v;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,    &v); mCaps.maxColorAttachments=(uint32)v;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,       &v); mCaps.maxVertexAttributes=(uint32)v;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,   &v); mCaps.maxUniformBufferRange=(uint32)v;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&v); mCaps.maxStorageBufferRange=(uint32)v;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_SIZE,   nullptr);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,0,&v); mCaps.maxComputeGroupSizeX=(uint32)v;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,1,&v); mCaps.maxComputeGroupSizeY=(uint32)v;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,2,&v); mCaps.maxComputeGroupSizeZ=(uint32)v;
    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,&v); mCaps.maxComputeSharedMemory=(uint32)v;
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY,&v); mCaps.maxSamplerAnisotropy=(uint32)v;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,&v); mCaps.minUniformBufferAlign=(uint32)v;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,&v); mCaps.minStorageBufferAlign=(uint32)v;

    mCaps.computeShaders    = true; // GL 4.3+
    mCaps.geometryShaders   = true;
    mCaps.tessellationShaders=true;
    mCaps.drawIndirect      = true;
    mCaps.multiViewport     = true;
    mCaps.independentBlend  = true;
    mCaps.timestampQueries  = true;
    mCaps.textureCompressionBC=true; // sur desktop

    // MSAA support
    for (NkSampleCount s : {NkSampleCount::S2,NkSampleCount::S4,
                              NkSampleCount::S8,NkSampleCount::S16}) {
        GLint maxS=0;
        glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES,&maxS);
        uint32 n=(uint32)s;
        if (n<=2)  mCaps.msaa2x  = maxS>=2;
        if (n<=4)  mCaps.msaa4x  = maxS>=4;
        if (n<=8)  mCaps.msaa8x  = maxS>=8;
        if (n<=16) mCaps.msaa16x = maxS>=16;
        break;
    }
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDevice_GL::CreateBuffer(const NkBufferDesc& desc) {
    if (desc.sizeBytes==0) return {};
    std::lock_guard<std::mutex> lock(mMutex);

    GLuint id=0;
    glCreateBuffers(1,&id);

    GLenum usage = ToGLBufferUsage(desc.usage, desc.bindFlags);
    glNamedBufferData(id, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);

    if (desc.debugName) {
        glObjectLabel(GL_BUFFER, id, -1, desc.debugName);
    }

    uint64 hid = NextId();
    mBuffers[hid] = {id, desc.sizeBytes, desc.usage, desc.bindFlags};

    NkBufferHandle h; h.id=hid; return h;
}

void NkDevice_GL::DestroyBuffer(NkBufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mBuffers.find(h.id);
    if (it==mBuffers.end()) return;
    glDeleteBuffers(1,&it->second.id);
    mBuffers.erase(it);
    h.id=0;
}

bool NkDevice_GL::WriteBuffer(NkBufferHandle buf, const void* data,
                               uint64 size, uint64 offset) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mBuffers.find(buf.id);
    if (it==mBuffers.end() || !data) return false;
    glNamedBufferSubData(it->second.id, (GLintptr)offset, (GLsizeiptr)size, data);
    return true;
}

bool NkDevice_GL::WriteBufferAsync(NkBufferHandle buf, const void* data,
                                    uint64 size, uint64 offset) {
    // GL : async via glMapNamedBufferRange + UNSYNCHRONIZED
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mBuffers.find(buf.id);
    if (it==mBuffers.end() || !data) return false;
    void* ptr = glMapNamedBufferRange(it->second.id, (GLintptr)offset,
        (GLsizeiptr)size,
        GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_INVALIDATE_RANGE_BIT);
    if (!ptr) return false;
    memcpy(ptr, data, (size_t)size);
    glUnmapNamedBuffer(it->second.id);
    return true;
}

bool NkDevice_GL::ReadBuffer(NkBufferHandle buf, void* out,
                              uint64 size, uint64 offset) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mBuffers.find(buf.id);
    if (it==mBuffers.end()) return false;
    glGetNamedBufferSubData(it->second.id, (GLintptr)offset, (GLsizeiptr)size, out);
    return true;
}

NkMappedMemory NkDevice_GL::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mBuffers.find(buf.id);
    if (it==mBuffers.end()) return {};
    uint64 mapSz = sz>0 ? sz : it->second.size-off;
    void* ptr=glMapNamedBufferRange(it->second.id,(GLintptr)off,(GLsizeiptr)mapSz,
        GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);
    return {ptr, mapSz};
}

void NkDevice_GL::UnmapBuffer(NkBufferHandle buf) {
    auto it=mBuffers.find(buf.id);
    if (it!=mBuffers.end()) glUnmapNamedBuffer(it->second.id);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDevice_GL::CreateTexture(const NkTextureDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    GLuint id=0;
    GLenum target = ToGLTextureTarget(desc.type, desc.samples);
    glCreateTextures(target, 1, &id);

    uint32 mips = desc.mipLevels==0 ?
        (uint32)(floor(log2((double)std::max(desc.width,desc.height)))+1) :
        desc.mipLevels;

    GLenum internal = ToGLInternalFormat(desc.format);

    switch(desc.type) {
        case NkTextureType::Tex2D:
            if (desc.samples > NkSampleCount::S1)
                glTextureStorage2DMultisample(id,(GLsizei)desc.samples,
                    internal,desc.width,desc.height,GL_TRUE);
            else
                glTextureStorage2D(id,mips,internal,desc.width,desc.height);
            break;
        case NkTextureType::Tex2DArray:
        case NkTextureType::Cube:
        case NkTextureType::CubeArray:
            glTextureStorage3D(id,mips,internal,desc.width,desc.height,desc.arrayLayers);
            break;
        case NkTextureType::Tex3D:
            glTextureStorage3D(id,mips,internal,desc.width,desc.height,desc.depth);
            break;
        case NkTextureType::Tex1D:
            glTextureStorage1D(id,mips,internal,desc.width);
            break;
    }

    if (desc.initialData) {
        GLenum base=ToGLBaseFormat(desc.format), type2=ToGLType(desc.format);
        uint32 rp=desc.rowPitch>0?desc.rowPitch:desc.width*NkFormatBytesPerPixel(desc.format);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rp/std::max(1u,NkFormatBytesPerPixel(desc.format)));
        glTextureSubImage2D(id,0,0,0,desc.width,desc.height,base,type2,desc.initialData);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        if (mips>1) glGenerateTextureMipmap(id);
    }

    if (desc.debugName) glObjectLabel(GL_TEXTURE,id,-1,desc.debugName);

    uint64 hid=NextId();
    mTextures[hid]={id,target,desc};
    NkTextureHandle h; h.id=hid; return h;
}

void NkDevice_GL::DestroyTexture(NkTextureHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mTextures.find(h.id);
    if (it==mTextures.end()) return;
    glDeleteTextures(1,&it->second.id);
    mTextures.erase(it);
    h.id=0;
}

bool NkDevice_GL::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    auto it=mTextures.find(t.id);
    if (it==mTextures.end()) return false;
    auto& desc=it->second.desc;
    return WriteTextureRegion(t,p,0,0,0,desc.width,desc.height,1,0,0,rp);
}

bool NkDevice_GL::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
    uint32 mip,uint32 layer,uint32 rowPitch) {
    auto it=mTextures.find(t.id);
    if (it==mTextures.end()||!pixels) return false;
    auto& desc=it->second.desc;
    GLenum base=ToGLBaseFormat(desc.format), type2=ToGLType(desc.format);
    uint32 bpp=NkFormatBytesPerPixel(desc.format);
    uint32 rp2=rowPitch>0?rowPitch:w*bpp;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rp2/std::max(1u,bpp));
    if (desc.type==NkTextureType::Tex2D)
        glTextureSubImage2D(it->second.id,(GLint)mip,(GLint)x,(GLint)y,(GLsizei)w,(GLsizei)h,base,type2,pixels);
    else if (desc.type==NkTextureType::Tex3D || desc.type==NkTextureType::Tex2DArray)
        glTextureSubImage3D(it->second.id,(GLint)mip,(GLint)x,(GLint)y,(GLint)(layer+z),(GLsizei)w,(GLsizei)h,(GLsizei)d2,base,type2,pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    return true;
}

bool NkDevice_GL::GenerateMipmaps(NkTextureHandle t, NkFilter) {
    auto it=mTextures.find(t.id);
    if (it==mTextures.end()) return false;
    glGenerateTextureMipmap(it->second.id);
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDevice_GL::CreateSampler(const NkSamplerDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    GLuint id=0; glCreateSamplers(1,&id);
    glSamplerParameteri(id,GL_TEXTURE_MAG_FILTER,(GLint)ToGLFilter(d.magFilter,NkMipFilter::None));
    glSamplerParameteri(id,GL_TEXTURE_MIN_FILTER,(GLint)ToGLFilter(d.minFilter,d.mipFilter));
    glSamplerParameteri(id,GL_TEXTURE_WRAP_S,(GLint)ToGLWrap(d.addressU));
    glSamplerParameteri(id,GL_TEXTURE_WRAP_T,(GLint)ToGLWrap(d.addressV));
    glSamplerParameteri(id,GL_TEXTURE_WRAP_R,(GLint)ToGLWrap(d.addressW));
    glSamplerParameterf(id,GL_TEXTURE_LOD_BIAS,d.mipLodBias);
    glSamplerParameterf(id,GL_TEXTURE_MIN_LOD,d.minLod);
    glSamplerParameterf(id,GL_TEXTURE_MAX_LOD,d.maxLod);
    if (d.maxAnisotropy>1.f)
        glSamplerParameterf(id,GL_TEXTURE_MAX_ANISOTROPY,d.maxAnisotropy);
    if (d.compareEnable) {
        glSamplerParameteri(id,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(id,GL_TEXTURE_COMPARE_FUNC,(GLint)ToGLCompareOp(d.compareOp));
    }
    uint64 hid=NextId(); mSamplers[hid]={id};
    NkSamplerHandle h; h.id=hid; return h;
}

void NkDevice_GL::DestroySampler(NkSamplerHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mSamplers.find(h.id);
    if (it==mSamplers.end()) return;
    glDeleteSamplers(1,&it->second.id);
    mSamplers.erase(it); h.id=0;
}

// =============================================================================
// Shaders
// =============================================================================
GLuint NkDevice_GL::CompileGLStage(GLenum stage, const char* src) {
    GLuint s=glCreateShader(stage);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char buf[2048]; glGetShaderInfoLog(s,2048,nullptr,buf);
        NK_GL_ERR("Shader compile error:\n%s\n",buf);
        glDeleteShader(s); return 0;
    }
    return s;
}

NkShaderHandle NkDevice_GL::CreateShader(const NkShaderDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    GLuint prog=glCreateProgram();

    for (uint32 i=0;i<desc.stageCount;i++) {
        auto& s=desc.stages[i];
        const char* src = s.glslSource;
        if (!src) { NK_GL_ERR("GL shader: missing GLSL source for stage %d\n",i); continue; }
        GLenum glStage=ToGLShaderStage(s.stage);
        GLuint sh=CompileGLStage(glStage,src);
        if (sh) { glAttachShader(prog,sh); glDeleteShader(sh); }
    }

    glLinkProgram(prog);
    GLint ok=0; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if (!ok) {
        char buf[2048]; glGetProgramInfoLog(prog,2048,nullptr,buf);
        NK_GL_ERR("Shader link error:\n%s\n",buf);
        glDeleteProgram(prog); return {};
    }
    if (desc.debugName) glObjectLabel(GL_PROGRAM,prog,-1,desc.debugName);

    uint64 hid=NextId(); mShaders[hid]={prog};
    NkShaderHandle h; h.id=hid; return h;
}

void NkDevice_GL::DestroyShader(NkShaderHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mShaders.find(h.id);
    if (it==mShaders.end()) return;
    glDeleteProgram(it->second.program);
    mShaders.erase(it); h.id=0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDevice_GL::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit=mShaders.find(d.shader.id);
    if (sit==mShaders.end()) return {};

    GLPipeline p;
    p.program     = sit->second.program;
    p.vertexLayout= d.vertexLayout;
    p.gfxDesc     = d;
    p.isCompute   = false;

    // Créer le VAO correspondant au vertex layout
    glCreateVertexArrays(1,&p.vao);
    for (uint32 i=0;i<d.vertexLayout.attributeCount;i++) {
        auto& a=d.vertexLayout.attributes[i];
        glEnableVertexArrayAttrib(p.vao,a.location);
        // Déterminer si float, int ou double
        switch(a.format) {
            case NkVertexFormat::Uint1: case NkVertexFormat::Uint2:
            case NkVertexFormat::Uint4: case NkVertexFormat::Int1:
            case NkVertexFormat::Int2:  case NkVertexFormat::Int4:
                glVertexArrayAttribIFormat(p.vao,a.location,
                    a.format==NkVertexFormat::Float3?3:4,GL_INT,(GLuint)a.offset);
                break;
            default:
                glVertexArrayAttribFormat(p.vao,a.location,
                    a.format==NkVertexFormat::Float3?3:
                    a.format==NkVertexFormat::Float2?2:
                    a.format==NkVertexFormat::Float4?4:3,
                    GL_FLOAT,GL_FALSE,(GLuint)a.offset);
        }
        glVertexArrayAttribBinding(p.vao,a.location,a.binding);
    }
    for (uint32 i=0;i<d.vertexLayout.bindingCount;i++) {
        auto& b=d.vertexLayout.bindings[i];
        if (b.perInstance)
            glVertexArrayBindingDivisor(p.vao,b.binding,1);
    }

    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

NkPipelineHandle NkDevice_GL::CreateComputePipeline(const NkComputePipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit=mShaders.find(d.shader.id);
    if (sit==mShaders.end()) return {};

    GLPipeline p;
    p.program   = sit->second.program;
    p.compDesc  = d;
    p.isCompute = true;

    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

void NkDevice_GL::DestroyPipeline(NkPipelineHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mPipelines.find(h.id);
    if (it==mPipelines.end()) return;
    if (it->second.vao) glDeleteVertexArrays(1,&it->second.vao);
    mPipelines.erase(it); h.id=0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle NkDevice_GL::CreateRenderPass(const NkRenderPassDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid=NextId(); mRenderPasses[hid]=d;
    NkRenderPassHandle h; h.id=hid; return h;
}
void NkDevice_GL::DestroyRenderPass(NkRenderPassHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    mRenderPasses.erase(h.id); h.id=0;
}

NkFramebufferHandle NkDevice_GL::CreateFramebuffer(const NkFramebufferDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    GLuint fbo=0;
    glCreateFramebuffers(1,&fbo);

    for (uint32 i=0;i<d.colorCount;i++) {
        auto it=mTextures.find(d.colorAttachments[i].id);
        if (it!=mTextures.end())
            glNamedFramebufferTexture(fbo,GL_COLOR_ATTACHMENT0+i,it->second.id,0);
    }
    if (d.depthAttachment.IsValid()) {
        auto it=mTextures.find(d.depthAttachment.id);
        if (it!=mTextures.end()) {
            auto& desc=it->second.desc;
            GLenum att = NkFormatHasStencil(desc.format)
                ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            glNamedFramebufferTexture(fbo,att,it->second.id,0);
        }
    }

    GLenum status=glCheckNamedFramebufferStatus(fbo,GL_FRAMEBUFFER);
    if (status!=GL_FRAMEBUFFER_COMPLETE)
        NK_GL_ERR("Framebuffer incomplete: 0x%X\n",(unsigned)status);

    if (d.debugName) glObjectLabel(GL_FRAMEBUFFER,fbo,-1,d.debugName);

    uint64 hid=NextId(); mFramebuffers[hid]={fbo,d.width,d.height};
    NkFramebufferHandle h; h.id=hid; return h;
}

void NkDevice_GL::DestroyFramebuffer(NkFramebufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mFramebuffers.find(h.id);
    if (it==mFramebuffers.end()) return;
    if (it->second.id!=0) glDeleteFramebuffers(1,&it->second.id);
    mFramebuffers.erase(it); h.id=0;
}

// =============================================================================
// Descriptor Sets (émulation GL)
// =============================================================================
NkDescSetHandle NkDevice_GL::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid=NextId(); mDescLayouts[hid]={d};
    NkDescSetHandle h; h.id=hid; return h;
}
void NkDevice_GL::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescLayouts.erase(h.id); h.id=0;
}
NkDescSetHandle NkDevice_GL::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
    std::lock_guard<std::mutex> lock(mMutex);
    GLDescSet ds; ds.layoutHandle=layoutHandle;
    uint64 hid=NextId(); mDescSets[hid]=ds;
    NkDescSetHandle h; h.id=hid; return h;
}
void NkDevice_GL::FreeDescriptorSet(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescSets.erase(h.id); h.id=0;
}
void NkDevice_GL::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (uint32 i=0;i<n;i++) {
        auto& w=writes[i];
        auto it=mDescSets.find(w.set.id);
        if (it==mDescSets.end()) continue;
        auto& b=it->second.bindings[w.binding];
        b.type=w.type;
        if (w.buffer.IsValid()) {
            auto bi=mBuffers.find(w.buffer.id);
            if (bi!=mBuffers.end()) { b.bufferId=bi->second.id; b.bufferOffset=w.bufferOffset; b.bufferRange=w.bufferRange; }
        }
        if (w.texture.IsValid()) {
            auto ti=mTextures.find(w.texture.id);
            if (ti!=mTextures.end()) b.textureId=ti->second.id;
        }
        if (w.sampler.IsValid()) {
            auto si=mSamplers.find(w.sampler.id);
            if (si!=mSamplers.end()) b.samplerId=si->second.id;
        }
    }
}

void NkDevice_GL::ApplyDescriptors(const GLDescSet& ds) {
    auto lit=mDescLayouts.find(ds.layoutHandle.id);
    if (lit==mDescLayouts.end()) return;
    auto& layout=lit->second.desc;
    for (uint32 i=0;i<layout.bindingCount;i++) {
        auto& lb=layout.bindings[i];
        auto& b=ds.bindings[lb.binding];
        switch(b.type) {
            case NkDescriptorType::UniformBuffer:
            case NkDescriptorType::UniformBufferDynamic:
                if (b.bufferId) glBindBufferRange(GL_UNIFORM_BUFFER,lb.binding,
                    b.bufferId,(GLintptr)b.bufferOffset,(GLsizeiptr)(b.bufferRange>0?b.bufferRange:65536));
                break;
            case NkDescriptorType::StorageBuffer:
            case NkDescriptorType::StorageBufferDynamic:
                if (b.bufferId) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,lb.binding,b.bufferId);
                break;
            case NkDescriptorType::SampledTexture:
            case NkDescriptorType::CombinedImageSampler:
                if (b.textureId) { glBindTextureUnit(lb.binding,b.textureId); }
                if (b.samplerId) { glBindSampler(lb.binding,b.samplerId); }
                break;
            case NkDescriptorType::StorageTexture:
                if (b.textureId) glBindImageTexture(lb.binding,b.textureId,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);
                break;
            default: break;
        }
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkDevice_GL::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_GL(this, t);
}
void NkDevice_GL::DestroyCommandBuffer(NkICommandBuffer*& cb) {
    delete cb; cb=nullptr;
}

// =============================================================================
// Submit & Sync
// =============================================================================
void NkDevice_GL::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i=0;i<n;i++) {
        auto* gl=dynamic_cast<NkCommandBuffer_GL*>(cbs[i]);
        if (gl) gl->Execute(this);
    }
    if (fence.IsValid()) {
        auto it=mFences.find(fence.id);
        if (it!=mFences.end()) {
            if (it->second.sync) glDeleteSync(it->second.sync);
            it->second.sync=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            it->second.signaled=false;
        }
    }
}

void NkDevice_GL::SubmitAndPresent(NkICommandBuffer* cb) {
    Submit(&cb,1,{});
    mCtx->Present();
}

NkFenceHandle NkDevice_GL::CreateFence(bool signaled) {
    std::lock_guard<std::mutex> lock(mMutex);
    GLFenceObj f; f.signaled=signaled;
    if (signaled) f.sync=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    uint64 hid=NextId(); mFences[hid]=f;
    NkFenceHandle h; h.id=hid; return h;
}
void NkDevice_GL::DestroyFence(NkFenceHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mFences.find(h.id);
    if (it==mFences.end()) return;
    if (it->second.sync) glDeleteSync(it->second.sync);
    mFences.erase(it); h.id=0;
}
bool NkDevice_GL::WaitFence(NkFenceHandle f, uint64 timeoutNs) {
    auto it=mFences.find(f.id);
    if (it==mFences.end()) return false;
    if (!it->second.sync) return true;
    GLenum r=glClientWaitSync(it->second.sync,GL_SYNC_FLUSH_COMMANDS_BIT,(GLuint64)timeoutNs);
    if (r==GL_ALREADY_SIGNALED||r==GL_CONDITION_SATISFIED) { it->second.signaled=true; return true; }
    return false;
}
bool NkDevice_GL::IsFenceSignaled(NkFenceHandle f) {
    auto it=mFences.find(f.id);
    if (it==mFences.end()) return false;
    if (!it->second.sync) return it->second.signaled;
    GLint v=0; GLsizei l=0;
    glGetSynciv(it->second.sync,GL_SYNC_STATUS,sizeof(v),&l,&v);
    return v==GL_SIGNALED;
}
void NkDevice_GL::ResetFence(NkFenceHandle f) {
    auto it=mFences.find(f.id);
    if (it==mFences.end()) return;
    if (it->second.sync) { glDeleteSync(it->second.sync); it->second.sync=nullptr; }
    it->second.signaled=false;
}
void NkDevice_GL::WaitIdle() { glFinish(); }

// =============================================================================
// Frame
// =============================================================================
void NkDevice_GL::BeginFrame(NkFrameContext& frame) {
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
}
void NkDevice_GL::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex+1) % kMaxFrames;
    ++mFrameNumber;
}
void NkDevice_GL::OnResize(uint32 w, uint32 h) {
    if (w==0||h==0) return;
    mWidth=w; mHeight=h;
    // Mettre à jour le FBO swapchain virtuel
    auto it=mFramebuffers.find(mSwapchainFB.id);
    if (it!=mFramebuffers.end()) { it->second.w=w; it->second.h=h; }
    mCtx->OnResize(w,h);
}

// =============================================================================
// Conversions GL
// =============================================================================
GLenum NkDevice_GL::ToGLInternalFormat(NkFormat f) {
    switch(f) {
        case NkFormat::R8_Unorm:      return GL_R8;
        case NkFormat::RG8_Unorm:     return GL_RG8;
        case NkFormat::RGBA8_Unorm:   return GL_RGBA8;
        case NkFormat::RGBA8_Srgb:    return GL_SRGB8_ALPHA8;
        case NkFormat::BGRA8_Unorm:   return GL_RGBA8; // swizzle manuel
        case NkFormat::BGRA8_Srgb:    return GL_SRGB8_ALPHA8;
        case NkFormat::R16_Float:     return GL_R16F;
        case NkFormat::RG16_Float:    return GL_RG16F;
        case NkFormat::RGBA16_Float:  return GL_RGBA16F;
        case NkFormat::R32_Float:     return GL_R32F;
        case NkFormat::RG32_Float:    return GL_RG32F;
        case NkFormat::RGB32_Float:   return GL_RGB32F;
        case NkFormat::RGBA32_Float:  return GL_RGBA32F;
        case NkFormat::R32_Uint:      return GL_R32UI;
        case NkFormat::R16_Uint:      return GL_R16UI;
        case NkFormat::RGBA16_Uint:   return GL_RGBA16UI;
        case NkFormat::D16_Unorm:     return GL_DEPTH_COMPONENT16;
        case NkFormat::D32_Float:     return GL_DEPTH_COMPONENT32F;
        case NkFormat::D24_Unorm_S8_Uint: return GL_DEPTH24_STENCIL8;
        case NkFormat::D32_Float_S8_Uint: return GL_DEPTH32F_STENCIL8;
        case NkFormat::BC1_RGB_Unorm: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case NkFormat::BC3_Unorm:     return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case NkFormat::BC5_Unorm:     return GL_COMPRESSED_RG_RGTC2;
        case NkFormat::BC7_Unorm:     return GL_COMPRESSED_RGBA_BPTC_UNORM;
        case NkFormat::R11G11B10_Float:return GL_R11F_G11F_B10F;
        default:                      return GL_RGBA8;
    }
}
GLenum NkDevice_GL::ToGLBaseFormat(NkFormat f) {
    if (NkFormatIsDepth(f)) return NkFormatHasStencil(f)?GL_DEPTH_STENCIL:GL_DEPTH_COMPONENT;
    switch(f) {
        case NkFormat::R8_Unorm: case NkFormat::R16_Float:
        case NkFormat::R32_Float: case NkFormat::R32_Uint: return GL_RED;
        case NkFormat::RG8_Unorm: case NkFormat::RG16_Float:
        case NkFormat::RG32_Float: return GL_RG;
        case NkFormat::RGB32_Float: return GL_RGB;
        default: return GL_RGBA;
    }
}
GLenum NkDevice_GL::ToGLType(NkFormat f) {
    switch(f) {
        case NkFormat::R16_Float: case NkFormat::RG16_Float:
        case NkFormat::RGBA16_Float: return GL_HALF_FLOAT;
        case NkFormat::R32_Float: case NkFormat::RG32_Float:
        case NkFormat::RGB32_Float: case NkFormat::RGBA32_Float:
        case NkFormat::D32_Float: return GL_FLOAT;
        case NkFormat::R32_Uint: case NkFormat::RGBA16_Uint: return GL_UNSIGNED_INT;
        case NkFormat::D24_Unorm_S8_Uint: return GL_UNSIGNED_INT_24_8;
        case NkFormat::D32_Float_S8_Uint: return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        default: return GL_UNSIGNED_BYTE;
    }
}
GLenum NkDevice_GL::ToGLTextureTarget(NkTextureType t, NkSampleCount s) {
    bool msaa = s > NkSampleCount::S1;
    switch(t) {
        case NkTextureType::Tex1D:      return GL_TEXTURE_1D;
        case NkTextureType::Tex2D:      return msaa?GL_TEXTURE_2D_MULTISAMPLE:GL_TEXTURE_2D;
        case NkTextureType::Tex3D:      return GL_TEXTURE_3D;
        case NkTextureType::Cube:       return GL_TEXTURE_CUBE_MAP;
        case NkTextureType::Tex2DArray: return GL_TEXTURE_2D_ARRAY;
        case NkTextureType::CubeArray:  return GL_TEXTURE_CUBE_MAP_ARRAY;
        default:                        return GL_TEXTURE_2D;
    }
}
GLenum NkDevice_GL::ToGLFilter(NkFilter f, NkMipFilter mf) {
    if (mf==NkMipFilter::None)    return f==NkFilter::Nearest?GL_NEAREST:GL_LINEAR;
    if (mf==NkMipFilter::Nearest) return f==NkFilter::Nearest?GL_NEAREST_MIPMAP_NEAREST:GL_LINEAR_MIPMAP_NEAREST;
    return f==NkFilter::Nearest?GL_NEAREST_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_LINEAR;
}
GLenum NkDevice_GL::ToGLWrap(NkAddressMode a) {
    switch(a) {
        case NkAddressMode::Repeat:         return GL_REPEAT;
        case NkAddressMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case NkAddressMode::ClampToEdge:    return GL_CLAMP_TO_EDGE;
        case NkAddressMode::ClampToBorder:  return GL_CLAMP_TO_BORDER;
        default:                            return GL_REPEAT;
    }
}
GLenum NkDevice_GL::ToGLCompareOp(NkCompareOp op) {
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
}
GLenum NkDevice_GL::ToGLBlendFactor(NkBlendFactor f) {
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
        case NkBlendFactor::ConstantColor:         return GL_CONSTANT_COLOR;
        case NkBlendFactor::OneMinusConstantColor: return GL_ONE_MINUS_CONSTANT_COLOR;
        case NkBlendFactor::SrcAlphaSaturate:      return GL_SRC_ALPHA_SATURATE;
        default:                                   return GL_ONE;
    }
}
GLenum NkDevice_GL::ToGLBlendOp(NkBlendOp op) {
    switch(op) {
        case NkBlendOp::Add:    return GL_FUNC_ADD;
        case NkBlendOp::Sub:    return GL_FUNC_SUBTRACT;
        case NkBlendOp::RevSub: return GL_FUNC_REVERSE_SUBTRACT;
        case NkBlendOp::Min:    return GL_MIN;
        case NkBlendOp::Max:    return GL_MAX;
        default:                return GL_FUNC_ADD;
    }
}
GLenum NkDevice_GL::ToGLPrimitive(NkPrimitiveTopology t) {
    switch(t) {
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
GLenum NkDevice_GL::ToGLShaderStage(NkShaderStage s) {
    switch(s) {
        case NkShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case NkShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case NkShaderStage::Geometry: return GL_GEOMETRY_SHADER;
        case NkShaderStage::TessCtrl: return GL_TESS_CONTROL_SHADER;
        case NkShaderStage::TessEval: return GL_TESS_EVALUATION_SHADER;
        case NkShaderStage::Compute:  return GL_COMPUTE_SHADER;
        default:                      return GL_VERTEX_SHADER;
    }
}
GLenum NkDevice_GL::ToGLBufferUsage(NkResourceUsage u, NkBindFlags) {
    switch(u) {
        case NkResourceUsage::Upload:    return GL_DYNAMIC_DRAW;
        case NkResourceUsage::Readback:  return GL_DYNAMIC_READ;
        case NkResourceUsage::Immutable: return GL_STATIC_DRAW;
        default:                         return GL_DYNAMIC_COPY;
    }
}

} // namespace nkentseu
