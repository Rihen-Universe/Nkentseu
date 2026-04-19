#pragma once
// =============================================================================
// NKNetwork/Replication/NkNetWorld.h
// =============================================================================
// Réplication des entités ECS entre pairs réseau.
//
// CONCEPTS FONDAMENTAUX :
//   NkNetEntity       — composant ECS marquant une entité répliquée
//   NkNetAuthority    — qui contrôle cette entité ? (server / owning client)
//   NkNetSnapshot     — état complet d'une entité à un instant T
//   NkNetDelta        — différence entre deux snapshots (bande passante min)
//   NkNetWorld        — gère la réplication de tout le monde ECS
//   NkNetSystem       — système ECS qui drive la réplication chaque frame
//
// MODÈLE SERVER-AUTHORITATIVE :
//   Serveur = autorité unique sur toutes les entités
//   Client  = prédit localement, reçoit corrections du serveur
//
// MODÈLE OWNER-AUTHORITATIVE (Peer-to-Peer) :
//   Chaque pair contrôle ses propres entités
//   Diffuse les mises à jour aux autres
//
// PIPELINE PAR FRAME (côté serveur) :
//   1. NkNetSystem::Execute() → pour chaque entité NK_REPLICATED :
//      a. Sérialise les composants marqués NK_NET_COMPONENT
//      b. Compare avec le snapshot précédent → calcule le delta
//      c. Pour chaque client : envoie le delta si l'entité est dans sa zone
//   2. Reçoit les inputs des clients → les applique avec validation
//   3. Envoie l'ACK de l'input + état corrigé si nécessaire
//
// PIPELINE PAR FRAME (côté client) :
//   1. Envoie les inputs au serveur (fiable)
//   2. Applique l'input localement (prediction)
//   3. Reçoit les snapshots serveur → reconciliation (rollback si divergence)
//
// ROLLBACK NETCODE (pour jeux de combat / RTS) :
//   NkNetRollback stocke kNkReplicationHistorySize frames d'état
//   Sur réception d'un état serveur plus récent → rollback + replay
// =============================================================================

