// =============================================================================
// NKNetwork/Replication/NkNetWorld.h
// =============================================================================
// DESCRIPTION :
//   Couche de réplication d'entités réseau — AGNOSTIQUE du système d'entités.
//   NKNetwork (couche System) ne connaît PAS NKECS (couche Runtime) : la
//   réplication fonctionne par ENREGISTREMENT d'objets applicatifs, chacun
//   décrit par un NkNetEntity (identité réseau + callbacks de sérialisation).
//   Le pont vers l'ECS (composants NetEntity, systèmes de jeu) se fait dans
//   les couches supérieures (Noge), exactement comme pour NKPhysics.
//
// MODÈLE (server-authoritative par défaut) :
//   • Serveur : RegisterEntity() les objets à répliquer → Update(dt) accumule
//     le temps et, à chaque tick (tickRate Hz), construit un snapshot DELTA
//     (seuls les états qui ont changé depuis le dernier envoi) diffusé à tous
//     les pairs sur canal SEQUENCED. Toutes les keyframeInterval ticks, un
//     snapshot COMPLET (keyframe) est diffusé — rattrapage des pertes et des
//     clients arrivés en cours de session.
//   • Client : l'application draine les messages (DrainAll) et passe chacun à
//     HandleMessage(). Entité inconnue → callback onEntitySpawn (l'application
//     crée son objet et l'enregistre) ; état reçu → readState ; destruction →
//     onEntityDespawn. Les inputs locaux remontent via SendInput().
//   • Interpolation : NkNetInterpolator bufferise des états horodatés côté
//     client et fournit la paire (A, B, alpha) encadrant le temps de rendu
//     (renderTime = now - délai d'interpolation). L'application applique le
//     lerp elle-même (elle seule connaît la sémantique de son état).
//
// PROTOCOLE FILAIRE (magic 0x52 'R', distinct du transport 0x4E et lobby 0x4C) :
//   SNAPSHOT : [magic u8][version u8][type u8][flags u8][tick u32][count u16]
//              puis par entité : [netId u64][prefab u32][owner u64]
//                                [stateSize u16][state bytes]
//   DESPAWN  : [magic u8][version u8][type u8][count u16][netId u64]*
//   INPUT    : [magic u8][version u8][type u8][seq u32][tick u32]
//              [size u16][bytes]
//   Un snapshot trop gros pour kNkMaxPayloadSize est scindé en plusieurs
//   messages auto-suffisants portant le même tick.
//
// MODÈLE DE THREADING :
//   • Toutes les méthodes de NkNetWorld s'appellent depuis le THREAD JEU
//     (Update, HandleMessage, Register/UnregisterEntity, SendInput).
//   • L'envoi/réception réseau reste géré par NkConnectionManager (thread-safe).
//
// DÉPENDANCES :
//   • NkConnection.h  : NkConnectionManager (envoi/broadcast), NkReceiveMsg
//   • NkBitStream.h   : sérialisation bit-précise des messages
//   • NkNetDefines.h  : NkNetId, NkPeerId, NkTimestampMs, canaux
//
// AUTEUR   : Rihen
// DATE     : 2026-07-12
// VERSION  : 1.0.0
// LICENCE  : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKNETWORLD_H
#define NKENTSEU_NETWORK_NKNETWORLD_H

