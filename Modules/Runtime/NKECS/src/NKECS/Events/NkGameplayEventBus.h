#pragma once
// =============================================================================
// Events/NkGameplayEventBus.h — v2 (STL-free public surface)
// =============================================================================
/**
 * @file NkGameplayEventBus.h
 * @brief Définition de NkGameplayEventBus : système d'événements gameplay pour l'ECS.
 * 
 * Ce fichier expose l'API publique pour :
 * - Souscrire à des événements gameplay typés avec des callbacks
 * - Émettre des événements immédiatement (Emit) ou de façon différée (Queue/Drain)
 * - Gérer la thread-safety via NkMutex pour les accès concurrents
 * - Désabonner proprement via des SubscriptionId uniques
 * - Purger les handlers désactivés sans allocation heap
 * 
 * 🔹 RENOMMAGE IMPORTANT : NkEventBus → NkGameplayEventBus
 *    Raison : évite le conflit avec EventBus (Nkentseu/Core/EventBus.h) qui gère
 *    les événements moteur (input, fenêtre, plateforme).
 * 
 *    • NkGameplayEventBus = événements GAMEPLAY entre systèmes ECS
 *    • EventBus           = événements MOTEUR (NkKeyPressEvent, NkWindowClose…)
 * 
 * 🔹 Modifications STL-free :
 *    • std::function → NkFunction (wrapper léger sans allocation heap)
 *    • std::vector → NkVector (conteneur séquentiel maison)
 *    • std::unique_ptr → NkOwned<T> / placement-new inline (pas de heap indirect)
 *    • std::mutex → NkMutex (NKThreading, abstraction portable)
 *    • typeid(T).name() → NkTypeRegistry::TypeId<T>() (compile-time, zéro runtime)
 *    • std::remove_if → boucle manuelle write-pointer (pas de <algorithm>)
 * 
 * 🔹 Philosophie de conception :
 *    • Zéro allocation heap sur le chemin critique (Emit/Drain)
 *    • Thread-safe par défaut via NkLockGuard sur chaque opération publique
 *    • Type-safe grâce aux templates + TypeRegistry pour l'identification
 *    • Deferred pattern (Queue/Drain) pour sécurité pendant l'itération ECS
 * 
 * @author nkentseu
 * @version 2.0
 * @date 2026
 */

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKThreading/NkMutex.h"

namespace nkentseu {
    namespace ecs {

        // Macro documentaire — sans effet sur le code, pour annotation Doxygen
        #define NK_EVENT(Type) /* no-op */

        // =====================================================================
        // 🎭 IEventQueue — Interface type-erased pour les canaux d'événements
        // =====================================================================
        /**
         * @struct IEventQueue
         * @brief Interface polymorphique pour la gestion des files d'événements.
         * 
         * 🔹 Objectif : Permettre le stockage hétérogène de canaux typés
         *    (NkEventChannel<T>) dans un conteneur non-template (NkGameplayEventBus).
         * 
         * 🔹 Méthodes virtuelles :
         *    • Drain() : exécute tous les événements en attente dans la file
         *    • Clear() : vide la file sans exécuter les événements
         *    • Empty() : vérifie si la file est vide
         * 
         * @note Interface minimale — les méthodes typées (Subscribe, Emit, etc.)
         *       sont exposées via NkEventChannel<T> et NkGameplayEventBus.
         * 
         * @warning Destructeur virtuel noexcept : essentiel pour la suppression
         *          correcte des objets dérivés via pointeur de base.
         */
        struct IEventQueue {
            /// Destructeur virtuel pour suppression polymorphique sûre
            virtual ~IEventQueue() noexcept = default;

            /**
             * @brief Exécute tous les événements en attente dans la file.
             * @note Appelle les handlers pour chaque événement, puis vide la file.
             */
            virtual void Drain() noexcept = 0;

            /**
             * @brief Vide la file sans exécuter les événements en attente.
             * @note Utile pour annuler des événements différés non encore traités.
             */
            virtual void Clear() noexcept = 0;

            /**
             * @brief Vérifie si la file contient des événements en attente.
             * @return true si la file est vide, false sinon.
             */
            virtual bool Empty() const noexcept = 0;
        };

        // =====================================================================
        // 📡 NkEventChannel<T> — Canal typé pour événements de type T
        // =====================================================================
        /**
         * @class NkEventChannel
         * @brief Canal thread-safe pour l'émission et la réception d'événements de type T.
         * 
         * @tparam T Type de l'événement géré par ce canal (doit être copiable/movable).
         * 
         * 🔹 Responsabilités :
         *    • Gestion des abonnements (Subscribe/Unsubscribe) avec IDs uniques
         *    • Émission immédiate (Emit) à tous les handlers actifs
         *    • File d'attente différée (Queue) avec exécution contrôlée (Drain)
         *    • Thread-safety via NkMutex sur toutes les opérations publiques
         *    • Purge automatique des handlers désactivés lors du Drain()
         * 
         * 🔹 Pattern Deferred (Queue/Drain) :
         *    • Queue() ajoute un événement à une file interne sans exécution immédiate
         *    • Drain() exécute tous les événements en file, puis purge les handlers inactifs
         *    • Essentiel pour éviter les modifications de structure pendant l'itération ECS
         * 
         * 🔹 Gestion mémoire :
         *    • Handlers stockés dans NkVector<HandlerEntry> — pas d'allocation par handler
         *    • File d'attente dans NkVector<T> — allocation unique au premier Queue()
         *    • Swap local dans Drain() pour éviter la réentrance et les itérations invalides
         * 
         * @warning Ne pas conserver de références aux événements émis au-delà du callback :
         *          l'événement peut être temporaire (rvalue) ou détruit après Drain().
         * 
         * @example
         * @code
         * NkEventChannel<DamageEvent> channel;
         * 
         * auto id = channel.Subscribe([](const DamageEvent& evt) {
         *     printf("Dégâts reçus : %.1f\n", evt.amount);
         * });
         * 
         * channel.Emit({10.0f, sourceEntity});  // Exécution immédiate
         * channel.Queue({5.0f, otherEntity});   // Exécution au prochain Drain()
         * channel.Drain();                      // Exécute les événements en file
         * @endcode
         */
        template<typename T>
        class NkEventChannel final : public IEventQueue {
            public:
                /// Type du callback handler : fonction prenant const T& en argument
                using Handler = NkFunction<void(const T&)>;

                /// Type des IDs de subscription pour ce canal
                using HandleId = uint32;

                /// Valeur sentinel pour un ID invalide
                static constexpr HandleId kInvalidHandle = 0xFFFFFFFFu;

