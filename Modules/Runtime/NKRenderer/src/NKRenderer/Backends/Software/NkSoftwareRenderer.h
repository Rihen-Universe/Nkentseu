#pragma once
// =============================================================================
// NkSoftwareRenderer.h - Renderer logiciel CPU (reference).
// Blit OS par plateforme:
//   Win32   : StretchDIBits (RGBA -> BGRA)
//   XCB     : xcb_image_put (RGBA -> BGRX)
//   XLib    : XPutImage     (RGBA -> BGRX)
//   Android : ANativeWindow_lock / unlockAndPost
//   WASM    : Canvas 2D putImageData (RGBA natif)
//   Cocoa   : NSBitmapImageRep + drawInRect
// =============================================================================

#include "NKRenderer/NkIRenderer.h"
#include "NKContainers/Sequential/NkVector.h"
#include <vector>

namespace nkentseu {

    class NkSoftwareRenderer : public NkIRenderer {
        public:
            NkSoftwareRenderer() = default;
            ~NkSoftwareRenderer() override;

            bool Init(const NkRendererConfig& config, const NkSurfaceDesc& surface) override;
            void Shutdown() override;
            bool IsValid() const override;

            const char*  GetName()               const override;
            bool         IsHardwareAccelerated() const override;
            NkError      GetLastError()          const override;
            const NkFramebufferInfo& GetFramebufferInfo() const override;

            void SetBackgroundColor(uint32 rgba) override;
            uint32 GetBackgroundColor()          const override;

            void BeginFrame(uint32 clearColor = 0xFFFFFFFF) override;
            void EndFrame() override;
            void Present(const NkSurfaceDesc& surface) override;
            void Resize(uint32 width, uint32 height) override;
            void SetPixel(int32 x, int32 y, uint32 rgba) override;

            const uint8* GetPixelBuffer() const noexcept { return mBuffer.Data(); }
            uint8*       GetPixelBuffer()       noexcept { return mBuffer.Data(); }

        private:
            NkVector<uint8> mBuffer;
            NkRendererConfig  mConfig   {};
            NkSurfaceDesc     mSurface  {};
            NkFramebufferInfo mFbInfo   {};
            NkError           mLastError{};
            uint32             mBgColor  = 0x141414FF;
            bool              mReady    = false;

            void AllocBuffer(uint32 w, uint32 h);
            void BlitOS(const NkSurfaceDesc& surface, uint32 w, uint32 h);
            void BlitWin32(const NkSurfaceDesc& sd, uint32 w, uint32 h);
            void BlitXCB  (const NkSurfaceDesc& sd, uint32 w, uint32 h);
            void BlitXLib (const NkSurfaceDesc& sd, uint32 w, uint32 h);
            void BlitANW    (const NkSurfaceDesc& sd, uint32 w, uint32 h);
            void BlitWASM   (const NkSurfaceDesc& sd, uint32 w, uint32 h);
            void BlitCocoa  (const NkSurfaceDesc& sd, uint32 w, uint32 h);
            void BlitWayland(const NkSurfaceDesc& sd, uint32 w, uint32 h);
    };

} // namespace nkentseu
