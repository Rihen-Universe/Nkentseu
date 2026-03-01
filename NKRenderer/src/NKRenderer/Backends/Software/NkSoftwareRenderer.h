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

#include "../INkRenderer.h"
#include <vector>

namespace nkentseu {

class NkSoftwareRenderer : public INkRenderer {
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

    void SetBackgroundColor(NkU32 rgba) override;
    NkU32 GetBackgroundColor()          const override;

    void BeginFrame(NkU32 clearColor = 0xFFFFFFFF) override;
    void EndFrame() override;
    void Present(const NkSurfaceDesc& surface) override;
    void Resize(NkU32 width, NkU32 height) override;
    void SetPixel(NkI32 x, NkI32 y, NkU32 rgba) override;

    const NkU8* GetPixelBuffer() const noexcept { return mBuffer.data(); }
    NkU8*       GetPixelBuffer()       noexcept { return mBuffer.data(); }

private:
    std::vector<NkU8> mBuffer;
    NkRendererConfig  mConfig   {};
    NkSurfaceDesc     mSurface  {};
    NkFramebufferInfo mFbInfo   {};
    NkError           mLastError{};
    NkU32             mBgColor  = 0x141414FF;
    bool              mReady    = false;

    void AllocBuffer(NkU32 w, NkU32 h);
    void BlitOS(const NkSurfaceDesc& surface, NkU32 w, NkU32 h);
    void BlitWin32(const NkSurfaceDesc& sd, NkU32 w, NkU32 h);
    void BlitXCB  (const NkSurfaceDesc& sd, NkU32 w, NkU32 h);
    void BlitXLib (const NkSurfaceDesc& sd, NkU32 w, NkU32 h);
    void BlitANW  (const NkSurfaceDesc& sd, NkU32 w, NkU32 h);
    void BlitWASM (const NkSurfaceDesc& sd, NkU32 w, NkU32 h);
    void BlitCocoa(const NkSurfaceDesc& sd, NkU32 w, NkU32 h);
};

} // namespace nkentseu
