#pragma once
// =============================================================================
// World/NkWorld.h — Interface publique du système ECS NkWorld (v3 Clean)
// =============================================================================
/**
 * @file NkWorld.h
 * @brief Définition de la classe NkWorld : façade principale du moteur ECS.
 *
 * Ce fichier expose l'API publique pour :
 * - Créer et gérer des entités et leurs composants
 * - Exécuter des requêtes (queries) sur les entités
 * - Émettre et gérer des événements gameplay
 * - Gérer les opérations différées (sécurité pendant l'itération)
 *
 * 🔹 COUCHE DE RESPONSABILITÉ :
 *    NkWorld est un système ECS AUTONOME et GÉNÉRIQUE.
 *    Il ne connaît PAS les concepts de haut niveau tels que :
 *    - NkGameObject  → défini dans Nkentseu (couche moteur)
 *    - NkPrefab      → défini dans Nkentseu (couche moteur)
 *    - NkBehaviourHost → défini dans Nkentseu (couche moteur)
 *
 *    Ces abstractions haut-niveau sont construites AU-DESSUS de NkWorld
 *    par les couches applicatives (Nkentseu). NkWorld ne les inclut pas,
 *    ne les forward-declare pas, et ne les fabrique pas.
 *
 * 🔹 Ce que NkWorld fournit :
 *    - CreateEntity() / Destroy() / DestroyDeferred()
 *    - Add<T> / Remove<T> / Get<T> / Has<T> / Set<T>
 *    - Query<Ts...> — itération cache-friendly sur les archétypes
 *    - Subscribe<T> / Emit<T> / QueueEvent<T> — bus d'événements
 *    - FlushDeferred() / DrainEvents() — gestion sécurisée
 *
 * 🔹 Modifications par rapport à la version précédente (v2) :
 *    • SUPPRESSION de CreateGameObject<T>() — appartient à Nkentseu
 *    • SUPPRESSION de CreatePrefabInstance<T>() — appartient à Nkentseu
 *    • SUPPRESSION de CreateBatch() — simplifié via NkEntityBuilder::BuildBatch()
 *    • Ajout de CreateBatch() propre sans dépendance à NkGameObject
 *    • std::vector → NkVector (conteneur maison, pas d'allocation dynamique)
 *    • std::function → NkFunction (wrapper léger sans heap)
 *    • typeid().name() → NkTypeRegistry::TypeName<T>() (noms lisibles)
 *
 * ⚠️ Les fichiers internes (NkArchetype, NkComponentPool, etc.) peuvent encore
 *    utiliser la STL temporairement — ils ne font pas partie de l'API publique.
 *
 * @author nkentseu
 * @version 3.0
 * @date 2026
 */

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Storage/NkArchetypeGraph.h"
#include "NKECS/Query/NkQuery.h"
#include "NKECS/Events/NkGameplayEventBus.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    namespace ecs {

        // Forward declaration pour les relations circulaires
        class NkWorld;

        // =====================================================================
        // 🏗️ NkEntityBuilder — API fluente pour la création chaînée d'entités
        // =====================================================================
        /**
         * @class NkEntityBuilder
         * @brief Builder fluide pour créer une entité avec ses composants.
         *
         * Exemple d'utilisation :
         * @code
         * auto id = world.Create()
         *     .With<NkTransform>({1, 2, 3})
         *     .With<NkHealth>(100)
         *     .Build();
         * @endcode
         */
        class NkEntityBuilder {
            public:
                explicit NkEntityBuilder(NkWorld& world) noexcept;

                template<typename T>
                NkEntityBuilder& With(const T& value = T{}) noexcept;

                template<typename T>
                NkEntityBuilder& With(T&& value) noexcept;

                [[nodiscard]] NkEntityId Build() noexcept;

            private:
                NkWorld&   mWorld;
                NkEntityId mId;

                struct PendingComponent {
                    NkComponentId id = kInvalidComponentId;
                    NkFunction<void(NkWorld&, NkEntityId)> apply;
                };
                NkVector<PendingComponent> mPending;
        };

        // =====================================================================
        // 📦 NkBatchBuilder — Création optimisée d'entités en masse
        // =====================================================================
        /**
         * @class NkBatchBuilder
         * @brief Builder pour créer plusieurs entités avec les mêmes composants.
         *
         * Usage :
         * @code
         * NkEntityId ids[1000];
         * world.CreateBatch(1000, ids)
         *     .With<NkPosition>()
         *     .With<NkVelocity>({1.0f, 0.0f, 0.0f})
         *     .Build();
         * @endcode
         *
         * @note Ce builder crée des entités ECS génériques — pour créer
         *       des GameObjects (couche Nkentseu), utiliser
         *       NkGameObjectFactory::CreateBatch() défini dans Nkentseu.
         */
        class NkBatchBuilder {
            public:
                NkBatchBuilder(NkWorld& world, NkEntityId* out, uint32 count) noexcept;

                template<typename T>
                NkBatchBuilder& With(const T& value = T{}) noexcept;

                void Build() noexcept;

            private:
                NkWorld&        mWorld;
                NkEntityId*     mOut;
                uint32          mCount;
                NkComponentMask mMask;

                struct PendingDefault {
                    NkComponentId id;
                    NkFunction<void(NkWorld&, NkEntityId)> apply;
                };
                NkVector<PendingDefault> mPending;
        };

        // =====================================================================
        // 🌍 NkWorld — Façade principale du système ECS
        // =====================================================================
        /**
         * @class NkWorld
         * @brief Conteneur principal gérant entités, composants, requêtes et événements.
         *
         * 🔹 Responsabilités :
         * - Allocation/libération d'entités via NkEntityIndex
         * - Gestion des archétypes (groupes d'entités par signature de composants)
         * - Exécution de queries filtrées par composants
         * - Système d'événements gameplay découplé (NkEventBus)
         * - Support des opérations différées (sécurité pendant les itérations)
         *
         * 🔹 Ce que NkWorld NE FAIT PAS (couche Nkentseu) :
         * - NkWorld ne crée pas de NkGameObject
         * - NkWorld ne connaît pas les Prefabs
         * - NkWorld ne gère pas les NkBehaviour
         * Ces abstractions sont fournies par Nkentseu qui enveloppe NkWorld.
         *
         * 🔹 Thread-safety : Non thread-safe par défaut. Utiliser un mutex externe
         *    si accès concurrent requis.
         */
        class NkWorld {
            public:
                // ── Constructeurs / Destructeur ──────────────────────────────
                NkWorld() noexcept;
                ~NkWorld() noexcept = default;

                NkWorld(const NkWorld&) = delete;
                NkWorld& operator=(const NkWorld&) = delete;

                NkWorld(NkWorld&&) noexcept = default;
                NkWorld& operator=(NkWorld&&) noexcept = default;

                // ── Création d'entités ───────────────────────────────────────
                /**
                 * @brief Crée une entité vide (sans composants).
                 * @return ID de la nouvelle entité.
                 */
                [[nodiscard]] NkEntityId CreateEntity() noexcept;

                /**
                 * @brief Retourne un builder pour création fluide d'entité.
                 * @return NkEntityBuilder configuré.
                 */
                [[nodiscard]] NkEntityBuilder Create() noexcept;

                /**
                 * @brief Prépare un builder pour création en masse.
                 * @param count Nombre d'entités à créer.
                 * @param out   Tableau de sortie pour les IDs (doit avoir 'count' éléments).
                 * @return NkBatchBuilder configuré.
                 *
                 * @note Pour créer des GameObjects en masse, utiliser
                 *       NkWorld::CreateBatch() puis NkGameObject::Wrap() depuis Nkentseu.
                 */
                NkBatchBuilder CreateBatch(uint32 count, NkEntityId* out) noexcept;

                // ── Destruction d'entités ────────────────────────────────────
                /**
                 * @brief Détruit immédiatement une entité et libère ses ressources.
                 * @param id ID de l'entité à détruire.
                 * @warning Ne pas appeler pendant l'itération sur un Query !
                 */
                void Destroy(NkEntityId id) noexcept;

                /**
                 * @brief Marque une entité pour destruction différée (sécurisée).
                 * @param id ID de l'entité à détruire.
                 * @note La destruction effective aura lieu au prochain FlushDeferred().
                 */
                void DestroyDeferred(NkEntityId id) noexcept;

                // ── État des entités ─────────────────────────────────────────
                [[nodiscard]] bool IsAlive(NkEntityId id) const noexcept {
                    return mEntityIndex.IsAlive(id);
                }

                [[nodiscard]] uint32 EntityCount() const noexcept {
                    return mEntityIndex.AliveCount();
                }

                // ── Gestion des composants (ajout immédiat) ─────────────────
                /**
                 * @brief Ajoute ou met à jour un composant de type T sur une entité.
                 * @tparam T Type du composant.
                 * @param id    ID de l'entité cible.
                 * @param value Valeur à copier.
                 * @return Référence au composant dans le monde.
                 */
                template<typename T>
                T& Add(NkEntityId id, const T& value = T{}) noexcept;

                template<typename T>
                T& Add(NkEntityId id, T&& value) noexcept;

                /**
                 * @brief Supprime immédiatement un composant d'une entité.
                 * @tparam T Type du composant à supprimer.
                 * @param id ID de l'entité cible.
                 * @warning Peut invalider les itérateurs si appelé pendant un Query !
                 */
                template<typename T>
                void Remove(NkEntityId id) noexcept;

                // ── Gestion des composants (opérations différées) ───────────
                template<typename T>
                void AddDeferred(NkEntityId id, const T& value = T{}) noexcept;

                template<typename T>
                void RemoveDeferred(NkEntityId id) noexcept;

                // ── Accès aux composants (lecture/écriture) ─────────────────
                template<typename T>
                [[nodiscard]] T* Get(NkEntityId id) noexcept;

                template<typename T>
                [[nodiscard]] const T* Get(NkEntityId id) const noexcept;

                /**
                 * @brief Récupère une référence garantie vers un composant.
                 * @warning Asserte si le composant n'existe pas.
                 */
                template<typename T>
                [[nodiscard]] T& GetRef(NkEntityId id) noexcept;

                template<typename T>
                [[nodiscard]] bool Has(NkEntityId id) const noexcept;

                /**
                 * @brief Met à jour la valeur d'un composant existant.
                 * @note Silencieusement ignoré si le composant n'existe pas.
                 *       Pour garantir l'ajout, utiliser Add<T>() à la place.
                 */
                template<typename T>
                void Set(NkEntityId id, const T& value) noexcept;

                // ── Système de requêtes (Queries) ───────────────────────────
                /**
                 * @brief Crée une requête filtrant les entités par composants requis.
                 * @tparam Ts... Liste des types de composants requis.
                 * @return NkQueryResult pour itération sur les entités correspondantes.
                 *
                 * @code
                 * for (auto [pos, vel] : world.Query<NkTransform, NkVelocity>()) {
                 *     pos.x += vel.x;
                 * }
                 * @endcode
                 */
                template<typename... Ts>
                [[nodiscard]] NkQueryResult<Ts...> Query() noexcept;

                // ── Système d'événements gameplay ───────────────────────────
                template<typename T, typename Fn>
                NkGameplayEventBus::SubscriptionId Subscribe(Fn&& fn) noexcept;

                template<typename T>
                void Unsubscribe(NkGameplayEventBus::SubscriptionId id) noexcept;

                template<typename T>
                void Emit(const T& event) noexcept;

                template<typename T>
                void Emit(T&& event) noexcept;

                template<typename T>
                void QueueEvent(const T& event) noexcept;

                // ── Gestion des opérations différées ────────────────────────
                /**
                 * @brief Exécute toutes les opérations différées en attente.
                 * @note À appeler à la fin de chaque frame/logic update.
                 */
                void FlushDeferred() noexcept;

                void DrainEvents() noexcept {
                    mEventBus.Drain();
                }

                // ── Accès aux sous-systèmes internes (usage avancé) ─────────
                [[nodiscard]] NkArchetypeGraph& Graph() noexcept {
                    return mGraph;
                }

                [[nodiscard]] NkEntityIndex& EntityIndex() noexcept {
                    return mEntityIndex;
                }

                [[nodiscard]] NkGameplayEventBus& EventBus() noexcept {
                    return mEventBus;
                }

                [[nodiscard]] static NkTypeRegistry& Registry() noexcept {
                    return NkTypeRegistry::Global();
                }

            private:
                // ── Implémentation privée de Add (template) ─────────────────
                template<typename T, typename TVal>
                T& AddImpl(NkEntityId id, TVal&& value) noexcept;

                void RemoveImpl(NkEntityId id, NkComponentId cid) noexcept;

                NkComponentMask SingleMask(NkComponentId cid) noexcept {
                    NkComponentMask m;
                    m.Set(cid);
                    return m;
                }

                struct DeferredOp {
                    enum class Kind : uint8 { Add, Remove, Destroy };
                    Kind          kind;
                    NkEntityId    entity;
                    NkComponentId componentId = kInvalidComponentId;
                    NkFunction<void(NkWorld&, NkEntityId)> fn;
                };

                NkArchetypeGraph   mGraph;
                NkEntityIndex      mEntityIndex;
                NkGameplayEventBus mEventBus;
                NkVector<DeferredOp> mDeferredOps;
        };

        // =====================================================================
        // 📦 Implémentations inline des builders (templates = header requis)
        // =====================================================================

        // ── NkEntityBuilder ─────────────────────────────────────────────────
        inline NkEntityBuilder::NkEntityBuilder(NkWorld& world) noexcept
            : mWorld(world), mId(world.CreateEntity()) {
        }

        template<typename T>
        NkEntityBuilder& NkEntityBuilder::With(const T& value) noexcept {
            mWorld.Add<T>(mId, value);
            return *this;
        }

        template<typename T>
        NkEntityBuilder& NkEntityBuilder::With(T&& value) noexcept {
            mWorld.Add<T>(mId, traits::NkMove(value));
            return *this;
        }

        inline NkEntityId NkEntityBuilder::Build() noexcept {
            return mId;
        }

        // ── NkBatchBuilder ──────────────────────────────────────────────────
        inline NkBatchBuilder::NkBatchBuilder(NkWorld& world,
                                              NkEntityId* out,
                                              uint32 count) noexcept
            : mWorld(world), mOut(out), mCount(count) {
        }

        template<typename T>
        NkBatchBuilder& NkBatchBuilder::With(const T& value) noexcept {
            mMask.Set(NkIdOf<T>());
            mPending.PushBack({
                NkIdOf<T>(),
                NkFunction<void(NkWorld&, NkEntityId)>(
                    [value](NkWorld& w, NkEntityId id) { w.Add<T>(id, value); })
            });
            return *this;
        }

        inline void NkBatchBuilder::Build() noexcept {
            for (uint32 i = 0; i < mCount; ++i) {
                mOut[i] = mWorld.CreateEntity();
                for (nk_usize j = 0; j < mPending.Size(); ++j) {
                    mPending[j].apply(mWorld, mOut[i]);
                }
            }
        }

        // ── Méthodes inline simples de NkWorld ─────────────────────────────
        inline NkEntityId NkWorld::CreateEntity() noexcept {
            return mEntityIndex.Allocate();
        }

        inline NkEntityBuilder NkWorld::Create() noexcept {
            return NkEntityBuilder(*this);
        }

        inline NkBatchBuilder NkWorld::CreateBatch(uint32 count, NkEntityId* out) noexcept {
            return NkBatchBuilder(*this, out, count);
        }

        // ── Templates d'accès aux composants (doivent rester inline) ───────
        template<typename T>
        T& NkWorld::Add(NkEntityId id, const T& value) noexcept {
            return AddImpl<T>(id, value);
        }

        template<typename T>
        T& NkWorld::Add(NkEntityId id, T&& value) noexcept {
            return AddImpl<T>(id, traits::NkMove(value));
        }

        template<typename T>
        void NkWorld::Remove(NkEntityId id) noexcept {
            RemoveImpl(id, NkIdOf<T>());
        }

        template<typename T>
        void NkWorld::AddDeferred(NkEntityId id, const T& value) noexcept {
            mDeferredOps.PushBack({
                DeferredOp::Kind::Add,
                id,
                NkIdOf<T>(),
                NkFunction<void(NkWorld&, NkEntityId)>(
                    [value](NkWorld& w, NkEntityId eid) { w.Add<T>(eid, value); })
            });
        }

        template<typename T>
        void NkWorld::RemoveDeferred(NkEntityId id) noexcept {
            mDeferredOps.PushBack({
                DeferredOp::Kind::Remove,
                id,
                NkIdOf<T>(),
                NkFunction<void(NkWorld&, NkEntityId)>(
                    [](NkWorld& w, NkEntityId eid) { w.Remove<T>(eid); })
            });
        }

        template<typename T>
        T* NkWorld::Get(NkEntityId id) noexcept {
            const NkEntityRecord* rec = mEntityIndex.GetRecord(id);
            if (!rec || rec->archetypeId == kInvalidArchetypeId) {
                return nullptr;
            }
            NkArchetype* arch = mGraph.Get(rec->archetypeId);
            return arch ? arch->GetComponent<T>(rec->row) : nullptr;
        }

        template<typename T>
        const T* NkWorld::Get(NkEntityId id) const noexcept {
            const NkEntityRecord* rec = mEntityIndex.GetRecord(id);
            if (!rec || rec->archetypeId == kInvalidArchetypeId) {
                return nullptr;
            }
            const NkArchetype* arch = mGraph.Get(rec->archetypeId);
            return arch ? arch->GetComponent<T>(rec->row) : nullptr;
        }

        template<typename T>
        T& NkWorld::GetRef(NkEntityId id) noexcept {
            T* ptr = Get<T>(id);
            NKECS_ASSERT(ptr != nullptr);
            return *ptr;
        }

        template<typename T>
        bool NkWorld::Has(NkEntityId id) const noexcept {
            const NkEntityRecord* rec = mEntityIndex.GetRecord(id);
            if (!rec || rec->archetypeId == kInvalidArchetypeId) {
                return false;
            }
            const NkArchetype* arch = mGraph.Get(rec->archetypeId);
            return arch && arch->Has(NkIdOf<T>());
        }

        template<typename T>
        void NkWorld::Set(NkEntityId id, const T& value) noexcept {
            if (T* ptr = Get<T>(id)) {
                *ptr = value;
            }
        }

        template<typename... Ts>
        NkQueryResult<Ts...> NkWorld::Query() noexcept {
            const NkComponentMask required =
                detail::MakeMask<std::remove_const_t<Ts>...>();
            return NkQueryResult<Ts...>(
                mGraph, mEntityIndex,
                required, NkComponentMask{}, NkComponentMask{});
        }

        template<typename T, typename Fn>
        NkGameplayEventBus::SubscriptionId NkWorld::Subscribe(Fn&& fn) noexcept {
            return mEventBus.Subscribe<T>(traits::NkForward<Fn>(fn));
        }

        template<typename T>
        void NkWorld::Unsubscribe(NkGameplayEventBus::SubscriptionId id) noexcept {
            mEventBus.Unsubscribe<T>(id);
        }

        template<typename T>
        void NkWorld::Emit(const T& event) noexcept {
            mEventBus.Emit(event);
        }

        template<typename T>
        void NkWorld::Emit(T&& event) noexcept {
            mEventBus.Emit(traits::NkMove(event));
        }

        template<typename T>
        void NkWorld::QueueEvent(const T& event) noexcept {
            mEventBus.Queue(event);
        }

        // =====================================================================
        // 🔐 Implémentation privée : AddImpl
        // =====================================================================
        template<typename T, typename TVal>
        T& NkWorld::AddImpl(NkEntityId id, TVal&& value) noexcept {
            const NkComponentId cid = NkIdOf<T>();
            NkEntityRecord* rec = mEntityIndex.GetRecord(id);
            NKECS_ASSERT(rec != nullptr);

            // CAS 1 : Composant déjà présent → mise à jour directe
            if (rec->archetypeId != kInvalidArchetypeId) {
                NkArchetype* arch = mGraph.Get(rec->archetypeId);
                if (arch && arch->Has(cid)) {
                    T* ptr = arch->GetComponent<T>(rec->row);
                    NKECS_ASSERT(ptr);
                    *ptr = std::forward<TVal>(value);
                    return *ptr;
                }
            }

            // CAS 2 : Nouveau composant → migration d'archétype
            const NkArchetypeId oldArchId = rec->archetypeId;
            const uint32        oldRow    = rec->row;

            const NkArchetypeId newArchId = (oldArchId == kInvalidArchetypeId)
                ? mGraph.GetOrCreate(SingleMask(cid))
                : mGraph.AddComponent(oldArchId, cid);

            NkArchetype* newArch = mGraph.Get(newArchId);
            NKECS_ASSERT(newArch);

            uint32 newRow = 0;

            if (oldArchId != kInvalidArchetypeId) {
                NkArchetype* oldArch = mGraph.Get(oldArchId);
                newRow = newArch->MigrateFrom(*oldArch, oldRow, id);

                const NkEntityId swapped = oldArch->RemoveEntity(oldRow);
                if (swapped.IsValid()) {
                    mEntityIndex.SetRecord(swapped, oldArchId, oldRow);
                }
            } else {
                newRow = newArch->AddEntity(id);
            }

            mEntityIndex.SetRecord(id, newArchId, newRow);

            T* ptr = newArch->GetComponent<T>(newRow);
            NKECS_ASSERT(ptr);
            *ptr = std::forward<TVal>(value);
            return *ptr;
        }

    } // namespace ecs
} // namespace nkentseu