                // ── Abonnement / Désabonnement ─────────────────────────────
                /**
                 * @brief Souscrit un handler à ce canal d'événements.
                 * @param handler Callback à exécuter lors de l'émission d'un événement.
                 * @return HandleId unique pour désabonnement ultérieur.
                 * 
                 * 🔹 Comportement :
                 *    • Le handler est copié/moved dans un HandlerEntry interne
                 *    • L'ID est généré incrémentalement (mNextId++) — unique par canal
                 *    • Thread-safe via NkLockGuard sur mMutex
                 * 
                 * @note Le handler reçoit l'événement par const reference — pas de copie.
                 *       Si le handler doit modifier l'événement, utiliser un type mutable.
                 * 
                 * @warning Les handlers sont appelés dans l'ordre d'abonnement.
                 *          Ne pas compter sur un ordre spécifique pour la logique métier.
                 */
                HandleId Subscribe(Handler handler) noexcept;

                /**
                 * @brief Désabonne un handler précédemment souscrit.
                 * @param id HandleId retourné par Subscribe().
                 * 
                 * 🔹 Mécanisme :
                 *    • Recherche linéaire du handler par ID dans mHandlers
                 *    • Marquage comme inactif (active=false) plutôt que suppression immédiate
                 *    • La purge effective a lieu au prochain Drain() pour sécurité d'itération
                 * 
                 * @note Silencieusement ignoré si l'ID n'est pas trouvé (robustesse).
                 *       Un handler désactivé n'est plus appelé, mais reste en mémoire
                 *       jusqu'au prochain Drain() qui le purge définitivement.
                 */
                void Unsubscribe(HandleId id) noexcept;

                // ── Émission immédiate ─────────────────────────────────────
                /**
                 * @brief Émet un événement à tous les handlers actifs immédiatement.
                 * @param event Événement à émettre (copié par const reference).
                 * 
                 * 🔹 Comportement :
                 *    • Parcours séquentiel de tous les handlers actifs
                 *    • Appel synchrone de chaque handler avec l'événement
                 *    • Thread-safe via NkLockGuard — bloque les autres threads
                 * 
                 * @warning ⚠️ Ne pas appeler Emit() pendant l'itération sur une query ECS
                 *          si les handlers peuvent modifier la structure du monde.
                 *          Préférer Queue() + Drain() différé dans ce cas.
                 * 
                 * @note Les handlers sont appelés dans l'ordre d'abonnement.
                 *       Un handler peut désabonner un autre handler pendant l'appel,
                 *       mais le changement ne sera effectif qu'après Drain().
                 */
                void Emit(const T& event) noexcept;

                // ── File différée : Queue / Drain ──────────────────────────
                /**
                 * @brief Ajoute un événement à la file d'attente pour exécution différée.
                 * @param event Événement à queue (copié par const reference).
                 * 
                 * 🔹 Pourquoi différer ?
                 *    • Sécurité : évite les modifications de structure pendant l'itération
                 *    • Performance : batche plusieurs événements pour un seul parcours handlers
                 *    • Flexibilité : permet d'annuler (Clear()) avant exécution
                 * 
                 * @note L'événement est copié dans mQueue — assurez-vous que T est copiable.
                 *       Pour éviter la copie, utiliser Queue(T&&) avec sémantique de move.
                 */
                void Queue(const T& event) noexcept;

                /**
                 * @brief Version avec sémantique de move pour Queue().
                 * @param event Événement à queue (déplacé pour éviter la copie).
                 * 
                 * @note Utile pour les événements lourds (conteneurs, strings, etc.).
                 *       L'événement source est dans un état valide mais indéfini après l'appel.
                 */
                void Queue(T&& event) noexcept;

                // ── Implémentation de IEventQueue ──────────────────────────
                /**
                 * @brief Exécute tous les événements en file et purge les handlers inactifs.
                 * 
                 * 🔹 Algorithme détaillé :
                 *    1. Swap local : isole mQueue dans toDispatch pour éviter la réentrance
                 *    2. Pour chaque événement dans toDispatch :
                 *       • Parcours de tous les handlers actifs
                 *       • Appel du handler avec l'événement courant
                 *    3. Purge des handlers désactivés (write-pointer pattern) :
                 *       • Parcours de lecture (r) et écriture (write) simultanés
                 *       • Copie uniquement des handlers actifs vers le début du vecteur
                 *       • Resize final pour tronquer les entrées inactives en fin de vecteur
                 * 
                 * 🔹 Sécurité :
                 *    • Le swap local empêche les nouveaux Queue() d'être exécutés
                 *      prématurément pendant le Drain() (évite boucles infinies)
                 *    • La purge différée des handlers évite l'invalidation d'itérateurs
                 *      si un handler se désabonne pendant l'exécution
                 * 
                 * @note À appeler à la fin de chaque frame ou phase de mise à jour gameplay.
                 *       Typiquement : world.FlushDeferred(); bus.Drain();
                 */
                void Drain() noexcept override;

                /**
                 * @brief Vide la file d'attente sans exécuter les événements.
                 * @note Utile pour annuler des événements différés (ex: changement de niveau).
                 */
                void Clear() noexcept override;

                /**
                 * @brief Vérifie si la file contient des événements en attente.
                 * @return true si mQueue est vide, false sinon.
                 */
                bool Empty() const noexcept override;

                // ── Métriques et debug ─────────────────────────────────────
                /**
                 * @brief Retourne le nombre de handlers actuellement souscrits.
                 * @return Nombre d'entrées dans mHandlers (actives + inactives avant purge).
                 * 
                 * @note Thread-safe via NkLockGuard — peut être appelé depuis n'importe quel thread.
                 *       Pour des métriques performance-critical, envisager un compteur atomique.
                 */
                [[nodiscard]] uint32 HandlerCount() const noexcept;

            private:
                /**
                 * @struct HandlerEntry
                 * @brief Représente un handler souscrit avec son état d'activation.
                 */
                struct HandlerEntry {
                    HandleId id;      ///< ID unique pour désabonnement
                    Handler  fn;      ///< Callback à exécuter
                    bool     active;  ///< Flag d'activation (false = marqué pour purge)
                };

                /// Mutex pour thread-safety sur toutes les opérations publiques
                mutable NkMutex mMutex;

                /// Liste des handlers souscrits (actifs et inactifs avant purge)
                NkVector<HandlerEntry> mHandlers;

                /// File d'attente des événements à exécuter au prochain Drain()
                NkVector<T> mQueue;

                /// Compteur incrémental pour génération d'IDs uniques
                HandleId mNextId = 1u;
        };

