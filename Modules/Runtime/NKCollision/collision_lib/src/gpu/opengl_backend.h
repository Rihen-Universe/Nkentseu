#pragma once
// opengl_backend.h — OpenGL 4.5 Compute Shader backend
#ifdef COL_ENABLE_OPENGL
#include "../include/collision/gpu_backend.h"
#include <GL/glew.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  OpenGL helpers
// ─────────────────────────────────────────────────────────────────────────────
static GLuint compileComputeShader(const char* src) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        GLchar log[2048]; GLsizei len;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("GL compute shader error:\n") + log);
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) {
        GLchar log[2048]; GLsizei len;
        glGetProgramInfoLog(prog, sizeof(log), &len, log);
        glDeleteProgram(prog);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("GL program link error:\n") + log);
    }
    glDeleteShader(shader);
    return prog;
}

// ─────────────────────────────────────────────────────────────────────────────
//  OpenGLComputeBackendFull
// ─────────────────────────────────────────────────────────────────────────────
class OpenGLComputeBackendFull : public ICollisionBackend {
public:
    OpenGLComputeBackendFull() {
        // Requires GL context to be current before construction
        if(glewInit() != GLEW_OK)
            throw std::runtime_error("GLEW init failed");
        if(!GLEW_ARB_compute_shader)
            throw std::runtime_error("Compute shaders not supported");
        buildShaders();
        // Create persistent atomic counter buffer
        glGenBuffers(1, &counterBuf_);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterBuf_);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    }

    ~OpenGLComputeBackendFull() override {
        glDeleteProgram(broadphaseProgram_);
        glDeleteProgram(narrowphaseProgram_);
        glDeleteBuffers(1, &counterBuf_);
    }

    GPUBackend  type()  const override { return GPUBackend::OpenGLCompute; }
    std::string name()  const override { return "OpenGL Compute 4.5"; }

    // ── Buffers ──────────────────────────────────────────────────────────────
    GPUBuffer createBuffer(size_t bytes, bool) override {
        GLuint id;
        glGenBuffers(1, &id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes,
                     nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        GPUBuffer b;
        b.handle   = reinterpret_cast<void*>((uintptr_t)id);
        b.byteSize = bytes;
        b.backend  = GPUBackend::OpenGLCompute;
        return b;
    }

    void destroyBuffer(GPUBuffer& b) override {
        GLuint id = (GLuint)(uintptr_t)b.handle;
        glDeleteBuffers(1, &id);
        b.handle = nullptr;
    }

    void uploadBuffer(GPUBuffer& dst, const void* src, size_t bytes) override {
        GLuint id = (GLuint)(uintptr_t)dst.handle;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)bytes, src);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void downloadBuffer(void* dst, const GPUBuffer& src, size_t bytes) override {
        GLuint id = (GLuint)(uintptr_t)src.handle;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                     (GLsizeiptr)bytes, GL_MAP_READ_BIT);
        if(ptr) { std::memcpy(dst, ptr, bytes); glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    // ── Broadphase ───────────────────────────────────────────────────────────
    uint32_t gpuBroadphase(
        const GPUBuffer& aabbsIn, uint32_t count,
        GPUBuffer& pairsOut, uint32_t maxPairs) override
    {
        GLuint zero = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterBuf_);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

        glUseProgram(broadphaseProgram_);
        glUniform1ui(uBPCount_,    count);
        glUniform1ui(uBPMaxPairs_, maxPairs);

        GLuint aabbId = (GLuint)(uintptr_t)aabbsIn.handle;
        GLuint pairId = (GLuint)(uintptr_t)pairsOut.handle;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,  0, aabbId);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,  1, pairId);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER,  0, counterBuf_);

        GLuint groups = (count + 7) / 8;
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                        GL_ATOMIC_COUNTER_BARRIER_BIT);

        GLuint result = 0;
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &result);
        glUseProgram(0);
        return std::min(result, (GLuint)maxPairs);
    }

    // ── Narrowphase ──────────────────────────────────────────────────────────
    uint32_t gpuNarrowphase(
        const GPUBuffer& aabbsIn, const GPUBuffer& pairsIn,
        uint32_t pairCount, GPUBuffer& contactsOut, uint32_t maxContacts) override
    {
        if(pairCount == 0) return 0;

        GLuint zero = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterBuf_);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

        glUseProgram(narrowphaseProgram_);
        glUniform1ui(uNPPairCount_,    pairCount);
        glUniform1ui(uNPMaxContacts_,  maxContacts);

        GLuint aabbId    = (GLuint)(uintptr_t)aabbsIn.handle;
        GLuint pairId    = (GLuint)(uintptr_t)pairsIn.handle;
        GLuint contactId = (GLuint)(uintptr_t)contactsOut.handle;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, aabbId);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pairId);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, contactId);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counterBuf_);

        GLuint groups = (pairCount + 127) / 128;
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                        GL_ATOMIC_COUNTER_BARRIER_BIT);

        GLuint result = 0;
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &result);
        glUseProgram(0);
        return std::min(result, (GLuint)maxContacts);
    }

    void sync()  override { glFinish(); }
    void flush() override { glFlush(); }

    size_t maxBufferSize() const override {
        GLint64 v; glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &v); return (size_t)v;
    }
    uint32_t warpSize()         const override { return 32; }
    uint32_t maxWorkgroupSize() const override {
        GLint v; glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &v); return (uint32_t)v;
    }
    bool supportsAtomics() const override { return GLEW_ARB_shader_atomic_counters; }

