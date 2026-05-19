#pragma once
// =============================================================================
// AppContext.h
// -----------------------------------------------------------------------------
// Contexte partage transmis a chaque Scene. Regroupe les references vers les
// sous-systemes du jeu pour eviter une cascade de getters dans les scenes.
//
// Toutes les references sont detenues par PongApp (proprietaire). AppContext
// est purement un POD de pointeurs/references — pas de proprietes.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "Pong/Render/SafeArea.h"

namespace nkentseu
{
    class NkWindow;
}

namespace nkentseu
{
    namespace pong
    {

        class GLRenderer2D;
        class FontAtlas;
        struct GameSettings;
        class SceneManager;
        class NetworkSession;

        // ─────────────────────────────────────────────────────────────────────
        // AppContext — passe a chaque Scene::OnUpdate/OnRender/OnEnter.
        // ─────────────────────────────────────────────────────────────────────
        struct AppContext
        {
            NkWindow*       window     = nullptr;
            GLRenderer2D*   renderer   = nullptr;
            FontAtlas*      font       = nullptr;
            GameSettings*   settings   = nullptr;
            SceneManager*   scenes     = nullptr;
            /// Reseau (LAN/Online). nullptr si pas initialise. Detenu par
            /// PongApp pour toute la duree de l'app. Les scenes l'utilisent
            /// pour piloter Host/Join + envoi/reception messages.
            NetworkSession* network    = nullptr;

            int           viewportW  = 0;
            int           viewportH  = 0;

            float         globalTime = 0.0f;   ///< Temps absolu depuis Init (secondes)

            SafeArea      safe;                ///< Recalcule chaque frame

            /// Pointeur vers le flag "quit demande" detenu par PongApp. Une
            /// scene peut mettre *quitRequested = true (item "Quitter") pour
            /// declencher la sortie au prochain frame. Peut etre nullptr.
            bool*         quitRequested = nullptr;
        };

    } // namespace pong
} // namespace nkentseu
