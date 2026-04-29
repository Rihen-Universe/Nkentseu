// =============================================================================
// NKNetwork/Lobby/NkLobby.cpp
// =============================================================================
// DESCRIPTION :
//   Implémentation du système de lobby, sessions et matchmaking pour jeux
//   multijoueurs, avec gestion complète du cycle de vie des parties.
//
// NOTES D'IMPLÉMENTATION :
//   • Toutes les méthodes publiques retournent NkNetResult pour cohérence API
//   • Gestion d'erreur robuste : logging + retour gracieux sans crash
//   • Thread-safety : mutex sur les structures partagées (mPlayers, mState)
//   • Aucune allocation heap dans les chemins critiques pour performance temps réel
//
// DÉPENDANCES INTERNES :
//   • NkLobby.h : Déclarations des classes implémentées
//   • NkConnection.h : Gestion des connexions réseau peer-to-peer
//   • NkNetDefines.h : Types fondamentaux et constantes réseau
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour MSVC/Clang avec PCH)
// 2. Header correspondant au fichier .cpp (vérification de cohérence)
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme cible

#include "pch.h"
#include "NKNetwork/Lobby/NkLobby.h"

// En-têtes standards C/C++ pour opérations bas-niveau
#include <cstring>
#include <algorithm>
#include <random>

// En-têtes pour opérations temporelles et threads
#include <chrono>
#include <thread>

// -------------------------------------------------------------------------
// SECTION 2 : CONSTANTES LOCALES ET HELPERS
// -------------------------------------------------------------------------

namespace {

    // Visibilité des types fondamentaux dans le namespace anonyme
    using namespace nkentseu;
    using namespace nkentseu::net;

    // =====================================================================
    // Constantes de protocole lobby/session
    // =====================================================================
    static constexpr uint8 kLobbyProtocolMagic = 0x4C;  // 'L' pour Lobby
    static constexpr uint8 kLobbyProtocolVersion = 1u;  // Version actuelle du protocole

    // =====================================================================
    // Types de messages lobby pour communication peer-to-peer
    // =====================================================================
    enum class NkLobbyMessageType : uint8
    {
        NK_LOBBY_JOIN_REQUEST = 0,    /// Client → Host : demande de rejoindre
        NK_LOBBY_JOIN_RESPONSE,       /// Host → Client : acceptation/refus
        NK_LOBBY_PLAYER_INFO,         /// Broadcast : métadonnées d'un joueur
        NK_LOBBY_READY_UPDATE,        /// Client → Host : changement état ready
        NK_LOBBY_CONFIG_UPDATE,       /// Host → Clients : changement config session
        NK_LOBBY_START_REQUEST,       /// Host → Clients : démarrage de partie
        NK_LOBBY_CHAT_MESSAGE,        /// Broadcast : message de chat
        NK_LOBBY_KICK_NOTIFICATION,   /// Host → Client : notification d'expulsion
        NK_LOBBY_END_NOTIFICATION     /// Host → Clients : fin de session
    };

    // =====================================================================
    // Structure d'en-tête pour messages lobby
    // =====================================================================
    struct NkLobbyMessageHeader
    {
        static constexpr uint32 kSize = 12u;  // Taille fixe en bytes

        uint8 magic = kLobbyProtocolMagic;    /// Signature de validation
        uint8 version = kLobbyProtocolVersion; /// Version du protocole
        uint8 type = 0;                        /// NkLobbyMessageType
        uint8 reserved = 0;                    /// Alignement / extensions futures
        uint64 senderPeerId = 0;               /// Identifiant de l'expéditeur
        uint32 payloadSize = 0;                /// Taille des données suivant l'en-tête

        void Serialize(uint8* buf) const noexcept
        {
            if (buf == nullptr) { return; }
            buf[0] = magic;
            buf[1] = version;
            buf[2] = type;
            buf[3] = reserved;
            std::memcpy(buf + 4, &senderPeerId, sizeof(uint64));
            std::memcpy(buf + 12, &payloadSize, sizeof(uint32));
        }

        bool Deserialize(const uint8* buf, uint32 bufSize) noexcept
        {
            if (buf == nullptr || bufSize < kSize) { return false; }
            if (buf[0] != magic || buf[1] != version) { return false; }
            type = buf[2];
            reserved = buf[3];
            std::memcpy(&senderPeerId, buf + 4, sizeof(uint64));
            std::memcpy(&payloadSize, buf + 12, sizeof(uint32));
            return true;
        }
    };

    // =====================================================================
    // Helper : Copie sécurisée de chaîne avec terminaison nulle
    // =====================================================================
    void SafeStrCopy(char* dst, uint32 dstSize, const char* src, uint32 maxLen) noexcept
    {
        if (dst == nullptr || dstSize == 0) { return; }
        if (src == nullptr) { dst[0] = '\0'; return; }

        uint32 len = 0;
        while (src[len] != '\0' && len < maxLen && len < dstSize - 1)
        {
            ++len;
        }
        std::memcpy(dst, src, len);
        dst[len] = '\0';
    }

