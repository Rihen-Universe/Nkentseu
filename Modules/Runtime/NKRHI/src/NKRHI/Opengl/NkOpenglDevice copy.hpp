// =============================================================================
// NkRHI_Device_GL.cpp — Implémentation OpenGL 4.3+ du NkIDevice
// Utilise Direct State Access (GL 4.5+) avec fallback GL 4.3
// =============================================================================
#include "NkOpenglDevice.h"
#include "NkOpenglCommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>

#define NK_GL_LOG(...) logger_src.Infof("[NkRHI_GL] " __VA_ARGS__)
#define NK_GL_ERR(...) logger_src.Infof("[NkRHI_GL][ERR] " __VA_ARGS__)
#define NK_GL_CHECK() do { GLenum e=glGetError(); if(e!=GL_NO_ERROR) NK_GL_ERR("GL error 0x%X at %s:%d\n",e,__FILE__,__LINE__); } while(0)

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS)
namespace {

static PIXELFORMATDESCRIPTOR BuildFallbackPfd(const NkWGLFallbackPixelFormat& fb) {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = fb.version;
    pfd.dwFlags    = static_cast<DWORD>(fb.flags);
    pfd.iPixelType = (fb.pixelType == NkPFDPixelType::NK_PFD_PIXEL_RGBA)
        ? PFD_TYPE_RGBA
        : PFD_TYPE_COLORINDEX;
    pfd.cColorBits   = fb.colorBits;
    pfd.cAlphaBits   = fb.alphaBits;
    pfd.cDepthBits   = fb.depthBits;
    pfd.cStencilBits = fb.stencilBits;
    pfd.cAccumBits   = fb.accumBits;
    pfd.cAuxBuffers  = fb.auxBuffers;
    pfd.iLayerType   = PFD_MAIN_PLANE;
    return pfd;
}

static void* NkOpenGLGetProcAddressCompat(const char* name) {
    if (!name) return nullptr;
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc && proc != reinterpret_cast<void*>(0x1) &&
        proc != reinterpret_cast<void*>(0x2) &&
        proc != reinterpret_cast<void*>(0x3) &&
        proc != reinterpret_cast<void*>(-1)) {
        return proc;
    }
    static HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
    return opengl32 ? reinterpret_cast<void*>(GetProcAddress(opengl32, name)) : nullptr;
}

} // namespace
#endif

NkOpenGLDevice::~NkOpenGLDevice() { if(mIsValid) Shutdown(); }