private:
    GLuint broadphaseProgram_  = 0;
    GLuint narrowphaseProgram_ = 0;
    GLuint counterBuf_         = 0;

    // Cached uniform locations
    GLint uBPCount_=0, uBPMaxPairs_=0;
    GLint uNPPairCount_=0, uNPMaxContacts_=0;

    void buildShaders() {
        // ── Broadphase shader (inline GLSL) ──────────────────────────────────
        const char* broadphaseSrc = R"(
#version 450 core
layout(local_size_x=8, local_size_y=8, local_size_z=1) in;

struct BodyAABB { vec4 minPad; vec4 maxPad; uvec2 bodyId; uvec2 pad; };
struct Pair     { uint idxA; uint idxB; };

layout(std430, binding=0) readonly  buffer AABBs { BodyAABB aabbs[]; };
layout(std430, binding=1) writeonly buffer Pairs { Pair pairs[]; };
layout(binding=0, offset=0) uniform atomic_uint pairCounter;

uniform uint uCount;
uniform uint uMaxPairs;

void main() {
    uint i = gl_GlobalInvocationID.x;
    uint j = gl_GlobalInvocationID.y;
    if(i>=uCount || j>=uCount || j<=i) return;

    vec3 minA=aabbs[i].minPad.xyz, maxA=aabbs[i].maxPad.xyz;
    vec3 minB=aabbs[j].minPad.xyz, maxB=aabbs[j].maxPad.xyz;

    if(all(lessThanEqual(minA,maxB)) && all(greaterThanEqual(maxA,minB))) {
        uint idx = atomicCounterIncrement(pairCounter);
        if(idx < uMaxPairs) { pairs[idx].idxA=i; pairs[idx].idxB=j; }
    }
})";

        // ── Narrowphase shader (inline GLSL) ─────────────────────────────────
        const char* narrowphaseSrc = R"(
#version 450 core
layout(local_size_x=128, local_size_y=1, local_size_z=1) in;

struct BodyAABB { vec4 minPad; vec4 maxPad; uvec2 bodyId; uvec2 pad; };
struct Pair     { uint idxA; uint idxB; };
struct Contact  { vec4 posA; vec4 posB; vec4 normalDepth;
                  uvec2 bodyA; uvec2 bodyB; };

layout(std430, binding=0) readonly  buffer AABBs    { BodyAABB aabbs[]; };
layout(std430, binding=1) readonly  buffer PairBuf  { Pair pairs[]; };
layout(std430, binding=2) writeonly buffer Contacts { Contact contacts[]; };
layout(binding=0, offset=0) uniform atomic_uint contactCounter;

uniform uint uPairCount;
uniform uint uMaxContacts;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if(tid >= uPairCount) return;

    uint ia=pairs[tid].idxA, ib=pairs[tid].idxB;
    vec3 minA=aabbs[ia].minPad.xyz, maxA=aabbs[ia].maxPad.xyz;
    vec3 minB=aabbs[ib].minPad.xyz, maxB=aabbs[ib].maxPad.xyz;

    vec3 cA=(minA+maxA)*0.5, cB=(minB+maxB)*0.5;
    float rA=length(maxA-cA), rB=length(maxB-cB);
    vec3 d=cB-cA; float dist=length(d); float radSum=rA+rB;
    if(dist >= radSum) return;

    vec3  n     = dist>1e-6 ? d/dist : vec3(0,1,0);
    float depth = radSum-dist;

    uint idx = atomicCounterIncrement(contactCounter);
    if(idx >= uMaxContacts) return;

    contacts[idx].posA        = vec4(cA+n*rA, 0.0);
    contacts[idx].posB        = vec4(cB-n*rB, 0.0);
    contacts[idx].normalDepth = vec4(n, depth);
    contacts[idx].bodyA       = aabbs[ia].bodyId;
    contacts[idx].bodyB       = aabbs[ib].bodyId;
})";

        broadphaseProgram_  = compileComputeShader(broadphaseSrc);
        narrowphaseProgram_ = compileComputeShader(narrowphaseSrc);

        // Cache uniform locations
        uBPCount_       = glGetUniformLocation(broadphaseProgram_,  "uCount");
        uBPMaxPairs_    = glGetUniformLocation(broadphaseProgram_,  "uMaxPairs");
        uNPPairCount_   = glGetUniformLocation(narrowphaseProgram_, "uPairCount");
        uNPMaxContacts_ = glGetUniformLocation(narrowphaseProgram_, "uMaxContacts");
    }
};

} // namespace col
#endif // COL_ENABLE_OPENGL
