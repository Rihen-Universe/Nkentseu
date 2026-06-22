/**
 * @File NkFunctional.h
 * @Description Fournit des foncteurs utilitaires pour le framework Nkentseu, similaires à STL::functional.
 * @Author TEUGUIA TADJUIDJE Rodolf
 * @Date 2025-06-10
 * @License Rihen
 */
#pragma once

#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    /**
     * @class NkHash
     * @brief Fonction de hachage pour les clés dans les conteneurs non triés, similaire à STL::hash.
     * @tparam T Type de la clé.
     * @note Les utilisateurs doivent spécialiser cette classe pour leurs types personnalisés.
     */
    template<typename T, typename Enable = void>
    struct NkHash {
        constexpr usize operator()([[maybe_unused]] const T& key) const noexcept {
            // sizeof(T) == 0 is always false for any complete type, but is
            // T-dependent so Clang defers evaluation until instantiation.
            static_assert(sizeof(T) == 0, "NkHash must be specialized for type T");
            return 0;
        }
    };

    /**
     * @brief Spécialisation générique de NkHash pour TOUS les types entiers natifs.
     * @note Couvre int/uint 8/16/32/64 sur tous les modèles de données sans trou de
     *       portabilité (LLP64 Windows, LP64 Linux/Android/macOS/HarmonyOS, ILP32 Web).
     *       Indispensable car `nkentseu::uint64` vaut `unsigned long long` partout, alors
     *       que `uint64_t` vaut `unsigned long` en LP64 : des spécialisations par type fixe
     *       (uint64_t) laissaient `NkHash<unsigned long long>` non défini sur Android/Linux/
     *       Harmony. Le mélange dépend de la largeur via `sizeof`. `bool` est exclu.
     */
    template<typename T>
    struct NkHash<T, traits::NkEnableIf_t<traits::NkIsIntegral_v<T> && !traits::NkIsSame_v<T, bool>>> {
        constexpr usize operator()(const T& key) const noexcept {
            if constexpr (sizeof(T) <= 4) {
                const uint32_t k = static_cast<uint32_t>(key);
                return static_cast<usize>(k ^ (k >> 16));
            } else {
                const uint64_t k = static_cast<uint64_t>(key);
                return static_cast<usize>(k ^ (k >> 32));
            }
        }
    };

    /**
     * @brief Spécialisation de NkHash pour float32.
     */
    template<>
    struct NkHash<float32> {
        constexpr usize operator()(const float32& key) const noexcept {
            // Handle NaN and infinities
            if (key != key) return 0; // NaN
            if (key == key * 0.0f) return key < 0.0f ? 1 : 2; // ±0, ±infinity
            union { float32 f; uint32_t i; } u = { key };
            return NkHash<uint32_t>{}(u.i);
        }
    };

    /**
     * @brief Spécialisation de NkHash pour float64.
     */
    template<>
    struct NkHash<float64> {
        constexpr usize operator()(const float64& key) const noexcept {
            // Handle NaN and infinities
            if (key != key) return 0; // NaN
            if (key == key * 0.0) return key < 0.0 ? 1 : 2; // ±0, ±infinity
            union { float64 f; uint64_t i; } u = { key };
            return NkHash<uint64_t>{}(u.i);
        }
    };

    /**
     * @brief Spécialisation de NkHash pour NkString.
     * @note Utilise l'algorithme FNV-1a pour un hachage rapide et bien distribué.
     */
    template<>
    struct NkHash<NkString> {
        usize operator()(const NkString& key) const noexcept {
            constexpr usize fnv_prime = sizeof(usize) == 8 ? 1099511628211ULL : 16777619U;
            constexpr usize fnv_offset = sizeof(usize) == 8 ? 1465739525896755127ULL : 2166136261U;
            
            usize hash = fnv_offset;
            const char* data = key.Data();
            for (usize i = 0; i < key.Length(); ++i) {
                hash ^= static_cast<usize>(data[i]);
                hash *= fnv_prime;
            }
            return hash;
        }
    };

    /**
     * @class NkEqual
     * @brief Prédicat d'égalité pour comparer les clés, similaire à STL::equal_to.
     * @tparam T Type de la clé.
     */
    template<typename T>
    struct NkEqual {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs == rhs;
        }
    };

    /**
     * @class NkLess
     * @brief Comparateur pour un ordre croissant, similaire à STL::less.
     * @tparam T Type de la clé.
     */
    template<typename T>
    struct NkLess {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs < rhs;
        }
    };

    /**
     * @class NkGreater
     * @brief Comparateur pour un ordre décroissant, similaire à STL::greater.
     * @tparam T Type de la clé.
     */
    template<typename T>
    struct NkGreater {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs > rhs;
        }
    };

    /**
     * @class NkLessEqual
     * @brief Comparateur pour un ordre inférieur ou égal, similaire à STL::less_equal.
     * @tparam T Type de la clé.
     */
    template<typename T>
    struct NkLessEqual {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs <= rhs;
        }
    };

    /**
     * @class NkGreaterEqual
     * @brief Comparateur pour un ordre supérieur ou égal, similaire à STL::greater_equal.
     * @tparam T Type de la clé.
     */
    template<typename T>
    struct NkGreaterEqual {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs >= rhs;
        }
    };

    /**
     * @class NkLogicalAnd
     * @brief Opérateur logique ET, similaire à STL::logical_and.
     * @tparam T Type des opérandes.
     */
    template<typename T>
    struct NkLogicalAnd {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs && rhs;
        }
    };

    /**
     * @class NkLogicalOr
     * @brief Opérateur logique OU, similaire à STL::logical_or.
     * @tparam T Type des opérandes.
     */
    template<typename T>
    struct NkLogicalOr {
        constexpr bool operator()(const T& lhs, const T& rhs) const noexcept {
            return lhs || rhs;
        }
    };

    /**
     * @class NkLogicalNot
     * @brief Opérateur logique NON, similaire à STL::logical_not.
     * @tparam T Type de l'opérande.
     */
    template<typename T>
    struct NkLogicalNot {
        constexpr bool operator()(const T& value) const noexcept {
            return !value;
        }
    };

} // namespace nkentseu