// =============================================================================
bool NkOpenGLDevice::Initialize(const NkDeviceInitInfo& init) {
    mInit = init;

    NkGpuPolicy::ApplyPreContext(mInit.context);

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    mNativeHwnd = init.surface.hwnd;
    if (!mNativeHwnd) {
        NK_GL_ERR("HWND missing in NkDeviceInitInfo.surface\n");
        return false;
    }

    mNativeHdc = GetDC(mNativeHwnd);
    if (!mNativeHdc) {
        NK_GL_ERR("GetDC failed for window handle\n");
        return false;
    }

    if (GetPixelFormat(mNativeHdc) == 0) {
        const PIXELFORMATDESCRIPTOR pfd = BuildFallbackPfd(init.context.opengl.wglFallback);
        const int pixelFormat = ChoosePixelFormat(mNativeHdc, &pfd);
        if (pixelFormat == 0 || !SetPixelFormat(mNativeHdc, pixelFormat, &pfd)) {
            NK_GL_ERR("Failed to setup WGL pixel format\n");
            ReleaseDC(mNativeHwnd, mNativeHdc);
            mNativeHdc = nullptr;
            return false;
        }
    }

    mNativeGlrc = wglCreateContext(mNativeHdc);
    if (!mNativeGlrc || !wglMakeCurrent(mNativeHdc, mNativeGlrc)) {
        NK_GL_ERR("Failed to create/make current WGL context\n");
        if (mNativeGlrc) wglDeleteContext(mNativeGlrc);
        mNativeGlrc = nullptr;
        ReleaseDC(mNativeHwnd, mNativeHdc);
        mNativeHdc = nullptr;
        return false;
    }

#ifndef NK_NO_GLAD2
    if (!gladLoadGL((GLADloadfunc)NkOpenGLGetProcAddressCompat)) {
        NK_GL_ERR("gladLoadGL failed\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(mNativeGlrc);
        mNativeGlrc = nullptr;
        ReleaseDC(mNativeHwnd, mNativeHdc);
        mNativeHdc = nullptr;
        return false;
    }
#endif

    mWglSwapIntervalExt = reinterpret_cast<BOOL(WINAPI*)(int)>(NkOpenGLGetProcAddressCompat("wglSwapIntervalEXT"));
    if (mWglSwapIntervalExt) {
        mWglSwapIntervalExt(static_cast<int>(init.context.opengl.swapInterval));
    }
#endif

    // Vérifier GL 4.3 minimum (compute shaders)
    GLint major=0, minor=0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 4 || (major == 4 && minor < 3)) {
        NK_GL_ERR("OpenGL 4.3+ required (got %d.%d)\n", major, minor);
        return false;
    }

    mWidth  = NkDeviceInitWidth(init);
    mHeight = NkDeviceInitHeight(init);
    if (mWidth == 0)  mWidth = 1280;
    if (mHeight == 0) mHeight = 720;

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

void NkOpenGLDevice::Shutdown() {
    WaitIdle();
    // Détruire toutes les ressources restantes
    mBuffers.ForEach([](const uint64&, GLBuffer& b) { glDeleteBuffers(1, &b.id); });
    mTextures.ForEach([](const uint64&, GLTexture& t) { glDeleteTextures(1, &t.id); });
    mSamplers.ForEach([](const uint64&, GLSampler& s) { glDeleteSamplers(1, &s.id); });
    mShaders.ForEach([](const uint64&, GLShader& sh) { glDeleteProgram(sh.program); });
    mPipelines.ForEach([](const uint64&, GLPipeline& p) {
        if (p.vao) glDeleteVertexArrays(1,&p.vao);
    });
    mFramebuffers.ForEach([](const uint64&, GLFBO& f) {
        if (f.id != 0) glDeleteFramebuffers(1,&f.id);
    });
    mFences.ForEach([](const uint64&, GLFenceObj& fn) {
        if (fn.sync) glDeleteSync(fn.sync);
    });
    mBuffers.Clear(); mTextures.Clear(); mSamplers.Clear();
    mShaders.Clear(); mPipelines.Clear(); mFramebuffers.Clear();
    mRenderPasses.Clear(); mDescLayouts.Clear(); mDescSets.Clear();
    mFences.Clear();

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    if (mNativeHdc && mNativeGlrc) {
        wglMakeCurrent(nullptr, nullptr);
    }
    if (mNativeGlrc) {
        wglDeleteContext(mNativeGlrc);
        mNativeGlrc = nullptr;
    }
    if (mNativeHwnd && mNativeHdc) {
        ReleaseDC(mNativeHwnd, mNativeHdc);
        mNativeHdc = nullptr;
    }
    mNativeHwnd = nullptr;
#endif

    mIsValid = false;
    NK_GL_LOG("Shutdown\n");
}

void NkOpenGLDevice::QueryCaps() {
    GLint v=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,         &v); mCaps.maxTextureDim2D=(uint32)v;
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,      &v); mCaps.maxTextureDim3D=(uint32)v;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,&v); mCaps.maxTextureCubeSize=(uint32)v;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &v); mCaps.maxTextureArrayLayers=(uint32)v;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,    &v); mCaps.maxColorAttachments=(uint32)v;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,       &v); mCaps.maxVertexAttributes=(uint32)v;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,   &v); mCaps.maxUniformBufferRange=(uint32)v;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&v); mCaps.maxStorageBufferRange=(uint32)v;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,0,&v); mCaps.maxComputeGroupSizeX=(uint32)v;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,1,&v); mCaps.maxComputeGroupSizeY=(uint32)v;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,2,&v); mCaps.maxComputeGroupSizeZ=(uint32)v;
    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,&v); mCaps.maxComputeSharedMemory=(uint32)v;
    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY,&v); mCaps.maxSamplerAnisotropy=(uint32)v;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,&v); mCaps.minUniformBufferAlign=(uint32)v;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,&v); mCaps.minStorageBufferAlign=(uint32)v;

    mCaps.computeShaders    = NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_API_OPENGL);
    mCaps.geometryShaders   = true;
    mCaps.tessellationShaders=true;
    mCaps.drawIndirect      = true;
    mCaps.multiViewport     = true;
    mCaps.independentBlend  = true;
    mCaps.timestampQueries  = true;
    mCaps.textureCompressionBC=true; // sur desktop

    // MSAA support
    GLint maxS=0;
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES,&maxS);
    mCaps.msaa2x  = maxS >= 2;
    mCaps.msaa4x  = maxS >= 4;
    mCaps.msaa8x  = maxS >= 8;
    mCaps.msaa16x = maxS >= 16;
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkOpenGLDevice::CreateBuffer(const NkBufferDesc& desc) {
    if (desc.sizeBytes==0) return {};
    threading::NkScopedLock lock(mMutex);

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

void NkOpenGLDevice::DestroyBuffer(NkBufferHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLBuffer* buffer = mBuffers.Find(h.id);
    if (!buffer) return;
    glDeleteBuffers(1,&buffer->id);
    mBuffers.Erase(h.id);
    h.id=0;
}

bool NkOpenGLDevice::WriteBuffer(NkBufferHandle buf, const void* data,
                               uint64 size, uint64 offset) {
    threading::NkScopedLock lock(mMutex);
    GLBuffer* buffer = mBuffers.Find(buf.id);
    if (!buffer || !data) return false;
    glNamedBufferSubData(buffer->id, (GLintptr)offset, (GLsizeiptr)size, data);
    return true;
}

bool NkOpenGLDevice::WriteBufferAsync(NkBufferHandle buf, const void* data,
                                    uint64 size, uint64 offset) {
    // GL : async via glMapNamedBufferRange + UNSYNCHRONIZED
    threading::NkScopedLock lock(mMutex);
    GLBuffer* buffer = mBuffers.Find(buf.id);
    if (!buffer || !data) return false;
    void* ptr = glMapNamedBufferRange(buffer->id, (GLintptr)offset,
        (GLsizeiptr)size,
        GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_INVALIDATE_RANGE_BIT);
    if (!ptr) return false;
    memcpy(ptr, data, (size_t)size);
    glUnmapNamedBuffer(buffer->id);
    return true;
}

bool NkOpenGLDevice::ReadBuffer(NkBufferHandle buf, void* out,
                              uint64 size, uint64 offset) {
    threading::NkScopedLock lock(mMutex);
    GLBuffer* buffer = mBuffers.Find(buf.id);
    if (!buffer) return false;
    glGetNamedBufferSubData(buffer->id, (GLintptr)offset, (GLsizeiptr)size, out);
    return true;
}

NkMappedMemory NkOpenGLDevice::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    threading::NkScopedLock lock(mMutex);
    GLBuffer* buffer = mBuffers.Find(buf.id);
    if (!buffer) return {};
    uint64 mapSz = sz>0 ? sz : buffer->size-off;
    void* ptr=glMapNamedBufferRange(buffer->id,(GLintptr)off,(GLsizeiptr)mapSz,
        GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);
    return {ptr, mapSz};
}