        // =====================================================================
        // 🚌 NkGameplayEventBus — Façade principale pour événements gameplay
        // =====================================================================
        /**
         * @class NkGameplayEventBus
         * @brief Orchestrateur centralisant la gestion des événements gameplay typés.
         * 
         * 🔹 Responsabilités :
         *    • Création et gestion dynamique de canaux NkEventChannel<T> par type d'événement
         *    • Encodage des SubscriptionId pour désabonnement type-safe (typeId << 32 | handleId)
         *    • Thread-safety globale via NkMutex sur les opérations de gestion de canaux
         *    • Drain/Clear coordonnés sur tous les canaux actifs
         * 
         * 🔹 Architecture interne :
         *    • Tableau fixe mEntries[kMaxChannels] pour stockage inline des canaux
         *    • Placement-new dans storage[] pour éviter les allocations heap par canal
         *    • Recherche linéaire par TypeId pour GetOrCreate/Find — O(n) mais n ≤ 256
         * 
         * 🔹 Différence avec EventBus (moteur) :
         *    • NkGameplayEventBus : événements métier entre systèmes ECS (DamageEvent, QuestCompleted)
         *    • EventBus (Core) : événements bas-niveau plateforme (NkKeyPressEvent, NkWindowResize)
         *    • Les deux peuvent coexister — choisir selon le niveau d'abstraction
         * 
         * @warning NkGameplayEventBus n'est PAS un singleton global — instanciez-le
         *          dans votre NkWorld ou GameManager selon l'architecture souhaitée.
         * 
         * @example
         * @code
         * NkGameplayEventBus bus;
         * 
         * // Souscription à un événement de dégâts
         * auto subId = bus.Subscribe<DamageEvent>([](const DamageEvent& evt) {
         *     ApplyDamage(evt.target, evt.amount);
         * });
         * 
         * // Émission immédiate
         * bus.Emit<DamageEvent>({playerId, 25.0f});
         * 
         * // Émission différée (sécurisée pendant itération)
         * bus.Queue<DamageEvent>({enemyId, 10.0f});
         * bus.Drain();  // Exécute tous les événements en file
         * 
         * // Désabonnement
         * bus.Unsubscribe<DamageEvent>(subId);
         * @endcode
         */
        class NkGameplayEventBus {
            public:
                /**
                 * @brief Type des IDs de subscription : encodage 64 bits.
                 * 
                 * 🔹 Layout :
                 *    • Bits 63-32 : TypeId de l'événement (via NkTypeRegistry)
                 *    • Bits 31-0  : HandleId local au canal (via NkEventChannel<T>)
                 * 
                 * 🔹 Avantages :
                 *    • Désabonnement type-safe : Unsubscribe<T>(id) vérifie que le TypeId correspond
                 *    • Pas de collision entre canaux : même HandleId, TypeId différent → ID différent
                 *    • Compact : un uint64 suffit pour identifier globalement un abonnement
                 * 
                 * @note L'encodage est interne — les utilisateurs manipulent opaque SubscriptionId.
                 */
                using SubscriptionId = uint64;

                // ── Abonnement / Désabonnement ─────────────────────────────
                /**
                 * @brief Souscrit un handler à un type d'événement T.
                 * @tparam T Type de l'événement à écouter.
                 * @tparam Fn Type du callable (fonction, lambda, functor).
                 * @param fn Callback à exécuter lors de l'émission d'un événement de type T.
                 * @return SubscriptionId unique pour désabonnement ultérieur.
                 * 
                 * 🔹 Mécanisme :
                 *    1. GetOrCreate<T>() : récupère ou crée le canal NkEventChannel<T>
                 *    2. channel.Subscribe() : enregistre le handler, retourne un HandleId local
                 *    3. Encodage : (TypeId<T> << 32) | HandleId → SubscriptionId global
                 * 
                 * @note Thread-safe via NkLockGuard sur mMutex du bus.
                 *       Le premier Subscribe<T>() alloue le canal inline (placement-new).
                 * 
                 * @warning Le handler reçoit const T& — ne pas modifier l'événement sauf si T
                 *          est explicitement mutable. Pour des modifications, utiliser
                 *          un événement wrapper avec champs mutable ou pointer.
                 */
                template<typename T, typename Fn>
                SubscriptionId Subscribe(Fn&& fn) noexcept;

                /**
                 * @brief Désabonne un handler précédemment souscrit.
                 * @tparam T Type de l'événement (doit correspondre au Subscribe original).
                 * @param id SubscriptionId retourné par Subscribe<T>().
                 * 
                 * 🔹 Vérification de type :
                 *    • Extrait le HandleId local : id & 0xFFFFFFFF
                 *    • Trouve le canal par TypeId<T>() — retourne nullptr si type inconnu
                 *    • Appelle channel.Unsubscribe(handleId) si le canal existe
                 * 
                 * @note Silencieusement ignoré si :
                 *    • Le type T n'a jamais eu de canal créé
                 *    • Le HandleId n'existe plus (déjà désabonné ou purgé)
                 * 
                 * @warning Ne pas désabonner un handler pendant son propre exécution —
                 *          le marquage comme inactif est sûr, mais la purge effective
                 *          n'aura lieu qu'au prochain Drain().
                 */
                template<typename T>
                void Unsubscribe(SubscriptionId id) noexcept;

                // ── Émission immédiate ─────────────────────────────────────
                /**
                 * @brief Émet un événement de type T à tous les handlers souscrits.
                 * @tparam T Type de l'événement à émettre.
                 * @param event Événement à émettre (copié par const reference).
                 * 
                 * 🔹 Comportement :
                 *    • Trouve le canal NkEventChannel<T> via Find<T>()
                 *    • Si trouvé : appelle channel.Emit(event) — exécution synchrone
                 *    • Si non trouvé : aucun handler n'est appelé (silencieux)
                 * 
                 * @warning ⚠️ Emit() est synchrone et bloque le thread courant via NkLockGuard.
                 *          Éviter les handlers lourds ou bloquants pour ne pas ralentir
                 *          l'émetteur. Pour du découplage temporel, utiliser Queue().
                 */
                template<typename T>
                void Emit(const T& event) noexcept;

                /**
                 * @brief Version avec sémantique de move pour Emit().
                 * @tparam T Type de l'événement.
                 * @param event Événement à émettre (déplacé pour éviter la copie).
                 * 
                 * @note Utile pour les événements contenant des conteneurs ou strings.
                 *       L'événement source est dans un état valide mais indéfini après l'appel.
                 */
                template<typename T>
                void Emit(T&& event) noexcept;

                // ── File différée : Queue / Drain ──────────────────────────
                /**
                 * @brief Ajoute un événement de type T à la file d'attente différée.
                 * @tparam T Type de l'événement.
                 * @param event Événement à queue (copié par const reference).
                 * 
                 * 🔹 Pourquoi Queue() ?
                 *    • Sécurité : évite les modifications de structure ECS pendant l'itération
                 *    • Performance : batche plusieurs événements pour un seul parcours handlers
                 *    • Flexibilité : permet Clear() pour annuler avant exécution
                 * 
                 * @note GetOrCreate<T>() garantit que le canal existe — allocation inline
                 *       au premier appel pour ce type. Thread-safe via NkLockGuard.
                 */
                template<typename T>
                void Queue(const T& event) noexcept;

