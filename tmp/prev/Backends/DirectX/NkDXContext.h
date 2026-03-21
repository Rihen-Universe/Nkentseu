#pragma once
// =============================================================================
// NkDXContext.h — Déclarations DX11 et DX12
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(_WIN32)

#include "NKContext/NkIGraphicsContext.h"
#include "NkDirectXContextData.h"

namespace nkentseu {

    // =========================================================================
    // DX11
    // =========================================================================
    class NkDX11Context final : public NkIGraphicsContext {
        public:
            NkDX11Context()  = default;
            ~NkDX11Context() override;

            bool          Initialize(const NkWindow& w, const NkContextDesc& d) override;
            void          Shutdown()                                             override;
            bool          IsValid()   const                                      override;
            bool          BeginFrame()                                           override;
            void          EndFrame()                                             override;
            void          Present()                                              override;
            bool          OnResize(uint32 w, uint32 h)                           override;
            void          SetVSync(bool enabled)                                 override;
            bool          GetVSync() const                                       override;
            NkGraphicsApi GetApi()   const                                       override;
            NkContextInfo GetInfo()  const                                       override;
            NkContextDesc GetDesc()  const                                       override;
            void*         GetNativeContextData()                                 override;
            bool          SupportsCompute() const                                override;

            ID3D11Device1*        GetDevice()  { return mData.device.Get(); }
            ID3D11DeviceContext1* GetContext() { return mData.context.Get(); }

        private:
            bool CreateDeviceAndSwapchain(const NkContextDesc& d, HWND hwnd);
            bool CreateRenderTargets();
            void DestroyRenderTargets();
            void HandleDeviceLost();

            NkDX11ContextData mData;
            NkContextDesc     mDesc;
            bool              mIsValid = false;
            bool              mVSync   = true;
    };

    // =========================================================================
    // DX12
    // =========================================================================
    class NkDX12Context final : public NkIGraphicsContext {
        public:
            NkDX12Context()  = default;
            ~NkDX12Context() override;

            bool          Initialize(const NkWindow& w, const NkContextDesc& d) override;
            void          Shutdown()                                             override;
            bool          IsValid()   const                                      override;
            bool          BeginFrame()                                           override;
            void          EndFrame()                                             override;
            void          Present()                                              override;
            bool          OnResize(uint32 w, uint32 h)                           override;
            void          SetVSync(bool enabled)                                 override;
            bool          GetVSync() const                                       override;
            NkGraphicsApi GetApi()   const                                       override;
            NkContextInfo GetInfo()  const                                       override;
            NkContextDesc GetDesc()  const                                       override;
            void*         GetNativeContextData()                                 override;
            bool          SupportsCompute() const                                override;

            ID3D12Device5*             GetDevice()      { return mData.device.Get(); }
            ID3D12CommandQueue*        GetCommandQueue(){ return mData.commandQueue.Get(); }
            ID3D12GraphicsCommandList4* GetCommandList(){ return mData.cmdList.Get(); }

            void FlushGPU();  // WaitIdle — appeler avant Shutdown ou resize manuel

        private:
            bool CreateDevice       (const NkDirectX12Desc& d);
            bool CreateCommandQueues(const NkDirectX12Desc& d);
            bool CreateSwapchain    (uint32 w, uint32 h, const NkDirectX12Desc& d);
            bool CreateDescriptorHeaps(const NkDirectX12Desc& d);
            bool CreateRenderTargets();
            bool CreateDepthBuffer  (uint32 w, uint32 h);
            bool CreateCommandObjects();
            bool CreateSyncObjects();

            void DestroySwapchainDependents();
            bool RecreateSwapchainDependents(uint32 w, uint32 h);
            void WaitForFence(uint32 frameIndex);

            NkDX12ContextData mData;
            NkContextDesc     mDesc;
            bool              mIsValid = false;
            bool              mVSync   = true;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS || _WIN32