void NkOpenGLDevice::UnmapBuffer(NkBufferHandle buf) {
    GLBuffer* buffer = mBuffers.Find(buf.id);
    if (buffer) glUnmapNamedBuffer(buffer->id);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkOpenGLDevice::CreateTexture(const NkTextureDesc& desc) {
    threading::NkScopedLock lock(mMutex);
    GLuint id=0;
    GLenum target = ToGLTextureTarget(desc.type, desc.samples);
    glCreateTextures(target, 1, &id);

    const uint32 maxDim = desc.width > desc.height ? desc.width : desc.height;
    uint32 mips = desc.mipLevels==0 ?
        (uint32)(std::floor(std::log2((double)maxDim))+1) :
        desc.mipLevels;

    GLenum internal = ToGLInternalFormat(desc.format);

    switch(desc.type) {
        case NkTextureType::NK_TEX2D:
            if (desc.samples > NkSampleCount::NK_S1)
                glTextureStorage2DMultisample(id,(GLsizei)desc.samples,
                    internal,desc.width,desc.height,GL_TRUE);
            else
                glTextureStorage2D(id,mips,internal,desc.width,desc.height);
            break;
        case NkTextureType::NK_TEX2D_ARRAY:
            glTextureStorage3D(id,mips,internal,desc.width,desc.height,desc.arrayLayers);
            break;
        case NkTextureType::NK_CUBE:
            // Cubemap immutable storage uses 2D allocation for the 6 faces.
            glTextureStorage2D(id,mips,internal,desc.width,desc.height);
            break;
        case NkTextureType::NK_CUBE_ARRAY:
            glTextureStorage3D(id,mips,internal,desc.width,desc.height,desc.arrayLayers);
            break;
        case NkTextureType::NK_TEX3D:
            glTextureStorage3D(id,mips,internal,desc.width,desc.height,desc.depth);
            break;
        case NkTextureType::NK_TEX1D:
            glTextureStorage1D(id,mips,internal,desc.width);
            break;
    }

    if (desc.initialData) {
        GLenum base=ToGLBaseFormat(desc.format), type2=ToGLType(desc.format);
        uint32 rp=desc.rowPitch>0?desc.rowPitch:desc.width*NkFormatBytesPerPixel(desc.format);
        const uint32 bpp = NkFormatBytesPerPixel(desc.format);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rp / (bpp > 0 ? bpp : 1u));
        glTextureSubImage2D(id,0,0,0,desc.width,desc.height,base,type2,desc.initialData);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        if (mips>1) glGenerateTextureMipmap(id);
    }

    if (desc.debugName) glObjectLabel(GL_TEXTURE,id,-1,desc.debugName);

    uint64 hid=NextId();
    mTextures[hid]={id,target,desc};
    NkTextureHandle h; h.id=hid; return h;
}

void NkOpenGLDevice::DestroyTexture(NkTextureHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLTexture* texture = mTextures.Find(h.id);
    if (!texture) return;
    glDeleteTextures(1,&texture->id);
    mTextures.Erase(h.id);
    h.id=0;
}

bool NkOpenGLDevice::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    GLTexture* texture = mTextures.Find(t.id);
    if (!texture) return false;
    const NkTextureDesc& desc = texture->desc;
    return WriteTextureRegion(t,p,0,0,0,desc.width,desc.height,1,0,0,rp);
}

