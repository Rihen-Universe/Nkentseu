#pragma once
#include "shapes.h"
#include "broadphase.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  GPU backend selection
// ─────────────────────────────────────────────────────────────────────────────
enum class GPUBackend {
    None,        // CPU only
    CUDA,
    OpenGLCompute,
    VulkanCompute,
    DirectX11,
    DirectX12,
    WebGPU,
    Auto         // picks best available
};

// ─────────────────────────────────────────────────────────────────────────────
//  GPU buffer — backend-agnostic handle
// ─────────────────────────────────────────────────────────────────────────────
struct GPUBuffer {
    void*    handle    = nullptr;  // backend-specific (cudaDevicePtr, VkBuffer, etc.)
    size_t   byteSize  = 0;
    GPUBackend backend = GPUBackend::None;
    bool     mapped    = false;

    bool valid() const { return handle!=nullptr; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  GPU data layout for AABB broadphase
//  Each body occupies one GPUBodyAABB in a flat buffer
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(16) GPUBodyAABB {
    float minX, minY, minZ, _pad0;
    float maxX, maxY, maxZ, _pad1;
    uint64_t bodyId;
    uint32_t _pad2[2];
};

// Result of GPU broadphase — pair indices
struct alignas(8) GPUPair {
    uint32_t idxA;
    uint32_t idxB;
};

// Result of GPU narrowphase contact
struct alignas(16) GPUContact {
    float    posAx, posAy, posAz, _p0;
    float    posBx, posBy, posBz, _p1;
    float    normX, normY, normZ, depth;
    uint64_t bodyA;
    uint64_t bodyB;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ICollisionBackend — pure virtual interface
//  All backends implement this; CollisionWorld holds one pointer
// ─────────────────────────────────────────────────────────────────────────────
class ICollisionBackend {
public:
    virtual ~ICollisionBackend() = default;

    virtual GPUBackend type()     const = 0;
    virtual bool       isGPU()    const { return type()!=GPUBackend::None; }
    virtual std::string name()    const = 0;

    // ── Buffer management ────────────────────────────────────────────────────
    virtual GPUBuffer createBuffer(size_t byteSize, bool readback=false) = 0;
    virtual void      destroyBuffer(GPUBuffer& buf) = 0;
    virtual void      uploadBuffer(GPUBuffer& dst, const void* src, size_t bytes) = 0;
    virtual void      downloadBuffer(void* dst, const GPUBuffer& src, size_t bytes) = 0;

    // ── Broadphase on GPU ────────────────────────────────────────────────────
    // Upload n AABBs, run parallel AABB-AABB test, write pairs into pairsOut
    // Returns number of pairs found
    virtual uint32_t gpuBroadphase(
        const GPUBuffer& aabbsIn,    // array of GPUBodyAABB
        uint32_t         count,
        GPUBuffer&       pairsOut,   // array of GPUPair (preallocated)
        uint32_t         maxPairs) = 0;

    // ── Narrowphase on GPU ───────────────────────────────────────────────────
    // For each pair, test detailed collision and write contact to contactsOut
    virtual uint32_t gpuNarrowphase(
        const GPUBuffer& aabbsIn,
        const GPUBuffer& pairsIn,
        uint32_t         pairCount,
        GPUBuffer&       contactsOut,
        uint32_t         maxContacts) = 0;

    // ── Synchronization ──────────────────────────────────────────────────────
    virtual void sync() = 0;       // wait for all GPU work to finish
    virtual void flush() = 0;      // submit pending commands

    // ── Capabilities ─────────────────────────────────────────────────────────
    virtual size_t   maxBufferSize()     const = 0;
    virtual uint32_t warpSize()          const = 0;  // 32 for NVIDIA, 64 for AMD
    virtual uint32_t maxWorkgroupSize()  const = 0;
    virtual bool     supportsAtomics()   const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  CPU Backend (fallback, always available)
// ─────────────────────────────────────────────────────────────────────────────
class CPUBackend : public ICollisionBackend {
public:
    GPUBackend  type()  const override { return GPUBackend::None; }
    bool        isGPU() const override { return false; }
    std::string name()  const override { return "CPU"; }

    GPUBuffer createBuffer(size_t bytes, bool) override {
        GPUBuffer b; b.byteSize=bytes; b.backend=GPUBackend::None;
        b.handle = std::malloc(bytes);
        return b;
    }
    void destroyBuffer(GPUBuffer& b) override {
        if(b.handle) std::free(b.handle);
        b.handle=nullptr;
    }
    void uploadBuffer(GPUBuffer& dst, const void* src, size_t bytes) override {
        std::memcpy(dst.handle, src, bytes);
    }
    void downloadBuffer(void* dst, const GPUBuffer& src, size_t bytes) override {
        std::memcpy(dst, src.handle, bytes);
    }

    uint32_t gpuBroadphase(
        const GPUBuffer& aabbsIn, uint32_t count,
        GPUBuffer& pairsOut, uint32_t maxPairs) override
    {
        auto* aabbs = (const GPUBodyAABB*)aabbsIn.handle;
        auto* pairs = (GPUPair*)pairsOut.handle;
        uint32_t pairCount = 0;

        // O(n²) brute force — replaced by BVH on real GPU path
        for(uint32_t i=0;i<count && pairCount<maxPairs;i++) {
            for(uint32_t j=i+1;j<count && pairCount<maxPairs;j++) {
                const auto& a=aabbs[i]; const auto& b=aabbs[j];
                if(a.minX<=b.maxX && a.maxX>=b.minX &&
                   a.minY<=b.maxY && a.maxY>=b.minY &&
                   a.minZ<=b.maxZ && a.maxZ>=b.minZ)
                {
                    pairs[pairCount++]={i,j};
                }
            }
        }
        return pairCount;
    }

    uint32_t gpuNarrowphase(
        const GPUBuffer& aabbsIn, const GPUBuffer& pairsIn,
        uint32_t pairCount, GPUBuffer& contactsOut, uint32_t maxContacts) override
    {
        (void)aabbsIn; (void)pairsIn; (void)pairCount;
        (void)contactsOut; (void)maxContacts;
        return 0; // delegated to CPU narrowphase in CollisionWorld
    }

    void     sync()  override {}
    void     flush() override {}
    size_t   maxBufferSize()    const override { return SIZE_MAX; }
    uint32_t warpSize()         const override { return 1; }
    uint32_t maxWorkgroupSize() const override { return 1; }
    bool     supportsAtomics()  const override { return true; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  CUDA Backend stub (requires CUDA SDK)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef COL_ENABLE_CUDA
#include <cuda_runtime.h>

class CUDABackend : public ICollisionBackend {
public:
    CUDABackend(int deviceIndex=0) {
        cudaSetDevice(deviceIndex);
        cudaGetDeviceProperties(&props_, deviceIndex);
    }

    GPUBackend  type()  const override { return GPUBackend::CUDA; }
    std::string name()  const override { return std::string("CUDA:") + props_.name; }

    GPUBuffer createBuffer(size_t bytes, bool readback) override {
        GPUBuffer b; b.byteSize=bytes; b.backend=GPUBackend::CUDA;
        if(readback) {
            cudaMallocManaged(&b.handle, bytes);  // unified memory for easy readback
        } else {
            cudaMalloc(&b.handle, bytes);
        }
        return b;
    }
    void destroyBuffer(GPUBuffer& b) override {
        if(b.handle) { cudaFree(b.handle); b.handle=nullptr; }
    }
    void uploadBuffer(GPUBuffer& dst, const void* src, size_t bytes) override {
        cudaMemcpy(dst.handle, src, bytes, cudaMemcpyHostToDevice);
    }
    void downloadBuffer(void* dst, const GPUBuffer& src, size_t bytes) override {
        cudaMemcpy(dst, src.handle, bytes, cudaMemcpyDeviceToHost);
    }

    uint32_t gpuBroadphase(
        const GPUBuffer& aabbsIn, uint32_t count,
        GPUBuffer& pairsOut, uint32_t maxPairs) override
    {
        // Launch broadphase kernel (defined in cuda_kernels.cu)
        return launchBroadphaseKernel(
            (const GPUBodyAABB*)aabbsIn.handle, count,
            (GPUPair*)pairsOut.handle, maxPairs);
    }

    uint32_t gpuNarrowphase(
        const GPUBuffer& aabbsIn, const GPUBuffer& pairsIn,
        uint32_t pairCount, GPUBuffer& contactsOut, uint32_t maxContacts) override
    {
        return launchNarrowphaseKernel(
            (const GPUBodyAABB*)aabbsIn.handle,
            (const GPUPair*)pairsIn.handle, pairCount,
            (GPUContact*)contactsOut.handle, maxContacts);
    }

    void sync()  override { cudaDeviceSynchronize(); }
    void flush() override {}

    size_t   maxBufferSize()    const override { return props_.totalGlobalMem; }
    uint32_t warpSize()         const override { return 32; }
    uint32_t maxWorkgroupSize() const override { return (uint32_t)props_.maxThreadsPerBlock; }
    bool     supportsAtomics()  const override { return true; }

private:
    cudaDeviceProp props_;
    // Declared here, defined in cuda_kernels.cu
    uint32_t launchBroadphaseKernel(const GPUBodyAABB* aabbs, uint32_t count,
                                    GPUPair* pairs, uint32_t maxPairs);
    uint32_t launchNarrowphaseKernel(const GPUBodyAABB* aabbs,
                                     const GPUPair* pairs, uint32_t pairCount,
                                     GPUContact* contacts, uint32_t maxContacts);
};
#endif // COL_ENABLE_CUDA

// ─────────────────────────────────────────────────────────────────────────────
//  OpenGL Compute Backend
// ─────────────────────────────────────────────────────────────────────────────
#ifdef COL_ENABLE_OPENGL
#include <GL/glew.h>

class OpenGLComputeBackend : public ICollisionBackend {
public:
    OpenGLComputeBackend() { initShaders(); }
    ~OpenGLComputeBackend() override { cleanup(); }

    GPUBackend  type()  const override { return GPUBackend::OpenGLCompute; }
    std::string name()  const override { return "OpenGL Compute"; }

    GPUBuffer createBuffer(size_t bytes, bool) override {
        GPUBuffer b; b.byteSize=bytes; b.backend=GPUBackend::OpenGLCompute;
        GLuint id; glGenBuffers(1,&id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        b.handle = (void*)(uintptr_t)id;
        return b;
    }
    void destroyBuffer(GPUBuffer& b) override {
        GLuint id=(GLuint)(uintptr_t)b.handle;
        glDeleteBuffers(1,&id); b.handle=nullptr;
    }
    void uploadBuffer(GPUBuffer& dst, const void* src, size_t bytes) override {
        GLuint id=(GLuint)(uintptr_t)dst.handle;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, src);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    void downloadBuffer(void* dst, const GPUBuffer& src, size_t bytes) override {
        GLuint id=(GLuint)(uintptr_t)src.handle;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bytes, GL_MAP_READ_BIT);
        if(ptr) { std::memcpy(dst, ptr, bytes); glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    uint32_t gpuBroadphase(
        const GPUBuffer& aabbsIn, uint32_t count,
        GPUBuffer& pairsOut, uint32_t maxPairs) override
    {
        glUseProgram(broadphaseProgram_);
        glUniform1ui(glGetUniformLocation(broadphaseProgram_,"uCount"), count);
        glUniform1ui(glGetUniformLocation(broadphaseProgram_,"uMaxPairs"), maxPairs);

        GLuint aabbId=(GLuint)(uintptr_t)aabbsIn.handle;
        GLuint pairId=(GLuint)(uintptr_t)pairsOut.handle;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, aabbId);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pairId);

        // Atomic counter for pair count
        GLuint counterBuf;
        glGenBuffers(1,&counterBuf);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterBuf);
        GLuint zero=0;
        glBufferData(GL_ATOMIC_COUNTER_BUFFER,4,&zero,GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counterBuf);

        uint32_t groups = (count+63)/64;
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_ATOMIC_COUNTER_BARRIER_BIT);

        GLuint result=0;
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 4, &result);
        glDeleteBuffers(1,&counterBuf);
        glUseProgram(0);
        return result;
    }

    uint32_t gpuNarrowphase(
        const GPUBuffer& aabbsIn, const GPUBuffer& pairsIn,
        uint32_t pairCount, GPUBuffer& contactsOut, uint32_t maxContacts) override
    {
        (void)aabbsIn; (void)pairsIn; (void)pairCount;
        (void)contactsOut; (void)maxContacts;
        return 0;
    }

    void sync()  override { glFinish(); }
    void flush() override { glFlush(); }

    size_t   maxBufferSize()    const override { GLint64 v; glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&v); return v; }
    uint32_t warpSize()         const override { return 32; }
    uint32_t maxWorkgroupSize() const override { GLint v; glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,0,&v); return v; }
    bool     supportsAtomics()  const override { return true; }

private:
    GLuint broadphaseProgram_ = 0;
    GLuint narrowphaseProgram_= 0;

    void initShaders();  // defined in opengl_kernels.cpp
    void cleanup() {
        if(broadphaseProgram_)  glDeleteProgram(broadphaseProgram_);
        if(narrowphaseProgram_) glDeleteProgram(narrowphaseProgram_);
    }
};
#endif // COL_ENABLE_OPENGL

// ─────────────────────────────────────────────────────────────────────────────
//  Backend factory — auto-selects best available
// ─────────────────────────────────────────────────────────────────────────────
class BackendFactory {
public:
    static std::unique_ptr<ICollisionBackend> create(GPUBackend requested) {
        switch(requested) {
#ifdef COL_ENABLE_CUDA
        case GPUBackend::CUDA:
            return std::make_unique<CUDABackend>();
#endif
#ifdef COL_ENABLE_OPENGL
        case GPUBackend::OpenGLCompute:
            return std::make_unique<OpenGLComputeBackend>();
#endif
        case GPUBackend::Auto:
            return createBest();
        default:
            return std::make_unique<CPUBackend>();
        }
    }

    static std::unique_ptr<ICollisionBackend> createBest() {
#ifdef COL_ENABLE_CUDA
        int deviceCount=0;
        if(cudaGetDeviceCount(&deviceCount)==cudaSuccess && deviceCount>0)
            return std::make_unique<CUDABackend>();
#endif
#ifdef COL_ENABLE_OPENGL
        return std::make_unique<OpenGLComputeBackend>();
#endif
        return std::make_unique<CPUBackend>();
    }
};

} // namespace col
