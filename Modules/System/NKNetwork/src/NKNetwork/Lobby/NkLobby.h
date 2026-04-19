#pragma once
// =============================================================================
// NKNetwork/Lobby/NkLobby.h
// =============================================================================
// Système de lobby, sessions et matchmaking.
//
// CONCEPTS :
//   NkSession      — une partie en cours ou en préparation
//   NkLobby        — salle d'attente avant le début d'une partie
//   NkLobbyPlayer  — joueur dans un lobby
//   NkMatchmaker   — algorithme de matchmaking (ELO, ping, région)
//   NkDiscovery    — découverte de serveurs en LAN
//
// FLUX TYPIQUE :
//   1. Client → NkLobby::Create("2v2 Ranked") ou NkLobby::Join(lobbyId)
//   2. Lobby plein → NkSession::Start()
//   3. NkConnectionManager::Connect(serverAddr)
//   4. Game loop avec NkNetWorld
//   5. NkSession::End() → retour au lobby ou déconnexion
//
// DÉCOUVERTE LAN :
//   NkDiscovery::Broadcast() → envoie UDP broadcast sur le réseau local
//   NkDiscovery::Listen()   → écoute les broadcasts, liste les serveurs
// =============================================================================

#include "Protocol/NkConnection.h"

namespace nkentseu {
    namespace net {

        // =====================================================================
        // NkSessionState — état d'une session
        // =====================================================================
        enum class NkSessionState : uint8 {
            Idle,       ///< Pas encore créée
            Lobby,      ///< En attente de joueurs
            Loading,    ///< Chargement de la map
            InProgress, ///< Partie en cours
            Ended,      ///< Partie terminée
        };

        // =====================================================================
        // NkPlayerInfo — informations publiques d'un joueur
        // =====================================================================
        struct NkPlayerInfo {
            static constexpr uint32 kMaxName = 64u;

            NkPeerId  peerId;
            char      displayName[kMaxName] = {};
            uint32    elo     = 1000;    ///< Rating ELO
            uint32    ping    = 0;       ///< RTT en ms
            bool      isHost  = false;   ///< Est le host/serveur
            bool      isReady = false;
            uint8     team    = 0;       ///< Équipe (0=non assigné)
            uint32    slot    = 0;       ///< Slot dans le lobby
        };

        // =====================================================================
        // NkSessionConfig — configuration d'une session
        // =====================================================================
        struct NkSessionConfig {
            static constexpr uint32 kMaxName = 128u;
            char    name[kMaxName]  = {};
            char    mapName[kMaxName] = {};
            char    gameMode[64]    = {};
            uint32  maxPlayers      = 8;
            uint32  minPlayers      = 2;
            bool    isPrivate       = false;
            bool    isRanked        = false;
            char    password[64]    = {};  ///< Vide = pas de mot de passe
            uint16  port            = 7777;

            // Matchmaking
            uint32  minELO          = 0;
            uint32  maxELO          = UINT32_MAX;
            float32 maxPingMs       = 200.f;
            NkString region;                   ///< "EU", "NA", "ASIA"...
        };

        // =====================================================================
        // NkSession — une partie (lobby + in-game)
        // =====================================================================
        class NkSession {
        public:
            NkSession()  noexcept = default;
            ~NkSession() noexcept = default;

            // ── Création / Rejoindre ──────────────────────────────────────────
            /**
             * @brief Crée une nouvelle session (devient host).
             * Lance un serveur UDP sur la configuration donnée.
             */
            [[nodiscard]] NkNetResult Create(const NkSessionConfig& cfg) noexcept;

            /**
             * @brief Rejoint une session existante.
             * @param addr     Adresse du serveur/host.
             * @param password Mot de passe si requis.
             */
            [[nodiscard]] NkNetResult Join(const NkAddress& addr,
                                            const char* password = nullptr) noexcept;

            /**
             * @brief Quitte la session proprement.
             */
            void Leave() noexcept;

            // ── Contrôle (host uniquement) ────────────────────────────────────
            /**
             * @brief Lance la partie (host seulement).
             * Notifie tous les joueurs de commencer à charger.
             */
            [[nodiscard]] NkNetResult Start() noexcept;

            /**
             * @brief Termine la partie.
             */
            void End() noexcept;

            /**
             * @brief Expulse un joueur (host uniquement).
             */
            void Kick(NkPeerId peer, const char* reason = nullptr) noexcept;

            /**
             * @brief Signale que ce joueur est prêt (client).
             */
            void SetReady(bool ready) noexcept;

            // ── État ──────────────────────────────────────────────────────────
            [[nodiscard]] NkSessionState    GetState()       const noexcept { return mState; }
            [[nodiscard]] bool              IsHost()         const noexcept { return mIsHost; }
            [[nodiscard]] bool              IsInProgress()   const noexcept { return mState == NkSessionState::InProgress; }
            [[nodiscard]] uint32            GetPlayerCount() const noexcept { return mPlayerCount; }
            [[nodiscard]] const NkSessionConfig& GetConfig() const noexcept { return mConfig; }
            [[nodiscard]] const NkPlayerInfo* FindPlayer(NkPeerId peer) const noexcept;
            [[nodiscard]] NkConnectionManager* GetConnMgr() noexcept { return &mConnMgr; }