#include "Protocol/NkConnection.h"
#include "Protocol/NkBitStream.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/System/NkSystem.h"
#include "NKECS/Events/NkGameplayEventBus.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkNetAuthority — qui contrôle une entité répliquée
        // =====================================================================
        enum class NkNetAuthority : uint8 {
            Server,      ///< Serveur = seul à modifier (NPCs, monde, physics)
            Client,      ///< Client propriétaire = envoie input, serveur valide
            Shared,      ///< Les deux peuvent modifier (ex: UI partagée)
            NoAuthority, ///< Entité en lecture seule côté client
        };

        // =====================================================================
        // Macro NK_NET_COMPONENT — marque un composant comme répliqué
        // =====================================================================
        /**
         * @macro NK_NET_COMPONENT(T)
         * Indique que le composant T doit être inclus dans les snapshots réseau.
         * Le composant doit implémenter :
         *   void NetSerialize(NkBitWriter& w) const noexcept;
         *   void NetDeserialize(NkBitReader& r) noexcept;
         *   bool NetEquals(const T& other) const noexcept;  // pour delta
         */
        #define NK_NET_COMPONENT(T) \
            static_assert(true, "NK_NET_COMPONENT: " #T " doit implémenter NetSerialize/NetDeserialize/NetEquals")

        // =====================================================================
        // NkNetEntity — composant ECS marquant une entité répliquée
        // =====================================================================
        /**
         * @struct NkNetEntity
         * @brief Composant ECS attaché à toute entité réseau.
         *
         * Ajouté par NkNetWorld::SpawnNetEntity() ou reçu du réseau.
         * La présence de ce composant = l'entité est répliquée.
         */
        struct NkNetEntity {
            NkNetId       netId;        ///< Identifiant réseau global
            NkPeerId      ownerId;      ///< Pair propriétaire
            NkNetAuthority authority   = NkNetAuthority::Server;

            bool          isLocal      = false;  ///< true = créé sur ce pair
            bool          dirty        = true;   ///< State changed → needs sync
            bool          pendingSpawn = false;  ///< En attente de spawn côté distant
            bool          pendingDestroy = false;///< En attente de destroy côté distant

            uint32        spawnTick    = 0;      ///< Frame de création
            float32       priority     = 1.f;    ///< Priorité de réplication [0..N]
            float32       relevance    = 1.f;    ///< Pertinence pour un client [0..1]
        };
        NK_COMPONENT(NkNetEntity)

        // =====================================================================
        // NkNetInput — input d'un client (envoyé au serveur pour validation)
        // =====================================================================
        struct NkNetInput {
            uint32  tick      = 0;       ///< Frame côté client
            uint32  seqNum    = 0;       ///< Séquence de l'input
            NkVec3f moveDir   = {};
            float32 yaw       = 0.f;
            float32 pitch     = 0.f;
            bool    jump      = false;
            bool    attack    = false;
            bool    interact  = false;
            uint8   buttons   = 0;       ///< Bitfield de boutons

            void Serialize  (NkBitWriter& w) const noexcept;
            void Deserialize(NkBitReader& r) noexcept;
        };
        NK_COMPONENT(NkNetInput)
        // Macro d'implémentation de NetSerialize/Deserialize/Equals
        #undef NK_NET_COMPONENT  // Remove placeholder
        #define NK_NET_COMPONENT(T) /* impl requis : NetSerialize, NetDeserialize, NetEquals */

        // =====================================================================
        // NkNetSnapshot — état d'une entité à un instant T
        // =====================================================================
        struct NkNetSnapshot {
            NkNetId       netId;
            uint32        tick         = 0;    ///< Frame de capture
            NkTimestampMs timestamp    = 0;

            // Données sérialisées des composants répliqués
            uint8         data[512]    = {};   ///< Payload compressé
            uint32        dataSize     = 0;

            // Transform toujours répliqué (optimisé hors bitstream)
            NkVec3f       position     = {};
            NkQuatf       rotation     = NkQuatf::Identity();
            NkVec3f       scale        = {1,1,1};
            NkVec3f       velocity     = {};   ///< Pour interpolation client

            bool IsValid() const noexcept { return netId.IsValid(); }
        };

        // =====================================================================
        // NkNetRelevance — zone de pertinence (qui voit quoi)
        // =====================================================================
        /**
         * @struct NkNetRelevanceZone
         * @brief Filtre les entités envoyées à chaque client.
         *
         * Les entités hors de la zone d'un client ne lui sont pas envoyées.
         * Permet de gérer des mondes massifs (MMO, open world).
         */
        struct NkNetRelevanceZone {
            NkVec3f center      = {};
            float32 radius      = 1000.f;   ///< Zone visible par le client (m)
            float32 alwaysRadius = 50.f;    ///< Zone toujours répliquée
            bool    globalVisibility = false; ///< Voit tout (spectateur, admin)
        };
        NK_COMPONENT(NkNetRelevanceZone)

        // =====================================================================
        // NkNetWorld — gestionnaire de la réplication ECS
        // =====================================================================
        class NkNetWorld {
        public:
            NkNetWorld()  noexcept = default;
            ~NkNetWorld() noexcept = default;

            NkNetWorld(const NkNetWorld&) = delete;
            NkNetWorld& operator=(const NkNetWorld&) = delete;

            // ── Initialisation ────────────────────────────────────────────────
            /**
             * @brief Lie le NkNetWorld au monde ECS et au manager de connexions.
             * @param world   Monde ECS local.
             * @param connMgr Gestionnaire de connexions réseau.
             * @param isServer true = ce pair est le serveur autoritaire.
             */
            void Init(NkWorld* world,
                       NkConnectionManager* connMgr,
                       bool isServer) noexcept;

            // ── Création d'entités répliquées ─────────────────────────────────
            /**
             * @brief Crée une entité ECS répliquée avec les composants donnés.
             * Côté serveur : crée localement + notifie tous les clients.
             * Côté client  : erreur (les clients ne créent pas d'entités sauf input).
             */
            NkEntityId SpawnNetEntity(const char* typeName,
                                       NkNetAuthority auth = NkNetAuthority::Server) noexcept;

            /**
             * @brief Détruit une entité répliquée.
             * Notifie tous les clients de la destruction.
             */
            void DestroyNetEntity(NkEntityId id) noexcept;

            /**
             * @brief Transfère l'autorité d'une entité à un client.
             * Utilisé pour les véhicules, objets manipulables, etc.
             */
            void TransferAuthority(NkEntityId id, NkPeerId newOwner) noexcept;

            // ── Inputs (client → serveur) ─────────────────────────────────────
            /**
             * @brief (Client) Envoie l'input courant au serveur.
             * Appelé depuis NkNetSystem chaque frame.
             */
            void SubmitInput(NkEntityId playerEntity,
                              const NkNetInput& input) noexcept;

            /**
             * @brief (Serveur) Applique les inputs reçus des clients.
             * Appelé depuis NkNetSystem.
             */
            void ApplyPendingInputs() noexcept;

            // ── Frame update ─────────────────────────────────────────────────
            /**
             * @brief Réplication complète pour une frame.
             * Côté serveur : collecte snapshots → envoie deltas.
             * Côté client  : reçoit snapshots → reconciliation.
             */
            void Update(float32 dt) noexcept;

            // ── Reconciliation (client) ───────────────────────────────────────
            /**
             * @brief Applique un snapshot reçu du serveur + reconciliation.
             * Compare avec la prédiction locale, rollback si nécessaire.
             */
            void ApplySnapshot(const NkNetSnapshot& snap,
                                NkEntityId localEntity) noexcept;

            // ── Configuration ─────────────────────────────────────────────────
            float32  replicationRate   = 20.f;  ///< Snapshots/s envoyés
            float32  inputSendRate     = 60.f;  ///< Inputs/s envoyés
            bool     interpolateRemote = true;  ///< Interpole les entités distantes
            float32  interpDelay       = 0.1f;  ///< Délai d'interpolation (s)
            bool     enablePrediction  = true;  ///< Prédiction côté client
            bool     enableRollback    = false; ///< Rollback netcode

            // ── Accès ─────────────────────────────────────────────────────────
            [[nodiscard]] bool     IsServer()   const noexcept { return mIsServer; }
            [[nodiscard]] uint32   GetTick()    const noexcept { return mTick; }
            [[nodiscard]] NkEntityId FindByNetId(NkNetId id) const noexcept;

            // ── Callbacks ─────────────────────────────────────────────────────
            using SpawnCb   = NkFunction<void(NkEntityId, const NkNetEntity&)>;
            using DestroyCb = NkFunction<void(NkEntityId, NkNetId)>;
            using InputCb   = NkFunction<void(NkPeerId, const NkNetInput&)>;

            SpawnCb   onEntitySpawned;   ///< Appelé quand une entité distante apparaît
            DestroyCb onEntityDestroyed; ///< Appelé quand une entité distante disparaît
            InputCb   onInputReceived;   ///< (Serveur) Input reçu d'un client

        private:
            // ── Sérialisation ─────────────────────────────────────────────────
            void SerializeEntity  (NkEntityId id, NkBitWriter& w) const noexcept;
            void DeserializeEntity(NkEntityId id, NkBitReader& r) noexcept;

            uint32 SerializeDelta(NkEntityId id,
                                   const NkNetSnapshot& prev,
                                   uint8* out, uint32 outSize) const noexcept;

            // ── Snapshot history ─────────────────────────────────────────────
            struct SnapHistory {
                NkNetSnapshot snaps[kNkReplicationHistorySize] = {};
                uint32        head = 0;
                void Push(const NkNetSnapshot& s) noexcept {
                    snaps[head % kNkReplicationHistorySize] = s;
                    ++head;
                }
                const NkNetSnapshot* GetAt(uint32 tick) const noexcept;
                const NkNetSnapshot& Latest() const noexcept {
                    return snaps[(head - 1) % kNkReplicationHistorySize];
                }
            };

            // ── Données membres ───────────────────────────────────────────────
            NkWorld*              mWorld    = nullptr;
            NkConnectionManager*  mConnMgr  = nullptr;
            bool                  mIsServer = false;
            uint32                mTick     = 0;
            float32               mRepTimer = 0.f;
            float32               mInputTimer = 0.f;

            // Map NkNetId → NkEntityId (pour retrouver l'entité locale)
            static constexpr uint32 kMaxNetEntities = 4096u;
            NkNetId    mNetIds [kMaxNetEntities] = {};
            NkEntityId mLocalIds[kMaxNetEntities] = {};
            uint32     mNetEntityCount = 0;

            // History pour rollback
            SnapHistory mSnapHistory[kMaxNetEntities];

            // Input buffer côté client (prédiction)
            static constexpr uint32 kInputBufSize = 128u;
            NkNetInput mInputHistory[kInputBufSize] = {};
            uint32     mInputHead = 0;

            // Input buffer côté serveur (reçus des clients)
            struct PendingInput {
                NkPeerId     peer;
                NkNetInput   input;
                NkEntityId   entity;
            };
            NkVector<PendingInput> mPendingInputs;
            threading::NkMutex     mInputMutex;

            // Génération des NkNetId
            uint32  mNextNetId = 1;
            uint8   mOwnerByte = 0;  ///< Représentation simplifiée du PeerId local
        };

        // =====================================================================
        // NkNetSystem — système ECS qui drive la réplication
        // =====================================================================
        class NkNetSystem final : public NkSystem {
        public:
            explicit NkNetSystem(NkNetWorld* netWorld) noexcept
                : mNetWorld(netWorld) {}

            [[nodiscard]] NkSystemDesc Describe() const override {
                return NkSystemDesc{}
                    .Reads<NkNetEntity>()
                    .Reads<NkTransform>()
                    .InGroup(NkSystemGroup::PostUpdate)
                    .WithPriority(50.f)    // Dernier des PostUpdate
                    .Sequential()
                    .Named("NkNetSystem");
            }

            void Execute(NkWorld& world, float32 dt) noexcept override {
                if (mNetWorld) mNetWorld->Update(dt);
            }

        private:
            NkNetWorld* mNetWorld = nullptr;
        };

        // =====================================================================
        // NkNetInterpolator — interpolation des entités distantes
        // =====================================================================
        /**
         * @class NkNetInterpolator
         * @brief Lisse le mouvement des entités distantes (anti-jitter).
         *
         * Maintient un buffer de snapshots reçus et les rejoue avec un délai
         * configurable (ex: 100ms). Permet un mouvement fluide même avec jitter.
         */
        class NkNetInterpolator {
        public:
            explicit NkNetInterpolator(float32 delayS = 0.1f) noexcept
                : mDelay(delayS) {}

            /**
             * @brief Insère un snapshot reçu dans le buffer.
             */
            void Push(const NkNetSnapshot& snap) noexcept;

            /**
             * @brief Retourne l'état interpolé pour le temps courant.
             * @param renderTime Temps de rendu (en général : now - delay).
             */
            [[nodiscard]] NkNetSnapshot Sample(NkTimestampMs renderTime) const noexcept;

            void SetDelay(float32 s) noexcept { mDelay = s; }
            [[nodiscard]] float32 GetDelay() const noexcept { return mDelay; }

        private:
            static constexpr uint32 kBufSize = 32u;
            NkNetSnapshot mBuffer[kBufSize] = {};
            uint32        mHead  = 0;
            uint32        mCount = 0;
            float32       mDelay;
        };

    } // namespace net
} // namespace nkentseu
