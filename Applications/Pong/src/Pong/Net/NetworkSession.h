#pragma once
// =============================================================================
// NetworkSession.h
// -----------------------------------------------------------------------------
// Wrapper autour de NkConnectionManager pour le mode multijoueur LAN du Pong.
//
// Etats :
//   - Idle         : aucun socket actif (init ou post-shutdown)
//   - Hosting      : serveur a demarre StartServer(), en attente d'un peer
//   - Joining      : client a appele Connect(), handshake en cours
//   - Connected    : >= 1 peer connecte cote serveur, OU client handshake OK
//   - Disconnected : ancienne connexion perdue / erreur
//
// L'instance vit pour toute la duree de l'app (cree par PongApp, accessible
// via AppContext::network). Les scenes (NetworkLobbyScene, GameplayScene)
// l'utilisent pour piloter le reseau.
//
// Note : pour la Phase 1 on n'implemente pas le sync gameplay — uniquement
// le lobby et la connexion. La Phase 2 ajoutera les messages d'input et
// l'authoritative server side.
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"
#include <atomic>

// Forward declarations pour eviter de polluer le header.
namespace nkentseu { namespace net {
    class NkConnectionManager;
    struct NkReceiveMsg;
    struct NkAddress;
}}

namespace nkentseu
{
    namespace pong
    {

        enum class NetworkRole : uint8
        {
            None,
            Host,    ///< Cote serveur — heberge le match
            Client,  ///< Cote client — rejoint un match
        };

        enum class NetworkState : uint8
        {
            Idle,           ///< Pas demarre / arrete proprement
            Hosting,        ///< StartServer() OK, en attente d'un peer
            Joining,        ///< Connect() en cours, handshake non termine
            Connected,      ///< Au moins 1 peer connecte (les 2 cotes)
            Disconnected,   ///< Connexion perdue (peer parti / timeout)
        };

        class NetworkSession
        {
        public:
            NetworkSession();
            ~NetworkSession();

            // ── Init / Shutdown ─────────────────────────────────────────────
            /// Initialise les sockets de la plateforme. Doit etre appele une
            /// fois au demarrage de l'app (par PongApp). Idempotent.
            static void PlatformInit();
            /// Liberation symetrique de PlatformInit. Appel par PongApp::Shutdown.
            static void PlatformShutdown();

            // ── Lifecycle session ───────────────────────────────────────────
            /// Demarre en mode HOST : ecoute sur @p port pour 1 client max.
            /// Retourne true en cas de succes. Le state passe a Hosting.
            bool StartHost(uint16 port = 7777);
            /// Demarre en mode CLIENT : tente de rejoindre @p ip:@p port.
            /// State -> Joining puis Connected si handshake OK.
            bool StartJoin(const char* ip, uint16 port = 7777);
            /// Coupe la session courante (deconnecte, ferme socket).
            void Shutdown();

            /// Doit etre appele chaque frame (par PongApp). Draine la file
            /// des messages, met a jour le state machine.
            void Tick(float dt);

            // ── Accesseurs ──────────────────────────────────────────────────
            NetworkState State()     const noexcept { return mState.load(); }
            NetworkRole  Role()      const noexcept { return mRole; }
            int          PeerCount() const noexcept { return mPeerCount.load(); }
            const char*  LastError() const noexcept { return mLastError; }

            /// Envoie un buffer brut a tous les peers (ou au serveur si client).
            /// Reliable Ordered par defaut. @p ch optionnel pour unreliable.
            bool Broadcast(const uint8* data, uint32 size,
                           uint8 reliable = 1);

            /// Vide la file de messages recus depuis la derniere frame.
            /// L'appelant traite chaque message (input paddle, ball state, etc.).
            void DrainReceived(NkVector<net::NkReceiveMsg>& out);

        private:
            // Cache pointeur pour eviter d'inclure le header lourd ici.
            net::NkConnectionManager* mConnMgr = nullptr;
            // State + peerCount sont modifies depuis le thread reseau (via
            // callbacks NkConnectionManager) et lus depuis le thread jeu —
            // d'ou std::atomic pour eviter les data races.
            std::atomic<NetworkState> mState     { NetworkState::Idle };
            std::atomic<int>          mPeerCount { 0 };
            NetworkRole               mRole      = NetworkRole::None;
            // Timeout connexion (passe a Disconnected si Joining trop long).
            float                     mJoinElapsed = 0.0f;
            static constexpr float    kJoinTimeoutSec = 8.0f;
            // Heartbeat de debug log : intervalle entre 2 messages de status.
            float                     mDebugLogElapsed = 0.0f;
            char                      mLastError[128] = { 0 };
        };

    } // namespace pong
} // namespace nkentseu
