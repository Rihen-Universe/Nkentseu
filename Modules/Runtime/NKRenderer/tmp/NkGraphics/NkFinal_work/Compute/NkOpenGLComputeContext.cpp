// =============================================================================
// NkOpenGLComputeContext.cpp
// Compute via GL_ARB_compute_shader (GL 4.3+)
// Les buffers sont des Shader Storage Buffer Objects (SSBO).
// =============================================================================
#include "NkOpenGLComputeContext.h"

// GLAD2 ou headers natifs GL
#ifndef NK_NO_GLAD2
#   include <glad/gl.h>
#else
#   include <GL/gl.h>
#   include <GL/glext.h>
#endif

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#define NK_CGL_LOG(...) printf("[NkGLCompute] " __VA_ARGS__)
#define NK_CGL_ERR(...) printf("[NkGLCompute][ERROR] " __VA_ARGS__)

namespace nkentseu {

NkOpenGLComputeContext::~NkOpenGLComputeContext() { if (mIsValid) Shutdown(); }

void NkOpenGLComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) return;
    mApi      = gfx->GetApi();
    mIsValid  = true;
    mOwnsContext = false;
    NK_CGL_LOG("Initialized from graphics context (%s)\n",
               NkGraphicsApiName(mApi));
}

bool NkOpenGLComputeContext::Init(const NkContextDesc&) {
    // Standalone : nécessiterait un pbuffer/offscreen context
    // Pour l'usage courant, utiliser InitFromGraphicsContext
    NK_CGL_ERR("Standalone GL compute not implemented — use InitFromGraphicsContext\n");
    return false;
}

void NkOpenGLComputeContext::Shutdown() {
    // Les ressources individuelles (buffers, shaders) doivent être détruits
    // avant Shutdown via DestroyBuffer/DestroyShader/DestroyPipeline.
    mIsValid = false;
    NK_CGL_LOG("Shutdown\n");
}

bool NkOpenGLComputeContext::IsValid() const { return mIsValid; }

// ── Buffers (SSBO) ────────────────────────────────────────────────────────────
NkComputeBuffer NkOpenGLComputeContext::CreateBuffer(const NkComputeBufferDesc& desc) {
    NkComputeBuffer buf;
    if (!mIsValid || desc.sizeBytes == 0) return buf;

    GLuint ssbo = 0;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

    GLenum usage = GL_DYNAMIC_COPY;
    if (desc.cpuReadable && desc.cpuWritable) usage = GL_DYNAMIC_COPY;
    else if (desc.cpuWritable)               usage = GL_DYNAMIC_DRAW;
    else if (desc.cpuReadable)               usage = GL_DYNAMIC_READ;
    else                                     usage = GL_STATIC_DRAW;

    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)desc.sizeBytes,
                 desc.initialData, usage);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    buf.handle    = (void*)(uintptr_t)ssbo;
    buf.sizeBytes = desc.sizeBytes;
    buf.valid     = true;
    return buf;
}

void NkOpenGLComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) return;
    GLuint ssbo = (GLuint)(uintptr_t)buf.handle;
    glDeleteBuffers(1, &ssbo);
    buf = NkComputeBuffer{};
}

bool NkOpenGLComputeContext::WriteBuffer(NkComputeBuffer& buf,
    const void* data, uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) return false;
    GLuint ssbo = (GLuint)(uintptr_t)buf.handle;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    (GLintptr)offset, (GLsizeiptr)bytes, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return true;
}

bool NkOpenGLComputeContext::ReadBuffer(const NkComputeBuffer& buf,
    void* outData, uint64 bytes, uint64 offset) {
    if (!buf.valid || !outData) return false;
    GLuint ssbo = (GLuint)(uintptr_t)buf.handle;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                 (GLintptr)offset, (GLsizeiptr)bytes,
                                 GL_MAP_READ_BIT);
    if (!ptr) return false;
    memcpy(outData, ptr, (size_t)bytes);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return true;
}

