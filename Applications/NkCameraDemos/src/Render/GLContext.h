#pragma once
// =============================================================================
// GLContext.h
// -----------------------------------------------------------------------------
// Wrapper minimal autour de NkContext (NkOpenGLContext) + chargement glad.
// Copie litterale du pattern Pong/Render/GLContext.h (namespace pong -> cameradem).
//
// Multi-plateforme :
//   - Windows  : WGL + OpenGL 4.6 Core
//   - Linux X11: GLX + OpenGL 4.6 Core
//   - Wayland  : EGL + OpenGL ES 3.0
//   - Android  : EGL + OpenGL ES 3.0
//   - macOS    : NSGL + OpenGL Core
//   - iOS      : EAGL + OpenGL ES
//   - Web      : WebGL2 (via Emscripten)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    class NkWindow;
    class NkIGraphicsContext;
}

namespace nkentseu
{
    namespace cameradem
    {

        // ─────────────────────────────────────────────────────────────────────
        // GLContext — encapsule la creation du contexte OpenGL et le chargement
        // des points d'entree GL via glad. Apres Init() reussi, on peut appeler
        // directement glClear / glDrawArrays / glBindBuffer / etc.
        // ─────────────────────────────────────────────────────────────────────
        class GLContext
        {
        public:
            GLContext()  = default;
            ~GLContext();

            // Lifecycle
            bool Init(NkWindow& window);
            void Shutdown();

            // Frame
            bool BeginFrame();
            void EndFrame();
            void Present();
            bool OnResize(uint32 w, uint32 h);

            // Recreation de surface (Android lifecycle)
            bool RecreateSurface(NkWindow& window);

            // Accesseurs
            bool                IsValid() const noexcept { return mContext != nullptr; }
            NkIGraphicsContext* Raw()             noexcept { return mContext; }

        private:
            NkIGraphicsContext* mContext     = nullptr;
            bool                mGladLoaded  = false;
        };

    } // namespace cameradem
} // namespace nkentseu
