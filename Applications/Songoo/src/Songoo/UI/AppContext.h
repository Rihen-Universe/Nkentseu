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
#include "Songoo/Render/SafeArea.h"

namespace nkentseu
{
    class NkWindow;
}

namespace nkentseu
{
    namespace songoo
    {

        class GLRenderer2D;
        class FontAtlas;
        struct GameSettings;
        class SceneManager;
        class NetworkSession;
        class NetworkDiscovery;
        class AudioManager;

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
            /// Decouverte LAN (beacon broadcast UDP + scan). Detenu par
            /// PongApp. NetworkLobbyScene appelle StartBeacon (cote host) ou
            /// StartScan (cote client) puis lit GetHosts() pour lister.
            NetworkDiscovery* discovery = nullptr;
            /// Gestionnaire audio NKAudio (SFX procedural). Les scenes
            /// appellent ctx.audio->PlayPaddle(), PlayScore(), PlayMenu()...
            /// nullptr si l'init a echoue (no-op silencieux).
            AudioManager*     audio     = nullptr;

            int           viewportW  = 0;
            int           viewportH  = 0;

            float         globalTime = 0.0f;   ///< Temps absolu depuis Init (secondes)

            SafeArea      safe;                ///< Recalcule chaque frame

            /// Pointeur vers le flag "quit demande" detenu par PongApp. Une
            /// scene peut mettre *quitRequested = true (item "Quitter") pour
            /// declencher la sortie au prochain frame. Peut etre nullptr.
            bool*         quitRequested = nullptr;

            // ── Identite reseau du joueur local (Pays/Ville-Code) ────────────
            // Genere une fois au demarrage de PongApp via
            // africa::PickRandomCountryCityCode. Affiche dans le
            // NetworkLobbyScene (notre carte de visite envers les autres
            // joueurs) et envoye via PktHello aux pairs.
            // Format affichable : "Cameroun/Douala-123456789".
            // Le code 9 chiffres reduit le risque de collision a quasi-zero
            // (1 milliard de combinaisons en plus des ~550 pays/villes).
            // Garanti null-terminated.
            char myCountry[32] = { 0 };
            char myCity[32]    = { 0 };
            char myCode[16]    = { 0 };  ///< "%09u" zero-padded
        };

    } // namespace songoo
} // namespace nkentseu
