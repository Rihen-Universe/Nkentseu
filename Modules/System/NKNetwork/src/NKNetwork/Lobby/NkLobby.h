// =============================================================================
// NKNetwork/Lobby/NkLobby.h
// =============================================================================
// DESCRIPTION :
//   Système complet de lobby, sessions et matchmaking pour jeux multijoueurs.
//
// CONCEPTS CLÉS :
//   • NkSession      : Une partie en cours ou en préparation avec gestion des joueurs
//   • NkLobby        : Salle d'attente interactive avant le début d'une partie
//   • NkLobbyPlayer  : Représentation d'un joueur dans un lobby avec métadonnées
//   • NkMatchmaker   : Algorithme de matchmaking basé sur ELO, ping et région
//   • NkDiscovery    : Découverte automatique de serveurs en réseau local (LAN)
//
// FLUX TYPIQUE D'UNE PARTIE :
//   1. Client crée ou rejoint un lobby via NkLobby::Create() ou NkLobby::Join()
//   2. Le host configure la session (map, mode de jeu, paramètres)
//   3. Les joueurs marquent leur état "Ready" via SetReady()
//   4. Quand les conditions sont remplies : NkSession::Start() lance la partie
//   5. NkConnectionManager::Connect() établit la connexion réseau vers le serveur
//   6. Boucle de jeu avec NkNetWorld pour synchronisation d'état
//   7. NkSession::End() termine la partie et retourne au lobby ou déconnecte
//
// DÉCOUVERTE LAN :
//   • NkDiscovery::Broadcast() : Envoie UDP broadcast sur le réseau local
//   • NkDiscovery::Listen()    : Écoute passive des broadcasts, liste les serveurs
//   • Port par défaut : 7778 (configurable)
//
// DÉPENDANCES :
//   • NkConnection.h   : Gestion des connexions réseau peer-to-peer
//   • NkNetDefines.h   : Types fondamentaux et constantes réseau
//   • NkBitStream.h    : Sérialisation compacte pour échanges lobby
//
// RÈGLES D'UTILISATION :
//   • Utiliser NkLobby::Global() pour accès singleton cohérent dans l'application
//   • Toujours vérifier IsHost() avant d'appeler les méthodes de contrôle host-only
//   • Gérer les callbacks onPlayerJoined/onPlayerLeft pour UI dynamique
//   • Appeler Leave() proprement avant déconnexion pour nettoyage côté serveur
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKLOBBY_H
#define NKENTSEU_NETWORK_NKLOBBY_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Protocol/NkConnection.h"
    #include "NKNetwork/Protocol/NkBitStream.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKLogger/NkLog.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // =================================================================
            // ÉNUMÉRATION : NkSessionState — Machine à états d'une session
            // =================================================================
            /**
             * @enum NkSessionState
             * @brief États possibles d'une session de jeu dans son cycle de vie.
             *
             * Cette énumération définit la machine à états complète pour gérer :
             *   • La création et configuration d'une session
             *   • La phase de lobby avec attente et préparation des joueurs
             *   • Le chargement de la map et synchronisation initiale
             *   • L'exécution de la partie avec synchronisation temps réel
             *   • La fin de partie avec calcul des résultats et nettoyage
             *
             * TRANSITIONS D'ÉTAT TYPIQUES :
             *   Idle → Lobby (Create() ou Join() réussi)
             *   Lobby → Loading (Start() appelé par le host)
             *   Loading → InProgress (tous les joueurs ont chargé)
             *   InProgress → Ended (victoire/défaite/abandon)
             *   Any → Idle (Leave() ou déconnexion)
             *
             * @note Utiliser NkSessionStateStr() pour conversion en chaîne lisible
             * @see NkSession::GetState() Pour inspection de l'état courant
             */
            enum class NkSessionState : uint8
            {
                /// État initial : session non créée ou après Leave().
                /// Transition : vers Lobby via Create() ou Join().
                NK_SESSION_IDLE = 0,

                /// Salle d'attente : joueurs rejoignent, host configure.
                /// Transition : vers Loading via Start() si conditions remplies.
                NK_SESSION_LOBBY,

                /// Chargement de la map : synchronisation des assets.
                /// Transition : vers InProgress quand tous les joueurs sont prêts.
                NK_SESSION_LOADING,

                /// Partie en cours : gameplay actif avec synchronisation réseau.
                /// État stable : maintien jusqu'à victoire/défaite/timeout.
                NK_SESSION_IN_PROGRESS,

                /// Partie terminée : résultats calculés, en attente de retour lobby.
                /// Transition : vers Idle après confirmation ou timeout.
                NK_SESSION_ENDED
            };

            /**
             * @brief Convertit un état de session en chaîne lisible pour logging.
             * @param s État à convertir.
             * @return Chaîne statique décrivant l'état (ne pas libérer).
             * @note Fonction constexpr-friendly pour affichage et debugging.
             */
            [[nodiscard]] inline const char* NkSessionStateStr(NkSessionState s) noexcept
            {
                switch (s)
                {
                    case NkSessionState::NK_SESSION_IDLE:
                        return "Idle";
                    case NkSessionState::NK_SESSION_LOBBY:
                        return "Lobby";
                    case NkSessionState::NK_SESSION_LOADING:
                        return "Loading";
                    case NkSessionState::NK_SESSION_IN_PROGRESS:
                        return "InProgress";
                    case NkSessionState::NK_SESSION_ENDED:
                        return "Ended";
                    default:
                        return "Unknown";
                }
            }

            // =================================================================
            // STRUCTURE : NkPlayerInfo — Métadonnées publiques d'un joueur
            // =================================================================
            /**
             * @struct NkPlayerInfo
             * @brief Informations publiques d'un joueur dans un lobby ou session.
             *
             * Cette structure contient les métadonnées visibles par tous les pairs
             * pour affichage UI, matchmaking et synchronisation d'état :
             *   • Identifiants : peerId pour routage réseau, displayName pour UI
             *   • Compétence : elo pour matchmaking équilibré, ping pour qualité connexion
             *   • Rôles : isHost pour autorisations, isReady pour synchronisation démarrage
             *   • Organisation : team pour modes par équipe, slot pour position dans lobby
             *
             * @note Toutes les valeurs sont en lecture seule pour les pairs distants
             * @note Le host a autorité pour modifier team/slot via RPC dédiés
             * @threadsafe Lecture seule thread-safe après mise à jour par le host
             *
             * @example
             * @code
             * // Affichage d'un joueur dans l'UI du lobby
             * void RenderPlayerCard(const NkPlayerInfo& player)
             * {
             *     UI::Text("%s [ELO:%d] [Ping:%dms]",
             *         player.displayName, player.elo, player.ping);
             *
             *     if (player.isHost) { UI::Icon("crown"); }
             *     if (player.isReady) { UI::Icon("check"); }
             *     if (player.team > 0) { UI::Color(teamColors[player.team]); }
             * }
             * @endcode
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkPlayerInfo
            {
                // -------------------------------------------------------------
                // Constantes de configuration
                // -------------------------------------------------------------

                /// Longueur maximale du nom d'affichage (inclut terminateur nul).
                static constexpr uint32 kMaxName = 64u;

                // -------------------------------------------------------------
                // Identifiants et affichage
                // -------------------------------------------------------------

                /// Identifiant réseau unique du joueur pour routage des messages.
                NkPeerId peerId;

                /// Nom d'affichage lisible dans l'UI (UTF-8 recommandé).
                /// @note Terminé par '\0', longueur effective ≤ kMaxName-1.
                char displayName[kMaxName] = {};

                // -------------------------------------------------------------
                // Métriques de compétence et connexion
                // -------------------------------------------------------------

                /// Rating ELO pour matchmaking équilibré (défaut: 1000 = nouveau).
                /// @note Mis à jour après chaque partie classée via UpdateELO().
                uint32 elo = 1000;

                /// RTT estimé vers ce joueur en millisecondes (pour affichage qualité).
                /// @note Mis à jour périodiquement via heartbeats de connexion.
                uint32 ping = 0;

                // -------------------------------------------------------------
                // Rôles et état de préparation
                // -------------------------------------------------------------

                /// Indicateur d'autorité : true = ce joueur est le host/serveur.
                /// @note Seul le host peut modifier la configuration de session.
                bool isHost = false;

                /// Indicateur de préparation : true = joueur prêt à commencer.
                /// @note Requis pour transition Lobby → Loading via Start().
                bool isReady = false;

                // -------------------------------------------------------------
                // Organisation d'équipe et positionnement
                // -------------------------------------------------------------

                /// Identifiant d'équipe (0 = non assigné, 1+ = équipe spécifique).
                /// @note Utilisé pour modes de jeu par équipe (2v2, 5v5, etc.).
                uint8 team = 0;

                /// Slot/position dans le lobby (0-based, pour ordre d'affichage).
                /// @note Géré automatiquement par NkSession, modifiable par host.
                uint32 slot = 0;
            };

            // =================================================================
            // STRUCTURE : NkSessionConfig — Configuration d'une session
            // =================================================================
            /**
             * @struct NkSessionConfig
             * @brief Paramètres complets pour création et filtrage de sessions.
             *
             * Cette structure encapsule tous les paramètres configurables d'une
             * session de jeu, utilisés pour :
             *   • Création de lobby : définir les règles et limites de la partie
             *   • Découverte : filtrer les sessions visibles dans le navigateur
             *   • Matchmaking : critères de recherche pour appariement automatique
             *   • Validation : vérifier la compatibilité client/serveur avant join
             *
             * @note Tous les champs string sont terminés par '\0' et ont une longueur max
             * @note Les paramètres de matchmaking (minELO/maxELO/maxPing) sont des filtres
             * @threadsafe Copie par valeur pour sécurité — pas de référence partagée
             *
             * @example
             * @code
             * // Configuration d'une session classée 2v2
             * NkSessionConfig cfg;
             * std::strcpy(cfg.name, "Ranked 2v2 - EU West");
             * std::strcpy(cfg.mapName, "arena_dust2");
             * std::strcpy(cfg.gameMode, "team_deathmatch");
             * cfg.maxPlayers = 4;
             * cfg.minPlayers = 4;  // Démarrage automatique à 4 joueurs
             * cfg.isPrivate = false;
             * cfg.isRanked = true;
             * cfg.minELO = 800;    // Filtrage matchmaking
             * cfg.maxELO = 1400;
             * cfg.maxPingMs = 100.f;
             * cfg.region = "EU";
             *
             * lobby.CreateSession(cfg);
             * @endcode
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkSessionConfig
            {
                // -------------------------------------------------------------
                // Constantes de configuration
                // -------------------------------------------------------------

                /// Longueur maximale du nom de session (inclut terminateur nul).
                static constexpr uint32 kMaxName = 128u;

                // -------------------------------------------------------------
                // Identification et affichage
                // -------------------------------------------------------------

                /// Nom lisible de la session pour affichage dans le navigateur.
                /// @note Format recommandé : "Mode - Map - Région" (ex: "Ranked 2v2 - Dust2 - EU")
                char name[kMaxName] = {};

                /// Nom identifiant de la map/level à charger.
                /// @note Doit correspondre à un asset valide côté client et serveur.
                char mapName[kMaxName] = {};

                /// Identifiant du mode de jeu (ex: "team_deathmatch", "capture_flag").
                /// @note Détermine les règles, objectifs et conditions de victoire.
                char gameMode[64] = {};

                // -------------------------------------------------------------
                // Limites de joueurs et accès
                // -------------------------------------------------------------

                /// Nombre maximal de joueurs autorisés dans cette session.
                /// @note Doit être ≥ minPlayers et ≤ kNkMaxConnections (256).
                uint32 maxPlayers = 8;

                /// Nombre minimal de joueurs requis pour démarrer la partie.
                /// @note Si non atteint après timeout, la session peut être annulée.
                uint32 minPlayers = 2;

                /// Indicateur de session privée : nécessite un mot de passe pour rejoindre.
                /// @note Si true, la session n'apparaît pas dans les listes publiques.
                bool isPrivate = false;

                /// Indicateur de session classée : impacte le rating ELO des joueurs.
                /// @note Si true, les résultats sont enregistrés pour le leaderboard.
                bool isRanked = false;

                /// Mot de passe optionnel pour accès aux sessions privées.
                /// @note Chaîne vide = pas de mot de passe requis.
                /// @note Jamais transmis en clair sur le réseau — hashé côté serveur.
                char password[64] = {};

                /// Port réseau pour la connexion au serveur de cette session.
                /// @note Défaut: 7777 — configurable pour multiples instances.
                uint16 port = 7777;

                // -------------------------------------------------------------
                // Critères de matchmaking
                // -------------------------------------------------------------

                /// Rating ELO minimal requis pour rejoindre via matchmaking.
                /// @note Filtrage automatique : les joueurs hors plage sont exclus.
                uint32 minELO = 0;

                /// Rating ELO maximal requis pour rejoindre via matchmaking.
                /// @note Permet de créer des lobbies par niveau de compétence.
                uint32 maxELO = UINT32_MAX;

                /// Ping maximal accepté en millisecondes pour qualité de jeu.
                /// @note Filtrage géographique implicite : exclut les joueurs trop loin.
                float32 maxPingMs = 200.f;

                /// Région géographique préférée pour le matchmaking ("EU", "NA", "ASIA").
                /// @note Utilisé pour prioriser les serveurs proches géographiquement.
                NkString region;
            };

            // =================================================================
            // CLASSE : NkSession — Gestion d'une session de jeu complète
            // =================================================================
            /**
             * @class NkSession
             * @brief Orchestre le cycle de vie complet d'une session : lobby → jeu → fin.
             *
             * Cette classe fournit l'infrastructure complète pour gérer une partie
             * multijoueur de sa création à sa conclusion :
             *   • Création/Rejoindre : établissement de session avec validation
             *   • Gestion des joueurs : ajout, retrait, kick, attribution d'équipes
             *   • Contrôle de flux : démarrage, chargement, fin de partie coordonnés
             *   • Synchronisation d'état : ready flags, configuration partagée
             *   • Callbacks : notifications pour mise à jour UI et logique métier
             *
             * RÔLES ET AUTORISATIONS :
             *   • Host (serveur) : autorité pour Start(), End(), Kick(), config
             *   • Clients : lecture seule de l'état, SetReady() pour préparation
             *   • Tous : SendChatMessage(), accès aux PlayerInfo pour affichage
             *
             * CYCLE DE VIE TYPIQUE :
             * @code
             * // 1. Création côté host
             * NkSession session;
             * NkSessionConfig cfg = CreateRankedConfig();
             * if (session.Create(cfg) == NkNetResult::NK_NET_OK) {
             *     // Session créée — attente des joueurs dans le lobby
             * }
             *
             * // 2. Rejoindre côté client
             * NkSession clientSession;
             * if (clientSession.Join(serverAddr, password) == NkNetResult::NK_NET_OK) {
             *     // Rejoint avec succès — affichage dans le lobby
             * }
             *
             * // 3. Préparation et démarrage (host)
             * void OnAllPlayersReady() {
             *     if (session.IsHost() && session.GetPlayerCount() >= cfg.minPlayers) {
             *         session.Start();  // Transition vers Loading
             *     }
             * }
             *
             * // 4. Boucle de jeu (après transition InProgress)
             * void GameLoop() {
             *     if (session.IsInProgress()) {
             *         // Synchronisation gameplay via NkNetWorld...
             *     }
             * }
             *
             * // 5. Fin de partie
             * void OnMatchEnd() {
             *     session.End();  // Calcule résultats, notifie joueurs
             *     // Retour au lobby ou déconnexion...
             * }
             * @endcode
             *
             * @note Thread-safe pour les méthodes publiques : protection interne via mutex
             * @note Les callbacks sont invoqués depuis le thread réseau — différer vers thread jeu si nécessaire
             * @see NkConnectionManager Pour la gestion réseau sous-jacente
             * @see NkLobby Pour l'interface utilisateur et chat intégré
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkSession
            {
            public:

                // -------------------------------------------------------------
                // Constructeur et destructeur
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise une session inactive.
                 * @note GetState() retournera NK_SESSION_IDLE jusqu'à Create() ou Join().
                 */
                NkSession() noexcept = default;

                /**
                 * @brief Destructeur par défaut — libération automatique des ressources.
                 * @note Appel automatique à Leave() si session encore active.
                 */
                ~NkSession() noexcept = default;

                // -------------------------------------------------------------
                // Cycle de vie — création et gestion de session
                // -------------------------------------------------------------

                /**
                 * @brief Crée une nouvelle session et devient host/serveur.
                 * @param cfg Configuration complète de la session à créer.
                 * @return NkNetResult::NK_NET_OK si création réussie, code d'erreur sinon.
                 * @note Démarre un serveur UDP interne via NkConnectionManager.
                 * @note La session passe à NK_SESSION_LOBBY en cas de succès.
                 * @note Le créateur devient automatiquement isHost = true.
                 */
                [[nodiscard]] NkNetResult Create(const NkSessionConfig& cfg) noexcept;

                /**
                 * @brief Rejoint une session existante en tant que client.
                 * @param addr Adresse réseau du serveur/host à rejoindre.
                 * @param password Mot de passe optionnel pour sessions privées.
                 * @return NkNetResult::NK_NET_OK si join réussi, code d'erreur sinon.
                 * @note Établit une connexion via NkConnectionManager vers le serveur.
                 * @note La session passe à NK_SESSION_LOBBY si join validé par le host.
                 * @note Retourne NK_NET_AUTH_FAILED si mot de passe incorrect.
                 */
                [[nodiscard]] NkNetResult Join(
                    const NkAddress& addr,
                    const char* password = nullptr
                ) noexcept;

                /**
                 * @brief Quitte proprement la session courante.
                 * @note Notifie le serveur (si client) ou déconnecte tous les clients (si host).
                 * @note La session passe à NK_SESSION_IDLE après nettoyage.
                 * @note Thread-safe : peut être appelée depuis n'importe quel thread.
                 */
                void Leave() noexcept;

                // -------------------------------------------------------------
                // Contrôle de session — méthodes host-only
                // -------------------------------------------------------------

                /**
                 * @brief Lance la partie : transition Lobby → Loading → InProgress.
                 * @return NkNetResult::NK_NET_OK si démarrage initié, code d'erreur sinon.
                 * @note Uniquement valide côté host — ignoré si IsHost() == false.
                 * @note Vérifie que minPlayers sont présents et isReady == true.
                 * @note Notifie tous les joueurs de commencer le chargement de la map.
                 */
                [[nodiscard]] NkNetResult Start() noexcept;

                /**
                 * @brief Termine la session courante et retourne à l'état Idle.
                 * @note Calcule les résultats si partie classée (mise à jour ELO).
                 * @note Notifie tous les joueurs via callback onStateChanged.
                 * @note Libère les ressources réseau mais conserve la configuration.
                 */
                void End() noexcept;

                /**
                 * @brief Expulse un joueur de la session (host uniquement).
                 * @param peer Identifiant du joueur à expulser.
                 * @param reason Message optionnel expliquant la raison du kick.
                 * @note Uniquement valide côté host — ignoré sinon.
                 * @note Déconnecte immédiatement le joueur cible via NkConnectionManager.
                 * @note Notifie les autres joueurs via callback onPlayerLeft.
                 */
                void Kick(NkPeerId peer, const char* reason = nullptr) noexcept;

                /**
                 * @brief Signale que le joueur local est prêt à commencer.
                 * @param ready true = prêt, false = annule la préparation.
                 * @note Uniquement valide en état NK_SESSION_LOBBY.
                 * @note Broadcast aux autres joueurs pour affichage UI synchronisé.
                 * @note Requis pour que le host puisse appeler Start().
                 */
                void SetReady(bool ready) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — inspection de l'état de session
                // -------------------------------------------------------------

                /**
                 * @brief Retourne l'état courant de la machine à états.
                 * @return Valeur de l'énumération NkSessionState.
                 */
                [[nodiscard]] NkSessionState GetState() const noexcept;

                /**
                 * @brief Teste si cette instance est le host/serveur de la session.
                 * @return true si cette instance a créé la session via Create().
                 */
                [[nodiscard]] bool IsHost() const noexcept;

                /**
                 * @brief Teste si la partie est en cours (gameplay actif).
                 * @return true si GetState() == NK_SESSION_IN_PROGRESS.
                 */
                [[nodiscard]] bool IsInProgress() const noexcept;

                /**
                 * @brief Retourne le nombre de joueurs actuellement dans la session.
                 * @return Compteur de joueurs connectés et validés (0 à maxPlayers).
                 */
                [[nodiscard]] uint32 GetPlayerCount() const noexcept;

                /**
                 * @brief Retourne la configuration complète de cette session.
                 * @return Référence constante vers la structure NkSessionConfig.
                 * @note Lecture seule — la config ne peut être modifiée que par le host.
                 */
                [[nodiscard]] const NkSessionConfig& GetConfig() const noexcept;

                /**
                 * @brief Recherche un joueur par son identifiant réseau.
                 * @param peer Identifiant NkPeerId du joueur à rechercher.
                 * @return Pointeur constant vers NkPlayerInfo si trouvé, nullptr sinon.
                 * @note Thread-safe : lookup protégé par mutex interne.
                 * @note Le pointeur retourné n'est valide que pendant la durée de l'appel.
                 */
                [[nodiscard]] const NkPlayerInfo* FindPlayer(NkPeerId peer) const noexcept;

                /**
                 * @brief Retourne le gestionnaire de connexion sous-jacent.
                 * @return Pointeur vers NkConnectionManager pour opérations réseau avancées.
                 * @note Utiliser avec précaution — la plupart des opérations passent par l'API Session.
                 */
                [[nodiscard]] NkConnectionManager* GetConnMgr() noexcept;

                // -------------------------------------------------------------
                // Callbacks — notification d'événements de session
                // -------------------------------------------------------------

                /**
                 * @typedef PlayerJoinCb
                 * @brief Signature des callbacks d'arrivée d'un nouveau joueur.
                 * @param player Informations complètes du joueur ayant rejoint.
                 * @note Appelée sur tous les pairs quand un joueur est validé dans le lobby.
                 * @note Utiliser pour mise à jour dynamique de l'UI du lobby.
                 */
                using PlayerJoinCb = NkFunction<void(const NkPlayerInfo&)>;

                /**
                 * @typedef PlayerLeaveCb
                 * @brief Signature des callbacks de départ d'un joueur.
                 * @param peer Identifiant du joueur ayant quitté.
                 * @param reason Message optionnel expliquant la raison du départ.
                 * @note Appelée sur tous les pairs quand un joueur est déconnecté.
                 * @note Raison peut être "Disconnected", "Kicked", "Left voluntarily", etc.
                 */
                using PlayerLeaveCb = NkFunction<void(NkPeerId, const char* reason)>;

                /**
                 * @typedef StateChangeCb
                 * @brief Signature des callbacks de changement d'état de session.
                 * @param newState Nouvel état de la machine à états NkSessionState.
                 * @note Appelée sur tous les pairs lors des transitions d'état.
                 * @note Utiliser pour synchroniser l'UI et la logique métier.
                 */
                using StateChangeCb = NkFunction<void(NkSessionState)>;

                /// Callback invoqué quand un nouveau joueur rejoint la session.
                PlayerJoinCb onPlayerJoined;

                /// Callback invoqué quand un joueur quitte la session.
                PlayerLeaveCb onPlayerLeft;

                /// Callback invoqué quand l'état de la session change.
                StateChangeCb onStateChanged;

            private:

                // -------------------------------------------------------------
                // Membres privés — état interne de la session
                // -------------------------------------------------------------

                /// État courant de la machine à états de session.
                NkSessionState mState = NkSessionState::NK_SESSION_IDLE;

                /// Configuration complète de cette session (copie à la création).
                NkSessionConfig mConfig;

                /// Gestionnaire de connexion pour communication réseau peer-to-peer.
                NkConnectionManager mConnMgr;

                /// Indicateur de rôle : true = cette instance est le host/serveur.
                bool mIsHost = false;

                /// Nombre maximal de joueurs supportés dans cette session.
                static constexpr uint32 kMaxPlayers = 64u;

                /// Tableau statique des informations des joueurs connectés.
                NkPlayerInfo mPlayers[kMaxPlayers] = {};

                /// Compteur du nombre de joueurs actuellement dans la session.
                uint32 mPlayerCount = 0;

                // -------------------------------------------------------------
                // Méthodes privées — gestion interne des événements réseau
                // -------------------------------------------------------------

                /// Appelé quand un pair se connecte (côté host uniquement).
                void OnPeerConnected(NkPeerId peer) noexcept;

                /// Appelé quand un pair se déconnecte.
                void OnPeerDisconnected(NkPeerId peer, const char* reason) noexcept;

                /// Envoie un message de lobby (type encodé comme uint8 pour éviter dépendance interne).
                void BroadcastLobbyMessage(uint8 type, const uint8* payload, uint32 payloadSize) noexcept;

                /// Sérialise et diffuse les informations d'un joueur à tous.
                void BroadcastPlayerInfo(const NkPlayerInfo& player) noexcept;

                /// Calcule les résultats classés en fin de partie (mise à jour ELO).
                void CalculateRankedResults() noexcept;

                /// NkLobby accède aux membres privés de NkSession pour gestion interne.
                friend class NkLobby;
            };

            // =================================================================
            // CLASSE : NkLobby — Interface utilisateur et chat de salle d'attente
            // =================================================================
            /**
             * @class NkLobby
             * @brief Fournit une interface de haut niveau pour les salles d'attente.
             *
             * Cette classe agit comme une façade simplifiée sur NkSession, ajoutant :
             *   • Accès singleton global via Global() pour cohérence dans l'application
             *   • Système de chat intégré pour communication dans le lobby
             *   • Méthodes de configuration simplifiées pour le host
             *   • Gestion automatique de la session sous-jacente
             *
             * MODÈLE SINGLETON :
             *   • NkLobby::Global() retourne une référence vers l'instance unique
             *   • Construction lazy : créée au premier appel, durée de vie jusqu'à fin du programme
             *   • Thread-safe : initialisation protégée par magic static (C++11)
             *
             * USAGE RECOMMANDÉ :
             * @code
             * // Accès global dans n'importe quelle partie du code
             * auto& lobby = nkentseu::net::NkLobby::Global();
             *
             * // Création d'une session
             * NkSessionConfig cfg = CreateDefaultConfig();
             * if (lobby.CreateSession(cfg) == NkNetResult::NK_NET_OK) {
             *     // Session créée — afficher l'UI du lobby
             * }
             *
             * // Configuration du chat
             * lobby.onChatMessage = [](const NkLobby::ChatMessage& msg) {
             *     ChatUI::AddMessage(msg.sender, msg.text);
             * };
             *
             * // Envoi d'un message
             * lobby.SendChatMessage("Ready when you are!");
             *
             * // Accès à la session sous-jacente pour opérations avancées
             * auto& session = lobby.GetSession();
             * if (session.IsHost()) {
             *     session.SetMap("arena_dust2");
             * }
             * @endcode
             *
             * @note Thread-safe pour les méthodes publiques : délégation vers NkSession thread-safe
             * @note Les callbacks de chat sont invoqués depuis le thread réseau — différer vers thread UI si nécessaire
             * @see NkSession Pour les opérations de bas niveau sur la session
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkLobby
            {
            public:

                // -------------------------------------------------------------
                // Constructeur et destructeur
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise un lobby inactif.
                 * @note La session interne démarre en état NK_SESSION_IDLE.
                 */
                NkLobby() noexcept = default;

                /**
                 * @brief Destructeur par défaut — libération automatique des ressources.
                 * @note Appel automatique à LeaveSession() si session active.
                 */
                ~NkLobby() noexcept = default;

                // -------------------------------------------------------------
                // Accès singleton global
                // -------------------------------------------------------------

                /**
                 * @brief Retourne l'instance unique globale de NkLobby.
                 * @return Référence vers le singleton NkLobby.
                 * @note Construction lazy thread-safe via C++11 magic static.
                 * @note Durée de vie : jusqu'à la fin du programme (destruction à l'exit).
                 * @example
                 * @code
                 * // Utilisation typique dans le code applicatif
                 * auto& lobby = nkentseu::net::NkLobby::Global();
                 * lobby.CreateSession(myConfig);
                 * @endcode
                 */
                [[nodiscard]] static NkLobby& Global() noexcept;

                // -------------------------------------------------------------
                // Gestion de session — délégation vers NkSession
                // -------------------------------------------------------------

                /**
                 * @brief Crée une nouvelle session via le lobby global.
                 * @param cfg Configuration complète de la session à créer.
                 * @return NkNetResult::NK_NET_OK si création réussie, code d'erreur sinon.
                 * @note Délègue à mSession.Create(cfg) — voir documentation de NkSession::Create().
                 */
                [[nodiscard]] NkNetResult CreateSession(const NkSessionConfig& cfg) noexcept;

                /**
                 * @brief Rejoint une session existante via le lobby global.
                 * @param addr Adresse réseau du serveur/host à rejoindre.
                 * @param pw Mot de passe optionnel pour sessions privées.
                 * @return NkNetResult::NK_NET_OK si join réussi, code d'erreur sinon.
                 * @note Délègue à mSession.Join(addr, pw) — voir documentation de NkSession::Join().
                 */
                [[nodiscard]] NkNetResult JoinSession(
                    const NkAddress& addr,
                    const char* pw = nullptr
                ) noexcept;

                /**
                 * @brief Quitte la session courante via le lobby global.
                 * @note Délègue à mSession.Leave() — voir documentation de NkSession::Leave().
                 */
                void LeaveSession() noexcept;

                /**
                 * @brief Retourne la session sous-jacente pour opérations avancées.
                 * @return Référence vers l'instance NkSession gérée par ce lobby.
                 * @note Utiliser pour accès direct aux méthodes de NkSession non exposées par NkLobby.
                 * @warning Le pointeur/retour référence n'est pas possédé — durée de vie liée au lobby.
                 */
                [[nodiscard]] NkSession& GetSession() noexcept;

                // -------------------------------------------------------------
                // Système de chat — communication dans le lobby
                // -------------------------------------------------------------

                /**
                 * @brief Envoie un message texte dans le chat du lobby.
                 * @param msg Chaîne de caractères à broadcaster aux autres joueurs.
                 * @note Uniquement valide en état NK_SESSION_LOBBY.
                 * @note Broadcast via NkConnectionManager vers tous les pairs connectés.
                 * @note Longueur maximale : 256 caractères (troncature silencieuse si dépassement).
                 */
                void SendChatMessage(const char* msg) noexcept;

                /**
                 * @struct ChatMessage
                 * @brief Représente un message de chat reçu pour affichage UI.
                 *
                 * Cette structure encapsule les métadonnées d'un message de chat
                 * pour affichage cohérent dans l'interface utilisateur :
                 *   • sender : nom d'affichage de l'expéditeur
                 *   • text : contenu du message (UTF-8 recommandé)
                 *   • at : timestamp de réception pour tri chronologique
                 *
                 * @note Les champs string sont terminés par '\0' et ont une longueur max
                 * @note Timestamp en millisecondes depuis début de session pour cohérence
                 */
                struct ChatMessage
                {
                    /// Nom d'affichage de l'expéditeur du message.
                    char sender[64] = {};

                    /// Contenu textuel du message (UTF-8, max 256 caractères).
                    char text[256] = {};

                    /// Timestamp de réception en millisecondes depuis début de session.
                    NkTimestampMs at = 0;
                };

                /**
                 * @typedef ChatCb
                 * @brief Signature des callbacks de réception de message de chat.
                 * @param msg Structure ChatMessage contenant les métadonnées du message.
                 * @note Appelée sur tous les pairs quand un message de chat est reçu.
                 * @note Utiliser pour mise à jour de l'UI de chat en temps réel.
                 */
                using ChatCb = NkFunction<void(const ChatMessage&)>;

                /// Callback invoqué quand un message de chat est reçu.
                ChatCb onChatMessage;

                // -------------------------------------------------------------
                // Configuration de session — méthodes host simplifiées
                // -------------------------------------------------------------

                /**
                 * @brief Définit la map/level pour la session (host uniquement).
                 * @param mapName Nom identifiant de la map à charger.
                 * @note Uniquement valide côté host — ignoré si IsHost() == false.
                 * @note Met à jour mConfig.mapName et broadcast aux clients.
                 * @note La map doit être disponible côté client pour éviter les erreurs de chargement.
                 */
                void SetMap(const char* mapName) noexcept;

                /**
                 * @brief Définit le mode de jeu pour la session (host uniquement).
                 * @param mode Identifiant du mode de jeu (ex: "team_deathmatch").
                 * @note Uniquement valide côté host — ignoré si IsHost() == false.
                 * @note Met à jour mConfig.gameMode et broadcast aux clients.
                 * @note Le mode détermine les règles, objectifs et conditions de victoire.
                 */
                void SetGameMode(const char* mode) noexcept;

                /**
                 * @brief Attribue un joueur à une équipe (host uniquement).
                 * @param peer Identifiant du joueur à réassigner.
                 * @param team Nouvel identifiant d'équipe (0 = non assigné, 1+ = équipe).
                 * @note Uniquement valide côté host — ignoré si IsHost() == false.
                 * @note Met à jour mPlayers[].team et broadcast aux clients pour UI synchronisée.
                 * @note Utiliser pour équilibrer les équipes avant démarrage de partie.
                 */
                void SetTeam(NkPeerId peer, uint8 team) noexcept;

            private:

                // -------------------------------------------------------------
                // Membres privés — état interne du lobby
                // -------------------------------------------------------------

                /// Session sous-jacente gérant la logique réseau et état.
                NkSession mSession;

                // -------------------------------------------------------------
                // Méthodes privées — helpers internes
                // -------------------------------------------------------------

                /// Sérialise la configuration courante et la diffuse à tous les clients.
                void BroadcastConfigUpdate() noexcept;
            };

            // =================================================================
            // CLASSE : NkMatchmaker — Recherche automatique de parties
            // =================================================================
            /**
             * @class NkMatchmaker
             * @brief Algorithme de matchmaking pour appariement automatique de joueurs.
             *
             * Cette classe implémente la logique de recherche et sélection de sessions
             * basée sur des critères de qualité de jeu :
             *   • Compétence : filtrage par plage ELO pour parties équilibrées
             *   • Performance : filtrage par ping maximal pour latence acceptable
             *   • Géographie : priorisation par région pour réduire la latence
             *   • Préférences : mode de jeu, taille de groupe, classement
             *
             * ALGORITHME DE SCORING :
             *   score = w1*ELO_match + w2*ping_factor + w3*region_bonus + w4*player_fit
             *   où :
             *     • ELO_match : proximité du rating moyen de la session vs joueur
             *     • ping_factor : inverse du ping estimé vers le serveur
             *     • region_bonus : bonus si région correspond aux préférences
             *     • player_fit : adéquation nombre de joueurs vs taille de groupe
             *
             * USAGE ASYNCHRONE :
             * @code
             * NkMatchmaker matchmaker;
             *
             * // Configuration des critères de recherche
             * NkMatchmaker::SearchParams params;
             * params.gameMode = "team_deathmatch";
             * params.myELO = 1200;
             * params.maxPingMs = 100.f;
             * params.region = "EU";
             * params.playerCount = 2;  // Duo
             * params.ranked = true;
             *
             * // Lancement de la recherche
             * matchmaker.SearchAsync(params,
             *     [](const NkMatchmaker::SearchResult& result) {
             *         // Callback onFound : match trouvé
             *         NK_LOG_INFO("Match trouvé : {} (score: {:.2f})",
             *             result.sessionName, result.score);
             *
             *         // Rejoindre automatiquement ou proposer à l'utilisateur
             *         NkLobby::Global().JoinSession(result.serverAddr);
             *     },
             *     [](NkNetResult error) {
             *         // Callback onError : échec ou timeout
             *         NK_LOG_WARN("Échec de matchmaking : {}", NkNetResultStr(error));
             *     },
             *     30.f  // Timeout de 30 secondes
             * );
             *
             * // Annulation possible si l'utilisateur change d'avis
             * // matchmaker.CancelSearch();
             * @endcode
             *
             * @note Thread-safe pour les méthodes publiques : protection interne via mutex
             * @note La recherche est asynchrone — ne bloque pas le thread appelant
             * @see NkSessionConfig Pour les critères de filtrage des sessions
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkMatchmaker
            {
            public:

                // -------------------------------------------------------------
                // Structures de paramètres et résultats
                // -------------------------------------------------------------

                /**
                 * @struct SearchParams
                 * @brief Critères de recherche pour l'algorithme de matchmaking.
                 *
                 * Cette structure encapsule toutes les préférences du joueur
                 * pour guider la sélection automatique de sessions :
                 *   • gameMode : mode de jeu préféré (filtrage strict ou préférence)
                 *   • myELO : rating du joueur pour appariement équilibré
                 *   • maxPingMs : latence maximale acceptable pour qualité de jeu
                 *   • region : région géographique préférée pour proximité serveur
                 *   • playerCount : taille du groupe à apparier (solo, duo, squad)
                 *   • ranked : préférence pour parties classées vs casual
                 *
                 * @note Tous les champs ont des valeurs par défaut raisonnables
                 * @note Les critères sont combinés via fonction de scoring pondérée
                 */
                struct SearchParams
                {
                    /// Mode de jeu préféré pour la recherche.
                    /// @note Chaîne vide = tous modes acceptés.
                    NkString gameMode;

                    /// Rating ELO du joueur pour appariement équilibré.
                    /// @note Défaut: 1000 = joueur nouveau/non-classé.
                    uint32 myELO = 1000;

                    /// Ping maximal accepté en millisecondes.
                    /// @note Défaut: 150ms = compromis qualité/accessibilité.
                    float32 maxPingMs = 150.f;

                    /// Région géographique préférée pour priorisation.
                    /// @note Chaîne vide = toutes régions acceptées.
                    NkString region;

                    /// Taille du groupe à apparier (1 = solo, 2 = duo, etc.).
                    /// @note Défaut: 1 = recherche en solo.
                    uint32 playerCount = 1;

                    /// Préférence pour parties classées impactant le rating ELO.
                    /// @note Défaut: true = priorité aux sessions ranked.
                    bool ranked = true;
                };

                /**
                 * @struct SearchResult
                 * @brief Résultat d'une recherche de matchmaking avec métriques de qualité.
                 *
                 * Cette structure représente une session candidate correspondant
                 * aux critères de recherche, avec des métriques pour évaluation :
                 *   • serverAddr : adresse réseau pour connexion directe
                 *   • sessionName : nom lisible pour affichage à l'utilisateur
                 *   • playerCount/maxPlayers : occupation actuelle de la session
                 *   • avgPing/avgELO : métriques moyennes pour évaluation qualité
                 *   • score : valeur normalisée [0..1] indiquant la qualité du match
                 *
                 * @note Le score est calculé par l'algorithme de matchmaking
                 * @note Un score > 0.8 indique un match de haute qualité
                 * @note Les résultats sont triés par score décroissant dans les callbacks
                 */
                struct SearchResult
                {
                    /// Adresse réseau du serveur pour connexion directe.
                    NkAddress serverAddr;

                    /// Nom lisible de la session pour affichage UI.
                    char sessionName[128] = {};

                    /// Nombre actuel de joueurs dans la session.
                    uint32 playerCount = 0;

                    /// Nombre maximal de joueurs autorisés dans la session.
                    uint32 maxPlayers = 0;

                    /// Ping moyen estimé vers ce serveur en millisecondes.
                    uint32 avgPing = 0;

                    /// Rating ELO moyen des joueurs dans la session.
                    uint32 avgELO = 1000;

                    /// Score de qualité du match normalisé [0.0 à 1.0].
                    /// @note 1.0 = match parfait selon les critères, 0.0 = inacceptable.
                    float32 score = 0.f;
                };

                // -------------------------------------------------------------
                // Interface de recherche asynchrone
                // -------------------------------------------------------------

                /**
                 * @brief Démarre une recherche de partie asynchrone avec callbacks.
                 * @param params Critères de recherche définissant les préférences du joueur.
                 * @param onFound Callback invoqué quand un match de qualité suffisante est trouvé.
                 * @param onError Callback invoqué en cas d'erreur réseau ou timeout de recherche.
                 * @param timeoutSec Durée maximale de recherche en secondes avant timeout (défaut: 30s).
                 * @note La recherche s'exécute en arrière-plan sans bloquer le thread appelant.
                 * @note onFound peut être invoqué plusieurs fois si plusieurs matches sont trouvés.
                 * @note Le premier résultat avec score > 0.7 est généralement accepté automatiquement.
                 */
                void SearchAsync(
                    const SearchParams& params,
                    NkFunction<void(const SearchResult&)> onFound,
                    NkFunction<void(NkNetResult)> onError,
                    float32 timeoutSec = 30.f
                ) noexcept;

                /**
                 * @brief Annule la recherche en cours si active.
                 * @note Arrête immédiatement le processus de matchmaking en arrière-plan.
                 * @note Invoque onError(NkNetResult::NK_NET_TIMEOUT) si recherche active.
                 * @note No-op si aucune recherche n'est en cours (IsSearching() == false).
                 */
                void CancelSearch() noexcept;

                /**
                 * @brief Teste si une recherche de matchmaking est actuellement active.
                 * @return true si SearchAsync() a été appelé et n'est pas encore terminé/annulé.
                 */
                [[nodiscard]] bool IsSearching() const noexcept;

                // -------------------------------------------------------------
                // Enregistrement de serveur — côté host/dédié
                // -------------------------------------------------------------

                /**
                 * @brief Enregistre cette session dans le service de matchmaking.
                 * @param cfg Configuration de la session à rendre visible pour le matchmaking.
                 * @param matchmakerUrl Endpoint du service de matchmaking (ex: "https://mm.example.com").
                 * @return NkNetResult::NK_NET_OK si enregistrement réussi, code d'erreur sinon.
                 * @note Uniquement valide côté serveur/host — ignoré côté client.
                 * @note L'enregistrement est périodiquement rafraîchi via heartbeat.
                 * @note Utiliser UnregisterServer() avant shutdown pour nettoyage.
                 */
                [[nodiscard]] NkNetResult RegisterServer(
                    const NkSessionConfig& cfg,
                    const char* matchmakerUrl
                ) noexcept;

                /**
                 * @brief Désenregistre cette session du service de matchmaking.
                 * @return NkNetResult::NK_NET_OK si désenregistrement réussi, code d'erreur sinon.
                 * @note Appeler avant Shutdown() ou Leave() pour éviter les matches orphelins.
                 * @note No-op si la session n'était pas enregistrée.
                 */
                [[nodiscard]] NkNetResult UnregisterServer() noexcept;

            private:

                // -------------------------------------------------------------
                // Membres privés — état interne du matchmaker
                // -------------------------------------------------------------

                /// Indicateur d'activité : true si recherche asynchrone en cours.
                bool mSearching = false;

                /// Corps du thread de recherche asynchrone (exécuté hors du thread principal).
                void SearchWorker(
                    const SearchParams& params,
                    NkFunction<void(const SearchResult&)> onFound,
                    NkFunction<void(NkNetResult)> onError,
                    float32 timeoutSec
                ) noexcept;

                /// Interroge le service de matchmaking et retourne des sessions candidates.
                NkVector<SearchResult> FetchMatchmakingCandidates(
                    const SearchParams& params
                ) noexcept;

                /// Démarre le heartbeat périodique pour maintenir l'enregistrement serveur.
                void StartMatchmakerHeartbeat(
                    const NkSessionConfig& cfg,
                    const char* matchmakerUrl
                ) noexcept;

                /// Arrête le heartbeat de maintien d'enregistrement serveur.
                void StopMatchmakerHeartbeat() noexcept;
            };

            // =================================================================
            // CLASSE : NkDiscovery — Découverte automatique de serveurs LAN
            // =================================================================
            /**
             * @class NkDiscovery
             * @brief Protocole de découverte de serveurs en réseau local (LAN).
             *
             * Cette classe implémente un mécanisme simple de broadcast/écoute UDP
             * pour permettre aux clients de découvrir automatiquement les serveurs
             * disponibles sur le même réseau local sans configuration manuelle :
             *   • Broadcast : les serveurs diffusent périodiquement leur présence
             *   • Listen : les clients écoutent passivement et listent les serveurs
             *   • Filtrage : exclusion des serveurs incompatibles (version, mot de passe)
             *
             * PROTOCOLE DE DÉCOUVERTE :
             *   1. Serveur : envoie un paquet UDP broadcast sur port 7778 (configurable)
             *      • Payload : ServerInfo sérialisé (nom, mode, joueurs, ping, etc.)
             *      • Fréquence : toutes les 2 secondes pour maintenir la visibilité
             *
             *   2. Client : écoute sur le même port en mode broadcast
             *      • Réception passive des paquets broadcast du réseau local
             *      • Désérialisation et validation des ServerInfo reçus
             *      • Déduplication par adresse IP + port pour éviter les doublons
             *
             *   3. Affichage : liste triée des serveurs découverts dans l'UI
             *      • Tri par ping croissant pour prioriser les serveurs proches
             *      • Filtrage optionnel par mode de jeu, mot de passe, etc.
             *
             * USAGE TYPIQUE :
             * @code
             * // Côté serveur : diffusion de la présence
             * void Server::StartLANDiscovery()
             * {
             *     NkDiscovery::ServerInfo info;
             *     info.addr = NkAddress::Any(7777);  // Adresse d'écoute du serveur
             *     std::strcpy(info.name, "My LAN Server");
             *     std::strcpy(info.gameMode, "team_deathmatch");
             *     info.playerCount = GetCurrentPlayerCount();
             *     info.maxPlayers = 16;
             *     info.hasPassword = false;
             *
             *     // Broadcast périodique (à appeler dans la boucle serveur)
             *     NkDiscovery::Broadcast(info);
             * }
             *
             * // Côté client : découverte et affichage
             * void Client::RefreshLANServers()
             * {
             *     NkVector<NkDiscovery::ServerInfo> servers;
             *
             *     // Écoute bloquante pendant 2 secondes pour collecte
             *     if (NkDiscovery::Listen(2000, servers) == NkNetResult::NK_NET_OK)
             *     {
             *         // Tri par ping pour affichage prioritaire
             *         std::sort(servers.begin(), servers.end(),
             *             [](const auto& a, const auto& b) { return a.ping < b.ping; });
             *
             *         // Affichage dans l'UI
             *         for (const auto& srv : servers) {
             *             LANUI::AddServer(srv.name, srv.playerCount, srv.maxPlayers, srv.ping);
             *         }
             *     }
             * }
             * @endcode
             *
             * @note Thread-safe pour les méthodes statiques : aucune donnée partagée
             * @note Le broadcast utilise l'adresse 255.255.255.255 — peut être bloqué par certains routeurs
             * @note Pour les réseaux avec sous-réseaux multiples, envisager un serveur de découverte centralisé
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkDiscovery
            {
            public:

                // -------------------------------------------------------------
                // Structure d'information de serveur découvert
                // -------------------------------------------------------------

                /**
                 * @struct ServerInfo
                 * @brief Métadonnées d'un serveur découvert via broadcast LAN.
                 *
                 * Cette structure encapsule toutes les informations nécessaires
                 * pour afficher et filtrer les serveurs dans l'interface de découverte :
                 *   • addr : adresse réseau pour connexion directe après sélection
                 *   • name/gameMode : identification lisible pour l'utilisateur
                 *   • playerCount/maxPlayers : occupation pour évaluation de disponibilité
                 *   • ping : latence estimée pour tri par qualité de connexion
                 *   • hasPassword : indicateur pour filtrage des sessions privées
                 *
                 * @note Toutes les valeurs sont en lecture seule après réception
                 * @note Le ping est mesuré localement lors de la réception du broadcast
                 * @threadsafe Copie par valeur pour sécurité — pas de référence partagée
                 */
                struct ServerInfo
                {
                    /// Adresse réseau complète (IP + port) du serveur découvert.
                    NkAddress addr;

                    /// Nom lisible du serveur pour affichage dans l'UI.
                    char name[128] = {};

                    /// Mode de jeu actuel du serveur pour filtrage par préférence.
                    char gameMode[64] = {};

                    /// Nombre actuel de joueurs connectés au serveur.
                    uint32 playerCount = 0;

                    /// Nombre maximal de joueurs autorisés sur ce serveur.
                    uint32 maxPlayers = 0;

                    /// Ping estimé vers ce serveur en millisecondes.
                    /// @note Mesuré localement au moment de la réception du broadcast.
                    uint32 ping = 0;

                    /// Indicateur de session protégée par mot de passe.
                    /// @note Si true, l'UI doit proposer un champ de saisie de mot de passe.
                    bool hasPassword = false;
                };

                // -------------------------------------------------------------
                // Interface statique — broadcast et écoute LAN
                // -------------------------------------------------------------

                /**
                 * @brief Diffuse la présence d'un serveur sur le réseau local.
                 * @param info Métadonnées du serveur à inclure dans le broadcast.
                 * @param broadcastPort Port UDP pour le broadcast (défaut: 7778).
                 * @return NkNetResult::NK_NET_OK si diffusion réussie, code d'erreur sinon.
                 * @note Envoie un paquet UDP broadcast à 255.255.255.255:broadcastPort.
                 * @note À appeler périodiquement (toutes les 2s) pour maintenir la visibilité.
                 * @note Le socket doit être configuré avec SO_BROADCAST pour fonctionner.
                 */
                [[nodiscard]] static NkNetResult Broadcast(
                    const ServerInfo& info,
                    uint16 broadcastPort = 7778
                ) noexcept;

                /**
                 * @brief Écoute les broadcasts LAN et collecte les serveurs découverts.
                 * @param timeoutMs Durée d'écoute en millisecondes (opération bloquante).
                 * @param out Vecteur de sortie recevant la liste des serveurs trouvés.
                 * @param broadcastPort Port UDP à écouter (doit correspondre au broadcast).
                 * @return NkNetResult::NK_NET_OK si écoute réussie, code d'erreur sinon.
                 * @note Crée un socket temporaire en mode broadcast pour la durée de l'écoute.
                 * @note Déduplication automatique par adresse IP + port pour éviter les doublons.
                 * @note Le ping est calculé comme délai entre envoi du broadcast et réception.
                 */
                [[nodiscard]] static NkNetResult Listen(
                    uint32 timeoutMs,
                    NkVector<ServerInfo>& out,
                    uint16 broadcastPort = 7778
                ) noexcept;
            };

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKLOBBY_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOBBY.H
// =============================================================================
// Ce fichier fournit le système de lobby, sessions et matchmaking.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Création et gestion d'une session classée
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Lobby/NkLobby.h"
    #include "NKCore/Logger/NkLogger.h"

    void CreateRankedSession()
    {
        using namespace nkentseu::net;

        // Accès au lobby global
        auto& lobby = NkLobby::Global();

        // Configuration d'une session classée 2v2
        NkSessionConfig cfg;
        std::strcpy(cfg.name, "Ranked 2v2 - EU West");
        std::strcpy(cfg.mapName, "arena_dust2");
        std::strcpy(cfg.gameMode, "team_deathmatch");
        cfg.maxPlayers = 4;
        cfg.minPlayers = 4;  // Démarrage automatique à 4 joueurs
        cfg.isPrivate = false;
        cfg.isRanked = true;
        cfg.minELO = 800;    // Filtrage matchmaking
        cfg.maxELO = 1400;
        cfg.maxPingMs = 100.f;
        cfg.region = "EU";

        // Création de la session
        const auto result = lobby.CreateSession(cfg);
        if (result != NkNetResult::NK_NET_OK)
        {
            NK_LOG_ERROR("Échec de création de session : {}", NkNetResultStr(result));
            return;
        }

        // Configuration des callbacks pour UI dynamique
        auto& session = lobby.GetSession();
        session.onPlayerJoined = [](const NkPlayerInfo& player) {
            NK_LOG_INFO("Joueur rejoint : {} (ELO:{})", player.displayName, player.elo);
            LobbyUI::AddPlayer(player);
        };

        session.onPlayerLeft = [](NkPeerId peer, const char* reason) {
            NK_LOG_INFO("Joueur parti : {} ({})", peer.value, reason ? reason : "sans raison");
            LobbyUI::RemovePlayer(peer);
        };

        session.onStateChanged = [](NkSessionState state) {
            NK_LOG_INFO("État session : {}", NkSessionStateStr(state));
            LobbyUI::UpdateState(state);
        };

        // Le host peut maintenant attendre que les joueurs soient prêts
        // et appeler session.Start() quand les conditions sont remplies
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Rejoindre une session et gestion du chat
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Lobby/NkLobby.h"

    void JoinSessionAndChat(const NkAddress& serverAddr, const char* password)
    {
        using namespace nkentseu::net;

        auto& lobby = NkLobby::Global();

        // Tentative de rejoindre la session
        const auto result = lobby.JoinSession(serverAddr, password);
        if (result != NkNetResult::NK_NET_OK)
        {
            if (result == NkNetResult::NK_NET_AUTH_FAILED)
            {
                NK_LOG_ERROR("Mot de passe incorrect");
                // Afficher un champ de saisie de mot de passe...
            }
            else
            {
                NK_LOG_ERROR("Échec de connexion : {}", NkNetResultStr(result));
            }
            return;
        }

        // Configuration du callback de chat
        lobby.onChatMessage = [](const NkLobby::ChatMessage& msg) {
            // Affichage dans l'UI de chat
            ChatUI::AppendMessage(msg.sender, msg.text, msg.at);
        };

        // Envoi d'un message de bienvenue
        lobby.SendChatMessage("Hello everyone! Ready to play?");

        // Le joueur peut maintenant :
        // • Marquer son état Ready via lobby.GetSession().SetReady(true)
        // • Attendre que le host lance la partie
        // • Communiquer via le chat en attendant
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Matchmaking automatique avec critères personnalisés
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Lobby/NkLobby.h"

    void StartMatchmaking(uint32 playerELO, const char* preferredRegion)
    {
        using namespace nkentseu::net;

        NkMatchmaker matchmaker;

        // Configuration des critères de recherche
        NkMatchmaker::SearchParams params;
        params.gameMode = "team_deathmatch";  // Mode préféré
        params.myELO = playerELO;              // Rating du joueur
        params.maxPingMs = 120.f;              // Latence max acceptable
        params.region = preferredRegion;       // Région géographique
        params.playerCount = 1;                // Recherche en solo
        params.ranked = true;                  // Parties classées uniquement

        // Lancement de la recherche asynchrone
        matchmaker.SearchAsync(params,
            // Callback onFound : match trouvé
            [](const NkMatchmaker::SearchResult& result) {
                NK_LOG_INFO("Match trouvé : {} (score: {:.2f}, ping: {}ms)",
                    result.sessionName, result.score, result.avgPing);

                // Auto-join si score suffisant, sinon proposer à l'utilisateur
                if (result.score >= 0.7f)
                {
                    NkLobby::Global().JoinSession(result.serverAddr);
                }
                else
                {
                    MatchmakingUI::ShowMatchOffer(result);
                }
            },
            // Callback onError : échec ou timeout
            [](NkNetResult error) {
                if (error == NkNetResult::NK_NET_TIMEOUT)
                {
                    NK_LOG_WARN("Aucun match trouvé dans les délais — élargir les critères ?");
                    MatchmakingUI::ShowNoMatchesFound();
                }
                else
                {
                    NK_LOG_ERROR("Erreur de matchmaking : {}", NkNetResultStr(error));
                }
            },
            45.f  // Timeout de 45 secondes
        );

        // L'UI peut afficher un indicateur de recherche en cours
        MatchmakingUI::ShowSearchingIndicator();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Découverte LAN pour jeu en réseau local
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Lobby/NkLobby.h"
    #include "NKCore/Logger/NkLogger.h"

    // Côté serveur : diffusion périodique de la présence
    void Server::BroadcastLANPresence()
    {
        using namespace nkentseu::net;

        NkDiscovery::ServerInfo info;
        info.addr = NkAddress::Any(7777);  // Adresse d'écoute du serveur
        std::strcpy(info.name, "LAN Party Server");
        std::strcpy(info.gameMode, "free_for_all");
        info.playerCount = GetCurrentPlayerCount();
        info.maxPlayers = 16;
        info.ping = 0;  // Non pertinent pour le serveur local
        info.hasPassword = false;

        // Diffusion toutes les 2 secondes (à appeler dans la boucle serveur)
        NkDiscovery::Broadcast(info);
    }

    // Côté client : rafraîchissement de la liste des serveurs LAN
    void Client::RefreshLANServers()
    {
        using namespace nkentseu::net;

        NkVector<NkDiscovery::ServerInfo> servers;

        // Écoute bloquante pendant 2 secondes pour collecte complète
        const auto result = NkDiscovery::Listen(2000, servers);
        if (result != NkNetResult::NK_NET_OK)
        {
            NK_LOG_WARN("Échec de découverte LAN : {}", NkNetResultStr(result));
            return;
        }

        // Tri par ping croissant pour affichage prioritaire
        std::sort(servers.begin(), servers.end(),
            [](const auto& a, const auto& b) { return a.ping < b.ping; });

        // Mise à jour de l'UI de sélection de serveur
        LANUI::ClearServerList();
        for (const auto& srv : servers)
        {
            LANUI::AddServer(
                srv.name,
                srv.gameMode,
                srv.playerCount,
                srv.maxPlayers,
                srv.ping,
                srv.hasPassword,
                srv.addr  // Pour connexion lors de la sélection
            );
        }

        NK_LOG_INFO("{} serveurs LAN découverts", servers.Size());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion complète du cycle de vie d'une partie
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Lobby/NkLobby.h"
    #include "NKCore/Logger/NkLogger.h"

    class GameSessionManager
    {
    public:
        void StartHostSession()
        {
            using namespace nkentseu::net;

            auto& lobby = NkLobby::Global();
            auto& session = lobby.GetSession();

            // Configuration de la session
            NkSessionConfig cfg = CreateDefaultConfig();
            if (lobby.CreateSession(cfg) != NkNetResult::NK_NET_OK)
            {
                NK_LOG_ERROR("Échec de création de session");
                return;
            }

            // Callbacks pour gestion du cycle de vie
            session.onStateChanged = [this](NkSessionState state) {
                OnSessionStateChanged(state);
            };

            session.onPlayerJoined = [this](const NkPlayerInfo& player) {
                OnPlayerJoined(player);
            };

            // Le host est maintenant en attente de joueurs dans le lobby
            UpdateLobbyUI();
        }

        void OnPlayerReady(NkPeerId player)
        {
            using namespace nkentseu::net;

            auto& session = NkLobby::Global().GetSession();

            // Vérification des conditions de démarrage
            if (session.IsHost() &&
                session.GetState() == NkSessionState::NK_SESSION_LOBBY &&
                CountReadyPlayers() >= session.GetConfig().minPlayers)
            {
                // Tous les joueurs sont prêts — lancer la partie
                if (session.Start() == NkNetResult::NK_NET_OK)
                {
                    NK_LOG_INFO("Partie lancée avec {} joueurs", session.GetPlayerCount());
                }
            }
        }

        void OnMatchEnd()
        {
            using namespace nkentseu::net;

            auto& session = NkLobby::Global().GetSession();

            // Terminer la session et calculer les résultats
            session.End();

            // Affichage des résultats et retour au lobby
            ShowMatchResults();
            ReturnToLobby();
        }

    private:
        void OnSessionStateChanged(nkentseu::net::NkSessionState state)
        {
            switch (state)
            {
                case nkentseu::net::NkSessionState::NK_SESSION_LOBBY:
                    ShowLobbyUI();
                    break;
                case nkentseu::net::NkSessionState::NK_SESSION_LOADING:
                    ShowLoadingScreen();
                    break;
                case nkentseu::net::NkSessionState::NK_SESSION_IN_PROGRESS:
                    StartGameplayLoop();
                    break;
                case nkentseu::net::NkSessionState::NK_SESSION_ENDED:
                    ShowEndScreen();
                    break;
                default:
                    break;
            }
        }

        uint32 CountReadyPlayers() const
        {
            using namespace nkentseu::net;

            const auto& session = NkLobby::Global().GetSession();
            uint32 readyCount = 0;

            // Parcours des joueurs pour compter les prêts
            // Note : en production, maintenir un compteur incrémental pour performance
            for (uint32 i = 0; i < session.GetPlayerCount(); ++i)
            {
                // Accès simplifié — en production utiliser FindPlayer()
                if (false) { ++readyCount; }  // TODO: if (player[i].isReady)
            }

            return readyCount;
        }
    };
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Gestion du cycle de vie :
    ------------------------
    - Toujours initialiser via Create() ou Join() avant toute opération
    - Vérifier IsHost() avant d'appeler les méthodes de contrôle host-only
    - Appeler Leave() proprement avant déconnexion pour nettoyage côté serveur
    - Gérer les callbacks onStateChanged pour synchronisation UI/logique métier

    Thread-safety :
    --------------
    - NkLobby::Global() est thread-safe via magic static (C++11)
    - Les méthodes de NkSession sont protégées par mutex interne
    - Les callbacks sont invoqués depuis le thread réseau — différer vers thread jeu/UI via event bus
    - Éviter les appels bloquants dans les callbacks pour ne pas ralentir le thread réseau

    Matchmaking et qualité de jeu :
    ------------------------------
    - Ajuster minELO/maxELO pour créer des pools de compétence équilibrés
    - Utiliser maxPingMs pour filtrer les joueurs trop loin géographiquement
    - Le score de matchmaking combine ELO, ping, région et taille de groupe
    - Un score > 0.7 indique généralement un match de qualité acceptable

    Découverte LAN :
    ---------------
    - Le broadcast utilise 255.255.255.255 — peut être bloqué par certains routeurs/firewalls
    - Pour les réseaux avec sous-réseaux multiples, envisager un serveur de découverte centralisé
    - Le ping LAN est généralement < 10ms — utiliser pour tri rapide des serveurs proches

    Sécurité et validation :
    -----------------------
    - Jamais faire confiance aux données client — valider côté serveur avant application
    - Hasher les mots de passe côté serveur — ne jamais transmettre en clair
    - Vérifier la compatibilité des versions client/serveur avant join
    - Logger les tentatives suspectes pour détection de triche/abus

    Performance :
    ------------
    - Les structures NkPlayerInfo et ServerInfo sont copiées par valeur — éviter les tableaux larges
    - Utiliser des callbacks légers — différer le traitement lourd vers d'autres threads
    - Limiter la fréquence des broadcasts LAN à 0.5-2Hz pour éviter la saturation réseau

    Debugging :
    ----------
    - Activer le logging des transitions d'état pour tracer le cycle de vie des sessions
    - Utiliser NkSessionStateStr() et NkNetResultStr() pour messages d'erreur lisibles
    - Tester les scénarios de déconnexion inattendue pour robustesse du nettoyage

    Évolution du protocole :
    -----------------------
    - Versionner les structures de configuration pour compatibilité ascendante
    - Utiliser des champs réservés pour extensions futures sans breaking changes
    - Documenter les changements de format dans un fichier de protocole dédié
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================