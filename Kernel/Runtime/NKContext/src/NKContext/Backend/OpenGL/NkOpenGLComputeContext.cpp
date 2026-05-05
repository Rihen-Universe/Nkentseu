// =============================================================================
// NkOpenGLComputeContext.cpp
// Compute via OpenGL compute shaders (GL 4.3+).
// =============================================================================
#include "NkOpenGLComputeContext.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/NkContainers.h"

#include <cstdio>
#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(MemoryBarrier)
#undef MemoryBarrier
#endif

#define NK_CGL_LOG(...) logger.Infof("[NkGLCompute] " __VA_ARGS__)
#define NK_CGL_ERR(...) logger.Errorf("[NkGLCompute] " __VA_ARGS__)

#ifndef NK_NO_GLAD2
#   include <glad/gl.h>
#else
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <GL/gl.h>
#       include <GL/glext.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       include <GL/gl.h>
#       include <GL/glext.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <GLES3/gl31.h>
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <GLES3/gl3.h>
#   else
#       include <GL/gl.h>
#       include <GL/glext.h>
#   endif
#endif

#ifndef GL_COMPUTE_SHADER
#   define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#   define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#   define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif
#ifndef GL_BUFFER_UPDATE_BARRIER_BIT
#   define GL_BUFFER_UPDATE_BARRIER_BIT 0x00000200
#endif
#ifndef GL_MAX_COMPUTE_WORK_GROUP_SIZE
#   define GL_MAX_COMPUTE_WORK_GROUP_SIZE 0x91BF
#endif
#ifndef GL_MAX_COMPUTE_SHARED_MEMORY_SIZE
#   define GL_MAX_COMPUTE_SHARED_MEMORY_SIZE 0x8262
#endif
#ifndef GL_MAP_READ_BIT
#   define GL_MAP_READ_BIT 0x0001
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   define NK_GL_APIENTRY APIENTRY
#else
#   define NK_GL_APIENTRY
#endif

