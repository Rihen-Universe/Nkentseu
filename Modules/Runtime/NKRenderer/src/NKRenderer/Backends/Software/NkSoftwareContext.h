#pragma once
// =============================================================================
// NkSoftwareContext.h
// Contexte de rendu logiciel (Software Renderer).
//
// Fonctionnement :
//   - Alloue un framebuffer CPU (pixels RGBA8)
//   - Le renderer dessine directement dans ce buffer
//   - Le buffer est copié vers la fenêtre via les mécanismes natifs :
//       Windows   → BitBlt (GDI) ou SetDIBitsToDevice
//       XLib      → XPutImage  (XShmPutImage pour les performances)
//       XCB       → xcb_put_image
//       Wayland   → wl_shm buffer (déjà dans NkSurfaceDesc::shmPixels)
//       Android   → ANativeWindow_lock / ANativeWindow_unlockAndPost
//       Web       → putImageData sur un canvas 2D
// =============================================================================
#include "NKRenderer/NkIGraphicsContext.h"
#include "NKWindow/Core/NkSurface.h"
#include <vector>

namespace nkentseu {

    // -------------------------------------------------------------------------
    struct NkSoftwareFramebuffer {
        std::vector<uint8> pixels;
        uint32 width  = 0;
        uint32 height = 0;
        uint32 stride = 0;

        void Resize(uint32 w, uint32 h) {
            width=w; height=h; stride=w*4;
            pixels.assign(stride*h, 0);
        }
        void Clear(uint8 r=0,uint8 g=0,uint8 b=0,uint8 a=255) {
            for (uint32 i=0;i<pixels.size();i+=4){
                pixels[i+0]=r;pixels[i+1]=g;pixels[i+2]=b;pixels[i+3]=a;
            }
        }
        uint8*       RowPtr(uint32 y)       { return pixels.data()+y*stride; }
        const uint8* RowPtr(uint32 y) const { return pixels.data()+y*stride; }
        void SetPixel(uint32 x,uint32 y,uint8 r,uint8 g,uint8 b,uint8 a=255) {
            if(x>=width||y>=height) return;
            uint8* p=RowPtr(y)+x*4; p[0]=r;p[1]=g;p[2]=b;p[3]=a;
        }
        bool IsValid() const { return width>0&&height>0&&!pixels.empty(); }
    };

    // -------------------------------------------------------------------------
    struct NkSoftwareContextData {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        void* hwnd      = nullptr;  // HWND
        void* hdc       = nullptr;  // HDC
        void* dibBitmap = nullptr;  // HBITMAP DIBSection
        void* dibDC     = nullptr;  // HDC mémoire
        void* dibBits   = nullptr;  // Pointeur pixels DIB
#elif defined(NKENTSEU_WINDOWING_XLIB)
        void*         display  = nullptr;
        unsigned long window   = 0;
        void*         gc       = nullptr;  // GC
        void*         ximage   = nullptr;  // XImage
        void*         shmInfo  = nullptr;  // XShmSegmentInfo*
        bool          useSHM   = false;
        int           shmid    = -1;
#elif defined(NKENTSEU_WINDOWING_XCB)
        void*    connection = nullptr;
        unsigned long window= 0;
        uint32   gc         = 0;   // xcb_gcontext_t
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
        void*  wlDisplay  = nullptr;
        void*  wlSurface  = nullptr;
        void*  wlBuffer   = nullptr;
        void*  shmPixels  = nullptr;
        bool   waylandConfigured = false;
        uint32 shmStride  = 0;
        uint64 shmSize    = 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
        void*  nativeWindow = nullptr;
#elif defined(NKENTSEU_PLATFORM_MACOS)
        void*  nsView    = nullptr;
        void*  cgContext = nullptr;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        const char* canvasId = "#canvas";
#endif
        uint32 width  = 0;
        uint32 height = 0;
    };

    // -------------------------------------------------------------------------
    class NkSoftwareContext final : public NkIGraphicsContext {
        public:
            NkSoftwareContext()  = default;
            ~NkSoftwareContext() override;

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

            NkSoftwareFramebuffer& GetBackBuffer()  { return mBackBuffer; }
            NkSoftwareFramebuffer& GetFrontBuffer() { return mFrontBuffer; }

        private:
            bool InitNativePresenter (const NkSurfaceDesc& surf);
            void ShutdownNativePresenter();
            void PresentNative();

            NkSoftwareFramebuffer  mBackBuffer;
            NkSoftwareFramebuffer  mFrontBuffer;
            NkSoftwareContextData  mData;
            NkContextDesc          mDesc;
            NkSurfaceDesc          mCachedSurface; // PATCH OnResize : stocké pour réinit
            bool                   mIsValid = false;
            bool                   mVSync   = false;
    };

} // namespace nkentseu