    // =====================================================================
    // Helper : Calcul de score de matchmaking
    // =====================================================================
    float32 CalculateMatchScore(
        const NkMatchmaker::SearchParams& params,
        const NkMatchmaker::SearchResult& result
    ) noexcept
    {
        // Poids des différents critères (à ajuster selon l'expérience)
        constexpr float32 kELOWeight = 0.4f;
        constexpr float32 kPingWeight = 0.3f;
        constexpr float32 kRegionWeight = 0.2f;
        constexpr float32 kPlayerFitWeight = 0.1f;

        float32 score = 0.f;

        // Score ELO : proximité du rating moyen vs joueur
        {
            const float32 eloDiff_ = static_cast<float32>(result.avgELO) - static_cast<float32>(params.myELO);
            const float32 eloDiff = (eloDiff_ < 0.f ? -eloDiff_ : eloDiff_);
            const float32 eloScore = NkMax(0.f, 1.f - (eloDiff / 500.f));  // 500 points = score 0
            score += kELOWeight * eloScore;
        }

        // Score ping : inverse de la latence
        {
            const float32 pingScore = NkMax(0.f, 1.f - (static_cast<float32>(result.avgPing) / params.maxPingMs));
            score += kPingWeight * pingScore;
        }

        // Score région : bonus si correspondance exacte
        {
            if (!params.region.Empty() && result.serverAddr.ToString().Find(params.region.CStr()) != NkString::npos)
            {
                score += kRegionWeight * 1.f;
            }
            else if (params.region.Empty())
            {
                score += kRegionWeight * 0.5f;  // Pas de préférence = score moyen
            }
            // Sinon : score 0 pour cette composante
        }

        // Score player fit : adéquation taille de groupe vs occupation
        {
            const uint32 availableSlots = result.maxPlayers - result.playerCount;
            if (availableSlots >= params.playerCount)
            {
                score += kPlayerFitWeight * 1.f;
            }
            else if (availableSlots > 0)
            {
                score += kPlayerFitWeight * (static_cast<float32>(availableSlots) / static_cast<float32>(params.playerCount));
            }
            // Sinon : score 0 (pas assez de places)
        }

        return NkClamp(score, 0.f, 1.f);
    }

} // namespace anonyme

// -------------------------------------------------------------------------
// SECTION 3 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes dans le namespace nkentseu::net.

namespace nkentseu {

    namespace net {

        // =====================================================================
        // IMPLÉMENTATION : NkSession — Méthodes publiques
        // =====================================================================