namespace nkentseu {

#ifdef NK_NO_GLAD2

struct NkOpenGLComputeContext::Functions {
    using GenBuffersFn      = void      (NK_GL_APIENTRY*)(GLsizei, GLuint*);
    using BindBufferFn      = void      (NK_GL_APIENTRY*)(GLenum, GLuint);
    using BufferDataFn      = void      (NK_GL_APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
    using DeleteBuffersFn   = void      (NK_GL_APIENTRY*)(GLsizei, const GLuint*);
    using BufferSubDataFn   = void      (NK_GL_APIENTRY*)(GLenum, GLintptr, GLsizeiptr, const void*);
    using MapBufferRangeFn  = void*     (NK_GL_APIENTRY*)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
    using UnmapBufferFn     = GLboolean (NK_GL_APIENTRY*)(GLenum);
    using CreateShaderFn    = GLuint    (NK_GL_APIENTRY*)(GLenum);
    using ShaderSourceFn    = void      (NK_GL_APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    using CompileShaderFn   = void      (NK_GL_APIENTRY*)(GLuint);
    using GetShaderivFn     = void      (NK_GL_APIENTRY*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLogFn= void      (NK_GL_APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using DeleteShaderFn    = void      (NK_GL_APIENTRY*)(GLuint);
    using CreateProgramFn   = GLuint    (NK_GL_APIENTRY*)(void);
    using AttachShaderFn    = void      (NK_GL_APIENTRY*)(GLuint, GLuint);
    using LinkProgramFn     = void      (NK_GL_APIENTRY*)(GLuint);
    using GetProgramivFn    = void      (NK_GL_APIENTRY*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLogFn = void    (NK_GL_APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using DeleteProgramFn   = void      (NK_GL_APIENTRY*)(GLuint);
    using UseProgramFn      = void      (NK_GL_APIENTRY*)(GLuint);
    using BindBufferBaseFn  = void      (NK_GL_APIENTRY*)(GLenum, GLuint, GLuint);
    using DispatchComputeFn = void      (NK_GL_APIENTRY*)(GLuint, GLuint, GLuint);
    using FinishFn          = void      (NK_GL_APIENTRY*)(void);
    using MemoryBarrierFn   = void      (NK_GL_APIENTRY*)(GLbitfield);
    using GetIntegeriVFn    = void      (NK_GL_APIENTRY*)(GLenum, GLuint, GLint*);
    using GetIntegervFn     = void      (NK_GL_APIENTRY*)(GLenum, GLint*);

    GenBuffersFn       GenBuffers       = nullptr;
    BindBufferFn       BindBuffer       = nullptr;
    BufferDataFn       BufferData       = nullptr;
    DeleteBuffersFn    DeleteBuffers    = nullptr;
    BufferSubDataFn    BufferSubData    = nullptr;
    MapBufferRangeFn   MapBufferRange   = nullptr;
    UnmapBufferFn      UnmapBuffer      = nullptr;
    CreateShaderFn     CreateShader     = nullptr;
    ShaderSourceFn     ShaderSource     = nullptr;
    CompileShaderFn    CompileShader    = nullptr;
    GetShaderivFn      GetShaderiv      = nullptr;
    GetShaderInfoLogFn GetShaderInfoLog = nullptr;
    DeleteShaderFn     DeleteShader     = nullptr;
    CreateProgramFn    CreateProgram    = nullptr;
    AttachShaderFn     AttachShader     = nullptr;
    LinkProgramFn      LinkProgram      = nullptr;
    GetProgramivFn     GetProgramiv     = nullptr;
    GetProgramInfoLogFn GetProgramInfoLog = nullptr;
    DeleteProgramFn    DeleteProgram    = nullptr;
    UseProgramFn       UseProgram       = nullptr;
    BindBufferBaseFn   BindBufferBase   = nullptr;
    DispatchComputeFn  DispatchCompute  = nullptr;
    FinishFn           Finish           = nullptr;
    MemoryBarrierFn    MemoryBarrier    = nullptr;
    GetIntegeriVFn     GetIntegeriV     = nullptr;
    GetIntegervFn      GetIntegerv      = nullptr;

    bool Load(NkOpenGLComputeContext::ProcAddressLoader loader) {
        if (!loader) return false;
        GenBuffers        = reinterpret_cast<GenBuffersFn>(loader("glGenBuffers"));
        BindBuffer        = reinterpret_cast<BindBufferFn>(loader("glBindBuffer"));
        BufferData        = reinterpret_cast<BufferDataFn>(loader("glBufferData"));
        DeleteBuffers     = reinterpret_cast<DeleteBuffersFn>(loader("glDeleteBuffers"));
        BufferSubData     = reinterpret_cast<BufferSubDataFn>(loader("glBufferSubData"));
        MapBufferRange    = reinterpret_cast<MapBufferRangeFn>(loader("glMapBufferRange"));
        UnmapBuffer       = reinterpret_cast<UnmapBufferFn>(loader("glUnmapBuffer"));
        CreateShader      = reinterpret_cast<CreateShaderFn>(loader("glCreateShader"));
        ShaderSource      = reinterpret_cast<ShaderSourceFn>(loader("glShaderSource"));
        CompileShader     = reinterpret_cast<CompileShaderFn>(loader("glCompileShader"));
        GetShaderiv       = reinterpret_cast<GetShaderivFn>(loader("glGetShaderiv"));
        GetShaderInfoLog  = reinterpret_cast<GetShaderInfoLogFn>(loader("glGetShaderInfoLog"));
        DeleteShader      = reinterpret_cast<DeleteShaderFn>(loader("glDeleteShader"));
        CreateProgram     = reinterpret_cast<CreateProgramFn>(loader("glCreateProgram"));
        AttachShader      = reinterpret_cast<AttachShaderFn>(loader("glAttachShader"));
        LinkProgram       = reinterpret_cast<LinkProgramFn>(loader("glLinkProgram"));
        GetProgramiv      = reinterpret_cast<GetProgramivFn>(loader("glGetProgramiv"));
        GetProgramInfoLog = reinterpret_cast<GetProgramInfoLogFn>(loader("glGetProgramInfoLog"));
        DeleteProgram     = reinterpret_cast<DeleteProgramFn>(loader("glDeleteProgram"));
        UseProgram        = reinterpret_cast<UseProgramFn>(loader("glUseProgram"));
        BindBufferBase    = reinterpret_cast<BindBufferBaseFn>(loader("glBindBufferBase"));
        DispatchCompute   = reinterpret_cast<DispatchComputeFn>(loader("glDispatchCompute"));
        Finish            = reinterpret_cast<FinishFn>(loader("glFinish"));
        MemoryBarrier     = reinterpret_cast<MemoryBarrierFn>(loader("glMemoryBarrier"));
        GetIntegeriV      = reinterpret_cast<GetIntegeriVFn>(loader("glGetIntegeri_v"));
        GetIntegerv       = reinterpret_cast<GetIntegervFn>(loader("glGetIntegerv"));

        return GenBuffers && BindBuffer && BufferData && DeleteBuffers &&
               BufferSubData && MapBufferRange && UnmapBuffer && CreateShader &&
               ShaderSource && CompileShader && GetShaderiv && GetShaderInfoLog &&
               DeleteShader && CreateProgram && AttachShader && LinkProgram &&
               GetProgramiv && GetProgramInfoLog && DeleteProgram && UseProgram &&
               BindBufferBase && DispatchCompute && Finish && MemoryBarrier &&
               GetIntegeriV && GetIntegerv;
    }
};

#else

struct NkOpenGLComputeContext::Functions {};

#endif // NK_NO_GLAD2

static bool ReadTextFile(const char* path, NkVector<char>& outText) {
    if (!path || !*path) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    const long fileSize = ftell(file);
    if (fileSize < 0) {
        fclose(file);
        return false;
    }
    rewind(file);

    outText.Resize((NkVector<char>::SizeType)fileSize + 1, '\0');
    const size_t readBytes = fread(outText.Data(), 1, (size_t)fileSize, file);
    fclose(file);
    if (readBytes != (size_t)fileSize) {
        outText.Clear();
        return false;
    }
    outText[(NkVector<char>::SizeType)fileSize] = '\0';
    return true;
}

NkOpenGLComputeContext::~NkOpenGLComputeContext() {
    if (mIsValid) {
        Shutdown();
    } else if (mFns) {
        delete mFns;
        mFns = nullptr;
    }
}

bool NkOpenGLComputeContext::SetProcAddressLoader(ProcAddressLoader loader) {
#ifdef NK_NO_GLAD2
    if (!loader) {
        return false;
    }
    if (!mFns) {
        mFns = new Functions();
    }
    if (!mFns->Load(loader)) {
        NK_CGL_ERR("Failed to load required OpenGL compute entry points");
        return false;
    }
    return true;
#else
    (void)loader;
    return true;
#endif
}

void NkOpenGLComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) return;
    mApi = gfx->GetApi();
    mOwnsContext = false;

#ifdef NK_NO_GLAD2
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(gfx);
    if (!SetProcAddressLoader(loader)) {
        mIsValid = false;
        NK_CGL_ERR("InitFromGraphicsContext failed: no usable GL loader from context");
        return;
    }
#endif

    mIsValid = true;
    NK_CGL_LOG("Initialized from graphics context (%s)\n", NkGraphicsApiName(mApi));
}

bool NkOpenGLComputeContext::Init(const NkContextDesc&) {
    NK_CGL_ERR("Standalone GL compute not implemented, use InitFromGraphicsContext\n");
    mIsValid = false;
    return false;
}

void NkOpenGLComputeContext::Shutdown() {
    mIsValid = false;
    mCurrentProgram = 0;
    if (mFns) {
        delete mFns;
        mFns = nullptr;
    }
    NK_CGL_LOG("Shutdown\n");
}

bool NkOpenGLComputeContext::IsValid() const { return mIsValid; }

NkComputeBuffer NkOpenGLComputeContext::CreateBuffer(const NkComputeBufferDesc& desc) {
    NkComputeBuffer buf;
    if (!mIsValid || desc.sizeBytes == 0) return buf;

    GLuint ssbo = 0;
#ifdef NK_NO_GLAD2
    mFns->GenBuffers(1, &ssbo);
    mFns->BindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
#else
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
#endif

    GLenum usage = GL_DYNAMIC_COPY;
    if (desc.cpuReadable && desc.cpuWritable) usage = GL_DYNAMIC_COPY;
    else if (desc.cpuWritable)               usage = GL_DYNAMIC_DRAW;
    else if (desc.cpuReadable)               usage = GL_DYNAMIC_READ;
    else                                     usage = GL_STATIC_DRAW;

#ifdef NK_NO_GLAD2
    mFns->BufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);
    mFns->BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#else
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#endif

    buf.handle    = (void*)(uintptr_t)ssbo;
    buf.sizeBytes = desc.sizeBytes;
    buf.valid     = true;
    return buf;
}

void NkOpenGLComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) return;
    GLuint ssbo = (GLuint)(uintptr_t)buf.handle;
#ifdef NK_NO_GLAD2
    mFns->DeleteBuffers(1, &ssbo);
#else
    glDeleteBuffers(1, &ssbo);
#endif
    buf = NkComputeBuffer{};
}

bool NkOpenGLComputeContext::WriteBuffer(NkComputeBuffer& buf, const void* data, uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) return false;
    GLuint ssbo = (GLuint)(uintptr_t)buf.handle;
#ifdef NK_NO_GLAD2
    mFns->BindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    mFns->BufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)offset, (GLsizeiptr)bytes, data);
    mFns->BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#else
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)offset, (GLsizeiptr)bytes, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#endif
    return true;
}