                /**
                 * @brief Version avec sémantique de move pour Queue().
                 * @tparam T Type de l'événement.
                 * @param event Événement à queue (déplacé pour éviter la copie).
                 */
                template<typename T>
                void Queue(T&& event) noexcept;

                // ── Drain / Clear coordonnés sur tous les canaux ───────────
                /**
                 * @brief Exécute tous les événements en file sur tous les canaux actifs.
                 * 
                 * 🔹 Algorithme :
                 *    • Parcours séquentiel de mEntries[0..mCount-1]
                 *    • Pour chaque entrée avec queue != nullptr : appelle queue->Drain()
                 *    • Chaque Drain() de canal exécute ses événements et purge ses handlers
                 * 
                 * 🔹 Ordre d'exécution :
                 *    • Les canaux sont drainés dans l'ordre de création (premier Subscribe<T>)
                 *    • À l'intérieur d'un canal : ordre FIFO des événements, ordre d'abonnement des handlers
                 * 
                 * @note À appeler à la fin de chaque frame ou phase de mise à jour gameplay.
                 *       Pattern typique :
                 *       @code
                 *       world.FlushDeferred();  // Opérations ECS différées
                 *       eventBus.Drain();        // Événements gameplay différés
                 *       @endcode
                 */
                void Drain() noexcept;

                /**
                 * @brief Vide toutes les files d'attente sans exécuter les événements.
                 * @note Utile pour :
                 *    • Changement de niveau/scene : annuler les événements en transit
                 *    • Debug : reset de l'état événementiel
                 *    • Optimisation : skip des événements non critiques en cas de lag
                 */
                void Clear() noexcept;

                /**
                 * @brief Vérifie s'il reste des événements en attente sur n'importe quel canal.
                 * @return true si au moins un canal a !Empty(), false sinon.
                 * 
                 * @note Thread-safe via NkLockGuard — peut être appelé depuis n'importe quel thread.
                 *       Utile pour :
                 *       • Déterminer si le jeu peut passer en état "idle"
                 *       • Debug : vérifier qu'aucun événement n'est "perdu"
                 */
                [[nodiscard]] bool HasPending() const noexcept;

            private:
                /**
                 * @struct ChannelEntry
                 * @brief Stockage inline pour un canal NkEventChannel<T> sans allocation heap.
                 * 
                 * 🔹 Mécanisme :
                 *    • typeId : identifiant compile-time du type T via NkTypeRegistry
                 *    • queue : pointeur polymorphique vers IEventQueue (pour Drain/Clear)
                 *    • storage[] : buffer inline surprovisionné pour placement-new du canal
                 * 
                 * 🔹 Pourquoi surprovisionner avec sizeof(NkEventChannel<int>) * 4 ?
                 *    • NkEventChannel<T> a une taille qui dépend de T (via NkVector<T>)
                 *    • On prend une marge conservative pour couvrir la majorité des types
                 *    • En production : remplacer par un allocateur d'arènes dédié si besoin
                 * 
                 * @warning static_assert dans GetOrCreate() vérifie que sizeof(NkEventChannel<T>)
                 *          tient dans storage[] — assertion compile-time pour sécurité.
                 */
                struct ChannelEntry {
                    /// TypeId du canal (via NkTypeRegistry::TypeId<T>())
                    NkComponentId typeId = kInvalidComponentId;

                    /// Pointeur polymorphique vers l'interface IEventQueue
                    IEventQueue* queue = nullptr;

                    /// Buffer inline pour placement-new du canal — évite heap allocation
                    uint8 storage[sizeof(NkEventChannel<int>) * 4] = {};
                };

                /// Nombre maximum de types d'événements distincts supportés
                static constexpr uint32 kMaxChannels = 256u;

                /// Tableau fixe d'entrées de canaux — pas d'allocation dynamique
                ChannelEntry mEntries[kMaxChannels] = {};

                /// Nombre de canaux actuellement actifs (≤ kMaxChannels)
                uint32 mCount = 0;

                /// Mutex pour thread-safety sur la gestion des canaux
                mutable threading::NkMutex mMutex;

                /**
                 * @brief Récupère ou crée un canal NkEventChannel<T> pour le type T.
                 * @tparam T Type de l'événement.
                 * @return Référence au canal NkEventChannel<T> créé ou réutilisé.
                 * 
                 * 🔹 Algorithme :
                 *    1. Calcule TypeId<T>() via NkTypeRegistry (compile-time → runtime lookup)
                 *    2. Recherche linéaire dans mEntries[0..mCount-1] par typeId
                 *    3. Si trouvé : cast et retour du canal existant
                 *    4. Si non trouvé :
                 *       • Asserte que mCount < kMaxChannels (limite de types)
                 *       • Placement-new de NkEventChannel<T> dans storage[] de la nouvelle entrée
                 *       • Initialisation de typeId et queue, incrément de mCount
                 *       • Retour du nouveau canal
                 * 
                 * @note Thread-safe via NkLockGuard du bus (appelé depuis Subscribe/Queue publics).
                 *       La création de canal est rare (premier événement de ce type) — impact négligeable.
                 * 
                 * @warning Le placement-new dans storage[] suppose que sizeof(NkEventChannel<T>)
                 *          ≤ sizeof(storage) — vérifié par static_assert au compile-time.
                 */
                template<typename T>
                NkEventChannel<T>& GetOrCreate() noexcept;

                /**
                 * @brief Trouve un canal existant pour le type T, ou retourne nullptr.
                 * @tparam T Type de l'événement recherché.
                 * @return Pointeur vers NkEventChannel<T> si trouvé, nullptr sinon.
                 * 
                 * @note Utilisé par Emit/Unsubscribe pour éviter de créer un canal
                 *       inutilement si aucun handler n'est souscrit pour ce type.
                 * 
                 * @warning Retourne nullptr si le type T n'a jamais eu de Subscribe/Queue/Emit.
                 *          Les appelants doivent vérifier le résultat avant déréférencement.
                 */
                template<typename T>
                NkEventChannel<T>* Find() noexcept;
        };

        // =====================================================================
        // 🔗 Alias de compatibilité transitoire
        // =====================================================================
        /**
         * @brief Alias pour migration progressive : NkEventBus → NkGameplayEventBus.
         * @deprecated Utiliser directement NkGameplayEventBus dans le nouveau code.
         * 
         * @note Cet alias sera supprimé après migration complète du codebase.
         *       Nouveaux développements : toujours utiliser NkGameplayEventBus
         *       pour éviter toute confusion avec EventBus (moteur).
         */
        using NkEventBus = NkGameplayEventBus;

