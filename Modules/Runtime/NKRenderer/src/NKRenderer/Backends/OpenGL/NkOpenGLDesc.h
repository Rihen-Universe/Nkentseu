#pragma once
// =============================================================================
// NkOpenGLDesc.h
// Descripteur de contexte OpenGL — séparé de NkContextDesc pour la lisibilité.
//
// Intègre :
//   - Paramètres du contexte core (version, profil, flags)
//   - Paramètres du framebuffer (couleur, depth, stencil, MSAA, sRGB)
//   - PIXELFORMATDESCRIPTOR bootstrap WGL (NkWGLFallbackPixelFormat)
//   - Hints GLX spécifiques pour Linux XLib/XCB
//   - Hints EGL pour Wayland/Android
//   - Options GLAD2 (loader)
//
// GLAD2 — rôle dans le pipeline :
//   WGL : gladLoadWGL(hdc)          → charge les extensions WGL ARB
//         gladLoadGL(wglGetProcAddress) → charge toutes les fonctions GL
//   GLX : gladLoadGLX(display, screen)
//         gladLoadGL(glXGetProcAddress)
//   EGL : gladLoadEGL(display)
//         gladLoadGLES2(eglGetProcAddress) ou gladLoadGL
//   Web : gladLoadGLES2 via Emscripten
//
// Génération GLAD2 (https://gen.glad.dav1d.de/) :
//   - API        : gl (OpenGL) ou gles2 (OpenGL ES)
//   - Spec       : gl
//   - Extensions : WGL_ARB_create_context, WGL_ARB_pixel_format,
//                  WGL_EXT_swap_control, WGL_ARB_framebuffer_sRGB,
//                  WGL_ARB_multisample,
//                  GLX_ARB_create_context, GLX_ARB_multisample,
//                  GLX_EXT_swap_control
//   - Generator  : C/C++
//   - Options    : Loader=On, MX=Off (ou On pour multi-contextes)
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NkWGLPixelFormat.h"

namespace nkentseu {

    // -------------------------------------------------------------------------
    // NkGLProfile — profil de contexte OpenGL
    // -------------------------------------------------------------------------
    enum class NkGLProfile : uint32 {
        Core          = 0,  // Profil Core — supprime les fonctionnalités dépréciées
        Compatibility = 1,  // Profil Compatibility — toutes les fonctionnalités
        ES            = 2,  // OpenGL ES (EGL / Android / WASM)
    };

    // -------------------------------------------------------------------------
    // NkGLContextFlags — flags de création du contexte
    // -------------------------------------------------------------------------
    enum class NkGLContextFlags : uint32 {
        NoneFlag        = 0,
        Debug           = 1 << 0,  // GL_KHR_debug — messages d'erreur détaillés
        ForwardCompat   = 1 << 1,  // Supprime les fonctions dépréciées (Core seulement)
        RobustAccess    = 1 << 2,  // GL_ARB_robustness — protection contre les UB
        NoError         = 1 << 3,  // GL_KHR_no_error — désactive les checks GL (perf)
    };

    inline NkGLContextFlags operator|(NkGLContextFlags a, NkGLContextFlags b) {
        return static_cast<NkGLContextFlags>(
            static_cast<uint32>(a) | static_cast<uint32>(b));
    }
    
    inline bool HasFlag(NkGLContextFlags set, NkGLContextFlags flag) {
        return (static_cast<uint32>(set) & static_cast<uint32>(flag)) != 0;
    }

    // -------------------------------------------------------------------------
    // NkGLSwapInterval — contrôle VSync
    // -------------------------------------------------------------------------
    enum class NkGLSwapInterval : int32 {
        Immediate     = 0,  // Pas de VSync — tearing possible
        VSync         = 1,  // VSync classique — 60 Hz
        AdaptiveVSync = -1, // VSync adaptatif (WGL_EXT_swap_control_tear)
                            // Si non supporté, fallback sur Immediate
    };

    // -------------------------------------------------------------------------
    // NkGLXHints — hints GLX spécifiques (Linux XLib/XCB uniquement)
    // Configurent glXChooseFBConfig au-delà des paramètres standards.
    // -------------------------------------------------------------------------
    struct NkGLXHints {
        bool  transparentWindow  = false;  // GLX_TRANSPARENT_TYPE = GLX_TRANSPARENT_RGB
        bool  stereoRendering    = false;  // GLX_STEREO — rendu stéréoscopique
        bool  floatingPointFB    = false;  // GLX_RENDER_TYPE = GLX_RGBA_FLOAT_BIT_ARB
        int32 redBits            = 8;
        int32 greenBits          = 8;
        int32 blueBits           = 8;
        int32 alphaBits          = 8;
        // Drawable types autorisés
        bool  allowWindow        = true;   // GLX_WINDOW_BIT
        bool  allowPixmap        = false;  // GLX_PIXMAP_BIT
        bool  allowPbuffer       = false;  // GLX_PBUFFER_BIT
    };

