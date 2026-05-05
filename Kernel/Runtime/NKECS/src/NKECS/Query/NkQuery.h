// =============================================================================
// Query/NkQuery.h
// =============================================================================
// Système de requêtes typées pour interroger efficacement les entités de l'ECS.
//
// Une requête (`NkQueryResult<Ts...>`) permet de parcourir toutes les entités
// possédant un ensemble donné de composants. Elle s'appuie sur le graphe
// d'archétypes pour ne visiter que les groupes d'entités pertinents.
//
// Caractéristiques principales :
//   - Zéro allocation dynamique pendant l'itération.
//   - Zéro appel virtuel : tout est résolu à la compilation via des templates.
//   - Accès direct aux tableaux contigus (SoA) pour une excellente localité
//     spatiale et des optimisations SIMD.
//   - Filtres additionnels `.With<T>()` et `.Without<T>()` pour affiner la
//     sélection sans modifier la signature de base.
//   - Deux modes d'itération :
//        * `ForEach` : itère entité par entité (pratique pour la logique métier).
//        * `ForEachBatch` : itère archétype par archétype avec accès aux
//          tableaux bruts (idéal pour le traitement vectorisé/SIMD).
// =============================================================================

#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Storage/NkArchetypeGraph.h"
#include <tuple>      // pour std::tuple, std::index_sequence

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // Helpers internes pour manipuler les packs de types (namespace detail)
        // =====================================================================
        namespace detail {

            // Construit un `NkComponentMask` à partir d'une liste de types variadique.
            // Chaque bit actif correspond à l'ID d'un des types `Ts`.
            template<typename... Ts>
            NkComponentMask MakeMask() noexcept {
                NkComponentMask mask{};
                // Fold expression C++17 : itère sur chaque type, supprime const et référence.
                (mask.Set(NkIdOf<std::remove_const_t<Ts>>()), ...);
                return mask;
            }

            // Retourne un pointeur vers le début du tableau brut des composants de type `T`
            // au sein de l'archétype `arch`. Utilisé pour les itérations batch.
            template<typename T>
            T* GetPoolPtr(NkArchetype* arch, uint32 /*row*/) noexcept {
                using TRaw = std::remove_const_t<T>;
                return arch->ComponentData<TRaw>();
            }

        } // namespace detail

        // =====================================================================
        // NkQueryResult<Ts...>
        // =====================================================================
        // Représente le résultat d'une requête sur le monde ECS.
        // Les types `Ts` définissent les composants que l'on souhaite manipuler
        // dans la boucle de traitement.
        //
        // La classe est généralement construite par `NkWorld::Query<Ts...>()`.
        // Elle est non copiable mais déplaçable (retournée par valeur).
        template<typename... Ts>
        class NkQueryResult {
          public:
            // -----------------------------------------------------------------
            // Constructeur (appelé par NkWorld)
            // -----------------------------------------------------------------
            // `required` : masque de base contenant tous les `Ts`.
            // `withFilter` : masque additionnel pour `.With<T>()`.
            // `withoutFilter` : masque pour `.Without<T>()`.
            NkQueryResult(NkArchetypeGraph& graph,
                          NkEntityIndex&    index,
                          NkComponentMask   required,
                          NkComponentMask   withFilter,
                          NkComponentMask   withoutFilter) noexcept
                : mGraph(graph)
                , mIndex(index)
                , mRequired(required)
                , mWithFilter(withFilter)
                , mWithoutFilter(withoutFilter) {
                // Fusionne `required` et `withFilter` dans le masque final d'inclusion.
                for (uint32 w = 0; w < NkComponentMask::kWords; ++w) {
                    mFinalInclude.words[w] = mRequired.words[w] | mWithFilter.words[w];
                }
            }

            // -----------------------------------------------------------------
            // Filtres additionnels (fluent API)
            // -----------------------------------------------------------------

            // Ajoute le composant `T` comme requis pour la requête.
            // Modifie le masque `mFinalInclude` en conséquence.
            template<typename T>
            NkQueryResult& With() noexcept {
                mWithFilter.Set(NkIdOf<std::remove_const_t<T>>());
                // Recalcule le masque final.
                for (uint32 w = 0; w < NkComponentMask::kWords; ++w) {
                    mFinalInclude.words[w] = mRequired.words[w] | mWithFilter.words[w];
                }
                return *this;
            }

            // Exclut les entités possédant le composant `T`.
            template<typename T>
            NkQueryResult& Without() noexcept {
                mWithoutFilter.Set(NkIdOf<std::remove_const_t<T>>());
                return *this;
            }

            // -----------------------------------------------------------------
            // ForEach : itération entité par entité
            // -----------------------------------------------------------------
            // Parcourt toutes les entités correspondant aux critères et appelle
            // `fn` pour chacune.
            //
            // Signature de `fn` : void(NkEntityId, T1&, T2&, ...)
            // Si un composant est qualifié `const T`, il sera passé en `const T&`.
            template<typename Fn>
            void ForEach(Fn&& fn) noexcept {
                mGraph.ForEachMatching(mFinalInclude, mWithoutFilter,
                    [&](NkArchetype* arch) noexcept {
                        const uint32 count = arch->Count();
                        if (count == 0) {
                            return;
                        }

                        // Récupère un tuple contenant les pointeurs vers le début
                        // de chaque pool de composants concernée.
                        auto ptrs = std::make_tuple(
                            detail::GetPoolPtr<Ts>(arch, 0)...
                        );

                        // Pointeur vers le tableau des identifiants d'entités.
                        const NkEntityId* entities = arch->Entities().data;

                        // Boucle sur chaque ligne (entité) de l'archétype.
                        for (uint32 row = 0; row < count; ++row) {
                            // Appelle `fn` avec l'identifiant et les composants à la ligne `row`.
                            CallFn(fn, entities[row], ptrs, row,
                                   std::index_sequence_for<Ts...>{});
                        }
                    });
            }

            // -----------------------------------------------------------------
            // ForEachBatch : itération par archétype (accès aux tableaux bruts)
            // -----------------------------------------------------------------
            // Parcourt les archétypes compatibles et appelle `fn` une fois par
            // archétype avec les pointeurs vers les tableaux de données.
            //
            // Signature de `fn` : void(uint32 count, const NkEntityId* entities,
            //                          T1* comp1, T2* comp2, ...)
            // Idéal pour du traitement vectorisé (SIMD) ou des calculs en masse.
            template<typename Fn>
            void ForEachBatch(Fn&& fn) noexcept {
                mGraph.ForEachMatching(mFinalInclude, mWithoutFilter,
                    [&](NkArchetype* arch) noexcept {
                        const uint32 count = arch->Count();
                        if (count == 0) {
                            return;
                        }
                        CallBatch(fn, arch, count, std::index_sequence_for<Ts...>{});
                    });
            }

            // -----------------------------------------------------------------
            // Utilitaires de statistiques
            // -----------------------------------------------------------------

            // Retourne le nombre total d'entités satisfaisant la requête.
            [[nodiscard]] uint32 Count() const noexcept {
                uint32 total = 0;
                // On doit utiliser `const_cast` car `ForEachMatching` n'est pas const.
                // (Pourrait être amélioré avec une version const de ForEachMatching).
                const_cast<NkArchetypeGraph&>(mGraph).ForEachMatching(
                    mFinalInclude, mWithoutFilter,
                    [&](NkArchetype* arch) noexcept {
                        total += arch->Count();
                    });
                return total;
            }

            // Retourne l'identifiant de la première entité trouvée, ou `Invalid()`.
            [[nodiscard]] NkEntityId First() const noexcept {
                NkEntityId result = NkEntityId::Invalid();
                const_cast<NkArchetypeGraph&>(mGraph).ForEachMatching(
                    mFinalInclude, mWithoutFilter,
                    [&](NkArchetype* arch) noexcept {
                        if (!result.IsValid() && arch->Count() > 0) {
                            result = arch->GetEntity(0);
                        }
                    });
                return result;
            }

            // Vérifie si au moins une entité correspond à la requête.
            [[nodiscard]] bool Any() const noexcept {
                return Count() > 0;
            }

          private:
            // -----------------------------------------------------------------
            // Helper pour appeler `fn` avec les composants déroulés
            // -----------------------------------------------------------------
            template<typename Fn, typename Ptrs, size_t... Is>
            static void CallFn(Fn& fn,
                               NkEntityId id,
                               Ptrs& ptrs,
                               uint32 row,
                               std::index_sequence<Is...>) noexcept {
                // `ptrs` est un tuple de pointeurs (T1*, T2*, ...).
                // On accède à l'élément `row` de chaque tableau et on les passe à `fn`.
                fn(id, std::get<Is>(ptrs)[row]...);
            }

            // -----------------------------------------------------------------
            // Helper pour appeler `fn` en mode batch
            // -----------------------------------------------------------------
            template<typename Fn, size_t... Is>
            static void CallBatch(Fn& fn,
                                  NkArchetype* arch,
                                  uint32 count,
                                  std::index_sequence<Is...>) noexcept {
                using TPack = std::tuple<Ts...>;
                // Passe le nombre d'entités, le tableau d'IDs, et les pointeurs bruts
                // vers les tableaux de chaque type de composant.
                fn(count,
                   arch->Entities().data,
                   detail::GetPoolPtr<std::tuple_element_t<Is, TPack>>(arch, 0)...);
            }

            // -----------------------------------------------------------------
            // Membres privés
            // -----------------------------------------------------------------
            NkArchetypeGraph& mGraph;            // Référence au graphe d'archétypes du monde
            NkEntityIndex&    mIndex;            // Référence à l'index d'entités (non utilisé ici)
            NkComponentMask   mRequired;         // Masque des composants de base (Ts...)
            NkComponentMask   mWithFilter;       // Composants ajoutés via .With<T>()
            NkComponentMask   mWithoutFilter;    // Composants exclus via .Without<T>()
            NkComponentMask   mFinalInclude;     // Masque final d'inclusion (Required ∪ With)
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKQUERY.H
// =============================================================================
//
// Les requêtes sont le moyen principal d'interagir avec les entités dans l'ECS.
// Elles sont généralement créées via `world.Query<T...>()`.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Requête simple avec deux composants
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECS.h"

    struct Position { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
    NK_COMPONENT(Position);
    NK_COMPONENT(Velocity);

    void ExampleSimpleQuery() {
        nkentseu::NkWorld world;

        // Création de quelques entités.
        for (int i = 0; i < 100; ++i) {
            nkentseu::NkEntityId e = world.CreateEntity();
            world.AddComponent<Position>(e, { (float)i, 0.0f, 0.0f });
            if (i % 2 == 0) {
                world.AddComponent<Velocity>(e, { 1.0f, 0.0f, 0.0f });
            }
        }

        // Requête sur les entités ayant Position ET Velocity.
        world.Query<Position, Velocity>().ForEach(
            [](nkentseu::NkEntityId id, Position& pos, Velocity& vel) {
                // Mise à jour de la position en fonction de la vélocité.
                pos.x += vel.vx;
                pos.y += vel.vy;
                pos.z += vel.vz;
            });

        // Nombre d'entités traitées.
        uint32 count = world.Query<Position, Velocity>().Count();
        printf("Updated %u entities\n", count);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation de With() et Without()
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECS.h"

    struct Health { float value; };
    struct Armor  { float value; };
    struct Dead   {}; // tag
    NK_COMPONENT(Health);
    NK_COMPONENT(Armor);
    NK_COMPONENT(Dead);

    void ExampleWithWithout() {
        nkentseu::NkWorld world;

        // Création d'entités avec différentes combinaisons...
        auto e1 = world.CreateEntity();
        world.AddComponent<Health>(e1, {100});
        world.AddComponent<Armor>(e1,  {50});

        auto e2 = world.CreateEntity();
        world.AddComponent<Health>(e2, {20});
        world.AddComponent<Dead>(e2);

        auto e3 = world.CreateEntity();
        world.AddComponent<Health>(e3, {80});
        world.AddComponent<Armor>(e3,  {30});
        world.AddComponent<Dead>(e3);

        // Requête : toutes les entités avec Health, mais SANS Dead,
        // et on ajoute Armor comme requis supplémentaire via With.
        world.Query<Health>()
            .With<Armor>()
            .Without<Dead>()
            .ForEach([](nkentseu::NkEntityId id, Health& health) {
                // Seule l'entité e1 sera traitée (Health + Armor, pas Dead).
                printf("Entity %u has %f health\n", id.index, health.value);
            });

        // On peut aussi enchaîner plusieurs With/Without.
        world.Query<Health>()
            .Without<Dead>()
            .Without<Armor>()
            .ForEach([](nkentseu::NkEntityId id, Health& health) {
                // Aucune entité ne correspond (e2 a Dead, e1 a Armor).
            });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Accès en lecture seule (const)
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECS.h"

    struct Transform { float x, y, z; };
    NK_COMPONENT(Transform);

    void ExampleConstAccess() {
        nkentseu::NkWorld world;
        // ... remplissage ...

        // Requête avec des composants const : garantit qu'on ne modifie pas.
        world.Query<const Transform>().ForEach(
            [](nkentseu::NkEntityId id, const Transform& t) {
                // Lecture seule autorisée.
                printf("Entity %u at (%f, %f, %f)\n", id.index, t.x, t.y, t.z);
                // t.x = 10.0f; // Erreur de compilation.
            });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : ForEachBatch pour traitement vectorisé
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECS.h"
    #include <xmmintrin.h> // SSE intrinsics (exemple)

    struct Position { float x, y, z, w; }; // aligné pour SIMD
    NK_COMPONENT(Position);

    void ExampleBatchSIMD() {
        nkentseu::NkWorld world;
        // ... peuplement ...

        world.Query<Position>().ForEachBatch(
            [](uint32 count, const nkentseu::NkEntityId* ids, Position* pos) {
                // Traitement par blocs de 4 (SSE).
                uint32 i = 0;
                for (; i + 3 < count; i += 4) {
                    __m128 x = _mm_loadu_ps(&pos[i].x);
                    __m128 y = _mm_loadu_ps(&pos[i+1].x);
                    __m128 z = _mm_loadu_ps(&pos[i+2].x);
                    __m128 w = _mm_loadu_ps(&pos[i+3].x);
                    // ... opérations SIMD ...
                }
                // Traitement du reste.
                for (; i < count; ++i) {
                    pos[i].x += 1.0f;
                }
            });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation de First() et Any()
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECS.h"

    struct Player {};
    NK_COMPONENT(Player);

    void ExampleFirstAny() {
        nkentseu::NkWorld world;
        // ... création d'entités ...

        // Vérifie s'il existe au moins un joueur.
        if (world.Query<Player>().Any()) {
            // Récupère l'identifiant du premier joueur trouvé.
            nkentseu::NkEntityId playerId = world.Query<Player>().First();
            if (playerId.IsValid()) {
                // Faire quelque chose avec le joueur.
                printf("Found player entity: %u\n", playerId.index);
            }
        }
    }
*/

// =============================================================================