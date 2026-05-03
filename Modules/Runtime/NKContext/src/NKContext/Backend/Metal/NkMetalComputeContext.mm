// =============================================================================
// NkMetalComputeContext.mm â€” Compute Metal via MTLComputeCommandEncoder
// Compiler avec -x objective-c++
// Lier : -framework Metal
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "NkMetalComputeContext.h"
#include "NKContext/Graphics/Metal/NkMetalContext.h"
#include "NKContainers/NkContainers.h"
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>

#define NK_MTLC_LOG(...) logger.Infof("[NkMetalCompute] " __VA_ARGS__)
#define NK_MTLC_ERR(...) logger.Errorf("[NkMetalCompute] " __VA_ARGS__)

namespace nkentseu {

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

NkMetalComputeContext::~NkMetalComputeContext() { if (mIsValid) Shutdown(); }

// =============================================================================
void NkMetalComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) return;
    auto* md = static_cast<NkMetalContextData*>(gfx->GetNativeContextData());
    if (!md || !md->device || !md->commandQueue) return;

    mDevice      = md->device;
    mQueue       = md->commandQueue;
    mOwnsDevice  = false;
    mIsValid     = true;
    memset(mBoundBuffers, 0, sizeof(mBoundBuffers));
    mBoundBufferCount = 0;
    NK_MTLC_LOG("Initialized from graphics context\n");
}

bool NkMetalComputeContext::Init(const NkContextDesc&) {
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    if (!dev) { NK_MTLC_ERR("MTLCreateSystemDefaultDevice failed\n"); return false; }
    mDevice = (void*)CFBridgingRetain(dev);

    id<MTLCommandQueue> q = [dev newCommandQueue];
    if (!q) { NK_MTLC_ERR("newCommandQueue failed\n"); return false; }
    mQueue = (void*)CFBridgingRetain(q);

    mOwnsDevice = true;
    mIsValid = true;
    NK_MTLC_LOG("Standalone compute context initialized: %s\n",
                [[dev name] UTF8String]);
    return true;
}

void NkMetalComputeContext::Shutdown() {
    if (!mIsValid) return;
    WaitIdle();
    if (mOwnsDevice) {
        if (mQueue)  { CFBridgingRelease(mQueue);  mQueue  = nullptr; }
        if (mDevice) { CFBridgingRelease(mDevice); mDevice = nullptr; }
    }
    mCurrentPipeline  = nullptr;
    mBoundBufferCount = 0;
    mIsValid = false;
    NK_MTLC_LOG("Shutdown\n");
}

bool NkMetalComputeContext::IsValid() const { return mIsValid; }

// =============================================================================
// Buffers
// =============================================================================
NkComputeBuffer NkMetalComputeContext::CreateBuffer(const NkComputeBufferDesc& desc) {
    NkComputeBuffer out;
    if (!mIsValid || desc.sizeBytes == 0) return out;

    id<MTLDevice> dev = (__bridge id<MTLDevice>)mDevice;

    MTLResourceOptions opts;
    if (desc.cpuReadable || desc.cpuWritable)
        opts = MTLResourceStorageModeShared;
    else
        opts = MTLResourceStorageModePrivate;

    id<MTLBuffer> buf;
    if (desc.initialData && (opts == MTLResourceStorageModeShared)) {
        buf = [dev newBufferWithBytes:desc.initialData
                               length:(NSUInteger)desc.sizeBytes
                              options:opts];
    } else {
        buf = [dev newBufferWithLength:(NSUInteger)desc.sizeBytes
                               options:opts];
        if (desc.initialData && opts != MTLResourceStorageModePrivate) {
            memcpy(buf.contents, desc.initialData, (size_t)desc.sizeBytes);
        }
    }

    if (!buf) { NK_MTLC_ERR("newBufferWithLength failed\n"); return out; }

    out.handle    = (void*)CFBridgingRetain(buf);
    out.sizeBytes = desc.sizeBytes;
    out.valid     = true;
    return out;
}

void NkMetalComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) return;
    CFBridgingRelease(buf.handle);
    buf = NkComputeBuffer{};
}

bool NkMetalComputeContext::WriteBuffer(NkComputeBuffer& buf,
    const void* data, uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) return false;
    id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)buf.handle;
    if (!mtlBuf.contents) {
        NK_MTLC_ERR("WriteBuffer: buffer is Private (not CPU-accessible)\n");
        return false;
    }
    memcpy((uint8*)mtlBuf.contents + offset, data, (size_t)bytes);
    return true;
}

bool NkMetalComputeContext::ReadBuffer(const NkComputeBuffer& buf,
    void* outData, uint64 bytes, uint64 offset) {
    if (!buf.valid || !outData) return false;
    id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)buf.handle;
    if (!mtlBuf.contents) {
        NK_MTLC_ERR("ReadBuffer: buffer is Private (not CPU-accessible)\n");
        return false;
    }
    memcpy(outData, (const uint8*)mtlBuf.contents + offset, (size_t)bytes);
    return true;
}

// =============================================================================
// Shaders MSL
// =============================================================================
NkComputeShader NkMetalComputeContext::CreateShaderFromSource(
    const char* src, const char* entry)
{
    NkComputeShader s;
    if (!mIsValid || !src) return s;

    id<MTLDevice> dev = (__bridge id<MTLDevice>)mDevice;
    NSError* err = nil;
    NSString* source = [NSString stringWithUTF8String:src];

    id<MTLLibrary> lib = [dev newLibraryWithSource:source
                                           options:nil
                                             error:&err];
    if (!lib) {
        NK_MTLC_ERR("MSL compile: %s\n",
                    err ? [[err localizedDescription] UTF8String] : "unknown");
        return s;
    }

    id<MTLFunction> fn = [lib newFunctionWithName:
        [NSString stringWithUTF8String:entry]];
    if (!fn) {
        NK_MTLC_ERR("Function '%s' not found in library\n", entry);
        return s;
    }

    s.handle = (void*)CFBridgingRetain(fn);
    s.valid  = true;
    return s;
}