#include "NKNetwork/NkNetworkApi.h"
#include "NKNetwork/Core/NkNetDefines.h"
#include "NKNetwork/Protocol/NkBitStream.h"
#include "NKNetwork/Protocol/NkConnection.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {

	namespace net {

		// =================================================================
		// CONSTANTES DU PROTOCOLE DE RÉPLICATION
		// =================================================================

		/// Signature des messages de réplication ('R'), distincte du transport
		/// RUDP (0x4E 'N') et du protocole lobby (0x4C 'L').
		static constexpr uint8 kNkNetReplMagic = 0x52;

		/// Version du protocole de réplication (incompatibilités → rejet).
		static constexpr uint8 kNkNetReplVersion = 1;

		/// Taille maximale de l'état sérialisé d'UNE entité dans un snapshot.
		static constexpr uint32 kNkNetMaxEntityStateSize = 256u;

		/// Taille maximale du payload d'un input client.
		static constexpr uint32 kNkNetMaxInputSize = 128u;

		// =================================================================
		// ÉNUMÉRATION : NkNetAuthority — Autorité de simulation d'une entité
		// =================================================================
		/**
		 * @enum NkNetAuthority
		 * @brief Qui fait foi pour l'état d'une entité répliquée.
		 */
		enum class NkNetAuthority : uint8 {
			/// Le serveur simule et diffuse ; les clients appliquent (défaut).
			NK_NET_AUTHORITY_SERVER = 0,

			/// Le client propriétaire simule (ex : son propre pawn) ; le
			/// serveur valide puis relaie. V1 : le serveur reste l'émetteur
			/// des snapshots, l'état client remonte via SendInput().
			NK_NET_AUTHORITY_CLIENT,

			/// Autorité partagée (objets physiques transférables).
			NK_NET_AUTHORITY_SHARED
		};

		// =================================================================
		// STRUCTURE : NkNetEntity — Descripteur d'objet répliqué
		// =================================================================
		/**
		 * @struct NkNetEntity
		 * @brief Décrit UN objet applicatif répliqué : identité réseau stable
		 *        (NkNetId), type de fabrique (prefabId), propriétaire, autorité
		 *        et callbacks de sérialisation d'état.
		 *
		 * L'application (ou le pont ECS de Noge) fournit :
		 *   • user       : pointeur opaque vers son objet (entité ECS, acteur…)
		 *   • writeState : sérialise l'état répliqué (positions, vie…) — appelé
		 *                  à chaque tick côté autorité ;
		 *   • readState  : applique un état reçu — appelé côté réplique.
		 *
		 * @note L'état sérialisé doit rester < kNkNetMaxEntityStateSize octets.
		 * @note writeState doit être DÉTERMINISTE vis-à-vis de l'état : deux
		 *       appels sans changement doivent produire les mêmes octets
		 *       (c'est la base de la détection delta).
		 */
		struct NKENTSEU_NETWORK_CLASS_EXPORT NkNetEntity {
				/// Signature : sérialise l'état de l'objet dans le writer.
				using WriteStateFn = NkFunction<void(void *user, NkBitWriter &w)>;

				/// Signature : applique un état reçu depuis le reader.
				using ReadStateFn = NkFunction<void(void *user, NkBitReader &r)>;

				/// Identifiant réseau stable, cohérent sur tous les pairs.
				NkNetId netId;

				/// Identifiant de type applicatif (prefab/archétype) pour que le
				/// client sache QUOI instancier au spawn. Sémantique libre.
				uint32 prefabId = 0;

				/// Pair propriétaire (autorité d'input). Invalid() = serveur.
				NkPeerId ownerPeer;

				/// Autorité de simulation de cette entité.
				NkNetAuthority authority = NkNetAuthority::NK_NET_AUTHORITY_SERVER;

				/// Pointeur opaque vers l'objet applicatif (non possédé).
				void *user = nullptr;

				/// Sérialisation de l'état répliqué (côté autorité).
				WriteStateFn writeState;

				/// Application d'un état reçu (côté réplique).
				ReadStateFn readState;

				// -------------------------------------------------------------
				// État interne de delta-compression (géré par NkNetWorld)
				// -------------------------------------------------------------

				/// Copie du dernier état envoyé (baseline de comparaison delta).
				uint8 lastSent[kNkNetMaxEntityStateSize] = {};

				/// Taille en octets du dernier état envoyé.
				uint32 lastSentSize = 0;

				/// true dès que l'entité a été incluse dans au moins un snapshot.
				bool everSent = false;
		};

		// =================================================================
		// STRUCTURE : NkNetSnapshot — État horodaté (historique/interpolation)
		// =================================================================
		/**
		 * @struct NkNetSnapshot
		 * @brief Un état sérialisé horodaté, tel que reçu du réseau. Unité de
		 *        stockage de NkNetInterpolator (buffering côté client).
		 */
		struct NKENTSEU_NETWORK_CLASS_EXPORT NkNetSnapshot {
				/// Tick serveur auquel cet état a été produit.
				uint32 tick = 0;

				/// Timestamp local de réception (NkNetNowMs()).
				NkTimestampMs receivedAt = 0;

				/// Octets d'état (copie propriétaire).
				NkVector<uint8> state;
		};

		// =================================================================
		// STRUCTURE : NkNetInput — Input client horodaté (client → serveur)
		// =================================================================
		/**
		 * @struct NkNetInput
		 * @brief Un paquet d'input émis par un client : numéro de séquence
		 *        croissant (déduplication côté serveur), tick client et payload
		 *        opaque (l'application définit son propre format via NkBitStream).
		 */
		struct NKENTSEU_NETWORK_CLASS_EXPORT NkNetInput {
				/// Numéro de séquence croissant, unique par pair.
				uint32 sequence = 0;

				/// Tick local du client au moment de l'émission.
				uint32 tick = 0;

				/// Pair émetteur (rempli côté serveur à la réception).
				NkPeerId from;

				/// Taille utile du payload.
				uint32 size = 0;

				/// Payload opaque de l'input.
				uint8 data[kNkNetMaxInputSize] = {};
		};

		// =================================================================
		// CLASSE : NkNetInterpolator — Buffer d'états pour lissage client
		// =================================================================
		/**
		 * @class NkNetInterpolator
		 * @brief Ring-buffer d'états horodatés + recherche de la paire (A, B)
		 *        encadrant un temps de rendu, avec alpha ∈ [0, 1].
		 *
		 * USAGE TYPIQUE (dans le readState d'une entité) :
		 * @code
		 * interp.Push(tick, NkNetNowMs(), stateBytes, stateSize);
		 * // ... au rendu :
		 * const NkNetSnapshot *a = nullptr;
		 * const NkNetSnapshot *b = nullptr;
		 * float32 alpha = 0.f;
		 * if (interp.Sample(NkNetNowMs() - kInterpDelayMs, &a, &b, alpha)) {
		 *     ApplyLerp(*a, *b, alpha); // sémantique connue de l'app seule
		 * }
		 * @endcode
		 *
		 * @note Non thread-safe : utiliser depuis le thread jeu.
		 */
		class NKENTSEU_NETWORK_CLASS_EXPORT NkNetInterpolator {
			public:
				/// Capacité par défaut du buffer (≈1.6 s à 20 Hz).
				static constexpr uint32 kDefaultCapacity = 32u;

				explicit NkNetInterpolator(uint32 capacity = kDefaultCapacity) noexcept;

				/**
				 * @brief Insère un état horodaté (copie les octets).
				 * @param tick Tick serveur de production de l'état.
				 * @param receivedAt Timestamp local de réception.
				 * @param state Octets d'état (copiés).
				 * @param size Taille des octets d'état.
				 * @note Les états plus vieux que la capacité sont évincés (FIFO).
				 * @note Un état au même tick que le dernier inséré est ignoré.
				 */
				void Push(uint32 tick, NkTimestampMs receivedAt, const uint8 *state, uint32 size) noexcept;

				/**
				 * @brief Cherche la paire d'états encadrant renderTime.
				 * @param renderTime Temps cible (typiquement now - délai interp).
				 * @param outA Reçoit l'état juste avant renderTime.
				 * @param outB Reçoit l'état juste après renderTime.
				 * @param outAlpha Reçoit le facteur de lerp ∈ [0, 1] entre A et B.
				 * @return true si une paire exploitable existe. Si renderTime est
				 *         hors bornes : clamp sur le premier/dernier état
				 *         (A == B, alpha = 0) et retour true s'il y a ≥ 1 état.
				 */
				[[nodiscard]] bool Sample(NkTimestampMs renderTime, const NkNetSnapshot **outA,
										  const NkNetSnapshot **outB, float32 &outAlpha) const noexcept;

				/// Vide le buffer (changement de map, respawn…).
				void Clear() noexcept;

				/// Nombre d'états actuellement bufferisés.
				[[nodiscard]] uint32 Count() const noexcept;

				/// Tick du dernier état inséré (0 si vide).
				[[nodiscard]] uint32 LatestTick() const noexcept;

			private:
				/// États horodatés, ordonnés par insertion (receivedAt croissant).
				NkVector<NkNetSnapshot> mSnapshots;

				/// Capacité maximale avant éviction FIFO.
				uint32 mCapacity = kDefaultCapacity;
		};

		// =================================================================
		// CLASSE : NkNetSystem — Interface de système branché sur la réplication
		// =================================================================
		/**
		 * @class NkNetSystem
		 * @brief Interface à implémenter pour être notifié du cycle de
		 *        réplication (équivalent réseau d'un système de jeu). Les
		 *        systèmes s'enregistrent via NkNetWorld::AddSystem().
		 */
		class NKENTSEU_NETWORK_CLASS_EXPORT NkNetSystem {
			public:
				virtual ~NkNetSystem() noexcept = default;

				/// Serveur : appelé juste avant la construction d'un snapshot.
				/// Point d'accroche pour pousser l'état de jeu dans les objets.
				virtual void OnBeforeSnapshot(uint32 tick) noexcept;

				/// Client : appelé après application complète d'un snapshot.
				virtual void OnSnapshotApplied(uint32 tick) noexcept;

				/// Les deux côtés : une entité vient d'être enregistrée.
				virtual void OnEntityRegistered(const NkNetEntity &entity) noexcept;

				/// Les deux côtés : une entité vient d'être retirée.
				virtual void OnEntityUnregistered(NkNetId netId) noexcept;
		};

		// =================================================================
		// CLASSE : NkNetWorld — Orchestrateur de réplication
		// =================================================================
		/**
		 * @class NkNetWorld
		 * @brief Registre des entités répliquées + construction/application des
		 *        snapshots + transport des inputs, au-dessus d'un
		 *        NkConnectionManager existant (non possédé).
		 *
		 * INTÉGRATION (le monde ne « vole » PAS les messages : l'application
		 * draine et route) :
		 * @code
		 * NkNetWorld netWorld;
		 * netWorld.Init(&connMgr, connMgr.IsServer());
		 *
		 * // Boucle de jeu :
		 * netWorld.Update(dt); // serveur : émission des snapshots au tick
		 *
		 * NkVector<NkReceiveMsg> msgs;
		 * connMgr.DrainAll(msgs);
		 * for (const auto &msg : msgs) {
		 *     if (netWorld.HandleMessage(msg)) {
		 *         continue; // message de réplication consommé
		 *     }
		 *     // ... messages applicatifs (lobby, chat, RPC…)
		 * }
		 * @endcode
		 *
		 * @note Toutes les méthodes s'appellent depuis le thread jeu.
		 */
		class NKENTSEU_NETWORK_CLASS_EXPORT NkNetWorld {
			public:
				// -------------------------------------------------------------
				// Configuration
				// -------------------------------------------------------------

				/**
				 * @struct Config
				 * @brief Paramètres de cadence et de canal de la réplication.
				 */
				struct Config {
						/// Fréquence d'émission des snapshots en Hz (serveur).
						float32 tickRate = 20.f;

						/// Un snapshot COMPLET (keyframe) toutes les N ticks —
						/// rattrape pertes et late-joiners. 0 = keyframe à
						/// chaque tick (pas de delta).
						uint32 keyframeInterval = 30u;

						/// Canal des snapshots. SEQUENCED = les snapshots en
						/// retard sont jetés (recommandé). UNRELIABLE accepté.
						NkNetChannel snapshotChannel = NkNetChannel::NK_NET_CHANNEL_SEQUENCED;

						/// Canal des inputs client → serveur.
						NkNetChannel inputChannel = NkNetChannel::NK_NET_CHANNEL_SEQUENCED;
				};

				NkNetWorld() noexcept = default;
				~NkNetWorld() noexcept;

				// Non copiable — possède le registre des entités répliquées.
				NkNetWorld(const NkNetWorld &) = delete;
				NkNetWorld &operator=(const NkNetWorld &) = delete;

				// -------------------------------------------------------------
				// Cycle de vie
				// -------------------------------------------------------------

				/**
				 * @brief Initialise le monde répliqué sur un gestionnaire de
				 *        connexions EXISTANT (non possédé, non détruit).
				 * @param connMgr Gestionnaire de connexions déjà démarré.
				 * @param isServer true = autorité (émet les snapshots).
				 * @param config Paramètres de cadence (défauts raisonnables).
				 */
				void Init(NkConnectionManager *connMgr, bool isServer, const Config &config) noexcept;

				/// Variante avec la configuration par défaut.
				void Init(NkConnectionManager *connMgr, bool isServer) noexcept;

				/// Vide le registre et coupe le lien vers le gestionnaire.
				void Shutdown() noexcept;

				/// true entre Init() et Shutdown().
				[[nodiscard]] bool IsInitialized() const noexcept;

				/// true si ce monde est l'autorité (émetteur de snapshots).
				[[nodiscard]] bool IsServer() const noexcept;

				// -------------------------------------------------------------
				// Registre des entités répliquées
				// -------------------------------------------------------------

				/**
				 * @brief Alloue un nouvel identifiant réseau (SERVEUR uniquement).
				 * @return NkNetId valide et unique pour la session.
				 * @note Côté client, les NkNetId arrivent par les snapshots.
				 */
				[[nodiscard]] NkNetId AllocateNetId() noexcept;

				/**
				 * @brief Enregistre un objet répliqué décrit par desc.
				 * @param desc Descripteur complet (netId valide requis).
				 * @return true si enregistré, false si netId invalide/dupliqué.
				 * @note Serveur : l'entité part dans le prochain snapshot.
				 * @note Client : appelé depuis le callback onEntitySpawn.
				 */
				bool RegisterEntity(const NkNetEntity &desc) noexcept;

				/**
				 * @brief Retire une entité du registre.
				 * @param netId Identifiant de l'entité à retirer.
				 * @param notifyPeers Serveur : true = diffuse un DESPAWN fiable.
				 * @return true si l'entité existait.
				 */
				bool UnregisterEntity(NkNetId netId, bool notifyPeers = true) noexcept;

				/// Recherche une entité par identifiant réseau (nullptr si absente).
				/// @warning Pointeur invalidé par Register/UnregisterEntity.
				[[nodiscard]] NkNetEntity *FindEntity(NkNetId netId) noexcept;

				/// Nombre d'entités actuellement répliquées.
				[[nodiscard]] uint32 EntityCount() const noexcept;

				// -------------------------------------------------------------
				// Boucle de réplication
				// -------------------------------------------------------------

				/**
				 * @brief Avance le temps de réplication.
				 * @param dt Delta-time en secondes depuis le dernier appel.
				 * @note Serveur : émet un snapshot à chaque tick écoulé
				 *       (plusieurs ticks rattrapés si dt est grand).
				 * @note Client : no-op pour l'instant (inputs envoyés au fil de
				 *       l'eau par SendInput()).
				 */
				void Update(float32 dt) noexcept;

				/**
				 * @brief Route un message reçu vers la réplication.
				 * @param msg Message drainé depuis NkConnectionManager::DrainAll.
				 * @return true si le message était un message de réplication
				 *         (consommé), false sinon (à traiter par l'application).
				 */
				bool HandleMessage(const NkReceiveMsg &msg) noexcept;

				/// Force l'émission d'un snapshot COMPLET au prochain tick
				/// (ex : un nouveau client vient de se connecter).
				void ForceKeyframe() noexcept;

				/// Tick de réplication courant (serveur) ou dernier appliqué (client).
				[[nodiscard]] uint32 CurrentTick() const noexcept;

				// -------------------------------------------------------------
				// Inputs (client → serveur)
				// -------------------------------------------------------------

				/**
				 * @brief CLIENT : envoie un input opaque au serveur.
				 * @param data Payload de l'input (format applicatif, NkBitStream).
				 * @param size Taille du payload (≤ kNkNetMaxInputSize).
				 * @return NK_NET_OK si parti, code d'erreur sinon.
				 * @note Numérote automatiquement (séquence croissante).
				 */
				[[nodiscard]] NkNetResult SendInput(const uint8 *data, uint32 size) noexcept;

				/**
				 * @brief SERVEUR : draine les inputs reçus d'un pair donné.
				 * @param peer Pair émetteur.
				 * @param out Vecteur de sortie (les inputs y sont AJOUTÉS,
				 *        ordonnés par séquence croissante).
				 * @note Vide la file interne de ce pair (pas de doublons).
				 */
				void DrainInputs(NkPeerId peer, NkVector<NkNetInput> &out) noexcept;

				// -------------------------------------------------------------
				// Systèmes branchés
				// -------------------------------------------------------------

				/// Ajoute un système notifié du cycle de réplication (non possédé).
				void AddSystem(NkNetSystem *system) noexcept;

				/// Retire un système précédemment ajouté.
				void RemoveSystem(NkNetSystem *system) noexcept;

				// -------------------------------------------------------------
				// Callbacks applicatifs (côté client)
				// -------------------------------------------------------------

				/// Signature de spawn : l'application doit créer son objet et
				/// appeler RegisterEntity() AVANT de retourner pour que l'état
				/// du snapshot lui soit appliqué dans la foulée.
				using SpawnCb = NkFunction<void(NkNetId netId, uint32 prefabId, NkPeerId owner)>;

				/// Signature de despawn : l'objet applicatif doit être détruit.
				/// L'entité est retirée du registre APRÈS le callback.
				using DespawnCb = NkFunction<void(NkNetId netId)>;

				/// Client : entité inconnue rencontrée dans un snapshot.
				SpawnCb onEntitySpawn;

				/// Client : DESPAWN reçu du serveur.
				DespawnCb onEntityDespawn;

			private:
				// -------------------------------------------------------------
				// Types de messages du protocole de réplication
				// -------------------------------------------------------------
				enum class MsgType : uint8 {
					NK_REPL_SNAPSHOT = 1,
					NK_REPL_DESPAWN = 2,
					NK_REPL_INPUT = 3
				};

				/// File d'inputs par pair (serveur).
				struct PeerInputQueue {
						NkPeerId peer;
						uint32 lastSequence = 0;
						NkVector<NkNetInput> inputs;
				};

				// -------------------------------------------------------------
				// Méthodes privées
				// -------------------------------------------------------------

				/// Serveur : construit et diffuse le(s) snapshot(s) du tick.
				void BuildAndSendSnapshot(bool keyframe) noexcept;

				/// Client : applique un message SNAPSHOT reçu.
				void ApplySnapshot(NkBitReader &reader) noexcept;

				/// Client : applique un message DESPAWN reçu.
				void ApplyDespawn(NkBitReader &reader) noexcept;

				/// Serveur : enfile un message INPUT reçu.
				void QueueInput(const NkReceiveMsg &msg, NkBitReader &reader) noexcept;

				/// Diffuse un buffer déjà sérialisé sur le canal snapshot.
				void BroadcastBuffer(const uint8 *data, uint32 size, NkNetChannel channel) noexcept;

				/// Index d'une entité dans mEntities (-1 si absente).
				[[nodiscard]] int32 IndexOfEntity(NkNetId netId) const noexcept;

				/// File d'inputs d'un pair (créée à la volée).
				PeerInputQueue &GetOrCreateInputQueue(NkPeerId peer) noexcept;

				// -------------------------------------------------------------
				// Membres privés
				// -------------------------------------------------------------

				/// Gestionnaire de connexions sous-jacent (non possédé).
				NkConnectionManager *mConnMgr = nullptr;

				/// Rôle : true = autorité émettrice des snapshots.
				bool mIsServer = false;

				/// Paramètres de cadence et de canaux.
				Config mConfig;

				/// Registre des entités répliquées.
				NkVector<NkNetEntity> mEntities;

				/// Systèmes notifiés (non possédés).
				NkVector<NkNetSystem *> mSystems;

				/// Files d'inputs par pair (serveur).
				NkVector<PeerInputQueue> mInputQueues;

				/// Accumulateur de temps vers le prochain tick (serveur).
				float32 mTickAccumulator = 0.f;

				/// Tick courant (serveur : émis ; client : dernier appliqué).
				uint32 mTick = 0;

				/// Prochain identifiant réseau à allouer (serveur).
				uint32 mNextNetId = 1;

				/// Séquence d'input croissante (client).
				uint32 mInputSequence = 0;

				/// true = prochain snapshot forcé complet.
				bool mForceKeyframe = true;

				/// true entre Init() et Shutdown().
				bool mInitialized = false;
		};

	} // namespace net

} // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKNETWORLD_H

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// ============================================================
