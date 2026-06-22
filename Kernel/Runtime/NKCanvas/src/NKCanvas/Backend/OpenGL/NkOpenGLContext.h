#pragma once
// =============================================================================
// NkOpenGLContext.h
// Contexte OpenGL cross-plateforme.
// Aucun loader OpenGL n'est imposé; le runtime peut utiliser un loader externe.
// =============================================================================
#include "NKCanvas/Core/NkIGraphicsContext.h"
#include "NKCanvas/Core/NkSurfaceDesc.h"
#include "NkOpenGLContextData.h"

namespace nkentseu {

    class NkOpenGLContext final : public NkIGraphicsContext {
    public:
        NkOpenGLContext()  = default;
        ~NkOpenGLContext() override;

        // Interface NkIGraphicsContext
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

        // Multi-contexte : rendre courant pour le thread appelant
        bool MakeCurrent()    override;
        void ReleaseCurrent() override;

        /// Recree la surface graphique a partir du native window courant de
        /// la fenetre. Utilise quand la surface OS a ete detruite/recreee
        /// sans que le contexte GL soit perdu — typiquement sur Android
        /// (APP_CMD_TERM_WINDOW puis APP_CMD_INIT_WINDOW au retour de
        /// foreground). Sur PC c'est un no-op (la surface n'est pas perdue).
        /// Le contexte/display/config EGL sont conserves, seul l'eglSurface
        /// est detruit puis re-cree avec le nouveau ANativeWindow.
        /// @return true si la surface a ete recreee avec succes.
        bool RecreateSurface(const NkWindow& window) override;

        // Crée un contexte fils partageant les textures/buffers (asset loader thread)
        // Caller prend ownership — nullptr si partage non supporté
        NkOpenGLContext* CreateSharedContext(const NkWindow& window);

    private:
        bool LoadOpenGLEntryPoints(const NkOpenGLDesc& gl);
        void FillInfo();

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        bool InitWGL (const NkSurfaceDesc& s, const NkOpenGLDesc& gl);
        void ShutdownWGL();  void SwapWGL();  void SetVSyncWGL(bool);
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        bool InitGLX (const NkSurfaceDesc& s, const NkOpenGLDesc& gl);
        void ShutdownGLX();  void SwapGLX();  void SetVSyncGLX(bool);
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
        bool InitEGL (const NkSurfaceDesc& s, const NkOpenGLDesc& gl);
        void ShutdownEGL();  void SwapEGL();  void SetVSyncEGL(bool);
#elif defined(NKENTSEU_PLATFORM_MACOS)
        bool InitNSGL(const NkSurfaceDesc& s, const NkOpenGLDesc& gl);
        void ShutdownNSGL(); void SwapNSGL();
#elif defined(NKENTSEU_PLATFORM_IOS)
        bool InitEAGL(const NkSurfaceDesc& s, const NkOpenGLDesc& gl);
        void ShutdownEAGL(); void SwapEAGL();
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        bool InitWebGL(const NkSurfaceDesc& s, const NkOpenGLDesc& gl);
        void ShutdownWebGL(); void SwapWebGL();
#endif

        NkOpenGLContextData mData;
        NkContextDesc       mDesc;
        bool                mIsValid = false;
        bool                mVSync   = true;
        NkOpenGLContext*    mSharedParent = nullptr; // nullptr = contexte primaire
    };

} // namespace nkentseu
