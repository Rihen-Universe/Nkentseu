#pragma once

#include "NKRenderer/Deprecate/NkGraphicsContext.h"
#include "NKRenderer/Deprecate/NkNativeHandles.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    class NkPlatformGraphicsContext final : public NkGraphicsContext {
        public:
            explicit NkPlatformGraphicsContext(NkGraphicsApi api);
            ~NkPlatformGraphicsContext() override;

            NkContextError Init(const NkSurfaceDesc& surface,
                                const NkGraphicsContextConfig& config) override;
            void Shutdown() override;
            NkContextError Recreate(uint32 newWidth, uint32 newHeight) override;

            bool BeginFrame(NkFrameContext& frame) override;
            void EndFrame(NkFrameContext& frame) override;
            void Present(NkFrameContext& frame) override;

            bool            IsInitialized() const override;
            NkGraphicsApi   GetApi() const override;
            NkSwapchainDesc GetSwapchain() const override;
            NkContextError  GetLastError() const override;
            void            SetVSync(bool enabled) override;
            bool            GetVSync() const override;

        protected:
            void*       GetNativeHandleImpl(uint32 typeId) override;
            const void* GetNativeHandleImpl(uint32 typeId) const override;

        private:
            bool InitOpenGL(const NkSurfaceDesc& surface);
            bool InitVulkan(const NkSurfaceDesc& surface);
            bool InitMetal(const NkSurfaceDesc& surface);
            bool InitDirectX11(const NkSurfaceDesc& surface);
            bool InitDirectX12(const NkSurfaceDesc& surface);
            bool InitSoftware(const NkSurfaceDesc& surface);

            void SetError(uint32 code, const char* message);
            void ClearError();
            bool ResizeSoftwareBuffer(uint32 width, uint32 height);

            NkGraphicsApi           mApi         = NkGraphicsApi::NK_NONE;
            NkGraphicsContextConfig mConfig{};
            NkSurfaceDesc           mSurface{};
            NkSwapchainDesc         mSwapchain{};
            NkContextError          mLastError{};
            bool                    mInitialized = false;
            bool                    mNeedRecreate = false;
            uint32                  mFrameCounter = 0;

            NkOpenGLNativeHandles   mOpenGL{};
            NkVulkanNativeHandles   mVulkan{};
            NkMetalNativeHandles    mMetal{};
            NkDirectX11NativeHandles mDX11{};
            NkDirectX12NativeHandles mDX12{};
            NkSoftwareNativeHandles  mSoftware{};
            NkVector<uint8>          mSoftwarePixels{};

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            void* mOwnedDeviceContext = nullptr;
        #endif
    };

} // namespace nkentseu