bool NkOpenGLDevice::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
    uint32 mip,uint32 layer,uint32 rowPitch) {
    GLTexture* texture = mTextures.Find(t.id);
    if (!texture || !pixels) return false;
    const NkTextureDesc& desc = texture->desc;
    GLenum base=ToGLBaseFormat(desc.format), type2=ToGLType(desc.format);
    uint32 bpp=NkFormatBytesPerPixel(desc.format);
    uint32 rp2=rowPitch>0?rowPitch:w*bpp;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rp2/(bpp > 0 ? bpp : 1u));
    if (desc.type==NkTextureType::NK_TEX2D)
        glTextureSubImage2D(texture->id,(GLint)mip,(GLint)x,(GLint)y,(GLsizei)w,(GLsizei)h,base,type2,pixels);
    else if (desc.type==NkTextureType::NK_TEX3D || desc.type==NkTextureType::NK_TEX2D_ARRAY)
        glTextureSubImage3D(texture->id,(GLint)mip,(GLint)x,(GLint)y,(GLint)(layer+z),(GLsizei)w,(GLsizei)h,(GLsizei)d2,base,type2,pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    return true;
}

bool NkOpenGLDevice::GenerateMipmaps(NkTextureHandle t, NkFilter) {
    GLTexture* texture = mTextures.Find(t.id);
    if (!texture) return false;
    glGenerateTextureMipmap(texture->id);
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkOpenGLDevice::CreateSampler(const NkSamplerDesc& d) {
    threading::NkScopedLock lock(mMutex);
    GLuint id=0; glCreateSamplers(1,&id);
    glSamplerParameteri(id,GL_TEXTURE_MAG_FILTER,(GLint)ToGLFilter(d.magFilter,NkMipFilter::NK_NONE));
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

void NkOpenGLDevice::DestroySampler(NkSamplerHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLSampler* sampler = mSamplers.Find(h.id);
    if (!sampler) return;
    glDeleteSamplers(1,&sampler->id);
    mSamplers.Erase(h.id); h.id=0;
}

// =============================================================================
// Shaders
// =============================================================================
GLuint NkOpenGLDevice::CompileGLStage(GLenum stage, const char* src) {
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

NkShaderHandle NkOpenGLDevice::CreateShader(const NkShaderDesc& desc) {
    threading::NkScopedLock lock(mMutex);
    GLuint prog=glCreateProgram();
    uint32 attachedCount = 0;

    for (uint32 i=0;i<desc.stages.Size();i++) {
        auto& s=desc.stages[i];
        const char* src = s.glslSource;
        // A shader descriptor may carry multiple language variants (GLSL/HLSL/MSL).
        // OpenGL backend must only consume GLSL stages and silently skip the others.
        if (!src || !src[0]) { continue; }
        GLenum glStage=ToGLShaderStage(s.stage);
        GLuint sh=CompileGLStage(glStage,src);
        if (sh) {
            glAttachShader(prog,sh);
            glDeleteShader(sh);
            ++attachedCount;
        }
    }

    if (attachedCount == 0) {
        NK_GL_ERR("GL shader: no GLSL stage provided\n");
        glDeleteProgram(prog);
        return {};
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

void NkOpenGLDevice::DestroyShader(NkShaderHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLShader* shader = mShaders.Find(h.id);
    if (!shader) return;
    glDeleteProgram(shader->program);
    mShaders.Erase(h.id); h.id=0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkOpenGLDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    threading::NkScopedLock lock(mMutex);
    GLShader* shader = mShaders.Find(d.shader.id);
    if (!shader) return {};

    GLPipeline p;
    p.program     = shader->program;
    p.vertexLayout= d.vertexLayout;
    p.gfxDesc     = d;
    p.isCompute   = false;

    // Créer le VAO correspondant au vertex layout
    glCreateVertexArrays(1,&p.vao);
    for (uint32 i=0;i<d.vertexLayout.attributes.Size();i++) {
        auto& a=d.vertexLayout.attributes[i];
        glEnableVertexArrayAttrib(p.vao,a.location);
        GLint  compCount  = 3;
        GLenum compType   = GL_FLOAT;
        GLboolean norm    = GL_FALSE;
        bool isInteger    = false;
        switch (a.format) {
            case NkVertexFormat::NK_R32_UINT:   compCount = 1; compType = GL_UNSIGNED_INT; isInteger = true; break;
            case NkVertexFormat::NK_RG32_UINT:  compCount = 2; compType = GL_UNSIGNED_INT; isInteger = true; break;
            case NkVertexFormat::NK_RGBA32_UINT:compCount = 4; compType = GL_UNSIGNED_INT; isInteger = true; break;
            case NkVertexFormat::NK_R32_SINT:   compCount = 1; compType = GL_INT;          isInteger = true; break;
            case NkVertexFormat::NK_RG32_SINT:  compCount = 2; compType = GL_INT;          isInteger = true; break;
            case NkVertexFormat::NK_RGBA32_SINT:compCount = 4; compType = GL_INT;          isInteger = true; break;
            case NkVertexFormat::NK_R32_FLOAT:  compCount = 1; break;
            case NkVertexFormat::NK_RG32_FLOAT: compCount = 2; break;
            case NkVertexFormat::NK_RGB32_FLOAT:compCount = 3; break;
            case NkVertexFormat::NK_RGBA32_FLOAT:compCount = 4; break;
            case NkVertexFormat::NK_RGBA8_UNORM:
            case NkVertexFormat::NK_R8G8B8A8_UNORM_PACKED:
                compCount = 4; compType = GL_UNSIGNED_BYTE; norm = GL_TRUE; break;
            case NkVertexFormat::NK_RGBA8_SNORM:
                compCount = 4; compType = GL_BYTE; norm = GL_TRUE; break;
            case NkVertexFormat::NK_A2B10G10R10_UNORM:
                compCount = 4; compType = GL_UNSIGNED_INT_2_10_10_10_REV; norm = GL_TRUE; break;
            default:
                break;
        }
        if (isInteger) {
            glVertexArrayAttribIFormat(p.vao, a.location, compCount, compType, (GLuint)a.offset);
        } else {
            glVertexArrayAttribFormat(p.vao, a.location, compCount, compType, norm, (GLuint)a.offset);
        }
        glVertexArrayAttribBinding(p.vao,a.location,a.binding);
    }
    for (uint32 i=0;i<d.vertexLayout.bindings.Size();i++) {
        auto& b=d.vertexLayout.bindings[i];
        if (b.perInstance)
            glVertexArrayBindingDivisor(p.vao,b.binding,1);
    }

    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

NkPipelineHandle NkOpenGLDevice::CreateComputePipeline(const NkComputePipelineDesc& d) {
    threading::NkScopedLock lock(mMutex);
    GLShader* shader = mShaders.Find(d.shader.id);
    if (!shader) return {};

    GLPipeline p;
    p.program   = shader->program;
    p.compDesc  = d;
    p.isCompute = true;

    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

void NkOpenGLDevice::DestroyPipeline(NkPipelineHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLPipeline* pipeline = mPipelines.Find(h.id);
    if (!pipeline) return;
    if (pipeline->vao) glDeleteVertexArrays(1,&pipeline->vao);
    mPipelines.Erase(h.id); h.id=0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle NkOpenGLDevice::CreateRenderPass(const NkRenderPassDesc& d) {
    threading::NkScopedLock lock(mMutex);
    uint64 hid=NextId(); mRenderPasses[hid]=d;
    NkRenderPassHandle h; h.id=hid; return h;
}
void NkOpenGLDevice::DestroyRenderPass(NkRenderPassHandle& h) {
    threading::NkScopedLock lock(mMutex);
    mRenderPasses.Erase(h.id); h.id=0;
}

NkFramebufferHandle NkOpenGLDevice::CreateFramebuffer(const NkFramebufferDesc& d) {
    threading::NkScopedLock lock(mMutex);
    GLuint fbo=0;
    glCreateFramebuffers(1,&fbo);

    for (uint32 i=0;i<d.colorAttachments.Size();i++) {
        GLTexture* texture = mTextures.Find(d.colorAttachments[i].id);
        if (texture)
            glNamedFramebufferTexture(fbo,GL_COLOR_ATTACHMENT0+i,texture->id,0);
    }
    if (d.depthAttachment.IsValid()) {
        GLTexture* texture = mTextures.Find(d.depthAttachment.id);
        if (texture) {
            const NkTextureDesc& desc = texture->desc;
            GLenum att = NkFormatHasStencil(desc.format)
                ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            glNamedFramebufferTexture(fbo,att,texture->id,0);
        }
    }

    GLenum status=glCheckNamedFramebufferStatus(fbo,GL_FRAMEBUFFER);
    if (status!=GL_FRAMEBUFFER_COMPLETE)
        NK_GL_ERR("Framebuffer incomplete: 0x%X\n",(unsigned)status);

    if (d.debugName) glObjectLabel(GL_FRAMEBUFFER,fbo,-1,d.debugName);

    uint64 hid=NextId(); mFramebuffers[hid]={fbo,d.width,d.height};
    NkFramebufferHandle h; h.id=hid; return h;
}

void NkOpenGLDevice::DestroyFramebuffer(NkFramebufferHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLFBO* fbo = mFramebuffers.Find(h.id);
    if (!fbo) return;
    if (fbo->id!=0) glDeleteFramebuffers(1,&fbo->id);
    mFramebuffers.Erase(h.id); h.id=0;
}

// =============================================================================
// Descriptor Sets (émulation GL)
// =============================================================================
NkDescSetHandle NkOpenGLDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    threading::NkScopedLock lock(mMutex);
    uint64 hid=NextId(); mDescLayouts[hid]={d};
    NkDescSetHandle h; h.id=hid; return h;
}
void NkOpenGLDevice::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    threading::NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id=0;
}
NkDescSetHandle NkOpenGLDevice::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
    threading::NkScopedLock lock(mMutex);
    GLDescSet ds; ds.layoutHandle=layoutHandle;
    uint64 hid=NextId(); mDescSets[hid]=ds;
    NkDescSetHandle h; h.id=hid; return h;
}
void NkOpenGLDevice::FreeDescriptorSet(NkDescSetHandle& h) {
    threading::NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id=0;
}
void NkOpenGLDevice::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    threading::NkScopedLock lock(mMutex);
    for (uint32 i=0;i<n;i++) {
        auto& w=writes[i];
        GLDescSet* set = mDescSets.Find(w.set.id);
        if (!set) continue;
        auto& b=set->bindings[w.binding];
        b.type=w.type;
        if (w.buffer.IsValid()) {
            GLBuffer* buffer = mBuffers.Find(w.buffer.id);
            if (buffer) { b.bufferId=buffer->id; b.bufferOffset=w.bufferOffset; b.bufferRange=w.bufferRange; }
        }
        if (w.texture.IsValid()) {
            GLTexture* texture = mTextures.Find(w.texture.id);
            if (texture) b.textureId=texture->id;
        }
        if (w.sampler.IsValid()) {
            GLSampler* sampler = mSamplers.Find(w.sampler.id);
            if (sampler) b.samplerId=sampler->id;
        }
    }
}

void NkOpenGLDevice::ApplyDescriptors(const GLDescSet& ds) {
    GLDescSetLayout* layoutObj = mDescLayouts.Find(ds.layoutHandle.id);
    if (!layoutObj) return;
    const NkDescriptorSetLayoutDesc& layout = layoutObj->desc;
    for (uint32 i=0;i<layout.bindings.Size();i++) {
        auto& lb=layout.bindings[i];
        auto& b=ds.bindings[lb.binding];
        switch(b.type) {
            case NkDescriptorType::NK_UNIFORM_BUFFER:
            case NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC:
                if (b.bufferId) glBindBufferRange(GL_UNIFORM_BUFFER,lb.binding,
                    b.bufferId,(GLintptr)b.bufferOffset,(GLsizeiptr)(b.bufferRange>0?b.bufferRange:65536));
                break;
            case NkDescriptorType::NK_STORAGE_BUFFER:
            case NkDescriptorType::NK_STORAGE_BUFFER_DYNAMIC:
                if (b.bufferId) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,lb.binding,b.bufferId);
                break;
            case NkDescriptorType::NK_SAMPLED_TEXTURE:
            case NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER:
                if (b.textureId) { glBindTextureUnit(lb.binding,b.textureId); }
                if (b.samplerId) { glBindSampler(lb.binding,b.samplerId); }
                break;
            case NkDescriptorType::NK_STORAGE_TEXTURE:
                if (b.textureId) glBindImageTexture(lb.binding,b.textureId,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);
                break;
            default: break;
        }
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkOpenGLDevice::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkOpenGLCommandBuffer(this, t);
}
void NkOpenGLDevice::DestroyCommandBuffer(NkICommandBuffer*& cb) {
    delete cb; cb=nullptr;
}

// =============================================================================
// Submit & Sync
// =============================================================================
void NkOpenGLDevice::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i=0;i<n;i++) {
        auto* gl=dynamic_cast<NkOpenGLCommandBuffer*>(cbs[i]);
        if (gl) gl->Execute(this);
    }
    if (fence.IsValid()) {
        GLFenceObj* fenceObj = mFences.Find(fence.id);
        if (fenceObj) {
            if (fenceObj->sync) glDeleteSync(fenceObj->sync);
            fenceObj->sync=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            fenceObj->signaled=false;
        }
    }
}

void NkOpenGLDevice::SubmitAndPresent(NkICommandBuffer* cb) {
    Submit(&cb,1,{});
    if (mInit.presentCallback) {
        mInit.presentCallback();
        return;
    }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    if (mNativeHdc) {
        SwapBuffers(mNativeHdc);
    }
#endif
}

NkFenceHandle NkOpenGLDevice::CreateFence(bool signaled) {
    threading::NkScopedLock lock(mMutex);
    GLFenceObj f; f.signaled=signaled;
    if (signaled) f.sync=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    uint64 hid=NextId(); mFences[hid]=f;
    NkFenceHandle h; h.id=hid; return h;
}
void NkOpenGLDevice::DestroyFence(NkFenceHandle& h) {
    threading::NkScopedLock lock(mMutex);
    GLFenceObj* fenceObj = mFences.Find(h.id);
    if (!fenceObj) return;
    if (fenceObj->sync) glDeleteSync(fenceObj->sync);
    mFences.Erase(h.id); h.id=0;
}
bool NkOpenGLDevice::WaitFence(NkFenceHandle f, uint64 timeoutNs) {
    GLFenceObj* fenceObj = mFences.Find(f.id);
    if (!fenceObj) return false;
    if (!fenceObj->sync) return true;
    GLenum r=glClientWaitSync(fenceObj->sync,GL_SYNC_FLUSH_COMMANDS_BIT,(GLuint64)timeoutNs);
    if (r==GL_ALREADY_SIGNALED||r==GL_CONDITION_SATISFIED) { fenceObj->signaled=true; return true; }
    return false;
}
bool NkOpenGLDevice::IsFenceSignaled(NkFenceHandle f) {
    GLFenceObj* fenceObj = mFences.Find(f.id);
    if (!fenceObj) return false;
    if (!fenceObj->sync) return fenceObj->signaled;
    GLint v=0; GLsizei l=0;
    glGetSynciv(fenceObj->sync,GL_SYNC_STATUS,sizeof(v),&l,&v);
    return v==GL_SIGNALED;
}
void NkOpenGLDevice::ResetFence(NkFenceHandle f) {
    GLFenceObj* fenceObj = mFences.Find(f.id);
    if (!fenceObj) return;
    if (fenceObj->sync) { glDeleteSync(fenceObj->sync); fenceObj->sync=nullptr; }
    fenceObj->signaled=false;
}
void NkOpenGLDevice::WaitIdle() { glFinish(); }

// =============================================================================
// Frame
// =============================================================================
bool NkOpenGLDevice::BeginFrame(NkFrameContext& frame) {
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
    return true;
}
void NkOpenGLDevice::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex+1) % MAX_FRAMES;
    ++mFrameNumber;
}
void NkOpenGLDevice::OnResize(uint32 w, uint32 h) {
    if (w==0||h==0) return;
    mWidth=w; mHeight=h;
    // Mettre à jour le FBO swapchain virtuel
    GLFBO* fbo = mFramebuffers.Find(mSwapchainFB.id);
    if (fbo) { fbo->w=w; fbo->h=h; }
    if (mInit.resizeCallback) {
        mInit.resizeCallback(w, h);
    }
}