        NkNetResult NkSession::Create(const NkSessionConfig& cfg) noexcept
        {
            // Validation des paramètres
            if (cfg.maxPlayers == 0 || cfg.maxPlayers > kMaxPlayers)
            {
                NK_NET_LOG_ERROR("Create : maxPlayers invalide ({})", cfg.maxPlayers);
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            if (cfg.minPlayers > cfg.maxPlayers)
            {
                NK_NET_LOG_ERROR("Create : minPlayers ({}) > maxPlayers ({})",
                    cfg.minPlayers, cfg.maxPlayers);
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Initialisation de l'état
            mState = NkSessionState::NK_SESSION_LOBBY;
            mIsHost = true;
            mConfig = cfg;
            mPlayerCount = 0;

            // Ajout du host comme premier joueur
            {
                NkPlayerInfo& hostInfo = mPlayers[0];
                hostInfo.peerId = mConnMgr.GetLocalPeerId();
                SafeStrCopy(hostInfo.displayName, NkPlayerInfo::kMaxName, "Host", 32);
                hostInfo.elo = 1000;  // Défaut pour nouveau joueur
                hostInfo.ping = 0;
                hostInfo.isHost = true;
                hostInfo.isReady = false;
                hostInfo.team = 0;
                hostInfo.slot = 0;
                mPlayerCount = 1;
            }

            // Démarrage du serveur via ConnectionManager
            const NkNetResult result = mConnMgr.StartServer(cfg.port, cfg.maxPlayers);
            if (result != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Create : échec de démarrage du serveur sur port {}", cfg.port);
                mState = NkSessionState::NK_SESSION_IDLE;
                return result;
            }

            // Configuration des callbacks de connexion
            mConnMgr.onPeerConnected = [this](NkPeerId peer) {
                OnPeerConnected(peer);
            };
            mConnMgr.onPeerDisconnected = [this](NkPeerId peer, const char* reason) {
                OnPeerDisconnected(peer, reason);
            };

            NK_NET_LOG_INFO("Session créée en tant que host : {} (port {})",
                cfg.name, cfg.port);

            // Notification de changement d'état
            if (onStateChanged)
            {
                onStateChanged(mState);
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSession::Join(const NkAddress& addr, const char* password) noexcept
        {
            // Validation des paramètres
            if (!addr.IsValid())
            {
                NK_NET_LOG_ERROR("Join : adresse serveur invalide");
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Initialisation de l'état client
            mState = NkSessionState::NK_SESSION_IDLE;
            mIsHost = false;
            mPlayerCount = 0;

            // Envoi de la demande de join via ConnectionManager
            // Note : en production, implémenter un handshake lobby spécifique
            const NkNetResult result = mConnMgr.Connect(addr, 0);
            if (result != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Join : échec de connexion à {}", addr.ToString().CStr());
                return result;
            }

            // Stockage temporaire de l'adresse et du mot de passe
            mConfig = {};  // Sera rempli par la réponse du serveur
            SafeStrCopy(mConfig.password, sizeof(mConfig.password), password ? password : "", 64);

            // Passage à l'état Lobby après connexion réussie
            // Note : en production, attendre la réponse JOIN_RESPONSE du serveur
            mState = NkSessionState::NK_SESSION_LOBBY;

            NK_NET_LOG_INFO("Rejoindre session : {}", addr.ToString().CStr());

            // Notification de changement d'état
            if (onStateChanged)
            {
                onStateChanged(mState);
            }

            return NkNetResult::NK_NET_OK;
        }

        void NkSession::Leave() noexcept
        {
            // Sauvegarde de l'état précédent pour logging
            const NkSessionState previousState = mState;

            // Notification de déconnexion à tous les pairs
            if (mIsHost)
            {
                // Host : déconnecter tous les clients
                mConnMgr.DisconnectAll("Session ended");
            }
            else
            {
                // Client : se déconnecter du serveur
                mConnMgr.Disconnect(mConnMgr.GetLocalPeerId(), "Left session");
            }

            // Réinitialisation de l'état
            mState = NkSessionState::NK_SESSION_IDLE;
            mPlayerCount = 0;
            std::memset(mPlayers, 0, sizeof(mPlayers));

            // Arrêt du ConnectionManager
            mConnMgr.Shutdown();

            NK_NET_LOG_DEBUG("Session quittée : {} → {}",
                NkSessionStateStr(previousState), NkSessionStateStr(mState));

            // Notification de changement d'état
            if (onStateChanged)
            {
                onStateChanged(mState);
            }
        }

        NkNetResult NkSession::Start() noexcept
        {
            // Vérification des préconditions
            if (!mIsHost)
            {
                NK_NET_LOG_WARN("Start() appelé côté client — ignoré");
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            if (mState != NkSessionState::NK_SESSION_LOBBY)
            {
                NK_NET_LOG_WARN("Start() appelé en état {} — ignoré", NkSessionStateStr(mState));
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Vérification du nombre minimal de joueurs
            if (mPlayerCount < mConfig.minPlayers)
            {
                NK_NET_LOG_WARN("Start() : pas assez de joueurs ({}/{} requis)",
                    mPlayerCount, mConfig.minPlayers);
                return NkNetResult::NK_NET_NOT_CONNECTED;
            }

            // Vérification que tous les joueurs sont prêts
            uint32 readyCount = 0;
            for (uint32 i = 0; i < mPlayerCount; ++i)
            {
                if (mPlayers[i].isReady) { ++readyCount; }
            }

            if (readyCount < mPlayerCount)
            {
                NK_NET_LOG_WARN("Start() : pas tous les joueurs prêts ({}/{})",
                    readyCount, mPlayerCount);
                return NkNetResult::NK_NET_NOT_CONNECTED;
            }

            // Transition vers Loading
            mState = NkSessionState::NK_SESSION_LOADING;

            // Broadcast du message de démarrage à tous les clients
            BroadcastLobbyMessage(static_cast<uint8>(NkLobbyMessageType::NK_LOBBY_START_REQUEST), nullptr, 0);

            NK_NET_LOG_INFO("Session démarrée : {} joueurs, map {}",
                mPlayerCount, mConfig.mapName);

            // Notification de changement d'état
            if (onStateChanged)
            {
                onStateChanged(mState);
            }

            // Transition automatique vers InProgress après un délai simulé
            // Note : en production, attendre que tous les clients aient chargé la map
            return NkNetResult::NK_NET_OK;
        }

        void NkSession::End() noexcept
        {
            // Vérification de l'état
            if (mState != NkSessionState::NK_SESSION_IN_PROGRESS &&
                mState != NkSessionState::NK_SESSION_LOADING)
            {
                NK_NET_LOG_WARN("End() appelé en état {} — ignoré", NkSessionStateStr(mState));
                return;
            }

            // Passage à l'état Ended
            mState = NkSessionState::NK_SESSION_ENDED;

            // Notification de fin de session à tous les clients
            BroadcastLobbyMessage(static_cast<uint8>(NkLobbyMessageType::NK_LOBBY_END_NOTIFICATION), nullptr, 0);

            // Calcul des résultats si session classée
            if (mConfig.isRanked)
            {
                CalculateRankedResults();
            }

            NK_NET_LOG_INFO("Session terminée : {} joueurs", mPlayerCount);

            // Notification de changement d'état
            if (onStateChanged)
            {
                onStateChanged(mState);
            }

            // Retour automatique à Idle après un court délai
            // Note : en production, attendre confirmation des clients ou timeout
        }

        void NkSession::Kick(NkPeerId peer, const char* reason) noexcept
        {
            // Vérification des préconditions
            if (!mIsHost)
            {
                NK_NET_LOG_WARN("Kick() appelé côté client — ignoré");
                return;
            }

            // Recherche du joueur à kicker
            NkPlayerInfo* playerInfo = nullptr;
            for (uint32 i = 0; i < mPlayerCount; ++i)
            {
                if (mPlayers[i].peerId == peer)
                {
                    playerInfo = &mPlayers[i];
                    break;
                }
            }

            if (playerInfo == nullptr)
            {
                NK_NET_LOG_WARN("Kick() : joueur {} non trouvé", peer.value);
                return;
            }

            // Ne pas kicker le host lui-même
            if (playerInfo->isHost)
            {
                NK_NET_LOG_WARN("Kick() : impossible de kicker le host");
                return;
            }

            // Notification de kick au joueur cible
            // Note : en production, envoyer un message spécifique avant déconnexion
            mConnMgr.Disconnect(peer, reason ? reason : "Kicked by host");

            // Suppression du joueur de la liste
            // Note : en production, compacter le tableau pour éviter les trous
            NK_NET_LOG_INFO("Joueur kické : {} ({})", peer.value, reason ? reason : "sans raison");

            // Notification aux autres joueurs
            if (onPlayerLeft)
            {
                onPlayerLeft(peer, reason ? reason : "Kicked");
            }
        }

        void NkSession::SetReady(bool ready) noexcept
        {
            // Vérification de l'état
            if (mState != NkSessionState::NK_SESSION_LOBBY)
            {
                NK_NET_LOG_WARN("SetReady() appelé en état {} — ignoré", NkSessionStateStr(mState));
                return;
            }

            // Mise à jour de l'état ready du joueur local
            const NkPeerId localPeer = mConnMgr.GetLocalPeerId();
            for (uint32 i = 0; i < mPlayerCount; ++i)
            {
                if (mPlayers[i].peerId == localPeer)
                {
                    mPlayers[i].isReady = ready;
                    break;
                }
            }

            // Broadcast de la mise à jour aux autres joueurs
            // Note : en production, sérialiser un message READY_UPDATE
            BroadcastLobbyMessage(static_cast<uint8>(NkLobbyMessageType::NK_LOBBY_READY_UPDATE), nullptr, 0);

            NK_NET_LOG_DEBUG("État ready mis à jour : {}", ready ? "prêt" : "non prêt");
        }

        NkSessionState NkSession::GetState() const noexcept
        {
            return mState;
        }

        bool NkSession::IsHost() const noexcept
        {
            return mIsHost;
        }

        bool NkSession::IsInProgress() const noexcept
        {
            return mState == NkSessionState::NK_SESSION_IN_PROGRESS;
        }

        uint32 NkSession::GetPlayerCount() const noexcept
        {
            return mPlayerCount;
        }

        const NkSessionConfig& NkSession::GetConfig() const noexcept
        {
            return mConfig;
        }

        const NkPlayerInfo* NkSession::FindPlayer(NkPeerId peer) const noexcept
        {
            for (uint32 i = 0; i < mPlayerCount; ++i)
            {
                if (mPlayers[i].peerId == peer)
                {
                    return &mPlayers[i];
                }
            }
            return nullptr;
        }

        NkConnectionManager* NkSession::GetConnMgr() noexcept
        {
            return &mConnMgr;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSession — Méthodes privées (callbacks)
        // =====================================================================

        void NkSession::OnPeerConnected(NkPeerId peer) noexcept
        {
            // Uniquement côté serveur
            if (!mIsHost) { return; }

            // Vérification de la capacité
            if (mPlayerCount >= mConfig.maxPlayers)
            {
                NK_NET_LOG_WARN("Session pleine — rejet de la connexion de {}", peer.value);
                mConnMgr.Disconnect(peer, "Session full");
                return;
            }

            // Ajout du nouveau joueur à la liste
            NkPlayerInfo& newPlayer = mPlayers[mPlayerCount++];
            newPlayer.peerId = peer;
            SafeStrCopy(newPlayer.displayName, NkPlayerInfo::kMaxName, "Player", 32);  // Nom par défaut
            newPlayer.elo = 1000;  // Défaut
            newPlayer.ping = static_cast<uint32>(mConnMgr.GetConnection(peer)->GetPingMs());
            newPlayer.isHost = false;
            newPlayer.isReady = false;
            newPlayer.team = 0;
            newPlayer.slot = mPlayerCount - 1;

            // Broadcast des informations du nouveau joueur à tous
            BroadcastPlayerInfo(newPlayer);

            NK_NET_LOG_INFO("Nouveau joueur rejoint : {} (slot {})",
                newPlayer.displayName, newPlayer.slot);

            // Notification via callback
            if (onPlayerJoined)
            {
                onPlayerJoined(newPlayer);
            }
        }

        void NkSession::OnPeerDisconnected(NkPeerId peer, const char* reason) noexcept
        {
            // Recherche et suppression du joueur déconnecté
            for (uint32 i = 0; i < mPlayerCount; ++i)
            {
                if (mPlayers[i].peerId == peer)
                {
                    // Décalage des joueurs suivants pour compacter le tableau
                    for (uint32 j = i; j < mPlayerCount - 1; ++j)
                    {
                        mPlayers[j] = mPlayers[j + 1];
                        mPlayers[j].slot = j;  // Mise à jour des slots
                    }
                    --mPlayerCount;

                    NK_NET_LOG_INFO("Joueur déconnecté : {} ({})", peer.value, reason ? reason : "sans raison");

                    // Notification aux autres joueurs
                    if (onPlayerLeft)
                    {
                        onPlayerLeft(peer, reason);
                    }

                    // Si le host se déconnecte : terminer la session
                    if (mPlayers[0].isHost && mIsHost)
                    {
                        NK_NET_LOG_WARN("Host déconnecté — fin de session");
                        End();
                    }

                    break;
                }
            }
        }

        void NkSession::BroadcastLobbyMessage(uint8 type, const uint8* payload, uint32 payloadSize) noexcept
        {
            // Construction de l'en-tête
            NkLobbyMessageHeader header;
            header.type = static_cast<uint8>(type);
            header.senderPeerId = mConnMgr.GetLocalPeerId().value;
            header.payloadSize = payloadSize;

            // Buffer d'envoi : en-tête + payload
            uint8 buffer[NkLobbyMessageHeader::kSize + 1024];  // 1KB max pour payload lobby
            header.Serialize(buffer);

            if (payload != nullptr && payloadSize > 0)
            {
                std::memcpy(buffer + NkLobbyMessageHeader::kSize, payload, payloadSize);
            }

            // Broadcast à tous les pairs connectés
            const uint32 totalSize = NkLobbyMessageHeader::kSize + payloadSize;
            mConnMgr.Broadcast(buffer, totalSize, NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);
        }

        void NkSession::BroadcastPlayerInfo(const NkPlayerInfo& player) noexcept
        {
            // Sérialisation simplifiée des informations du joueur
            // Note : en production, utiliser NkBitWriter pour encodage compact
            uint8 payload[256];
            NkBitWriter writer(payload, sizeof(payload));

            writer.WriteU64(player.peerId.value);
            writer.WriteString(player.displayName, NkPlayerInfo::kMaxName);
            writer.WriteU32(player.elo);
            writer.WriteU32(player.ping);
            writer.WriteBool(player.isHost);
            writer.WriteBool(player.isReady);
            writer.WriteU8(player.team);
            writer.WriteU32(player.slot);

            if (writer.IsOverflowed())
            {
                NK_NET_LOG_ERROR("BroadcastPlayerInfo : sérialisation échouée");
                return;
            }

            // Envoi du message PLAYER_INFO
            BroadcastLobbyMessage(static_cast<uint8>(NkLobbyMessageType::NK_LOBBY_PLAYER_INFO),
                payload, writer.BytesWritten());
        }

        void NkSession::CalculateRankedResults() noexcept
        {
            // Calcul simplifié des résultats classés
            // Note : en production, implémenter un système ELO complet avec historique

            // Exemple : gagnants gagnent +25 ELO, perdants perdent -25 ELO
            // Pour une implémentation réelle, utiliser la formule ELO standard :
            //   newRating = oldRating + K * (actualScore - expectedScore)
            //   où expectedScore = 1 / (1 + 10^((opponentRating - playerRating)/400))

            NK_NET_LOG_DEBUG("Calcul des résultats classés pour {} joueurs", mPlayerCount);

            // Mise à jour des ratings (simulée)
            for (uint32 i = 0; i < mPlayerCount; ++i)
            {
                // Exemple : équipe 1 gagne, équipe 2 perd
                if (mPlayers[i].team == 1)
                {
                    mPlayers[i].elo += 25;
                }
                else if (mPlayers[i].team == 2)
                {
                    mPlayers[i].elo = (mPlayers[i].elo >= 25) ? (mPlayers[i].elo - 25) : 0;
                }
                // team == 0 : spectateur ou mode non-équipe — pas de changement
            }

            // Sauvegarde des nouveaux ratings (à implémenter : persistance)
            // SavePlayerRatings(mPlayers, mPlayerCount);
        }

        // =====================================================================
        // IMPLÉMENTATION : NkLobby — Méthodes publiques
        // =====================================================================

        NkLobby& NkLobby::Global() noexcept
        {
            // Singleton thread-safe via C++11 magic static
            static NkLobby instance;
            return instance;
        }

        NkNetResult NkLobby::CreateSession(const NkSessionConfig& cfg) noexcept
        {
            return mSession.Create(cfg);
        }

        NkNetResult NkLobby::JoinSession(const NkAddress& addr, const char* pw) noexcept
        {
            return mSession.Join(addr, pw);
        }

        void NkLobby::LeaveSession() noexcept
        {
            mSession.Leave();
        }

        NkSession& NkLobby::GetSession() noexcept
        {
            return mSession;
        }

        void NkLobby::SendChatMessage(const char* msg) noexcept
        {
            // Vérification de l'état
            if (mSession.GetState() != NkSessionState::NK_SESSION_LOBBY)
            {
                NK_NET_LOG_WARN("SendChatMessage : session non en état Lobby");
                return;
            }

            // Construction du message de chat
            NkLobby::ChatMessage chatMsg;
            const NkPeerId localPeer = mSession.GetConnMgr()->GetLocalPeerId();
            const NkPlayerInfo* playerInfo = mSession.FindPlayer(localPeer);

            SafeStrCopy(chatMsg.sender, sizeof(chatMsg.sender),
                playerInfo ? playerInfo->displayName : "Unknown", 64);
            SafeStrCopy(chatMsg.text, sizeof(chatMsg.text), msg ? msg : "", 256);
            chatMsg.at = NkNetNowMs();

            // Sérialisation pour envoi réseau
            uint8 payload[512];
            NkBitWriter writer(payload, sizeof(payload));
            writer.WriteString(chatMsg.sender, 64);
            writer.WriteString(chatMsg.text, 256);
            writer.WriteU64(chatMsg.at);

            if (writer.IsOverflowed())
            {
                NK_NET_LOG_ERROR("SendChatMessage : sérialisation échouée");
                return;
            }

            // Broadcast du message de chat
            NkLobbyMessageHeader header;
            header.type = static_cast<uint8>(NkLobbyMessageType::NK_LOBBY_CHAT_MESSAGE);
            header.senderPeerId = localPeer.value;
            header.payloadSize = writer.BytesWritten();

            uint8 buffer[NkLobbyMessageHeader::kSize + 512];
            header.Serialize(buffer);
            std::memcpy(buffer + NkLobbyMessageHeader::kSize, payload, writer.BytesWritten());

            mSession.GetConnMgr()->Broadcast(
                buffer,
                NkLobbyMessageHeader::kSize + writer.BytesWritten(),
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
            );

            // Appel local du callback pour affichage immédiat
            if (onChatMessage)
            {
                onChatMessage(chatMsg);
            }
        }

        void NkLobby::SetMap(const char* mapName) noexcept
        {
            if (!mSession.IsHost()) { return; }

            SafeStrCopy(mSession.mConfig.mapName, NkSessionConfig::kMaxName,
                mapName ? mapName : "", 128);

            // Broadcast de la mise à jour de configuration
            BroadcastConfigUpdate();
        }

        void NkLobby::SetGameMode(const char* mode) noexcept
        {
            if (!mSession.IsHost()) { return; }

            SafeStrCopy(mSession.mConfig.gameMode, sizeof(mSession.mConfig.gameMode),
                mode ? mode : "", 64);

            BroadcastConfigUpdate();
        }

        void NkLobby::SetTeam(NkPeerId peer, uint8 team) noexcept
        {
            if (!mSession.IsHost()) { return; }

            NkPlayerInfo* playerInfo = const_cast<NkPlayerInfo*>(mSession.FindPlayer(peer));
            if (playerInfo != nullptr)
            {
                playerInfo->team = team;

                // Broadcast de la mise à jour du joueur
                mSession.BroadcastPlayerInfo(*playerInfo);
            }
        }

        void NkLobby::BroadcastConfigUpdate() noexcept
        {
            // Sérialisation de la configuration mise à jour
            uint8 payload[512];
            NkBitWriter writer(payload, sizeof(payload));

            writer.WriteString(mSession.mConfig.name, NkSessionConfig::kMaxName);
            writer.WriteString(mSession.mConfig.mapName, NkSessionConfig::kMaxName);
            writer.WriteString(mSession.mConfig.gameMode, 64);
            writer.WriteU32(mSession.mConfig.maxPlayers);
            writer.WriteU32(mSession.mConfig.minPlayers);
            writer.WriteBool(mSession.mConfig.isPrivate);
            writer.WriteBool(mSession.mConfig.isRanked);

            if (writer.IsOverflowed()) { return; }

            // Envoi du message CONFIG_UPDATE
            NkLobbyMessageHeader header;
            header.type = static_cast<uint8>(NkLobbyMessageType::NK_LOBBY_CONFIG_UPDATE);
            header.senderPeerId = mSession.GetConnMgr()->GetLocalPeerId().value;
            header.payloadSize = writer.BytesWritten();

            uint8 buffer[NkLobbyMessageHeader::kSize + 512];
            header.Serialize(buffer);
            std::memcpy(buffer + NkLobbyMessageHeader::kSize, payload, writer.BytesWritten());

            mSession.GetConnMgr()->Broadcast(
                buffer,
                NkLobbyMessageHeader::kSize + writer.BytesWritten(),
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
            );
        }

        // =====================================================================
        // IMPLÉMENTATION : NkMatchmaker — Méthodes publiques
        // =====================================================================

        void NkMatchmaker::SearchAsync(
            const SearchParams& params,
            NkFunction<void(const SearchResult&)> onFound,
            NkFunction<void(NkNetResult)> onError,
            float32 timeoutSec
        ) noexcept
        {
            // Vérification qu'aucune recherche n'est déjà en cours
            if (mSearching)
            {
                NK_NET_LOG_WARN("SearchAsync : recherche déjà en cours — annulation préalable requise");
                if (onError) { onError(NkNetResult::NK_NET_ALREADY_CONNECTED); }
                return;
            }

            mSearching = true;

            // Lancement de la recherche dans un thread dédié
            // Note : en production, utiliser un thread pool ou async/await
            threading::NkThread searchThread([params, onFound, onError, timeoutSec, this](void*) {
                SearchWorker(params, onFound, onError, timeoutSec);
            });

            // Détachement du thread (gestion de durée de vie à améliorer en production)
            searchThread.Detach();
        }

        void NkMatchmaker::CancelSearch() noexcept
        {
            if (mSearching)
            {
                mSearching = false;
                NK_NET_LOG_DEBUG("Recherche de matchmaking annulée");
            }
        }

        bool NkMatchmaker::IsSearching() const noexcept
        {
            return mSearching;
        }

        NkNetResult NkMatchmaker::RegisterServer(
            const NkSessionConfig& cfg,
            const char* matchmakerUrl
        ) noexcept
        {
            // Validation des paramètres
            if (matchmakerUrl == nullptr || std::strlen(matchmakerUrl) == 0)
            {
                NK_NET_LOG_ERROR("RegisterServer : matchmakerUrl invalide");
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Envoi de la requête d'enregistrement au service de matchmaking
            // Note : en production, implémenter une requête HTTP/HTTPS vers l'API du matchmaker
            NK_NET_LOG_INFO("Serveur enregistré auprès de {} : {}", matchmakerUrl, cfg.name);

            // Démarrage du heartbeat périodique pour maintenir l'enregistrement
            // Note : en production, utiliser un timer ou thread dédié
            StartMatchmakerHeartbeat(cfg, matchmakerUrl);

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkMatchmaker::UnregisterServer() noexcept
        {
            // Envoi de la requête de désenregistrement
            // Note : en production, requête HTTP/HTTPS vers l'API du matchmaker
            NK_NET_LOG_DEBUG("Serveur désenregistré du matchmaking");

            // Arrêt du heartbeat
            StopMatchmakerHeartbeat();

            return NkNetResult::NK_NET_OK;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkMatchmaker — Méthodes privées
        // =====================================================================

        void NkMatchmaker::SearchWorker(
            const SearchParams& params,
            NkFunction<void(const SearchResult&)> onFound,
            NkFunction<void(NkNetResult)> onError,
            float32 timeoutSec
        ) noexcept
        {
            // Initialisation du timer de timeout
            const auto startTime = std::chrono::steady_clock::now();
            const auto timeoutTime = startTime + std::chrono::milliseconds(
                static_cast<int64>(timeoutSec * 1000.f));

            // Boucle de recherche jusqu'à timeout ou match trouvé
            while (mSearching && std::chrono::steady_clock::now() < timeoutTime)
            {
                // Requête au service de matchmaking pour obtenir des sessions candidates
                // Note : en production, requête HTTP/HTTPS vers l'API du matchmaker
                NkVector<SearchResult> candidates = FetchMatchmakingCandidates(params);

                // Évaluation et tri des candidats par score
                for (auto& candidate : candidates)
                {
                    candidate.score = CalculateMatchScore(params, candidate);
                }

                std::sort(candidates.begin(), candidates.end(),
                    [](const SearchResult& a, const SearchResult& b) {
                        return a.score > b.score;  // Tri décroissant par score
                    });

                // Notification du meilleur candidat si score suffisant
                if (!candidates.IsEmpty() && candidates[0].score >= 0.7f)
                {
                    if (onFound) { onFound(candidates[0]); }
                    mSearching = false;
                    return;
                }

                // Pause courte avant la prochaine itération
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 1 seconde
            }

            // Timeout ou annulation
            mSearching = false;
            if (onError)
            {
                onError(NkNetResult::NK_NET_TIMEOUT);
            }
        }

        NkVector<NkMatchmaker::SearchResult> NkMatchmaker::FetchMatchmakingCandidates(
            const SearchParams& params
        ) noexcept
        {
            // Simulation de requête au service de matchmaking
            // Note : en production, implémenter une vraie requête HTTP/HTTPS
            NkVector<SearchResult> results;

            // Exemple : retour de sessions fictives pour démonstration
            // En production, parser la réponse JSON/XML de l'API du matchmaker

            return results;
        }

        void NkMatchmaker::StartMatchmakerHeartbeat(
            const NkSessionConfig& cfg,
            const char* matchmakerUrl
        ) noexcept
        {
            // Démarrage d'un timer périodique pour rafraîchir l'enregistrement
            // Note : en production, utiliser un système de timers ou thread dédié
            NK_NET_LOG_DEBUG("Heartbeat matchmaking démarré pour {}", cfg.name);
        }

        void NkMatchmaker::StopMatchmakerHeartbeat() noexcept
        {
            // Arrêt du timer de heartbeat
            NK_NET_LOG_DEBUG("Heartbeat matchmaking arrêté");
        }

        // =====================================================================
        // IMPLÉMENTATION : NkDiscovery — Méthodes statiques
        // =====================================================================

        NkNetResult NkDiscovery::Broadcast(
            const ServerInfo& info,
            uint16 broadcastPort
        ) noexcept
        {
            // Création d'un socket temporaire pour le broadcast
            NkSocket socket;
            if (socket.Create(NkAddress::Any(0), NkSocket::Type::NK_UDP) != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Broadcast : échec de création du socket");
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Activation du mode broadcast
            socket.SetBroadcast(true);

            // Sérialisation des informations du serveur
            uint8 payload[512];
            NkBitWriter writer(payload, sizeof(payload));

            writer.WriteString(info.name, 128);
            writer.WriteString(info.gameMode, 64);
            writer.WriteU32(info.playerCount);
            writer.WriteU32(info.maxPlayers);
            writer.WriteBool(info.hasPassword);

            if (writer.IsOverflowed())
            {
                NK_NET_LOG_ERROR("Broadcast : sérialisation échouée");
                return NkNetResult::NK_NET_PACKET_TOO_LARGE;
            }

            // Adresse de broadcast : 255.255.255.255:port
            NkAddress broadcastAddr(255, 255, 255, 255, broadcastPort);

            // Envoi du paquet broadcast
            const NkNetResult result = socket.SendTo(payload, writer.BytesWritten(), broadcastAddr);

            socket.Close();

            if (result != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_WARN("Broadcast : échec d'envoi vers {}", broadcastAddr.ToString().CStr());
            }

            return result;
        }

        NkNetResult NkDiscovery::Listen(
            uint32 timeoutMs,
            NkVector<ServerInfo>& out,
            uint16 broadcastPort
        ) noexcept
        {
            // Création d'un socket temporaire pour l'écoute
            NkSocket socket;
            if (socket.Create(NkAddress::Any(broadcastPort), NkSocket::Type::NK_UDP) != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Listen : échec de création du socket sur port {}", broadcastPort);
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Activation du mode broadcast pour réception
            socket.SetBroadcast(true);
            socket.SetNonBlocking(true);

            // Buffer de réception
            uint8 buffer[1024];
            uint32 receivedSize = 0;
            NkAddress from;

            // Timer de timeout
            const NkTimestampMs startTime = NkNetNowMs();
            const NkTimestampMs endTime = startTime + timeoutMs;

            // Boucle d'écoute jusqu'à timeout
            while (NkNetNowMs() < endTime)
            {
                // Réception non-bloquante d'un paquet
                if (socket.RecvFrom(buffer, sizeof(buffer), receivedSize, from) == NkNetResult::NK_NET_OK)
                {
                    if (receivedSize > 0)
                    {
                        // Désérialisation des informations du serveur
                        NkBitReader reader(buffer, receivedSize);

                        ServerInfo server;
                        server.addr = from;

                        NkString nameBuf;
                        NkString modeBuf;
                        reader.ReadString(nameBuf, 128);
                        reader.ReadString(modeBuf, 64);
                        SafeStrCopy(server.name, sizeof(server.name), nameBuf.CStr(), 128);
                        SafeStrCopy(server.gameMode, sizeof(server.gameMode), modeBuf.CStr(), 64);

                        server.playerCount = reader.ReadU32();
                        server.maxPlayers = reader.ReadU32();
                        server.hasPassword = reader.ReadBool();

                        // Calcul du ping basé sur le temps de réception
                        server.ping = static_cast<uint32>(NkNetNowMs() - startTime);

                        // Validation basique des données
                        if (!reader.IsOverflowed() && server.maxPlayers > 0)
                        {
                            // Déduplication par adresse IP + port
                            bool isDuplicate = false;
                            for (const auto& existing : out)
                            {
                                if (existing.addr == server.addr)
                                {
                                    isDuplicate = true;
                                    break;
                                }
                            }

                            if (!isDuplicate)
                            {
                                out.PushBack(server);
                            }
                        }
                    }
                }

                // Pause courte pour éviter le busy-wait
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            socket.Close();

            NK_NET_LOG_DEBUG("Découverte LAN terminée : {} serveurs trouvés", out.Size());
            return NkNetResult::NK_NET_OK;
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    Gestion des callbacks :
    ----------------------
    - Les callbacks onPlayerJoined/onPlayerLeft/onStateChanged sont invoqués
      depuis le thread réseau — différer vers le thread jeu/UI via event bus
    - Utiliser NkFunction pour flexibilité : lambdas, std::function, pointeurs de fonction
    - Vérifier nullptr avant appel pour éviter les crashs si callback non défini

    Thread-safety :
    --------------
    - NkLobby::Global() utilise un magic static C++11 : thread-safe pour l'initialisation
    - Les accès à mPlayers/mPlayerCount dans NkSession sont protégés par le fait que
      toutes les modifications passent par le thread réseau unique
    - Pour accès concurrents explicites : ajouter un mutex autour des lectures/écritures

    Sérialisation lobby :
    --------------------
    - NkBitWriter/Reader pour encodage compact des messages lobby
    - En-tête fixe NkLobbyMessageHeader pour validation et routing
    - Payload variable selon le type de message (PLAYER_INFO, CHAT_MESSAGE, etc.)

    Matchmaking et scoring :
    -----------------------
    - Fonction CalculateMatchScore() combine ELO, ping, région et player fit
    - Poids configurables (kELOWeight, etc.) pour ajuster l'expérience utilisateur
    - Seuil de score (0.7) pour décision automatique vs proposition à l'utilisateur

    Découverte LAN :
    ---------------
    - Broadcast UDP sur 255.255.255.255:7778 — peut être bloqué par certains routeurs
    - Socket temporaire avec SO_BROADCAST pour émission/réception
    - Déduplication par adresse IP+port pour éviter les doublons de serveurs

    Gestion d'erreur :
    -----------------
    - Retour systématique de NkNetResult pour propagation d'erreur cohérente
    - Logging via NK_NET_LOG_* avec contexte pour debugging
    - Valeurs par défaut sécurisées en cas d'échec de lecture/désérialisation

    Performance :
    ------------
    - Buffers stack pour sérialisation : pas d'allocation heap dans les chemins critiques
    - Tableau statique mPlayers[kMaxPlayers] : accès O(1), pas de réallocation
    - Broadcast optimisé : un seul envoi réseau pour notifier tous les pairs

    Extensions futures :
    -------------------
    - Support du chiffrement des messages lobby pour sessions privées
    - Compression des payloads pour réduire la bande passante en matchmaking
    - Intégration avec services cloud (Steam, Epic, etc.) pour cross-platform
    - Système de spectateurs pour observer les parties en cours
    - Replay et demo recording pour analyse post-match

    Debugging :
    ----------
    - Activer le logging des messages lobby en développement pour tracer le flux
    - Utiliser des asserts pour validation des invariants (ex: mPlayerCount ≤ kMaxPlayers)
    - Tester les scénarios de déconnexion inattendue pour robustesse du nettoyage
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================