            // ── Callbacks ─────────────────────────────────────────────────────
            using PlayerJoinCb  = NkFunction<void(const NkPlayerInfo&)>;
            using PlayerLeaveCb = NkFunction<void(NkPeerId, const char* reason)>;
            using StateChangeCb = NkFunction<void(NkSessionState)>;

            PlayerJoinCb   onPlayerJoined;
            PlayerLeaveCb  onPlayerLeft;
            StateChangeCb  onStateChanged;

        private:
            NkSessionState   mState   = NkSessionState::Idle;
            NkSessionConfig  mConfig;
            NkConnectionManager mConnMgr;
            bool             mIsHost  = false;

            static constexpr uint32 kMaxPlayers = 64u;
            NkPlayerInfo  mPlayers[kMaxPlayers] = {};
            uint32        mPlayerCount = 0;
        };

        // =====================================================================
        // NkLobby — salle d'attente avec chat et configuration
        // =====================================================================
        class NkLobby {
        public:
            NkLobby()  noexcept = default;
            ~NkLobby() noexcept = default;

            /**
             * @brief Accès global (singleton par application).
             */
            [[nodiscard]] static NkLobby& Global() noexcept {
                static NkLobby instance;
                return instance;
            }

            // ── Gestion de la session ─────────────────────────────────────────
            [[nodiscard]] NkNetResult CreateSession(const NkSessionConfig& cfg) noexcept {
                return mSession.Create(cfg);
            }
            [[nodiscard]] NkNetResult JoinSession(const NkAddress& addr,
                                                   const char* pw = nullptr) noexcept {
                return mSession.Join(addr, pw);
            }
            void LeaveSession() noexcept { mSession.Leave(); }

            [[nodiscard]] NkSession& GetSession() noexcept { return mSession; }

            // ── Chat ──────────────────────────────────────────────────────────
            /**
             * @brief Envoie un message dans le chat lobby.
             */
            void SendChatMessage(const char* msg) noexcept;

            struct ChatMessage {
                char       sender[64]  = {};
                char       text[256]   = {};
                NkTimestampMs at       = 0;
            };

            using ChatCb = NkFunction<void(const ChatMessage&)>;
            ChatCb onChatMessage;

            // ── Configuration (host) ──────────────────────────────────────────
            void SetMap    (const char* mapName) noexcept;
            void SetGameMode(const char* mode)   noexcept;
            void SetTeam   (NkPeerId peer, uint8 team) noexcept;

        private:
            NkSession mSession;
        };

        // =====================================================================
        // NkMatchmaker — recherche de partie par critères
        // =====================================================================
        class NkMatchmaker {
        public:
            struct SearchParams {
                NkString   gameMode;
                uint32     myELO       = 1000;
                float32    maxPingMs   = 150.f;
                NkString   region;
                uint32     playerCount = 1;   ///< Groupe de N joueurs
                bool       ranked      = true;
            };

            struct SearchResult {
                NkAddress  serverAddr;
                char       sessionName[128] = {};
                uint32     playerCount = 0;
                uint32     maxPlayers  = 0;
                uint32     avgPing     = 0;
                uint32     avgELO      = 1000;
                float32    score       = 0.f;  ///< Qualité du match [0..1]
            };

            /**
             * @brief Démarre une recherche de partie asynchrone.
             * @param params  Critères de recherche.
             * @param onFound Appelé quand un match est trouvé.
             * @param onError Appelé en cas d'erreur ou timeout.
             */
            void SearchAsync(const SearchParams& params,
                              NkFunction<void(const SearchResult&)> onFound,
                              NkFunction<void(NkNetResult)> onError,
                              float32 timeoutSec = 30.f) noexcept;

            /**
             * @brief Annule la recherche en cours.
             */
            void CancelSearch() noexcept;

            [[nodiscard]] bool IsSearching() const noexcept { return mSearching; }

            /**
             * @brief Enregistre ce serveur dans le matchmaking.
             * (Côté serveur dédié ou host)
             */
            NkNetResult RegisterServer(const NkSessionConfig& cfg,
                                        const char* matchmakerUrl) noexcept;

            NkNetResult UnregisterServer() noexcept;

        private:
            bool    mSearching = false;
        };

        // =====================================================================
        // NkDiscovery — découverte LAN
        // =====================================================================
        class NkDiscovery {
        public:
            struct ServerInfo {
                NkAddress   addr;
                char        name[128]    = {};
                char        gameMode[64] = {};
                uint32      playerCount  = 0;
                uint32      maxPlayers   = 0;
                uint32      ping         = 0;
                bool        hasPassword  = false;
            };

            /**
             * @brief (Serveur) Diffuse la présence du serveur sur le LAN.
             * Envoie un broadcast UDP sur le port donné.
             */
            static NkNetResult Broadcast(const ServerInfo& info,
                                          uint16 broadcastPort = 7778) noexcept;

            /**
             * @brief (Client) Écoute les broadcasts et liste les serveurs.
             * @param timeoutMs  Durée d'écoute en ms (bloquant).
             * @param out        Liste des serveurs trouvés.
             */
            static NkNetResult Listen(uint32 timeoutMs,
                                       NkVector<ServerInfo>& out,
                                       uint16 broadcastPort = 7778) noexcept;
        };

    } // namespace net
} // namespace nkentseu