bool NkOpenGLComputeContext::ReadBuffer(const NkComputeBuffer& buf, void* outData, uint64 bytes, uint64 offset) {
    if (!buf.valid || !outData) return false;
    GLuint ssbo = (GLuint)(uintptr_t)buf.handle;
#ifdef NK_NO_GLAD2
    mFns->BindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    void* ptr = mFns->MapBufferRange(GL_SHADER_STORAGE_BUFFER, (GLintptr)offset, (GLsizeiptr)bytes, GL_MAP_READ_BIT);
#else
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, (GLintptr)offset, (GLsizeiptr)bytes, GL_MAP_READ_BIT);
#endif
    if (!ptr) return false;
    memcpy(outData, ptr, (size_t)bytes);
#ifdef NK_NO_GLAD2
    mFns->UnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    mFns->BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#else
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#endif
    return true;
}

NkComputeShader NkOpenGLComputeContext::CreateShaderFromSource(const char* source, const char* /*entry*/) {
    NkComputeShader s;
    if (!source) return s;

#ifdef NK_NO_GLAD2
    GLuint shader = mFns->CreateShader(GL_COMPUTE_SHADER);
    mFns->ShaderSource(shader, 1, &source, nullptr);
    mFns->CompileShader(shader);
#else
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
#endif

    GLint ok = 0;
#ifdef NK_NO_GLAD2
    mFns->GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
#else
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
#endif
    if (!ok) {
        GLint len = 0;
#ifdef NK_NO_GLAD2
        mFns->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
#else
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
#endif
        char* log = new char[len + 1];
#ifdef NK_NO_GLAD2
        mFns->GetShaderInfoLog(shader, len, nullptr, log);
#else
        glGetShaderInfoLog(shader, len, nullptr, log);
#endif
        NK_CGL_ERR("Compile error:\n%s\n", log);
        delete[] log;
#ifdef NK_NO_GLAD2
        mFns->DeleteShader(shader);
#else
        glDeleteShader(shader);
#endif
        return s;
    }

    s.handle = (void*)(uintptr_t)shader;
    s.valid  = true;
    return s;
}