        // =====================================================================
        // 📦 Implémentations inline des templates (requis pour instantiation)
        // =====================================================================

        // ── NkEventChannel<T> : Abonnement ─────────────────────────────────
        template<typename T>
        typename NkEventChannel<T>::HandleId
        NkEventChannel<T>::Subscribe(Handler handler) noexcept {
            NkLockGuard lock(mMutex);
            const HandleId id = mNextId++;
            mHandlers.PushBack({id, static_cast<Handler&&>(handler), true});
            return id;
        }

        template<typename T>
        void NkEventChannel<T>::Unsubscribe(HandleId id) noexcept {
            NkLockGuard lock(mMutex);
            for (nk_usize i = 0; i < mHandlers.Size(); ++i) {
                if (mHandlers[i].id == id) {
                    mHandlers[i].active = false;
                    return;
                }
            }
        }

        // ── NkEventChannel<T> : Émission immédiate ────────────────────────
        template<typename T>
        void NkEventChannel<T>::Emit(const T& event) noexcept {
            NkLockGuard lock(mMutex);
            for (nk_usize i = 0; i < mHandlers.Size(); ++i) {
                auto& e = mHandlers[i];
                if (e.active && e.fn) {
                    e.fn(event);
                }
            }
        }

        // ── NkEventChannel<T> : File différée ─────────────────────────────
        template<typename T>
        void NkEventChannel<T>::Queue(const T& event) noexcept {
            NkLockGuard lock(mMutex);
            mQueue.PushBack(event);
        }

        template<typename T>
        void NkEventChannel<T>::Queue(T&& event) noexcept {
            NkLockGuard lock(mMutex);
            mQueue.PushBack(static_cast<T&&>(event));
        }

        // ── NkEventChannel<T> : Drain (exécution différée + purge) ────────
        template<typename T>
        void NkEventChannel<T>::Drain() noexcept {
            NkLockGuard lock(mMutex);

            // ── Étape 1 : Swap local pour éviter la réentrance ──
            NkVector<T> toDispatch;
            toDispatch.Swap(mQueue);

            // ── Étape 2 : Exécution des événements en file ──
            for (nk_usize i = 0; i < toDispatch.Size(); ++i) {
                for (nk_usize j = 0; j < mHandlers.Size(); ++j) {
                    auto& e = mHandlers[j];
                    if (e.active && e.fn) {
                        e.fn(toDispatch[i]);
                    }
                }
            }

            // ── Étape 3 : Purge des handlers désactivés (write-pointer) ──
            nk_usize write = 0;
            for (nk_usize r = 0; r < mHandlers.Size(); ++r) {
                if (mHandlers[r].active) {
                    if (write != r) {
                        mHandlers[write] = static_cast<HandlerEntry&&>(mHandlers[r]);
                    }
                    ++write;
                }
            }
            mHandlers.Resize(write);
        }

        // ── NkEventChannel<T> : Clear / Empty / HandlerCount ─────────────
        template<typename T>
        void NkEventChannel<T>::Clear() noexcept {
            NkLockGuard lock(mMutex);
            mQueue.Clear();
        }

        template<typename T>
        bool NkEventChannel<T>::Empty() const noexcept {
            NkLockGuard lock(mMutex);
            return mQueue.IsEmpty();
        }

        template<typename T>
        uint32 NkEventChannel<T>::HandlerCount() const noexcept {
            NkLockGuard lock(mMutex);
            return static_cast<uint32>(mHandlers.Size());
        }

        // ── NkGameplayEventBus : Subscribe / Unsubscribe ─────────────────
        template<typename T, typename Fn>
        typename NkGameplayEventBus::SubscriptionId
        NkGameplayEventBus::Subscribe(Fn&& fn) noexcept {
            NkLockGuard lock(mMutex);
            NkEventChannel<T>& ch = GetOrCreate<T>();
            typename NkEventChannel<T>::HandleId hid =
                ch.Subscribe(NkFunction<void(const T&)>(traits::NkForward<Fn>(fn)));
            const uint64 tid = static_cast<uint64>(
                NkTypeRegistry::Global().TypeId<T>()
            );
            return (tid << 32u) | static_cast<uint64>(hid);
        }

        template<typename T>
        void NkGameplayEventBus::Unsubscribe(SubscriptionId id) noexcept {
            NkLockGuard lock(mMutex);
            NkEventChannel<T>* ch = Find<T>();
            if (ch) {
                ch->Unsubscribe(static_cast<uint32>(id & 0xFFFFFFFFu));
            }
        }

        // ── NkGameplayEventBus : Emit (immédiat) ─────────────────────────
        template<typename T>
        void NkGameplayEventBus::Emit(const T& event) noexcept {
            NkLockGuard lock(mMutex);
            if (NkEventChannel<T>* ch = Find<T>()) {
                ch->Emit(event);
            }
        }

        template<typename T>
        void NkGameplayEventBus::Emit(T&& event) noexcept {
            NkLockGuard lock(mMutex);
            if (NkEventChannel<T>* ch = Find<T>()) {
                ch->Emit(traits::NkForward<T>(event));
            }
        }

        // ── NkGameplayEventBus : Queue (différé) ─────────────────────────
        template<typename T>
        void NkGameplayEventBus::Queue(const T& event) noexcept {
            NkLockGuard lock(mMutex);
            GetOrCreate<T>().Queue(event);
        }

        template<typename T>
        void NkGameplayEventBus::Queue(T&& event) noexcept {
            NkLockGuard lock(mMutex);
            GetOrCreate<T>().Queue(traits::NkForward<T>(event));
        }

        // ── NkGameplayEventBus : GetOrCreate / Find (privé) ──────────────
        template<typename T>
        NkEventChannel<T>& NkGameplayEventBus::GetOrCreate() noexcept {
            const NkComponentId tid = NkTypeRegistry::Global().TypeId<T>();
            for (uint32 i = 0; i < mCount; ++i) {
                if (mEntries[i].typeId == tid) {
                    return *static_cast<NkEventChannel<T>*>(mEntries[i].queue);
                }
            }
            NKECS_ASSERT(mCount < kMaxChannels);
            ChannelEntry& e = mEntries[mCount++];
            e.typeId = tid;
            // Placement new dans le stockage inline
            static_assert(
                sizeof(NkEventChannel<T>) <= sizeof(e.storage),
                "NkEventChannel<T> dépasse le stockage inline — augmenter storage[]"
            );
            NkEventChannel<T>* ch = new (e.storage) NkEventChannel<T>();
            e.queue = ch;
            return *ch;
        }

