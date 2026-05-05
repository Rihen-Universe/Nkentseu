// =============================================================================
// NkECSDefines.h
// Types fondamentaux, macros, constantes partagées dans tout le module NkECS.
// =============================================================================
#pragma once

// =============================================================================
// 1. En-têtes système et dépendances internes
// =============================================================================
#include <cstdint>               // Pour les types entiers à largeur fixe
#include <cstddef>               // Pour std::size_t
#include <type_traits>           // Pour les traits de type (utilitaires templates)
#include <utility>               // Pour std::forward, std::move, etc.
#include <limits>                // Pour les valeurs limites des types numériques

#include "NKCore/NkTypes.h"      // Définitions communes de types du moteur (uint8, float32, etc.)
#include "NKLogger/NkLog.h"      // Système de logging (utilisé pour les assertions)

namespace nkentseu {
    namespace ecs {

        // =============================================================================
        // 2. Alias de types numériques (cohérence avec les conventions du moteur)
        // =============================================================================
        using uint8   = nkentseu::uint8;    // Entier non signé 8 bits
        using uint16  = nkentseu::uint16;   // Entier non signé 16 bits
        using uint32  = nkentseu::uint32;   // Entier non signé 32 bits
        using uint64  = nkentseu::uint64;   // Entier non signé 64 bits

        using int8    = nkentseu::int8;     // Entier signé 8 bits
        using int16   = nkentseu::int16;    // Entier signé 16 bits
        using int32   = nkentseu::int32;    // Entier signé 32 bits
        using int64   = nkentseu::int64;    // Entier signé 64 bits

        using usize   = nkentseu::usize;    // Taille non signée (équivalent std::size_t)

        using float32 = nkentseu::float32;  // Flottant simple précision (32 bits)
        using float64 = nkentseu::float64;  // Flottant double précision (64 bits)

        // =============================================================================
        // 3. NkEntityId — Identifiant fort d'entité avec protection contre la réutilisation
        // =============================================================================
        // Combine un index (position dans un tableau interne) et une génération.
        // Lorsqu'une entité est détruite, sa génération est incrémentée ; un ancien
        // identifiant devient automatiquement invalide (dangling entity prevention).
        struct NkEntityId
        {
            // --- Constantes de valeurs invalides ---
            static constexpr uint32 kInvalidIndex = 0xFFFFFFFFu;  // Index réservé pour "aucune entité"
            static constexpr uint32 kInvalidGen   = 0u;           // Génération invalide

            // --- Membres ---
            uint32 index = kInvalidIndex;  // Index dans le tableau d'entités
            uint32 gen   = kInvalidGen;    // Numéro de génération (incrémenté à chaque destruction)

            // --- Vérification de validité ---
            // Retourne vrai si l'identifiant correspond à une entité vivante.
            [[nodiscard]] constexpr bool IsValid() const noexcept
            {
                return index != kInvalidIndex && gen != kInvalidGen;
            }

            // --- Sérialisation / désérialisation sur 64 bits ---
            // Empaquette les 32 bits de gen et les 32 bits d'index dans un uint64.
            [[nodiscard]] constexpr uint64 Pack() const noexcept
            {
                return (static_cast<uint64>(gen) << 32u) | static_cast<uint64>(index);
            }

            // Reconstruit un NkEntityId à partir d'une valeur 64 bits.
            [[nodiscard]] static constexpr NkEntityId Unpack(uint64 v) noexcept
            {
                return {
                    static_cast<uint32>(v & 0xFFFFFFFFu),  // Partie basse : index
                    static_cast<uint32>(v >> 32u)          // Partie haute : génération
                };
            }

            // --- Constructeur d'identifiant invalide ---
            [[nodiscard]] static constexpr NkEntityId Invalid() noexcept
            {
                return { kInvalidIndex, kInvalidGen };
            }

            // --- Opérateurs de comparaison ---
            constexpr bool operator==(NkEntityId o) const noexcept
            {
                return index == o.index && gen == o.gen;
            }

            constexpr bool operator!=(NkEntityId o) const noexcept
            {
                return !(*this == o);
            }

            // Permet d'utiliser NkEntityId comme clé dans un conteneur ordonné.
            constexpr bool operator<(NkEntityId o) const noexcept
            {
                return Pack() < o.Pack();
            }
        };