NkComputeShader NkOpenGLComputeContext::CreateShaderFromFile(const char* path, const char* entry) {
    NkVector<char> src;
    if (!ReadTextFile(path, src)) {
        NK_CGL_ERR("Cannot open: %s\n", path);
        return {};
    }
    return CreateShaderFromSource(src.Data(), entry);
}

void NkOpenGLComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) return;
#ifdef NK_NO_GLAD2
    mFns->DeleteShader((GLuint)(uintptr_t)s.handle);
#else
    glDeleteShader((GLuint)(uintptr_t)s.handle);
#endif
    s = NkComputeShader{};
}

NkComputePipeline NkOpenGLComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline p;
    if (!s.valid) return p;

#ifdef NK_NO_GLAD2
    GLuint prog = mFns->CreateProgram();
    mFns->AttachShader(prog, (GLuint)(uintptr_t)s.handle);
    mFns->LinkProgram(prog);
#else
    GLuint prog = glCreateProgram();
    glAttachShader(prog, (GLuint)(uintptr_t)s.handle);
    glLinkProgram(prog);
#endif

    GLint ok = 0;
#ifdef NK_NO_GLAD2
    mFns->GetProgramiv(prog, GL_LINK_STATUS, &ok);
#else
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
#endif
    if (!ok) {
        GLint len = 0;
#ifdef NK_NO_GLAD2
        mFns->GetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
#else
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
#endif
        char* log = new char[len + 1];
#ifdef NK_NO_GLAD2
        mFns->GetProgramInfoLog(prog, len, nullptr, log);
#else
        glGetProgramInfoLog(prog, len, nullptr, log);
#endif
        NK_CGL_ERR("Link error:\n%s\n", log);
        delete[] log;
#ifdef NK_NO_GLAD2
        mFns->DeleteProgram(prog);
#else
        glDeleteProgram(prog);
#endif
        return p;
    }

    p.handle = (void*)(uintptr_t)prog;
    p.valid  = true;
    return p;
}

