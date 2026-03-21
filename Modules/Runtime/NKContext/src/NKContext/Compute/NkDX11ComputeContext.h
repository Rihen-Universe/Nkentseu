#pragma once
// =============================================================================
// NkDX11ComputeContext.h — Compute DX11 via ID3D11ComputeShader (CS_5_0)
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include "NkIComputeContext.h"
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Graphics/DirectX/NkDirectXContextData.h"

#if defined(MemoryBarrier)
#undef MemoryBarrier
#endif

namespace nkentseu {

    class NkDX11ComputeContext final : public NkIComputeContext {
    public:
        NkDX11ComputeContext()  = default;
        ~NkDX11ComputeContext() override;

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
                                                  const char* entry="CSMain")  override;
        NkComputeShader   CreateShaderFromFile  (const char* path,
                                                  const char* entry="CSMain")  override;
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
        ComPtr<ID3D11Device1>        mDevice;
        ComPtr<ID3D11DeviceContext1>  mContext;
        bool mIsValid     = false;
        bool mOwnsDevice  = false;
    };

} // namespace nkentseu
#endif // WINDOWS