    // -------------------------------------------------------------------------
    // NkEGLHints — hints EGL spécifiques (Wayland / Android / iOS)
    // -------------------------------------------------------------------------
    struct NkEGLHints {
        int32  redBits           = 8;
        int32  greenBits         = 8;
        int32  blueBits          = 8;
        int32  alphaBits         = 8;
        bool   bindToTextureRGBA = false;  // EGL_BIND_TO_TEXTURE_RGBA
        bool   bindToTextureRGB  = false;  // EGL_BIND_TO_TEXTURE_RGB
        // Surface type
        bool   pbufferSurface    = false;  // EGL_PBUFFER_BIT en plus de WINDOW
        // Conformant
        bool   openGLConformant  = false;  // EGL_OPENGL_BIT (desktop GL via EGL)
    };

    // -------------------------------------------------------------------------
    // NkOpenGLDesc — descripteur complet du contexte OpenGL
    // -------------------------------------------------------------------------
    struct NkOpenGLDesc {

        // === Contexte =========================================================

        int32          majorVersion  = 4;
        int32          minorVersion  = 6;
        NkGLProfile    profile       = NkGLProfile::Core;
        NkGLContextFlags contextFlags = NkGLContextFlags::ForwardCompat;

        // === Framebuffer ======================================================

        int32  colorBits        = 32;   ///< R+G+B+A total (en pratique 32 = RGBA8)
        int32  redBits          = 8;
        int32  greenBits        = 8;
        int32  blueBits         = 8;
        int32  alphaBits        = 8;
        int32  depthBits        = 24;
        int32  stencilBits      = 8;
        int32  msaaSamples      = 0;    ///< 0 = désactivé, 2/4/8/16 = MSAA
        bool   srgbFramebuffer  = true; ///< GL_FRAMEBUFFER_SRGB
        bool   doubleBuffer     = true;
        bool   accumBuffer      = false; ///< Accumulation buffer (obsolète, rare)

        // === VSync ============================================================

        NkGLSwapInterval swapInterval = NkGLSwapInterval::VSync;

        // === GLAD2 ============================================================
        // Options du loader GLAD2.

        struct Glad2Options {
            /// Si true, GLAD2 est initialisé automatiquement par NkOpenGLContext.
            /// Si false, l'appelant initialise GLAD2 manuellement après Initialize().
            /// → Utile si tu veux contrôler le moment exact du chargement.
            bool autoLoad = true;

            /// Vérifier que la version GL demandée est bien disponible après chargement.
            bool validateVersion = true;

            /// Activer le debug callback GL via GLAD2 (nécessite Debug dans contextFlags).
            /// GLAD2 installe automatiquement glDebugMessageCallback.
            bool installDebugCallback = true;

            /// Niveau de sévérité minimum pour le debug callback GLAD2
            /// 0 = tout, 1 = low+, 2 = medium+, 3 = high seulement
            uint32 debugSeverityLevel = 2;
        } glad2;

        // === WGL (Windows uniquement) =========================================
        // PIXELFORMATDESCRIPTOR du contexte bootstrap.
        // Voir NkWGLFallbackPixelFormat pour la doc complète.
        NkWGLFallbackPixelFormat wglFallback{};

        // === GLX (Linux XLib/XCB uniquement) ==================================
        NkGLXHints glxHints;

        // === EGL (Wayland/Android/iOS) ========================================
        NkEGLHints eglHints;

        // === Presets rapides ==================================================

        static NkOpenGLDesc Desktop46(bool debug = false) {
            NkOpenGLDesc d;
            d.majorVersion = 4; d.minorVersion = 6;
            d.profile = NkGLProfile::Core;
            d.contextFlags = NkGLContextFlags::ForwardCompat
                           | (debug ? NkGLContextFlags::Debug : NkGLContextFlags::NoneFlag);
            d.glad2.installDebugCallback = debug;
            return d;
        }

        static NkOpenGLDesc Desktop33(bool debug = false) {
            NkOpenGLDesc d;
            d.majorVersion = 3; d.minorVersion = 3;
            d.profile = NkGLProfile::Core;
            d.contextFlags = NkGLContextFlags::ForwardCompat
                           | (debug ? NkGLContextFlags::Debug : NkGLContextFlags::NoneFlag);
            return d;
        }

        static NkOpenGLDesc ES32() {
            NkOpenGLDesc d;
            d.majorVersion = 3; d.minorVersion = 2;
            d.profile   = NkGLProfile::ES;
            d.contextFlags = NkGLContextFlags::NoneFlag;
            d.glad2.installDebugCallback = false;
            return d;
        }

        static NkOpenGLDesc WithMSAA(int samples = 4) {
            NkOpenGLDesc d = Desktop46();
            d.msaaSamples = samples;
            return d;
        }
    };

} // namespace nkentseu
