#pragma once
// =============================================================================
// NkGraphicsContextFactory.h
//
// DEUX responsabilités bien séparées :
//
// ── Étape 1 : PrepareWindowConfig()  ────────────────────────────────────────
//    À appeler AVANT NkWindow::Create() si et seulement si vous utilisez
//    OpenGL/GLX sur Linux (XLib ou XCB).
//    Pour TOUTES les autres combinaisons (Vulkan, Metal, DirectX, EGL,
//    OpenGL/WGL, Software) : ne rien faire, laisser vide.
//
//    Cette fonction interroge GLX, obtient la bonne XVisualInfo, et injecte
//    les hints dans NkWindowConfig::surfaceHints de façon opaque.
//
// ── Étape 2 : Create()  ─────────────────────────────────────────────────────
//    À appeler APRÈS NkWindow::Create().
//    Instancie le bon backend, appelle Init(), et retourne le contexte prêt.
//
// ── Usage type ──────────────────────────────────────────────────────────────
//
//    // Cas général (Vulkan / Metal / DirectX / Software / OpenGL non-GLX) :
//    NkWindowConfig wcfg;  wcfg.title = "...";
//    NkWindow window(wcfg);
//    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);
//
//    // Cas OpenGL/GLX sur Linux uniquement :
//    auto gcfg = NkGraphicsContextConfig::ForOpenGL(4, 6);
//    NkWindowConfig wcfg; wcfg.title = "...";
//    NkGraphicsContextFactory::PrepareWindowConfig(wcfg, gcfg);  // ← avant
//    NkWindow window(wcfg);
//    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);  // ← après
//
// ── Multi-renderer ──────────────────────────────────────────────────────────
//    Un seul contexte, plusieurs renderers :
//
//    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);
//    Renderer2D   r2d(*ctx);
//    Renderer3D   r3d(*ctx);
//    RendererDebug dbg(*ctx);
//    // Tous partagent le même device/contexte GL/VK.
// =============================================================================

#include "NkGraphicsContext.h"
#include "NkGraphicsContextConfig.h"
#include "NKWindow/Core/NkWindowConfig.h"

namespace nkentseu {

    class NkWindow;

    class NkGraphicsContextFactory {
        public:

            // =========================================================================
            // Étape 1 — AVANT NkWindow::Create()
            // Injecte les hints de surface nécessaires dans wcfg.surfaceHints.
            //
            // Effet par API :
            //   OpenGL/WGL    (Windows) : rien à faire (SetPixelFormat après HWND)
            //   OpenGL/GLX    (XLib)    : interroge glXChooseFBConfig, injecte GlxVisualId
            //   OpenGL/GLX    (XCB)     : idem via Xlib-xcb
            //   OpenGL/EGL    (Wayland) : rien (eglCreateWindowSurface après wl_surface)
            //   OpenGL/EGL    (Android) : rien (ANativeWindow disponible après Create)
            //   OpenGL/WebGL  (WASM)    : rien
            //   Vulkan                  : rien
            //   Metal                   : rien
            //   DirectX 11/12           : rien
            //   Software                : rien
            // =========================================================================
            static void PrepareWindowConfig(NkWindowConfig&                wcfg,
                                            const NkGraphicsContextConfig& gcfg);

            // =========================================================================
            // Étape 2 — APRÈS NkWindow::Create()
            // Crée, initialise et retourne le contexte.
            // Retourne nullptr si :
            //   - l'API demandée n'est pas disponible sur la plateforme
            //   - Init() échoue (vérifier GetLastError() sur le contexte retourné)
            // =========================================================================
            static NkGraphicsContextPtr Create(NkWindow&                       window,
                                            const NkGraphicsContextConfig&  config = {});

            // =========================================================================
            // Utilitaires
            // =========================================================================

            /// Retourne la meilleure API disponible sur la plateforme courante.
            /// Ordre : Metal > Vulkan > DirectX12 > OpenGL > Software
            static NkGraphicsApi BestAvailableApi();

            /// Vérifie si une API est disponible à l'exécution sur cette plateforme.
            static bool IsApiAvailable(NkGraphicsApi api);

        private:
            // Préparation spécifique OpenGL (seul cas qui touche NkWindowConfig)
            static void PrepareForOpenGL(NkWindowConfig&, const NkGraphicsContextConfig&);

            // Factories internes — appelées depuis Create()
            static NkGraphicsContextPtr CreateOpenGL  (const NkSurfaceDesc&, const NkGraphicsContextConfig&);
            static NkGraphicsContextPtr CreateVulkan  (const NkSurfaceDesc&, const NkGraphicsContextConfig&);
            static NkGraphicsContextPtr CreateMetal   (const NkSurfaceDesc&, const NkGraphicsContextConfig&);
            static NkGraphicsContextPtr CreateDirectX (const NkSurfaceDesc&, const NkGraphicsContextConfig&, bool dx12);
            static NkGraphicsContextPtr CreateSoftware(const NkSurfaceDesc&, const NkGraphicsContextConfig&);
    };

} // namespace nkentseu