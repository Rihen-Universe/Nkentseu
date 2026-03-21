#include "NkSoftwareComputeContext.h"
#include "NKContainers/NkContainers.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(MemoryBarrier)
#undef MemoryBarrier
#endif

namespace nkentseu {

#define NK_SWC_LOG(...) logger.Infof("[NkSoftwareCompute] " __VA_ARGS__)
#define NK_SWC_ERR(...) logger.Errorf("[NkSoftwareCompute] " __VA_ARGS__)

struct NkSoftwareComputeContext::BufferHandle {
    NkVector<uint8> data;
};

struct NkSoftwareComputeContext::ShaderHandle {
    NkVector<char> source;
};

struct NkSoftwareComputeContext::PipelineHandle {
    ShaderHandle* shader = nullptr;
};

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

NkSoftwareComputeContext::~NkSoftwareComputeContext() {
    if (mIsValid) {
        Shutdown();
    }
}

bool NkSoftwareComputeContext::Init(const NkContextDesc&) {
    mIsValid = true;
    mCurrentPipeline = nullptr;
    memset(mBoundBuffers, 0, sizeof(mBoundBuffers));
    NK_SWC_LOG("Standalone context initialized (CPU fallback)");
    return true;
}

void NkSoftwareComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    mIsValid = (gfx && gfx->IsValid() && gfx->GetApi() == NkGraphicsApi::NK_API_SOFTWARE);
    mCurrentPipeline = nullptr;
    memset(mBoundBuffers, 0, sizeof(mBoundBuffers));
    if (!mIsValid) {
        NK_SWC_ERR("InitFromGraphicsContext requires a valid software graphics context");
        return;
    }
    NK_SWC_LOG("Initialized from software graphics context");
}

bool NkSoftwareComputeContext::IsValid() const {
    return mIsValid;
}

void NkSoftwareComputeContext::Shutdown() {
    mIsValid = false;
    mCurrentPipeline = nullptr;
    memset(mBoundBuffers, 0, sizeof(mBoundBuffers));
}

NkComputeBuffer NkSoftwareComputeContext::CreateBuffer(const NkComputeBufferDesc& d) {
    NkComputeBuffer out;
    if (!mIsValid || d.sizeBytes == 0) {
        return out;
    }

    auto* h = new BufferHandle();
    h->data.Resize((NkVector<uint8>::SizeType)d.sizeBytes, static_cast<uint8>(0));
    if (d.initialData) {
        memcpy(h->data.Data(), d.initialData, (size_t)d.sizeBytes);
    }

    out.handle = h;
    out.sizeBytes = d.sizeBytes;
    out.valid = true;
    return out;
}

void NkSoftwareComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) {
        return;
    }
    delete static_cast<BufferHandle*>(buf.handle);
    buf = NkComputeBuffer{};
}

bool NkSoftwareComputeContext::WriteBuffer(NkComputeBuffer& buf, const void* data,
                                           uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) {
        return false;
    }
    if (offset + bytes > buf.sizeBytes) {
        return false;
    }

    auto* h = static_cast<BufferHandle*>(buf.handle);
    memcpy(h->data.Data() + offset, data, (size_t)bytes);
    return true;
}

bool NkSoftwareComputeContext::ReadBuffer(const NkComputeBuffer& buf, void* out,
                                          uint64 bytes, uint64 offset) {
    if (!buf.valid || !out) {
        return false;
    }
    if (offset + bytes > buf.sizeBytes) {
        return false;
    }

    auto* h = static_cast<BufferHandle*>(buf.handle);
    memcpy(out, h->data.Data() + offset, (size_t)bytes);
    return true;
}

NkComputeShader NkSoftwareComputeContext::CreateShaderFromSource(const char* src,
                                                                  const char* /*entry*/) {
    NkComputeShader out;
    if (!mIsValid || !src || !*src) {
        return out;
    }

    auto* h = new ShaderHandle();
    const size_t len = strlen(src);
    h->source.Resize((NkVector<char>::SizeType)len + 1, '\0');
    memcpy(h->source.Data(), src, len);
    h->source[(NkVector<char>::SizeType)len] = '\0';

    out.handle = h;
    out.valid = true;
    return out;
}

NkComputeShader NkSoftwareComputeContext::CreateShaderFromFile(const char* path,
                                                                const char* entry) {
    (void)entry;
    NkVector<char> src;
    if (!ReadTextFile(path, src)) {
        NK_SWC_ERR("Cannot open shader source: %s", path ? path : "<null>");
        return {};
    }
    return CreateShaderFromSource(src.Data(), "main");
}

void NkSoftwareComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) {
        return;
    }
    delete static_cast<ShaderHandle*>(s.handle);
    s = NkComputeShader{};
}

NkComputePipeline NkSoftwareComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline out;
    if (!s.valid) {
        return out;
    }

    auto* p = new PipelineHandle();
    p->shader = static_cast<ShaderHandle*>(s.handle);
    out.handle = p;
    out.valid = true;
    return out;
}

void NkSoftwareComputeContext::DestroyPipeline(NkComputePipeline& p) {
    if (!p.valid) {
        return;
    }
    delete static_cast<PipelineHandle*>(p.handle);
    p = NkComputePipeline{};
}

void NkSoftwareComputeContext::BindBuffer(uint32 slot, NkComputeBuffer& buf) {
    if (!buf.valid || slot >= 16) {
        return;
    }
    mBoundBuffers[slot] = buf.handle;
}

void NkSoftwareComputeContext::BindPipeline(const NkComputePipeline& p) {
    mCurrentPipeline = p.valid ? p.handle : nullptr;
}

void NkSoftwareComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    if (!mIsValid) {
        return;
    }
    if (!mCurrentPipeline) {
        NK_SWC_ERR("Dispatch called without a bound pipeline");
        return;
    }
    // Intentional no-op for now: CPU compute backend wiring is in place.
    (void)gx;
    (void)gy;
    (void)gz;
}

void NkSoftwareComputeContext::WaitIdle() {
    // CPU fallback is synchronous.
}

void NkSoftwareComputeContext::MemoryBarrier() {
    // CPU fallback is coherent in-process.
}

NkGraphicsApi NkSoftwareComputeContext::GetApi() const {
    return NkGraphicsApi::NK_API_SOFTWARE;
}

uint32 NkSoftwareComputeContext::GetMaxGroupSizeX() const {
    return 1024;
}

uint32 NkSoftwareComputeContext::GetMaxGroupSizeY() const {
    return 1;
}

uint32 NkSoftwareComputeContext::GetMaxGroupSizeZ() const {
    return 1;
}

uint64 NkSoftwareComputeContext::GetSharedMemoryBytes() const {
    return 0;
}

bool NkSoftwareComputeContext::SupportsAtomics() const {
    return true;
}

bool NkSoftwareComputeContext::SupportsFloat64() const {
    return true;
}

} // namespace nkentseu