// =============================================================================
// Conversions GL
// =============================================================================
GLenum NkOpenGLDevice::ToGLInternalFormat(NkGPUFormat f) {
    switch(f) {
        case NkGPUFormat::NK_R8_UNORM:      return GL_R8;
        case NkGPUFormat::NK_RG8_UNORM:     return GL_RG8;
        case NkGPUFormat::NK_RGBA8_UNORM:   return GL_RGBA8;
        case NkGPUFormat::NK_RGBA8_SRGB:    return GL_SRGB8_ALPHA8;
        case NkGPUFormat::NK_BGRA8_UNORM:   return GL_RGBA8; // swizzle manuel
        case NkGPUFormat::NK_BGRA8_SRGB:    return GL_SRGB8_ALPHA8;
        case NkGPUFormat::NK_R16_FLOAT:     return GL_R16F;
        case NkGPUFormat::NK_RG16_FLOAT:    return GL_RG16F;
        case NkGPUFormat::NK_RGBA16_FLOAT:  return GL_RGBA16F;
        case NkGPUFormat::NK_R32_FLOAT:     return GL_R32F;
        case NkGPUFormat::NK_RG32_FLOAT:    return GL_RG32F;
        case NkGPUFormat::NK_RGB32_FLOAT:   return GL_RGB32F;
        case NkGPUFormat::NK_RGBA32_FLOAT:  return GL_RGBA32F;
        case NkGPUFormat::NK_R32_UINT:      return GL_R32UI;
        case NkGPUFormat::NK_R16_UINT:      return GL_R16UI;
        case NkGPUFormat::NK_RGBA16_UINT:   return GL_RGBA16UI;
        case NkGPUFormat::NK_D16_UNORM:     return GL_DEPTH_COMPONENT16;
        case NkGPUFormat::NK_D32_FLOAT:     return GL_DEPTH_COMPONENT32F;
        case NkGPUFormat::NK_D24_UNORM_S8_UINT: return GL_DEPTH24_STENCIL8;
        case NkGPUFormat::NK_D32_FLOAT_S8_UINT: return GL_DEPTH32F_STENCIL8;
        case NkGPUFormat::NK_BC1_RGB_UNORM: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case NkGPUFormat::NK_BC3_UNORM:     return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case NkGPUFormat::NK_BC5_UNORM:     return GL_COMPRESSED_RG_RGTC2;
        case NkGPUFormat::NK_BC7_UNORM:     return GL_COMPRESSED_RGBA_BPTC_UNORM;
        case NkGPUFormat::NK_R11G11B10_FLOAT:return GL_R11F_G11F_B10F;
        default:                      return GL_RGBA8;
    }
}
GLenum NkOpenGLDevice::ToGLBaseFormat(NkGPUFormat f) {
    if (NkFormatIsDepth(f)) return NkFormatHasStencil(f)?GL_DEPTH_STENCIL:GL_DEPTH_COMPONENT;
    switch(f) {
        case NkGPUFormat::NK_R8_UNORM: case NkGPUFormat::NK_R16_FLOAT:
        case NkGPUFormat::NK_R32_FLOAT: case NkGPUFormat::NK_R32_UINT: return GL_RED;
        case NkGPUFormat::NK_RG8_UNORM: case NkGPUFormat::NK_RG16_FLOAT:
        case NkGPUFormat::NK_RG32_FLOAT: return GL_RG;
        case NkGPUFormat::NK_RGB32_FLOAT: return GL_RGB;
        default: return GL_RGBA;
    }
}
GLenum NkOpenGLDevice::ToGLType(NkGPUFormat f) {
    switch(f) {
        case NkGPUFormat::NK_R16_FLOAT: case NkGPUFormat::NK_RG16_FLOAT:
        case NkGPUFormat::NK_RGBA16_FLOAT: return GL_HALF_FLOAT;
        case NkGPUFormat::NK_R32_FLOAT: case NkGPUFormat::NK_RG32_FLOAT:
        case NkGPUFormat::NK_RGB32_FLOAT: case NkGPUFormat::NK_RGBA32_FLOAT:
        case NkGPUFormat::NK_D32_FLOAT: return GL_FLOAT;
        case NkGPUFormat::NK_R32_UINT: case NkGPUFormat::NK_RGBA16_UINT: return GL_UNSIGNED_INT;
        case NkGPUFormat::NK_D24_UNORM_S8_UINT: return GL_UNSIGNED_INT_24_8;
        case NkGPUFormat::NK_D32_FLOAT_S8_UINT: return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        default: return GL_UNSIGNED_BYTE;
    }
}
GLenum NkOpenGLDevice::ToGLTextureTarget(NkTextureType t, NkSampleCount s) {
    bool msaa = s > NkSampleCount::NK_S1;
    switch(t) {
        case NkTextureType::NK_TEX1D:      return GL_TEXTURE_1D;
        case NkTextureType::NK_TEX2D:      return msaa?GL_TEXTURE_2D_MULTISAMPLE:GL_TEXTURE_2D;
        case NkTextureType::NK_TEX3D:      return GL_TEXTURE_3D;
        case NkTextureType::NK_CUBE:       return GL_TEXTURE_CUBE_MAP;
        case NkTextureType::NK_TEX2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
        case NkTextureType::NK_CUBE_ARRAY:  return GL_TEXTURE_CUBE_MAP_ARRAY;
        default:                        return GL_TEXTURE_2D;
    }
}
GLenum NkOpenGLDevice::ToGLFilter(NkFilter f, NkMipFilter mf) {
    if (mf==NkMipFilter::NK_NONE)    return f==NkFilter::NK_NEAREST?GL_NEAREST:GL_LINEAR;
    if (mf==NkMipFilter::NK_NEAREST) return f==NkFilter::NK_NEAREST?GL_NEAREST_MIPMAP_NEAREST:GL_LINEAR_MIPMAP_NEAREST;
    return f==NkFilter::NK_NEAREST?GL_NEAREST_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_LINEAR;
}
GLenum NkOpenGLDevice::ToGLWrap(NkAddressMode a) {
    switch(a) {
        case NkAddressMode::NK_REPEAT:         return GL_REPEAT;
        case NkAddressMode::NK_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
        case NkAddressMode::NK_CLAMP_TO_EDGE:    return GL_CLAMP_TO_EDGE;
        case NkAddressMode::NK_CLAMP_TO_BORDER:  return GL_CLAMP_TO_BORDER;
        default:                            return GL_REPEAT;
    }
}
GLenum NkOpenGLDevice::ToGLCompareOp(NkCompareOp op) {
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
}
GLenum NkOpenGLDevice::ToGLBlendFactor(NkBlendFactor f) {
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
        case NkBlendFactor::NK_CONSTANT_COLOR:         return GL_CONSTANT_COLOR;
        case NkBlendFactor::NK_ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
        case NkBlendFactor::NK_SRC_ALPHA_SATURATE:      return GL_SRC_ALPHA_SATURATE;
        default:                                   return GL_ONE;
    }
}
GLenum NkOpenGLDevice::ToGLBlendOp(NkBlendOp op) {
    switch(op) {
        case NkBlendOp::NK_ADD:    return GL_FUNC_ADD;
        case NkBlendOp::NK_SUB:    return GL_FUNC_SUBTRACT;
        case NkBlendOp::NK_REV_SUB: return GL_FUNC_REVERSE_SUBTRACT;
        case NkBlendOp::NK_MIN:    return GL_MIN;
        case NkBlendOp::NK_MAX:    return GL_MAX;
        default:                return GL_FUNC_ADD;
    }
}
GLenum NkOpenGLDevice::ToGLPrimitive(NkPrimitiveTopology t) {
    switch(t) {
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
GLenum NkOpenGLDevice::ToGLShaderStage(NkShaderStage s) {
    switch(s) {
        case NkShaderStage::NK_VERTEX:   return GL_VERTEX_SHADER;
        case NkShaderStage::NK_FRAGMENT: return GL_FRAGMENT_SHADER;
        case NkShaderStage::NK_GEOMETRY: return GL_GEOMETRY_SHADER;
        case NkShaderStage::NK_TESS_CTRL: return GL_TESS_CONTROL_SHADER;
        case NkShaderStage::NK_TESS_EVAL: return GL_TESS_EVALUATION_SHADER;
        case NkShaderStage::NK_COMPUTE:  return GL_COMPUTE_SHADER;
        default:                      return GL_VERTEX_SHADER;
    }
}
GLenum NkOpenGLDevice::ToGLBufferUsage(NkResourceUsage u, NkBindFlags) {
    switch(u) {
        case NkResourceUsage::NK_UPLOAD:    return GL_DYNAMIC_DRAW;
        case NkResourceUsage::NK_READBACK:  return GL_DYNAMIC_READ;
        case NkResourceUsage::NK_IMMUTABLE: return GL_STATIC_DRAW;
        default:                         return GL_DYNAMIC_COPY;
    }
}

} // namespace nkentseu
