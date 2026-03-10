#pragma once
// =============================================================================
// NkSurfaceHint.h
// Hints opaques transmis dans NkWindowConfig AVANT la création de la fenêtre,
// et retranscrits dans NkSurfaceDesc APRÈS pour que le contexte graphique
// sache ce qui a été appliqué.
//
// NkWindow ne sait PAS ce que ces hints signifient.
// Il les transmet au backend platform (XLib, XCB…) qui les applique
// de façon mécanique lors de XCreateWindow / SetPixelFormat.
//
// Qui remplit les hints ?
//   → NkGraphicsContextFactory::PrepareWindowConfig()
//      appelé par l'utilisateur AVANT NkWindow::Create()
//
// Qui consomme les hints ?
//   → NkXLibWindow.cpp / NkXCBWindow.cpp  (pour GlxVisualId)
//   → NkOpenGLContext::Init()             (pour récupérer le FBConfig)
//
// Tous les autres backends (Vulkan, Metal, DirectX, EGL, Software)
// n'ont besoin d'aucun hint pré-création.
// =============================================================================

#include "NkTypes.h"   // uint32, uintptr (= uintptr_t)
#include "NKContainers/CacheFriendly/NkArray.h"

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // NkSurfaceHintKey — identifiants opaques de hint
    // NkWindow voit ces clés comme de simples entiers.
    // ---------------------------------------------------------------------------
    enum class NkSurfaceHintKey : uint32 {
        NK_NONE            = 0,

        // GLX — XLib/XCB
        NK_GLX_VISUAL_ID     = 0x0100, ///< VisualID choisi par glXChooseFBConfig
        NK_GLX_FB_CONFIG_PTR  = 0x0101, ///< GLXFBConfig* (opaque, castée en uintptr_t)

        // EGL — Wayland / Android / X11 avec EGL
        NK_EGL_DISPLAY      = 0x0200, ///< EGLDisplay (uintptr_t)
        NK_EGL_CONFIG       = 0x0201, ///< EGLConfig  (uintptr_t)

        // WGL — Win32
        // WGL n'a pas de contrainte de création sur HWND, pixel format
        // est défini après — aucun hint nécessaire.

        // Metal — CAMetalLayer déjà créée par NkCocoaWindow — aucun hint
        // Vulkan — surface créée après — aucun hint
        // DirectX — HWND suffisant — aucun hint
        // Software — aucun hint

        NK_MAX_HINTS        = 8,
    };

    // ---------------------------------------------------------------------------
    // NkSurfaceHint — une paire (clé, valeur opaque)
    // ---------------------------------------------------------------------------
    struct NkSurfaceHint {
        NkSurfaceHintKey key   = NkSurfaceHintKey::NK_NONE;
        uintptr           value = 0;   ///< uintptr = uintptr_t
    };

    // ---------------------------------------------------------------------------
    // NkSurfaceHints — tableau compact, zéro allocation dynamique
    // ---------------------------------------------------------------------------
    struct NkSurfaceHints {
        static constexpr uint32 kMaxHints = 8;

        NkArray<NkSurfaceHint, kMaxHints> hints{};
        uint32                                count = 0;

        /// Ajoute ou remplace un hint.
        void Set(NkSurfaceHintKey key, uintptr value) {
            for (uint32 i = 0; i < count; ++i) {
                if (hints[i].key == key) { hints[i].value = value; return; }
            }
            if (count < kMaxHints)
                hints[count++] = { key, value };
        }

        /// Retourne la valeur d'un hint, ou defaultVal si absent.
        uintptr Get(NkSurfaceHintKey key, uintptr defaultVal = 0) const {
            for (uint32 i = 0; i < count; ++i)
                if (hints[i].key == key) return hints[i].value;
            return defaultVal;
        }

        bool Has(NkSurfaceHintKey key) const {
            for (uint32 i = 0; i < count; ++i)
                if (hints[i].key == key) return true;
            return false;
        }

        void Clear() { hints = {}; count = 0; }
    };

} // namespace nkentseu