        template<typename T>
        NkEventChannel<T>* NkGameplayEventBus::Find() noexcept {
            const NkComponentId tid = NkTypeRegistry::Global().TypeId<T>();
            for (uint32 i = 0; i < mCount; ++i) {
                if (mEntries[i].typeId == tid) {
                    return static_cast<NkEventChannel<T>*>(mEntries[i].queue);
                }
            }
            return nullptr;
        }

        // =====================================================================
        // 📚 EXEMPLES D'UTILISATION COMPLETS ET DÉTAILLÉS
        // =====================================================================
        /**
         * @addtogroup NkGameplayEventBus_Examples
         * @{
         */

        /**
         * @example 01_basic_subscribe_emit.cpp
         * @brief Souscription et émission basique d'événements gameplay.
         * 
         * @code
         * #include "NKECS/Events/NkGameplayEventBus.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * // Définition d'un événement personnalisé
         * struct DamageEvent {
         *     NkEntityId target;
         *     float amount;
         *     NkEntityId source;  // Optionnel : qui a infligé les dégâts
         * };
         * NK_EVENT(DamageEvent)  // Macro documentaire, sans effet
         * 
         * void Example_BasicUsage() {
         *     NkGameplayEventBus bus;
         * 
         *     // ── Souscription à un événement avec lambda ─────────────────
         *     auto subId = bus.Subscribe<DamageEvent>(
         *         [](const DamageEvent& evt) {
         *             printf("Entity %u received %.1f damage from %u\n",
         *                    evt.target, evt.amount, evt.source);
         *             // Logique métier : appliquer les dégâts, jouer un son, etc.
         *         }
         *     );
         * 
         *     // ── Émission immédiate (synchrone) ─────────────────────────
         *     bus.Emit<DamageEvent>({playerId, 25.0f, enemyId});
         *     // Le handler est appelé IMMÉDIATEMENT dans le thread courant
         * 
         *     // ── Émission avec move sémantique (pour événements lourds) ─
         *     DamageEvent heavyEvent = BuildComplexDamageEvent();
         *     bus.Emit<DamageEvent>(std::move(heavyEvent));
         *     // heavyEvent est dans un état valide mais indéfini après l'appel
         * 
         *     // ── Désabonnement ──────────────────────────────────────────
         *     bus.Unsubscribe<DamageEvent>(subId);
         *     // Les futures émissions de DamageEvent n'appelleront plus ce handler
         * 
         *     // ── Souscription multiple au même type ─────────────────────
         *     auto sub1 = bus.Subscribe<DamageEvent>(LogDamageToConsole);
         *     auto sub2 = bus.Subscribe<DamageEvent>(ApplyArmorReduction);
         *     auto sub3 = bus.Subscribe<DamageEvent>(TriggerOnHitEffects);
         *     // Les trois handlers sont appelés dans l'ordre d'abonnement
         * }
         * @endcode
         */

        /**
         * @example 02_deferred_queue_drain.cpp
         * @brief Utilisation du pattern Queue/Drain pour sécurité pendant l'itération.
         * 
         * @code
         * #include "NKECS/Events/NkGameplayEventBus.h"
         * #include "NKECS/World/NkWorld.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * struct AreaDamageEvent {
         *     NkVec3 center;
         *     float radius;
         *     float damage;
         * };
         * NK_EVENT(AreaDamageEvent)
         * 
         * void Example_DeferredEvents() {
         *     NkWorld world;
         *     NkGameplayEventBus bus;
         * 
         *     // Handler qui peut modifier la structure du monde (détruire des entités)
         *     bus.Subscribe<AreaDamageEvent>(
         *         [&world](const AreaDamageEvent& evt) {
         *             // Query sur les entités dans la zone de dégâts
         *             for (auto [health, pos] : world.Query<Health, NkTransform>()) {
         *                 float dist = Distance(evt.center, pos->position);
         *                 if (dist <= evt.radius) {
         *                     health->value -= evt.damage;
         *                     if (health->value <= 0) {
         *                         // ⚠️ Destruction différée : sécurisée pendant l'itération
         *                         world.DestroyDeferred(pos->ownerEntity);
         *                     }
         *                 }
         *             }
         *         }
         *     );
         * 
         *     // ── Scénario : explosion de grenade pendant une query ──────
         *     // On itère sur les entités pour détecter les grenades actives
         *     for (auto [grenade, pos] : world.Query<Grenade, NkTransform>()) {
         *         if (grenade->IsExploded()) {
         *             // ❌ NE PAS FAIRE : Emit immédiat pourrait modifier la query en cours
         *             // bus.Emit<AreaDamageEvent>({pos->position, 5.f, 30.f});
         * 
         *             // ✅ FAIRE : Queue différé pour exécution après la boucle
         *             bus.Queue<AreaDamageEvent>({pos->position, 5.f, 30.f});
         *         }
         *     }
         * 
         *     // ── Exécution des événements différés ──────────────────────
         *     // À appeler APRÈS toutes les itérations de query de la frame
         *     world.FlushDeferred();  // Applique les DestroyDeferred, AddDeferred, etc.
         *     bus.Drain();            // Exécute les événements en file
         * 
         *     // ── Annulation d'événements différés (ex: changement de niveau)
         *     if (ShouldCancelPendingEvents()) {
         *         bus.Clear();  // Vide toutes les files sans exécuter
         *     }
         * 
         *     // ── Vérification d'événements en attente (debug/optimisation)
         *     if (bus.HasPending()) {
         *         printf("Attention : %u événements en attente\n", / * comptage custom * /);
         *     }
         * }
         * @endcode
         */

