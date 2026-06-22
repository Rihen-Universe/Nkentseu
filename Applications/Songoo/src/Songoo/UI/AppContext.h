#pragma once
// =============================================================================
// AppContext.h — Contexte partagé transmis à chaque Scène Songo'o
// Copie de la structure AppContext existante dans Nkentseu/Songoo,
// sans le réseau (non utilisé par Songo'o actuellement).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "Songoo/Render/SafeArea.h"

namespace nkentseu { class NkWindow; }

namespace nkentseu { namespace songoo {

    class GLRenderer2D;
    class FontAtlas;
    struct GameSettings;
    class SceneManager;
    class AudioManager;

    struct AppContext {
        NkWindow*      window       = nullptr;
        GLRenderer2D*  renderer     = nullptr;
        FontAtlas*     font         = nullptr;
        GameSettings*  settings     = nullptr;
        SceneManager*  scenes       = nullptr;
        AudioManager*  audio        = nullptr;

        int            viewportW    = 0;
        int            viewportH    = 0;
        float          globalTime   = 0.0f;
        SafeArea       safe;

        bool*          quitRequested = nullptr;
    };

}} // namespace nkentseu::songoo
