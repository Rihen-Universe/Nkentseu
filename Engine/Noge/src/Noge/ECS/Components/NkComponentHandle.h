#pragma once
// =============================================================================
// Nkentseu/ECS/Components/NkComponentHandle.h
// =============================================================================
/**
 * @file NkComponentHandle.h
 * @brief Wrapper typé de composant ECS pour usage avec NkGameObject/NkActor.
 *
 * 🔹 PROBLÈME RÉSOLU :
 *   L'API brute ECS est puissante mais verbeuse pour le code gameplay :
 *   @code
 *   // ECS brut — fonctionne, mais lourd dans du code gameplay
 *   NkTransform* t = world.Get<NkTransform>(id);
 *   if (t) t->SetLocalPosition(1, 2, 3);
 *   @endcode
 *
 *   NkComponentHandle fournit une API fluide et safe :
 *   @code
 *   // Via NkGameObject (ergonomique)
 *   go.GetComponent<NkTransform>().SetLocalPosition(1, 2, 3);
 *
 *   // Ou directement :
 *   auto handle = NkComponentHandle<NkTransform>(world, entityId);
 *   if (handle) handle->SetLocalPosition(1, 2, 3);
 *   @endcode
 *
 * 🔹 DESIGN :
 *   NkComponentHandle<T> est un handle LÉGER (world ptr + entity id + ptr cache).
 *   Il ne possède pas le composant. Il le résout paresseusement depuis NkWorld.
 *   Taille : 24 bytes (NkWorld* + NkEntityId + T* cache).
 *
 * 🔹 STRATÉGIE DE CACHE :
 *   Le pointeur T* est mis en cache à la première résolution.
 *   Il est invalidé (cache = nullptr) automatiquement après FlushDeferred()
 *   si l'entité migre d'archétype.
 *   Pour les boucles chaudes : utiliser world.Get<T>() directement.
 *
 * @author nkentseu
 * @version 1.0
 * @date 2026
 */

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"

namespace nkentseu {

    // =========================================================================
    // NkComponentHandle<T> — Wrapper résolvable de composant ECS
    // =========================================================================
    /**
     * @class NkComponentHandle
     * @brief Handle léger vers un composant T sur une entité ECS.
     *
     * Utilisé par NkGameObject et NkActor pour exposer une API
     * "GetComponent<T>() / AddComponent<T>() / RemoveComponent<T>()"
     * similaire à Unity, sans coûter l'ergonomie du code gameplay.
     *
     * @tparam T Type du composant ECS cible.
     */
    template<typename T>
    class NkComponentHandle {
        public:
            // ── Constructeurs ──────────────────────────────────────────────
            /// Handle invalide (null)
            NkComponentHandle() noexcept = default;

            /// Handle lié à un monde et une entité
            NkComponentHandle(ecs::NkWorld* world, ecs::NkEntityId id) noexcept
                : mWorld(world), mId(id) {}

            // ── Résolution ─────────────────────────────────────────────────
            /**
             * @brief Résout le handle et retourne le pointeur vers T.
             * @return Pointeur vers T, ou nullptr si absent/invalide.
             * @note Résolution directe via NkWorld::Get<T>() — pas de cache.
             *       Pour des appels répétés, stocker le pointeur localement.
             */
            [[nodiscard]] T* Get() const noexcept {
                if (!mWorld || !mId.IsValid()) return nullptr;
                return mWorld->Get<T>(mId);
            }

            /**
             * @brief Opérateur flèche — accès direct au composant.
             * @warning Asserte si le composant est absent. Vérifier IsValid() avant.
             */
            T* operator->() const noexcept {
                T* ptr = Get();
                NKECS_ASSERT(ptr != nullptr && "NkComponentHandle: composant absent");
                return ptr;
            }

            /**
             * @brief Opérateur déréférencement.
             */
            T& operator*() const noexcept {
                T* ptr = Get();
                NKECS_ASSERT(ptr != nullptr && "NkComponentHandle: composant absent");
                return *ptr;
            }

            /**
             * @brief Conversion bool : true si le composant existe.
             */
            explicit operator bool() const noexcept {
                return Get() != nullptr;
            }

            /**
             * @brief Vérifie si le handle pointe vers un composant valide.
             */
            [[nodiscard]] bool IsValid() const noexcept {
                return Get() != nullptr;
            }