NkComputeShader NkMetalComputeContext::CreateShaderFromFile(
    const char* path, const char* entry)
{
    NkVector<char> src;
    if (!ReadTextFile(path, src)) { NK_MTLC_ERR("Cannot open: %s\n", path); return {}; }
    return CreateShaderFromSource(src.Data(), entry);
}

void NkMetalComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) return;
    CFBridgingRelease(s.handle);
    s = NkComputeShader{};
}

// =============================================================================
// Pipeline (MTLComputePipelineState)
// =============================================================================
NkComputePipeline NkMetalComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline p;
    if (!s.valid) return p;

    id<MTLDevice> dev = (__bridge id<MTLDevice>)mDevice;
    id<MTLFunction> fn = (__bridge id<MTLFunction>)s.handle;

    NSError* err = nil;
    id<MTLComputePipelineState> pso =
        [dev newComputePipelineStateWithFunction:fn error:&err];
    if (!pso) {
        NK_MTLC_ERR("CreateComputePipelineState: %s\n",
                    err ? [[err localizedDescription] UTF8String] : "unknown");
        return p;
    }

    p.handle = (void*)CFBridgingRetain(pso);
    p.valid  = true;
    return p;
}

void NkMetalComputeContext::DestroyPipeline(NkComputePipeline& p) {
    if (!p.valid) return;
    CFBridgingRelease(p.handle);
    p = NkComputePipeline{};
}

// =============================================================================
// Dispatch
// =============================================================================
void NkMetalComputeContext::BindPipeline(const NkComputePipeline& p) {
    mCurrentPipeline = p.valid ? p.handle : nullptr;
}

void NkMetalComputeContext::BindBuffer(uint32 slot, NkComputeBuffer& buf) {
    if (!buf.valid || slot >= 16) return;
    mBoundBuffers[slot] = buf.handle;
    if (slot + 1 > mBoundBufferCount) mBoundBufferCount = slot + 1;
}

void NkMetalComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    if (!mIsValid || !mCurrentPipeline) {
        NK_MTLC_ERR("Dispatch: no pipeline bound\n"); return;
    }

    id<MTLCommandQueue>  q    = (__bridge id<MTLCommandQueue>)mQueue;
    id<MTLCommandBuffer> cmdb = [q commandBuffer];
    id<MTLComputePipelineState> pso =
        (__bridge id<MTLComputePipelineState>)mCurrentPipeline;

    id<MTLComputeCommandEncoder> enc = [cmdb computeCommandEncoder];
    [enc setComputePipelineState:pso];

    // Lier tous les buffers bindÃ©s
    for (uint32 i = 0; i < mBoundBufferCount; ++i) {
        if (!mBoundBuffers[i]) continue;
        id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)mBoundBuffers[i];
        [enc setBuffer:mtlBuf offset:0 atIndex:i];
    }

    // Taille de threadgroup optimale depuis le PSO
    NSUInteger tgSize = pso.maxTotalThreadsPerThreadgroup;
    if (tgSize > 256) tgSize = 256; // sÃ©curitÃ© conservative
    MTLSize threadgroupSize = {tgSize, 1, 1};
    MTLSize gridSize        = {gx, gy, gz};

    [enc dispatchThreadgroups:gridSize
        threadsPerThreadgroup:threadgroupSize];
    [enc endEncoding];
    [cmdb commit];
    [cmdb waitUntilCompleted]; // synchrone â€” pour WaitIdle implicite
}

void NkMetalComputeContext::WaitIdle() {
    // Dispatch() est dÃ©jÃ  synchrone (waitUntilCompleted)
    // Pour un usage async, remplacer par un MTLSharedEvent
}

void NkMetalComputeContext::MemoryBarrier() {
    // Metal gÃ¨re les barriÃ¨res automatiquement entre encoders
    // Pour un barrier intra-dispatch : utiliser threadgroup_barrier(mem_flags::mem_device)
    // dans le shader MSL
}

// =============================================================================
// CapacitÃ©s
// =============================================================================
NkGraphicsApi NkMetalComputeContext::GetApi() const { return NkGraphicsApi::NK_GFX_API_METAL; }

uint32 NkMetalComputeContext::GetMaxGroupSizeX() const {
    if (!mCurrentPipeline) return 1024;
    id<MTLComputePipelineState> pso =
        (__bridge id<MTLComputePipelineState>)mCurrentPipeline;
    return (uint32)pso.maxTotalThreadsPerThreadgroup;
}
uint32 NkMetalComputeContext::GetMaxGroupSizeY() const { return 1024; }
uint32 NkMetalComputeContext::GetMaxGroupSizeZ() const { return 1024; }

uint64 NkMetalComputeContext::GetSharedMemoryBytes() const {
    id<MTLDevice> dev = (__bridge id<MTLDevice>)mDevice;
    return (uint64)dev.maxThreadgroupMemoryLength;
}

bool NkMetalComputeContext::SupportsAtomics() const { return true; }
bool NkMetalComputeContext::SupportsFloat64() const { return false; } // Metal ne supporte pas fp64

} // namespace nkentseu

#endif // MACOS || IOS

