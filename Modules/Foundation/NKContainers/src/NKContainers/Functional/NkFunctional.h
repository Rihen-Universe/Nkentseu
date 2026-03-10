/**
 * @File NkFunctional.h
 * @Description Fournit des foncteurs utilitaires pour le framework Nkentseu, similaires à std::functional.
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
     * @brief Fonction de hachage pour les clés dans les conteneurs non triés, similaire à std::hash.
     * @tparam T Type de la clé.
     * @note Les utilisateurs doivent spécialiser cette classe pour leurs types personnalisés.
     */
    template<typename T>
    struct NkHash {
        constexpr usize operator()([[maybe_unused]] const T& key) const noexcept {
            // sizeof(T) == 0 is always false for any complete type, but is
            // T-dependent so Clang defers evaluation until instantiation.
            static_assert(sizeof(T) == 0, "NkHash must be specialized for type T");
            return 0;
        }
    };

    /**
     * @brief Spécialisation de NkHash pour int32_t.
     */
    template<>
    struct NkHash<int32_t> {
        constexpr usize operator()(const int32_t& key) const noexcept {
            return static_cast<usize>(key ^ (key >> 16));
        }
    };

    /**
     * @brief Spécialisation de NkHash pour uint32_t.
     */
    template<>
    struct NkHash<uint32_t> {
        constexpr usize operator()(const uint32_t& key) const noexcept {
            return static_cast<usize>(key ^ (key >> 16));
        }
    };

    /**
     * @brief Spécialisation de NkHash pour int64_t.
     */
    template<>
    struct NkHash<int64_t> {
        constexpr usize operator()(const int64_t& key) const noexcept {
            return static_cast<usize>(key ^ (key >> 32));
        }
    };

    /**
     * @brief Spécialisation de NkHash pour uint64_t.
     */
    template<>
    struct NkHash<uint64_t> {
        constexpr usize operator()(const uint64_t& key) const noexcept {
            return static_cast<usize>(key ^ (key >> 32));
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
     * @brief Prédicat d'égalité pour comparer les clés, similaire à std::equal_to.
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
     * @brief Comparateur pour un ordre croissant, similaire à std::less.
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
     * @brief Comparateur pour un ordre décroissant, similaire à std::greater.
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
     * @brief Comparateur pour un ordre inférieur ou égal, similaire à std::less_equal.
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
     * @brief Comparateur pour un ordre supérieur ou égal, similaire à std::greater_equal.
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
     * @brief Opérateur logique ET, similaire à std::logical_and.
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
     * @brief Opérateur logique OU, similaire à std::logical_or.
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
     * @brief Opérateur logique NON, similaire à std::logical_not.
     * @tparam T Type de l'opérande.
     */
    template<typename T>
    struct NkLogicalNot {
        constexpr bool operator()(const T& value) const noexcept {
            return !value;
        }
    };

} // namespace nkentseu