// ── Shaders ───────────────────────────────────────────────────────────────────
NkComputeShader NkOpenGLComputeContext::CreateShaderFromSource(
    const char* source, const char* /*entry*/) {
    NkComputeShader s;
    if (!source) return s;

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* log = new char[len+1];
        glGetShaderInfoLog(shader, len, nullptr, log);
        NK_CGL_ERR("Compile error:\n%s\n", log);
        delete[] log;
        glDeleteShader(shader);
        return s;
    }
    s.handle = (void*)(uintptr_t)shader;
    s.valid  = true;
    return s;
}

NkComputeShader NkOpenGLComputeContext::CreateShaderFromFile(
    const char* path, const char* entry) {
    std::ifstream f(path);
    if (!f.is_open()) { NK_CGL_ERR("Cannot open: %s\n", path); return {}; }
    std::stringstream ss; ss << f.rdbuf();
    return CreateShaderFromSource(ss.str().c_str(), entry);
}

void NkOpenGLComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) return;
    glDeleteShader((GLuint)(uintptr_t)s.handle);
    s = NkComputeShader{};
}

// ── Pipeline (program GL) ─────────────────────────────────────────────────────
NkComputePipeline NkOpenGLComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline p;
    if (!s.valid) return p;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, (GLuint)(uintptr_t)s.handle);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        char* log = new char[len+1];
        glGetProgramInfoLog(prog, len, nullptr, log);
        NK_CGL_ERR("Link error:\n%s\n", log);
        delete[] log;
        glDeleteProgram(prog);
        return p;
    }
    p.handle = (void*)(uintptr_t)prog;
    p.valid  = true;
    return p;
}

void NkOpenGLComputeContext::DestroyPipeline(NkComputePipeline& p) {
    if (!p.valid) return;
    glDeleteProgram((GLuint)(uintptr_t)p.handle);
    p = NkComputePipeline{};
}

// ── Dispatch ──────────────────────────────────────────────────────────────────
void NkOpenGLComputeContext::BindPipeline(const NkComputePipeline& p) {
    if (!p.valid) return;
    mCurrentProgram = (GLuint)(uintptr_t)p.handle;
    glUseProgram(mCurrentProgram);
}

void NkOpenGLComputeContext::BindBuffer(uint32 slot, NkComputeBuffer& buf) {
    if (!buf.valid) return;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot,
                     (GLuint)(uintptr_t)buf.handle);
}

void NkOpenGLComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    glDispatchCompute(gx, gy, gz);
}

// ── Synchronisation ───────────────────────────────────────────────────────────
void NkOpenGLComputeContext::WaitIdle() {
    glFinish();
}

void NkOpenGLComputeContext::MemoryBarrier() {
    // Barrière SSBO : protège les lectures après dispatch
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                    GL_BUFFER_UPDATE_BARRIER_BIT);
}

// ── Capacités ─────────────────────────────────────────────────────────────────
NkGraphicsApi NkOpenGLComputeContext::GetApi() const { return mApi; }

uint32 NkOpenGLComputeContext::GetMaxGroupSizeX() const {
    GLint v=0; glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &v); return (uint32)v;
}
uint32 NkOpenGLComputeContext::GetMaxGroupSizeY() const {
    GLint v=0; glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &v); return (uint32)v;
}
uint32 NkOpenGLComputeContext::GetMaxGroupSizeZ() const {
    GLint v=0; glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &v); return (uint32)v;
}
uint64 NkOpenGLComputeContext::GetSharedMemoryBytes() const {
    GLint v=0; glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &v); return (uint64)v;
}
bool NkOpenGLComputeContext::SupportsAtomics() const { return true; } // GL 4.3+
bool NkOpenGLComputeContext::SupportsFloat64() const {
    // GL_ARB_gpu_shader_fp64 requis
    return false; // conservative default
}

} // namespace nkentseu
