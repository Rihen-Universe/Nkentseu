#pragma once
// =============================================================================
// GLContext.h — Wrapper minimal NkContext (OpenGL/GLES) + glad load
// Multi-plateforme : Windows (WGL/GL 4.6), Linux X11 (GLX), Wayland/Android (EGL/GLES3),
//                    macOS (NSGL), iOS (EAGL), Web (WebGL2)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

namespace nkentseu {
    class NkWindow;
    class NkIGraphicsContext;
}

namespace nkentseu { 
    namespace pong {

        class GLContext {
            public:
                GLContext()  = default;
                ~GLContext();

                bool Init(NkWindow& window);
                void Shutdown();

                bool BeginFrame();
                void EndFrame();
                void Present();
                bool OnResize(uint32 w, uint32 h);

                bool IsValid() const noexcept { return mContext != nullptr; }
                NkIGraphicsContext* Raw() noexcept { return mContext; }

            private:
                NkIGraphicsContext* mContext = nullptr;
                bool                mGladLoaded = false;
        };

    }
} // namespace nkentseu::pong