        /**
         * @example 03_handler_lifecycle.cpp
         * @brief Gestion avancée du cycle de vie des handlers (désabonnement, purge).
         * 
         * @code
         * #include "NKECS/Events/NkGameplayEventBus.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * struct QuestCompletedEvent {
         *     uint32 questId;
         *     NkEntityId player;
         * };
         * NK_EVENT(QuestCompletedEvent)
         * 
         * class QuestManager {
         *     NkGameplayEventBus& mBus;
         *     NkGameplayEventBus::SubscriptionId mQuestSubId;
         * 
         * public:
         *     QuestManager(NkGameplayEventBus& bus) : mBus(bus) {
         *         // Souscription au démarrage du manager
         *         mQuestSubId = mBus.Subscribe<QuestCompletedEvent>(
         *             [this](const QuestCompletedEvent& evt) {
         *                 OnQuestCompleted(evt.questId, evt.player);
         *             }
         *         );
         *     }
         * 
         *     ~QuestManager() {
         *         // Désabonnement à la destruction pour éviter les dangling callbacks
         *         mBus.Unsubscribe<QuestCompletedEvent>(mQuestSubId);
         *     }
         * 
         * private:
         *     void OnQuestCompleted(uint32 questId, NkEntityId player) {
         *         // Logique métier : récompenses, dialogue, sauvegarde, etc.
         *     }
         * };
         * 
         * void Example_HandlerLifecycle() {
         *     NkGameplayEventBus bus;
         * 
         *     // ── Pattern RAII : souscription dans le constructeur, désabonnement dans le destructeur
         *     {
         *         QuestManager manager(bus);
         *         bus.Emit<QuestCompletedEvent>({42, playerId});  // Handler appelé
         *     }  // manager détruit → désabonnement automatique
         * 
         *     bus.Emit<QuestCompletedEvent>({43, playerId});  // Aucun handler — silencieux
         * 
         *     // ── Désabonnement conditionnel depuis un handler ───────────
         *     NkGameplayEventBus::SubscriptionId onceSubId = bus.Subscribe<QuestCompletedEvent>(
         *         [&bus, onceSubId](const QuestCompletedEvent& evt) {
         *             printf("Première complétion de quête %u\n", evt.questId);
         *             // Auto-désabonnement après première exécution
         *             bus.Unsubscribe<QuestCompletedEvent>(onceSubId);
         *         }
         *     );
         * 
         *     bus.Emit<QuestCompletedEvent>({1, playerId});  // Appelée
         *     bus.Emit<QuestCompletedEvent>({2, playerId});  // Ignorée (handler désabonné)
         * 
         *     // ── Purge des handlers inactifs lors du Drain() ────────────
         *     // Quand un handler est désabonné, il est marqué inactive mais pas supprimé
         *     // immédiatement pour éviter d'invalider l'itération en cours.
         *     // La purge effective a lieu au prochain Drain().
         * 
         *     // Pour forcer la purge immédiate (rarement nécessaire) :
         *     bus.Drain();  // Exécute les événements en file ET purge les handlers inactifs
         * }
         * @endcode
         */

        /**
         * @example 04_thread_safety_patterns.cpp
         * @brief Utilisation thread-safe de NkGameplayEventBus dans un contexte multi-thread.
         * 
         * @code
         * #include "NKECS/Events/NkGameplayEventBus.h"
         * #include "NKThreading/NkThread.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * struct NetworkMessageEvent {
         *     uint32 playerId;
         *     NkString payload;  // String personnalisée (copiable)
         * };
         * NK_EVENT(NetworkMessageEvent)
         * 
         * void Example_ThreadSafety() {
         *     NkGameplayEventBus bus;
         * 
         *     // ── Handler appelé depuis le thread principal (game loop) ──
         *     bus.Subscribe<NetworkMessageEvent>(
         *         [](const NetworkMessageEvent& evt) {
         *             // Ce handler s'exécute dans le thread qui appelle Emit/Drain
         *             // Typiquement : thread principal du jeu
         *             ProcessNetworkMessage(evt.playerId, evt.payload);
         *         }
         *     );
         * 
         *     // ── Thread réseau : émission depuis un worker thread ───────
         *     NkThread networkThread([&bus]() {
         *         while (networkRunning) {
         *             auto msg = ReceiveNetworkMessage();  // Bloquant
         *             if (msg.has_value()) {
         *                 // ✅ Thread-safe : Emit() acquiert le mutex interne
         *                 // ⚠️ Mais : le handler s'exécute dans CE thread (networkThread)
         *                 //    Si le handler accède à des données non thread-safe (ex: monde ECS),
         *                 //    cela peut causer des race conditions.
         *                 bus.Emit<NetworkMessageEvent>({msg->playerId, msg->payload});
         *             }
         *         }
         *     });
         * 
         *     // ── Solution recommandée : Queue + Drain dans le thread principal
         *     // Thread réseau :
         *     NkThread networkThreadSafe([&bus]() {
         *         while (networkRunning) {
         *             auto msg = ReceiveNetworkMessage();
         *             if (msg.has_value()) {
         *                 // ✅ Queue est thread-safe et ne déclenche pas l'exécution
         *                 bus.Queue<NetworkMessageEvent>({msg->playerId, msg->payload});
         *             }
         *         }
         *     });
         * 
         *     // Thread principal (game loop) :
         *     while (gameRunning) {
         *         UpdateGameLogic();
         * 
         *         // ✅ Drain exécute les événements dans le thread principal
         *         //    → Les handlers accèdent aux données ECS en toute sécurité
         *         bus.Drain();
         * 
         *         RenderFrame();
         *     }
         * 
         *     networkThreadSafe.Join();
         * 
         *     // ── Bonnes pratiques thread-safety :
         *     // • Préférer Queue/Drain pour découpler l'émission de l'exécution
         *     // • Documenter dans quel thread les handlers sont censés s'exécuter
         *     // • Éviter les handlers bloquants ou longs dans Emit() synchrone
         *     // • Pour les événements critiques temps-réel, envisager un bus dédié
         *     //   avec priorité ou thread-pool d'exécution
         * }
         * @endcode
         */