void NkOpenGLComputeContext::DestroyPipeline(NkComputePipeline& p) {
    if (!p.valid) return;
#ifdef NK_NO_GLAD2
    mFns->DeleteProgram((GLuint)(uintptr_t)p.handle);
#else
    glDeleteProgram((GLuint)(uintptr_t)p.handle);
#endif
    p = NkComputePipeline{};
}

void NkOpenGLComputeContext::BindPipeline(const NkComputePipeline& p) {
    if (!p.valid) return;
    mCurrentProgram = (GLuint)(uintptr_t)p.handle;
#ifdef NK_NO_GLAD2
    mFns->UseProgram(mCurrentProgram);
#else
    glUseProgram(mCurrentProgram);
#endif
}

void NkOpenGLComputeContext::BindBuffer(uint32 slot, NkComputeBuffer& buf) {
    if (!buf.valid) return;
#ifdef NK_NO_GLAD2
    mFns->BindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, (GLuint)(uintptr_t)buf.handle);
#else
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, (GLuint)(uintptr_t)buf.handle);
#endif
}

void NkOpenGLComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
#ifdef NK_NO_GLAD2
    mFns->DispatchCompute(gx, gy, gz);
#else
    glDispatchCompute(gx, gy, gz);
#endif
}

void NkOpenGLComputeContext::WaitIdle() {
#ifdef NK_NO_GLAD2
    mFns->Finish();
#else
    glFinish();
#endif
}

void NkOpenGLComputeContext::MemoryBarrier() {
#ifdef NK_NO_GLAD2
    mFns->MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
#else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
#endif
}

NkGraphicsApi NkOpenGLComputeContext::GetApi() const { return mApi; }

uint32 NkOpenGLComputeContext::GetMaxGroupSizeX() const {
    GLint v = 0;
#ifdef NK_NO_GLAD2
    mFns->GetIntegeriV(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &v);
#else
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &v);
#endif
    return (uint32)v;
}

uint32 NkOpenGLComputeContext::GetMaxGroupSizeY() const {
    GLint v = 0;
#ifdef NK_NO_GLAD2
    mFns->GetIntegeriV(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &v);
#else
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &v);
#endif
    return (uint32)v;
}

uint32 NkOpenGLComputeContext::GetMaxGroupSizeZ() const {
    GLint v = 0;
#ifdef NK_NO_GLAD2
    mFns->GetIntegeriV(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &v);
#else
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &v);
#endif
    return (uint32)v;
}

uint64 NkOpenGLComputeContext::GetSharedMemoryBytes() const {
    GLint v = 0;
#ifdef NK_NO_GLAD2
    mFns->GetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &v);
#else
    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &v);
#endif
    return (uint64)v;
}

bool NkOpenGLComputeContext::SupportsAtomics() const { return mIsValid; }
bool NkOpenGLComputeContext::SupportsFloat64() const { return false; }

} // namespace nkentseu

