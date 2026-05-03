#pragma once
// =============================================================================
// NkSoftwareComputeContext.h — Compute fallback CPU (software backend)
// =============================================================================
#include "NkIComputeContext.h"
#include "NKContext/Core/NkIGraphicsContext.h"

namespace nkentseu {

class NkSoftwareComputeContext final : public NkIComputeContext {
public:
    NkSoftwareComputeContext() = default;
    ~NkSoftwareComputeContext() override;

    bool Init(const NkContextDesc& desc);
    void InitFromGraphicsContext(NkIGraphicsContext* gfx);

    bool IsValid() const override;
    void Shutdown() override;

    NkComputeBuffer CreateBuffer(const NkComputeBufferDesc& d) override;
    void DestroyBuffer(NkComputeBuffer& buf) override;
    bool WriteBuffer(NkComputeBuffer& buf, const void* data,
                     uint64 bytes, uint64 offset = 0) override;
    bool ReadBuffer(const NkComputeBuffer& buf, void* out,
                    uint64 bytes, uint64 offset = 0) override;

    NkComputeShader CreateShaderFromSource(const char* src,
                                           const char* entry = "main") override;
    NkComputeShader CreateShaderFromFile(const char* path,
                                         const char* entry = "main") override;
    void DestroyShader(NkComputeShader& s) override;

    NkComputePipeline CreatePipeline(const NkComputeShader& s) override;
    void DestroyPipeline(NkComputePipeline& p) override;

    void BindBuffer(uint32 slot, NkComputeBuffer& buf) override;
    void BindPipeline(const NkComputePipeline& p) override;
    void Dispatch(uint32 gx, uint32 gy = 1, uint32 gz = 1) override;

    void WaitIdle() override;
    void MemoryBarrier() override;

    NkGraphicsApi GetApi() const override;
    uint32 GetMaxGroupSizeX() const override;
    uint32 GetMaxGroupSizeY() const override;
    uint32 GetMaxGroupSizeZ() const override;
    uint64 GetSharedMemoryBytes() const override;
    bool SupportsAtomics() const override;
    bool SupportsFloat64() const override;

private:
    struct BufferHandle;
    struct ShaderHandle;
    struct PipelineHandle;

    bool mIsValid = false;
    void* mCurrentPipeline = nullptr;
    void* mBoundBuffers[16] = {};
};

} // namespace nkentseu
