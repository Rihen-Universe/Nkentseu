#pragma once
// =============================================================================
// GLContext.h
// -----------------------------------------------------------------------------
// Wrapper minimal autour de NkContext (NkOpenGLContext) + chargement glad.
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
    // Forward declarations pour eviter d'inclure les lourds headers ici.
    class NkWindow;
    class NkIGraphicsContext;
}

namespace nkentseu
{
    namespace pong
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

            // ── Lifecycle ────────────────────────────────────────────────────
            /// Cree le contexte OpenGL sur la surface native de @p window puis
            /// charge les fonctions GL via glad. Retourne false sur echec.
            bool Init(NkWindow& window);
            /// Detruit le contexte et libere les ressources.
            void Shutdown();

            // ── Frame ────────────────────────────────────────────────────────
            /// Doit etre appele AVANT toute commande GL d'un frame.
            bool BeginFrame();
            /// Marque la fin du rendu (avant Present).
            void EndFrame();
            /// Echange les buffers (swap chain).
            void Present();
            /// A appeler quand la fenetre est redimensionnee.
            bool OnResize(uint32 w, uint32 h);

            /// A appeler quand la surface OS a ete recreee (Android, retour
            /// foreground apres APP_CMD_INIT_WINDOW). Recree l'eglSurface
            /// sans toucher au contexte GL ; sur PC c'est un no-op.
            bool RecreateSurface(NkWindow& window);

            // ── Accesseurs ───────────────────────────────────────────────────
            bool                IsValid() const noexcept { return mContext != nullptr; }
            NkIGraphicsContext* Raw()             noexcept { return mContext; }

        private:
            NkIGraphicsContext* mContext     = nullptr;  ///< Possede par GLContext
            bool                mGladLoaded  = false;    ///< true apres gladLoadGL(ES)
        };

    } // namespace pong
} // namespace nkentseu