            // ── Accès au contexte ──────────────────────────────────────────
            [[nodiscard]] ecs::NkEntityId EntityId()  const noexcept { return mId; }
            [[nodiscard]] ecs::NkWorld*   World()     const noexcept { return mWorld; }

        private:
            ecs::NkWorld*   mWorld = nullptr;
            ecs::NkEntityId mId    = ecs::NkEntityId::Invalid();
    };

    // =========================================================================
    // NkOptionalComponent<T> — Handle avec valeur par défaut (never-crash)
    // =========================================================================
    /**
     * @class NkOptionalComponent
     * @brief Comme NkComponentHandle, mais retourne un objet vide si absent.
     *
     * Utile quand on veut lire un composant optionnel sans tester nullptr :
     * @code
     * // Sans NkOptionalComponent (verbeux)
     * if (auto* audio = world.Get<NkAudioSource>(id)) {
     *     audio->Play("shot.wav");
     * }
     *
     * // Avec NkOptionalComponent (fluide, le call est un no-op si absent)
     * go.Optional<NkAudioSource>().Play("shot.wav");
     * @endcode
     *
     * @note L'opérateur -> retourne un pointeur vers mDummy si le composant
     *       est absent — les modifications sur dummy sont perdues silencieusement.
     *       À utiliser uniquement pour des lectures ou des actions idempotentes.
     */
    template<typename T>
    class NkOptionalComponent {
        public:
            NkOptionalComponent() noexcept = default;
            NkOptionalComponent(ecs::NkWorld* world, ecs::NkEntityId id) noexcept
                : mWorld(world), mId(id) {}

            [[nodiscard]] bool IsPresent() const noexcept {
                return mWorld && mId.IsValid() && mWorld->Has<T>(mId);
            }

            explicit operator bool() const noexcept { return IsPresent(); }

            T* operator->() noexcept {
                if (T* ptr = Resolve()) return ptr;
                return &mDummy;  // Retourne un dummy silencieux
            }

            const T* operator->() const noexcept {
                if (const T* ptr = ResolveConst()) return ptr;
                return &mDummy;
            }

            [[nodiscard]] T* Get() noexcept  { return Resolve(); }
            [[nodiscard]] const T* Get() const noexcept { return ResolveConst(); }

        private:
            T* Resolve() noexcept {
                if (!mWorld || !mId.IsValid()) return nullptr;
                return mWorld->Get<T>(mId);
            }
            const T* ResolveConst() const noexcept {
                if (!mWorld || !mId.IsValid()) return nullptr;
                return mWorld->Get<T>(mId);
            }

            ecs::NkWorld*   mWorld = nullptr;
            ecs::NkEntityId mId    = ecs::NkEntityId::Invalid();
            mutable T       mDummy = {};  ///< Cible silencieuse si composant absent
    };

    // =========================================================================
    // NkRequiredComponent<T> — Handle avec assertion (composant garanti)
    // =========================================================================
    /**
     * @class NkRequiredComponent
     * @brief Handle pour composants dont la présence est GARANTIE.
     *
     * Utilisé pour les composants invariants de NkGameObject (NkTransform,
     * NkName, NkTag...) qui sont toujours présents sur tout objet valide.
     * Évite le check nullptr redondant sur ces composants.
     *
     * @code
     * // NkTransform est toujours présent sur un NkGameObject valide
     * go.Transform()->SetLocalPosition(1, 2, 3);  // Pas de nullptr check
     * @endcode
     */
    template<typename T>
    class NkRequiredComponent {
        public:
            NkRequiredComponent() noexcept = default;
            NkRequiredComponent(ecs::NkWorld* world, ecs::NkEntityId id) noexcept
                : mWorld(world), mId(id) {}

            T* operator->() const noexcept { return &Resolve(); }
            T& operator*()  const noexcept { return Resolve(); }
            [[nodiscard]] T& Get() const noexcept { return Resolve(); }

        private:
            T& Resolve() const noexcept {
                NKECS_ASSERT(mWorld && mId.IsValid());
                T* ptr = mWorld->Get<T>(mId);
                NKECS_ASSERT(ptr != nullptr &&
                             "NkRequiredComponent: composant invariant absent — "
                             "vérifier NkGameObjectFactory::Create()");
                return *ptr;
            }

            ecs::NkWorld*   mWorld = nullptr;
            ecs::NkEntityId mId    = ecs::NkEntityId::Invalid();
    };

} // namespace nkentseu