        /**
         * @example 05_advanced_patterns.cpp
         * @brief Patterns avancés : filtrage, priorité, événement avec contexte.
         * 
         * @code
         * #include "NKECS/Events/NkGameplayEventBus.h"
         * 
         * using namespace nkentseu::ecs;
         * 
         * // ── Pattern 1 : Événement avec contexte mutable pour accumulation ─
         * struct DamageCalculationEvent {
         *     float baseDamage;
         *     float finalDamage;  // Modifiable par les handlers
         *     NkEntityId attacker;
         *     NkEntityId target;
         * 
         *     DamageCalculationEvent(float base, NkEntityId atk, NkEntityId tgt)
         *         : baseDamage(base), finalDamage(base), attacker(atk), target(tgt) {}
         * };
         * NK_EVENT(DamageCalculationEvent)
         * 
         * void Example_MutableEvent() {
         *     NkGameplayEventBus bus;
         * 
         *     // Handler 1 : bonus de critique
         *     bus.Subscribe<DamageCalculationEvent>(
         *         [](DamageCalculationEvent& evt) {  // Note : prend par référence non-const !
         *             if (IsCriticalHit(evt.attacker)) {
         *                 evt.finalDamage *= 2.0f;
         *             }
         *         }
         *     );
         * 
         *     // Handler 2 : réduction d'armure
         *     bus.Subscribe<DamageCalculationEvent>(
         *         [](DamageCalculationEvent& evt) {
         *             float armor = GetArmor(evt.target);
         *             evt.finalDamage *= (100.f / (100.f + armor));
         *         }
         *     );
         * 
         *     // Émission : les handlers modifient evt.finalDamage en chaîne
         *     DamageCalculationEvent evt(50.f, attackerId, targetId);
         *     bus.Emit<DamageCalculationEvent>(evt);  // Attention : passe par const T& par défaut !
         *     // Pour permettre la modification, il faudrait un bus spécialisé ou un wrapper mutable
         * 
         *     // 🔹 Alternative recommandée : retour de valeur ou callback de finalisation
         *     float ComputeFinalDamage(float base, NkEntityId atk, NkEntityId tgt) {
         *         float result = base;
         *         if (IsCriticalHit(atk)) result *= 2.0f;
         *         float armor = GetArmor(tgt);
         *         result *= (100.f / (100.f + armor));
         *         return result;
         *     }
         * }
         * 
         * // ── Pattern 2 : Filtrage par métadonnées dans le handler ───────
         * struct TaggedEvent {
         *     uint8 priority;  // 0 = basse, 255 = critique
         *     uint8 category;  // Bitmask de catégories
         *     // ... autres champs
         * };
         * 
         * void Example_FilteredHandler() {
         *     NkGameplayEventBus bus;
         * 
         *     // Handler qui ignore les événements basse priorité
         *     bus.Subscribe<TaggedEvent>(
         *         [](const TaggedEvent& evt) {
         *             if (evt.priority < 128) return;  // Skip basse priorité
         *             if (!(evt.category & CATEGORY_COMBAT)) return;  // Skip si pas combat
         *             ProcessHighPriorityCombatEvent(evt);
         *         }
         *     );
         * 
         *     // 🔹 Alternative : créer des bus séparés par catégorie/priority
         *     // NkGameplayEventBus combatBus, uiBus, audioBus;
         *     // Plus de contrôle, mais plus de complexité de gestion
         * }
         * 
         * // ── Pattern 3 : Événement avec callback de réponse (request/response)
         * template<typename ResponseT>
         * struct RequestEvent {
         *     NkFunction<void(const ResponseT&)> onResponse;  // Callback de réponse
         *     // ... champs de la requête
         * };
         * 
         * void Example_RequestResponse() {
         *     NkGameplayEventBus bus;
         * 
         *     // Handler qui traite une requête et appelle le callback de réponse
         *     bus.Subscribe<RequestEvent<int>>(
         *         [](const RequestEvent<int>& req) {
         *             int result = ComputeAnswer(req.data);
         *             if (req.onResponse) {
         *                 req.onResponse(result);  // Appel synchrone de la réponse
         *             }
         *         }
         *     );
         * 
         *     // Émission avec callback de réponse
         *     bus.Emit<RequestEvent<int>>({
         *         .onResponse = [](int answer) {
         *             printf("Réponse reçue : %d\n", answer);
         *         },
         *         // ... autres champs
         *     });
         * 
         *     // ⚠️ Attention : le callback onResponse s'exécute dans le thread
         *     // du handler, pas nécessairement dans le thread émetteur.
         *     // Pour un vrai async, utiliser Queue + futur/promise ou coroutine.
         * }
         * @endcode
         */

        /**
         * @example 06_best_practices_pitfalls.cpp
         * @brief Bonnes pratiques et pièges courants avec NkGameplayEventBus.
         * 
         * @code
         * // ✅ BONNES PRATIQUES
         * 
         * // 1. Désabonner les handlers à la destruction de leur propriétaire
         * class MySystem {
         *     NkGameplayEventBus& bus;
         *     SubscriptionId subId;
         * public:
         *     MySystem(NkGameplayEventBus& b) : bus(b) {
         *         subId = bus.Subscribe<MyEvent>([this](auto& e) { OnEvent(e); });
         *     }
         *     ~MySystem() { bus.Unsubscribe<MyEvent>(subId); }  // ✅ Évite dangling callback
         * };
         * 
         * // 2. Préférer Queue/Drain pour les événements pouvant modifier l'ECS
         * for (auto [comp] : world.Query<MyComp>()) {
         *     if (ShouldTriggerEvent(comp)) {
         *         bus.Queue<MyEvent>(BuildEvent(comp));  // ✅ Sécurisé
         *         // bus.Emit<MyEvent>(...);             // ❌ Risque d'invalidation d'itérateur
         *     }
         * }
         * world.FlushDeferred();
         * bus.Drain();  // ✅ Exécute dans un contexte sûr
         * 
         * // 3. Garder les handlers légers et non-bloquants pour Emit() synchrone
         * bus.Subscribe<FrameEvent>([](auto&) {
         *     // ✅ Logique rapide : mise à jour de compteur, flag, etc.
         * });
         * // ❌ Éviter : I/O, allocations heap lourdes, locks externes dans Emit()
         * 
         * // 4. Documenter le thread d'exécution attendu des handlers
         * /// @thread MainThread — ne pas appeler depuis un worker sans Queue/Drain
         * bus.Subscribe<RenderEvent>(OnRender);
         * 
         * // 5. Utiliser HasPending() pour optimiser les frames vides
         * if (!bus.HasPending() && !world.HasPendingOps()) {
         *     SkipExpensiveUpdate();  // ✅ Optimisation safe
         * }
         * 
         * 
         * // ❌ PIÈGES À ÉVITER
         * 
         * // 1. Conserver des pointeurs/références aux événements au-delà du handler
         * bus.Subscribe<MyEvent>([](const MyEvent& evt) {
         *     globalCache = &evt;  // ❌ Dangling pointer après retour du handler !
         * });
         * // ✅ Solution : copier les données nécessaires dans un contexte durable
         * 
         * // 2. Désabonner un handler depuis son propre callback sans précaution
         * SubscriptionId selfSub;
         * selfSub = bus.Subscribe<MyEvent>([&bus, selfSub](auto&) {
         *     bus.Unsubscribe<MyEvent>(selfSub);  // ⚠️ Marquage OK, mais purge différée
         *     // Le handler peut encore être appelé une fois si dans une boucle d'émission
         * });
         * // ✅ Solution : utiliser un flag "oneShot" interne au handler si nécessaire
         * 
         * // 3. Émettre depuis un thread worker avec Emit() sans synchronisation des données
         * NkThread worker([&bus, &world]() {
         *     bus.Emit<DataEvent>({world.GetCriticalData()});  // ❌ Race condition si world non thread-safe
         * });
         * // ✅ Solution : Queue depuis le worker, Drain dans le thread principal
         * 
         * // 4. Oublier de drainer les événements différés
         * bus.Queue<MyEvent>(evt);
         * // ... sans jamais appeler bus.Drain() → l'événement n'est jamais traité
         * // ✅ Pattern : appeler Drain() systématiquement en fin de frame/update
         * 
         * // 5. Confondre NkGameplayEventBus (gameplay) et EventBus (moteur)
         * // ❌ bus.Emit<NkKeyPressEvent>(...);  // NkKeyPressEvent est pour EventBus (Core)
         * // ✅ bus.Emit<PlayerJumpedEvent>(...);  // Événement gameplay métier
         * @endcode
         */

        /** @} */ // end of NkGameplayEventBus_Examples group

    } // namespace ecs
} // namespace nkentseu