        // =============================================================================
        // 4. Identifiants pour les composants et les archétypes
        // =============================================================================
        // NkComponentId : identifiant unique d'un type de composant.
        // Assigné statiquement via NkComponentTraits<T>::Id().
        using NkComponentId = uint32;

        // Valeur réservée pour "composant inexistant".
        static constexpr NkComponentId kInvalidComponentId = 0xFFFFFFFFu;

        // NkArchetypeId : identifiant d'un archétype (combinaison unique de ComponentIds).
        using NkArchetypeId = uint32;

        // Valeur réservée pour "archétype invalide".
        static constexpr NkArchetypeId kInvalidArchetypeId = 0xFFFFFFFFu;

        // =============================================================================
        // 5. Constantes globales de dimensionnement de l'ECS
        // =============================================================================
        // Nombre maximal de types de composants différents supportés.
        static constexpr uint32 kMaxComponentTypes = 256u;

        // Nombre maximal d'archétypes pouvant coexister.
        static constexpr uint32 kMaxArchetypes = 4096u;

        // Nombre maximal d'entités vivantes simultanément (~1 million).
        static constexpr uint32 kMaxEntities = (1u << 20u);

        // Taille d'un chunk de mémoire (16 Ko), utilisé pour le stockage dense.
        static constexpr uint32 kChunkSize = 16u * 1024u;

        // Nombre maximal de systèmes dans un groupe d'exécution.
        static constexpr uint32 kMaxSystemsPerGroup = 128u;

        // =============================================================================
        // 6. Macros utilitaires (attributs et optimisations)
        // =============================================================================
        // Indique que la valeur de retour ne doit pas être ignorée.
        #define NKECS_NODISCARD   [[nodiscard]]

        // Suggère l'inlining au compilateur.
        #define NKECS_INLINE      inline

        // Force l'inlining (spécifique MSVC, GCC, Clang).
        #define NKECS_FORCEINLINE __forceinline

        // Macro pour marquer un chemin de code inatteignable.
        // Aide le compilateur à optimiser et à éviter des warnings.
        #if defined(_MSC_VER)
        #  define NKECS_UNREACHABLE() __assume(false)           // MSVC
        #elif defined(__GNUC__) || defined(__clang__)
        #  define NKECS_UNREACHABLE() __builtin_unreachable()   // GCC / Clang
        #else
        #  define NKECS_UNREACHABLE() do {} while(0)            // Fallback (aucun effet)
        #endif

