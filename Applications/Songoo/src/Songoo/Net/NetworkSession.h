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
#include <mutex>

// Forward declarations pour eviter de polluer le header.
namespace nkentseu { namespace net {
    class NkConnectionManager;
    struct NkReceiveMsg;
    struct NkAddress;
    struct NkPeerId;
}}

namespace nkentseu
{
    namespace songoo
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
            Hosting,        ///< StartServer() OK, en attente d'un peer (challengers visibles dans GetChallengers)
            Joining,        ///< Connect() en cours, handshake non termine
            Connected,      ///< Au moins 1 peer connecte (les 2 cotes)
            Disconnected,   ///< Connexion perdue (peer parti / timeout)
        };

        // ─────────────────────────────────────────────────────────────────────
        // ChallengerInfo (Phase B) — challenger qui demande a rejoindre l'host.
        // Cote HOST : la session maintient la liste de tous les peers
        // connectes au transport. L'host doit en choisir UN via
        // AcceptChallenger ; les autres sont rejetes par DisconnectAll.
        // ─────────────────────────────────────────────────────────────────────
        struct ChallengerInfo
        {
            uint64 peerId         = 0;        ///< Identifiant transport (NkPeerId.value)
            char   country[32]    = { 0 };
            char   city[32]       = { 0 };
            char   code[16]       = { 0 };
            bool   hasIdentity    = false;    ///< true des reception de PktHello
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
            /// Les messages internes (kMsgHello) sont filtres ici et stockes
            /// dans mPeerCountry / mPeerCity automatiquement.
            void DrainReceived(NkVector<net::NkReceiveMsg>& out);

            // ── Identite Pays/Ville-Code (Phase A multijoueur) ────────────
            /// Definit l'identifiant local du joueur. Appele une fois par
            /// PongApp::Init apres tirage NkRandom (cf [[pong_multijoueur_lobby_pays_ville]]).
            /// Sera envoye via PktHello a chaque peer des Connected.
            /// Le code 9 chiffres reduit le risque de collision a quasi-zero.
            void SetLocalIdentity(const char* country, const char* city,
                                  const char* code) noexcept;

            /// Identifiant Pays/Ville-Code du peer distant (vide tant qu'on
            /// n'a pas recu son PktHello). Garanti null-terminated.
            const char* PeerCountry() const noexcept { return mPeerCountry; }
            const char* PeerCity()    const noexcept { return mPeerCity;    }
            const char* PeerCode()    const noexcept { return mPeerCode;    }
            /// true des qu'on a recu un Hello (utilisable pour gate l'UI).
            bool HasPeerIdentity() const noexcept { return mPeerCountry[0] != '\0'; }

            // ── Phase B : validation host de N challengers ─────────────────
            /// Liste des challengers actuellement en attente cote HOST.
            /// Vide en mode CLIENT. Re-cree a chaque appel (copie).
            void GetChallengers(NkVector<ChallengerInfo>& out) const noexcept;
            /// Nombre de challengers visibles (pour gate UI rapide).
            int  ChallengerCount() const noexcept;
            /// Accepte le challenger d'id @p peerId : envoie PktAccept, deconnecte
            /// tous les autres avec un PktReject, et passe le state en Connected.
            /// No-op si HOST != ce role ou si peerId pas dans la liste.
            void AcceptChallenger(uint64 peerId) noexcept;
            /// Refuse explicitement un challenger : envoie PktReject + Disconnect.
            /// Le state reste Hosting (autres challengers possibles).
            void RejectChallenger(uint64 peerId) noexcept;

            // ── Phase B : etat cote CLIENT ─────────────────────────────────
            /// true quand on a recu PktAccept du host (apres handshake transport).
            /// Tant que false en mode CLIENT Connected, l'UI doit afficher
            /// "EN ATTENTE DE VALIDATION".
            bool IsValidatedByHost() const noexcept { return mValidatedByHost; }
            /// true si on a recu PktReject (l'host nous a refuse).
            bool WasRejectedByHost() const noexcept { return mRejectedByHost; }

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

            // ── Identite Pays/Ville (Phase A multijoueur) ─────────────────
            // mLocal* : pseudo local injecte par PongApp::Init via
            //   SetLocalIdentity. Envoyes via PktHello des Connected.
            // mPeer*  : pseudo distant recu via PktHello. Vide tant qu'on
            //   n'a rien recu (le filtrage dans DrainReceived consomme le
            //   message et remplit ces champs sans le propager au caller).
            // mHelloSent : evite l'envoi multiple si Connected fluctue.
            char  mLocalCountry[32] = { 0 };
            char  mLocalCity[32]    = { 0 };
            char  mLocalCode[16]    = { 0 };
            char  mPeerCountry[32]  = { 0 };
            char  mPeerCity[32]     = { 0 };
            char  mPeerCode[16]     = { 0 };
            bool  mHelloSent        = false;
            /// Envoie PktHello aux peers (idempotent via mHelloSent).
            void SendHello();

            // ── File interne des messages NON-internes ────────────────────
            // Tick() draine NkConnectionManager a chaque frame, consomme les
            // messages internes (Hello/Accept/Reject) et pousse le reste ici.
            // DrainReceived() renvoie cette file au caller, qui ne voit JAMAIS
            // les messages de protocole transparent (cf bug 2026-05-20 :
            // l'host en Hosting ne drainait pas et perdait les Hello).
            NkVector<net::NkReceiveMsg> mPendingForUser;
            /// Helper : consomme les messages internes + remplit mPendingForUser.
            void DrainInternal();

            // ── Phase B : liste challengers cote HOST + sous-etat cote CLIENT ─
            // Liste protegee par un mutex implicite (toutes les ops viennent du
            // thread game via Tick + DrainReceived). NkConnectionManager fait
            // ses callbacks dans son propre thread, donc les modifs depuis
            // onPeerConnected/onPeerDisconnected sont marquees thread-safe via
            // mPendingMutex (lock minimal pour ne pas blocker).
            mutable std::mutex                 mChallengersMutex;
            NkVector<ChallengerInfo>           mChallengers;
            uint64                             mAcceptedPeerId  = 0;
            // Etats cote CLIENT (gates UI). Mis a jour par DrainReceived.
            bool                               mValidatedByHost = false;
            bool                               mRejectedByHost  = false;
            char                               mRejectReason[64] = { 0 };

            /// Ajoute un challenger (callback NkConnectionManager onPeerConnected).
            void AddChallenger(uint64 peerId) noexcept;
            /// Retire un challenger (callback onPeerDisconnected).
            void RemoveChallenger(uint64 peerId) noexcept;
            /// Met a jour l'identite d'un challenger via PktHello recu.
            void UpdateChallengerIdentity(uint64 peerId,
                                          const char* country,
                                          const char* city,
                                          const char* code) noexcept;
        };

    } // namespace songoo
} // namespace nkentseu
