#pragma once
// =============================================================================
// NkMetalComputeContext.h — Compute Metal via MTLComputeCommandEncoder
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#include "NkIComputeContext.h"
#include "NKRenderer/Context/Core/NkIGraphicsContext.h"

namespace nkentseu {

    class NkMetalComputeContext final : public NkIComputeContext {
    public:
        NkMetalComputeContext()  = default;
        ~NkMetalComputeContext() override;

        bool Init(const NkContextDesc& desc);
        void InitFromGraphicsContext(NkIGraphicsContext* gfx);

        bool IsValid() const override;
        void Shutdown()      override;

        NkComputeBuffer  CreateBuffer (const NkComputeBufferDesc& d)           override;
        void             DestroyBuffer(NkComputeBuffer& buf)                   override;
        bool             WriteBuffer  (NkComputeBuffer& buf, const void* data,
                                       uint64 bytes, uint64 offset=0)          override;
        bool             ReadBuffer   (const NkComputeBuffer& buf, void* out,
                                       uint64 bytes, uint64 offset=0)          override;

        NkComputeShader   CreateShaderFromSource(const char* src,
                                                  const char* entry="computeMain") override;
        NkComputeShader   CreateShaderFromFile  (const char* path,
                                                  const char* entry="computeMain") override;
        void              DestroyShader(NkComputeShader& s)                    override;

        NkComputePipeline CreatePipeline (const NkComputeShader& s)            override;
        void              DestroyPipeline(NkComputePipeline& p)                override;

        void BindBuffer  (uint32 slot, NkComputeBuffer& buf)                   override;
        void BindPipeline(const NkComputePipeline& p)                          override;
        void Dispatch    (uint32 gx, uint32 gy=1, uint32 gz=1)                 override;

        void WaitIdle()      override;
        void MemoryBarrier() override;

        NkGraphicsApi GetApi()              const override;
        uint32        GetMaxGroupSizeX()    const override;
        uint32        GetMaxGroupSizeY()    const override;
        uint32        GetMaxGroupSizeZ()    const override;
        uint64        GetSharedMemoryBytes()const override;
        bool          SupportsAtomics()     const override;
        bool          SupportsFloat64()     const override;

    private:
        void*  mDevice       = nullptr; // id<MTLDevice>
        void*  mQueue        = nullptr; // id<MTLCommandQueue>
        void*  mCurrentPipeline = nullptr; // id<MTLComputePipelineState>
        bool   mIsValid      = false;
        bool   mOwnsDevice   = false;

        // Buffers bindés pour le prochain dispatch [slot → MTLBuffer*]
        void*  mBoundBuffers[16] = {};
        uint32 mBoundBufferCount = 0;
    };

} // namespace nkentseu

// =============================================================================
// NkMetalComputeContext.mm — Objective-C++ implementation
// =============================================================================
/*
CONTENU DE NkMetalComputeContext.mm à créer séparément :

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#include "NkMetalComputeContext.h"
#include "../Metal/NkMetalContext.h"
#import <Metal/Metal.h>
...

void NkMetalComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    auto* md = static_cast<NkMetalContextData*>(gfx->GetNativeContextData());
    mDevice = md->device;
    mQueue  = md->commandQueue;
    mIsValid = true;
}

NkComputeBuffer NkMetalComputeContext::CreateBuffer(const NkComputeBufferDesc& d) {
    id<MTLDevice> dev = (__bridge id<MTLDevice>)mDevice;
    MTLResourceOptions opts = d.cpuReadable
        ? MTLResourceStorageModeShared
        : MTLResourceStorageModePrivate;
    id<MTLBuffer> buf = [dev newBufferWithLength:d.sizeBytes options:opts];
    if (d.initialData && d.cpuWritable)
        memcpy(buf.contents, d.initialData, d.sizeBytes);
    NkComputeBuffer out;
    out.handle = (void*)CFBridgingRetain(buf);
    out.sizeBytes = d.sizeBytes;
    out.valid = true;
    return out;
}

void NkMetalComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    id<MTLCommandQueue> q    = (__bridge id<MTLCommandQueue>)mQueue;
    id<MTLCommandBuffer> cmdb = [q commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmdb computeCommandEncoder];
    id<MTLComputePipelineState> pso  = (__bridge id<MTLComputePipelineState>)mCurrentPipeline;
    [enc setComputePipelineState:pso];
    for (uint32 i = 0; i < mBoundBufferCount; ++i) {
        id<MTLBuffer> buf = (__bridge id<MTLBuffer>)mBoundBuffers[i];
        [enc setBuffer:buf offset:0 atIndex:i];
    }
    MTLSize tgSize = {pso.maxTotalThreadsPerThreadgroup, 1, 1};
    MTLSize grid   = {gx, gy, gz};
    [enc dispatchThreadgroups:grid threadsPerThreadgroup:tgSize];
    [enc endEncoding];
    [cmdb commit];
    [cmdb waitUntilCompleted];
}
#endif
*/

#endif // MACOS || IOS
