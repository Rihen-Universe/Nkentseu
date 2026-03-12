#pragma once
// =============================================================================
// NkMetalContext.h + NkMetalContext.mm
// Metal (macOS/iOS) — production ready
// Compiler NkMetalContext.mm avec -x objective-c++
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "../Core/NkIGraphicsContext.h"

namespace nkentseu {

    struct NkMetalContextData {
        void* device          = nullptr;  // id<MTLDevice>
        void* commandQueue    = nullptr;  // id<MTLCommandQueue>
        void* layer           = nullptr;  // CAMetalLayer*
        void* currentDrawable = nullptr;  // id<CAMetalDrawable>
        void* commandBuffer   = nullptr;  // id<MTLCommandBuffer>
        void* currentEncoder  = nullptr;  // id<MTLRenderCommandEncoder>
        void* depthTexture    = nullptr;  // id<MTLTexture>
        void* library         = nullptr;  // id<MTLLibrary> (shaders compilés)

        uint32      width     = 0;
        uint32      height    = 0;
        uint32      sampleCount = 1;
        const char* renderer  = "Metal";
        const char* vendor    = "Apple";
        const char* version   = "Metal";
        uint32      vramMB    = 0;
    };

    class NkMetalContext final : public NkIGraphicsContext {
    public:
        NkMetalContext()  = default;
        ~NkMetalContext() override;

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

    private:
        bool CreateDepthTexture(uint32 w, uint32 h);

        NkMetalContextData mData;
        NkContextDesc      mDesc;
        bool               mIsValid = false;
        bool               mVSync   = true;
    };

} // namespace nkentseu

#endif // MACOS || IOS