        // Macro d'assertion utilisée en développement.
        // En cas d'échec, appelle la fonction interne AssertFail.
        #define NKECS_ASSERT(cond) \
            do { \
                if (!(cond)) { \
                    ::nkentseu::ecs::detail::AssertFail(#cond, __FILE__, __LINE__); \
                } \
            } while(0)

        // =============================================================================
        // 7. Détail : Gestion des assertions (header-only, logging via NkLogger)
        // =============================================================================
        namespace detail
        {
            // Fonction appelée lors d'un échec d'assertion.
            // En production, elle pourrait déclencher un crash handler.
            inline void AssertFail(const char* cond, const char* file, int line) noexcept
            {
                // Évite les warnings "unused parameter" en production.
                (void)cond;
                (void)file;
                (void)line;

                // Log l'erreur via le logger global du moteur.
                // Si le logger n'est pas disponible, on pourrait appeler std::abort().
                logger.Error("Assertion failed: {0} at {1}:{2}", cond, file, line);

                // Optionnel : provoquer un arrêt brutal.
                // std::abort();
            }
        } // namespace detail

        // =============================================================================
        // 8. Utilitaires de hachage (FNV-1a 64 bits)
        // =============================================================================
        // Le hachage FNV-1a est utilisé pour générer des identifiants uniques à partir
        // de chaînes de caractères (par exemple, pour les noms de composants).
        namespace detail
        {
            // Constantes de l'algorithme FNV-1a 64 bits.
            constexpr uint64 kFNVBasis = 14695981039346656037ULL;  // Valeur initiale du hash
            constexpr uint64 kFNVPrime = 1099511628211ULL;         // Multiplicateur

            // Hachage d'une chaîne de caractères terminée par '\0'.
            constexpr uint64 FNV1a(const char* s, uint64 h = kFNVBasis) noexcept
            {
                // Fin de chaîne : retourne le hash courant.
                return (!s || !*s) ? h : FNV1a(s + 1, (h ^ static_cast<uint64>(*s)) * kFNVPrime);
            }

            // Hachage d'un bloc mémoire quelconque.
            constexpr uint64 FNV1aBytes(const uint8* data, usize len, uint64 h = kFNVBasis) noexcept
            {
                for (usize i = 0; i < len; ++i)
                {
                    h = (h ^ static_cast<uint64>(data[i])) * kFNVPrime;
                }
                return h;
            }
        } // namespace detail

        // =============================================================================
        // 9. NkSpan — Vue légère sur un tableau contigu (compatibilité C++20)
        // =============================================================================
        // Implémentation minimaliste de std::span, utilisée en interne pour passer des
        // tableaux sans copie. Évite la dépendance à C++20.
        template<typename T>
        struct NkSpan
        {
            // Pointeur vers le premier élément.
            T*    data = nullptr;

            // Nombre d'éléments dans la vue.
            usize size = 0;

            // Constructeur par défaut (vide).
            constexpr NkSpan() noexcept = default;

            // Constructeur à partir d'un pointeur et d'une taille.
            constexpr NkSpan(T* d, usize s) noexcept
                : data(d)
                , size(s)
            {}

            // Constructeur à partir d'un tableau C-style (déduction de la taille).
            template<usize N>
            constexpr NkSpan(T (&arr)[N]) noexcept
                : data(arr)
                , size(N)
            {}

            // Itérateurs (compatibles avec les boucles for-range).
            constexpr T* begin() const noexcept { return data; }
            constexpr T* end()   const noexcept { return data + size; }

            // Accès à l'élément i (sans vérification de bornes).
            constexpr T& operator[](usize i) const noexcept { return data[i]; }

            // Teste si la vue est vide.
            constexpr bool empty() const noexcept { return size == 0; }
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKECSDEFINES.H
// =============================================================================
//
// Ce fichier définit les briques de base de l'ECS. Il n'est généralement pas
// inclus directement (il l'est via NKECS.h), mais il est utile de comprendre
// comment manipuler ses types.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Manipulation de NkEntityId
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECSDefines.h"

    void ExampleEntityId() {
        using namespace nkentseu::ecs;

        // Création d'un identifiant valide (index 42, génération 1).
        NkEntityId id1{42, 1};

        // Test de validité.
        if (id1.IsValid()) {
            // Empaquetage dans un uint64 (pratique pour le réseau/sérialisation).
            uint64 packed = id1.Pack();

            // Reconstruction depuis la valeur empaquetée.
            NkEntityId id2 = NkEntityId::Unpack(packed);

            // Comparaison.
            assert(id1 == id2);
        }

        // Identifiant invalide.
        NkEntityId invalid = NkEntityId::Invalid();
        assert(!invalid.IsValid());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation de NkSpan
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECSDefines.h"

    void ExampleSpan() {
        using namespace nkentseu::ecs;

        int arr[] = {1, 2, 3, 4, 5};

        // Construction d'un span sur un tableau C.
        NkSpan<int> span(arr);

        // Parcours avec une boucle for-range.
        for (int& value : span) {
            value *= 2;
        }

        // Accès par index.
        span[0] = 10;

        // Test de vacuité.
        assert(!span.empty());
        assert(span.size == 5);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Assertions et macros
// -----------------------------------------------------------------------------
/*
    #include "NkECS/NkECSDefines.h"

    void ExampleAssertions() {
        using namespace nkentseu::ecs;

        int* ptr = nullptr;

        // L'assertion échouera et loguera l'erreur via NkLogger.
        NKECS_ASSERT(ptr != nullptr);

        // Utilisation de NKECS_UNREACHABLE() après un switch exhaustif.
        enum Color { Red, Green, Blue };
        Color c = Red;
        switch (c) {
            case Red:   break;
            case Green: break;
            case Blue:  break;
            default:    NKECS_UNREACHABLE(); // Ne devrait jamais arriver
        }
    }
*/

// =============================================================================