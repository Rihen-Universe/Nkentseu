// =============================================================================
// NKCore/NkLimits.h
// Limites numériques portables sans dépendance à <limits>.
//
// Design :
//  - Template NkNumericLimits<T> inspiré de std::numeric_limits
//  - Spécialisations pour types entiers signés/non-signés (8/16/32/64 bits)
//  - Spécialisations pour flottants (float32/float64) avec valeurs IEEE 754
//  - Constantes constexpr pour évaluation compile-time
//  - Accès aux valeurs spéciales : min, max, epsilon, infinity, NaN
//  - Métadonnées : digits, radix, exponent ranges pour calculs génériques
//  - Intégration avec NkTypes.h pour les alias de types nk_*
//
// Intégration :
//  - Utilise NKCore/NkTypes.h pour les définitions de types fondamentaux
//  - Utilise NKCore/NkExport.h pour les macros d'export si nécessaire
//  - Compatible C++11+ avec support constexpr complet
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKLIMITS_H_INCLUDED
#define NKENTSEU_CORE_NKLIMITS_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de base pour les types et exports.

    #include "NkTypes.h"
    #include "NkExport.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : TEMPLATE GÉNÉRIQUE NKNUMERICLIMITS
        // ====================================================================
        // Définition du template de base avec spécialisations partielles.

        /**
         * @defgroup NumericLimitsTemplates Templates de Limites Numériques
         * @brief Template générique et spécialisations pour limites des types
         * @ingroup CoreUtilities
         *
         * Ce système fournit un accès portable aux propriétés et limites
         * des types numériques, sans dépendre de la bibliothèque standard.
         * Inspiré de std::numeric_limits mais adapté aux types nk_* du framework.
         *
         * @note
         *   - is_specialized = false pour le template générique (non spécialisé)
         *   - is_specialized = true pour les spécialisations supportées
         *   - Toutes les méthodes constexpr sont évaluables à la compilation
         *   - Les flottants ont des méthodes non-constexpr pour infinity/NaN (bit-level)
         *
         * @example
         * @code
         * // Utilisation générique avec template
         * template <typename T>
         * T ClampToRange(T value) {
         *     static_assert(nkentseu::NkNumericLimits<T>::is_specialized,
         *                   "Type not supported by NkNumericLimits");
         *     return nkentseu::NkClamp(value,
         *         nkentseu::NkNumericLimits<T>::min(),
         *         nkentseu::NkNumericLimits<T>::max());
         * }
         *
         * // Accès direct aux limites
         * auto minInt32 = nkentseu::NkNumericLimits<nk_int32>::min();  // -2147483648
         * auto maxUint64 = nkentseu::NkNumericLimits<nk_uint64>::max(); // 18446744073709551615
         * auto floatEpsilon = nkentseu::NkNumericLimits<nk_float32>::epsilon(); // ~1.19e-7
         * @endcode
         */
        /** @{ */

        /**
         * @brief Template générique pour limites numériques (non spécialisé par défaut)
         * @tparam T Type numérique pour lequel obtenir les limites
         * @ingroup NumericLimitsTemplates
         *
         * Ce template de base indique que le type T n'est pas supporté
         * (is_specialized = false). Les types supportés ont des spécialisations
         * explicites définies plus bas dans ce fichier.
         *
         * @note
         *   Utiliser static_assert avec is_specialized pour vérifier le support
         *   d'un type dans du code générique.
         */
        template <typename T>
        struct NkNumericLimits {
            /** @brief Indique si ce type a une spécialisation définie */
            static constexpr nk_bool is_specialized = false;
        };

        /** @} */ // End of NumericLimitsTemplates

        // ====================================================================
        // SECTION 4 : SPÉCIALISATIONS POUR TYPES ENTIERS 8 BITS
        // ====================================================================
        // Limites et propriétés pour nk_int8 et nk_uint8.

        /**
         * @brief Spécialisation pour nk_int8 (entier signé 8 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [-128, 127]
         *   - digits = 7 (bits de valeur, hors bit de signe)
         *   - digits10 = 2 (chiffres décimaux garantis sans perte)
         */
        template <>
        struct NkNumericLimits<nk_int8> {
            /** @brief Ce type est spécialisé */
            static constexpr nk_bool is_specialized = true;
            /** @brief Type signé */
            static constexpr nk_bool is_signed = true;
            /** @brief Type entier (non flottant) */
            static constexpr nk_bool is_integer = true;

            /**
             * @brief Valeur minimale représentable
             * @return NKENTSEU_INT8_MIN (-128)
             */
            static constexpr nk_int8 min() noexcept {
                return NKENTSEU_INT8_MIN;
            }

            /**
             * @brief Valeur maximale représentable
             * @return NKENTSEU_INT8_MAX (127)
             */
            static constexpr nk_int8 max() noexcept {
                return NKENTSEU_INT8_MAX;
            }

            /**
             * @brief Plus petite valeur (identique à min() pour les entiers signés)
             * @return Même valeur que min()
             */
            static constexpr nk_int8 lowest() noexcept {
                return min();
            }

            /** @brief Nombre de bits non-signes dans la représentation (7 pour int8) */
            static constexpr nk_int32 digits = 7;
            /** @brief Nombre de chiffres décimaux représentables sans perte */
            static constexpr nk_int32 digits10 = 2;
        };

        /**
         * @brief Spécialisation pour nk_uint8 (entier non-signé 8 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [0, 255]
         *   - digits = 8 (tous les bits sont de valeur)
         *   - digits10 = 2 (255 tient sur 3 chiffres mais 2 sont garantis)
         */
        template <>
        struct NkNumericLimits<nk_uint8> {
            /** @brief Ce type est spécialisé */
            static constexpr nk_bool is_specialized = true;
            /** @brief Type non-signé */
            static constexpr nk_bool is_signed = false;
            /** @brief Type entier (non flottant) */
            static constexpr nk_bool is_integer = true;

            /**
             * @brief Valeur minimale représentable
             * @return 0
             */
            static constexpr nk_uint8 min() noexcept {
                return 0;
            }

            /**
             * @brief Valeur maximale représentable
             * @return NKENTSEU_UINT8_MAX (255)
             */
            static constexpr nk_uint8 max() noexcept {
                return NKENTSEU_UINT8_MAX;
            }

            /**
             * @brief Plus petite valeur (identique à min() pour les non-signés)
             * @return Même valeur que min()
             */
            static constexpr nk_uint8 lowest() noexcept {
                return min();
            }

            /** @brief Nombre de bits de valeur (8 pour uint8) */
            static constexpr nk_int32 digits = 8;
            /** @brief Nombre de chiffres décimaux représentables sans perte */
            static constexpr nk_int32 digits10 = 2;
        };

        // ====================================================================
        // SECTION 5 : SPÉCIALISATIONS POUR TYPES ENTIERS 16 BITS
        // ====================================================================
        // Limites et propriétés pour nk_int16 et nk_uint16.

        /**
         * @brief Spécialisation pour nk_int16 (entier signé 16 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [-32768, 32767]
         *   - digits = 15, digits10 = 4
         */
        template <>
        struct NkNumericLimits<nk_int16> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = true;
            static constexpr nk_bool is_integer = true;

            static constexpr nk_int16 min() noexcept {
                return NKENTSEU_INT16_MIN;
            }

            static constexpr nk_int16 max() noexcept {
                return NKENTSEU_INT16_MAX;
            }

            static constexpr nk_int16 lowest() noexcept {
                return min();
            }

            static constexpr nk_int32 digits = 15;
            static constexpr nk_int32 digits10 = 4;
        };

        /**
         * @brief Spécialisation pour nk_uint16 (entier non-signé 16 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [0, 65535]
         *   - digits = 16, digits10 = 4
         */
        template <>
        struct NkNumericLimits<nk_uint16> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = false;
            static constexpr nk_bool is_integer = true;

            static constexpr nk_uint16 min() noexcept {
                return 0;
            }

            static constexpr nk_uint16 max() noexcept {
                return NKENTSEU_UINT16_MAX;
            }

            static constexpr nk_uint16 lowest() noexcept {
                return min();
            }

            static constexpr nk_int32 digits = 16;
            static constexpr nk_int32 digits10 = 4;
        };

        // ====================================================================
        // SECTION 6 : SPÉCIALISATIONS POUR TYPES ENTIERS 32 BITS
        // ====================================================================
        // Limites et propriétés pour nk_int32 et nk_uint32.

        /**
         * @brief Spécialisation pour nk_int32 (entier signé 32 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [-2147483648, 2147483647]
         *   - digits = 31, digits10 = 9
         */
        template <>
        struct NkNumericLimits<nk_int32> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = true;
            static constexpr nk_bool is_integer = true;

            static constexpr nk_int32 min() noexcept {
                return NKENTSEU_INT32_MIN;
            }

            static constexpr nk_int32 max() noexcept {
                return NKENTSEU_INT32_MAX;
            }

            static constexpr nk_int32 lowest() noexcept {
                return min();
            }

            static constexpr nk_int32 digits = 31;
            static constexpr nk_int32 digits10 = 9;
        };

        /**
         * @brief Spécialisation pour nk_uint32 (entier non-signé 32 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [0, 4294967295]
         *   - digits = 32, digits10 = 9
         */
        template <>
        struct NkNumericLimits<nk_uint32> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = false;
            static constexpr nk_bool is_integer = true;

            static constexpr nk_uint32 min() noexcept {
                return 0;
            }

            static constexpr nk_uint32 max() noexcept {
                return NKENTSEU_UINT32_MAX;
            }

            static constexpr nk_uint32 lowest() noexcept {
                return min();
            }

            static constexpr nk_int32 digits = 32;
            static constexpr nk_int32 digits10 = 9;
        };

        // ====================================================================
        // SECTION 7 : SPÉCIALISATIONS POUR TYPES ENTIERS 64 BITS
        // ====================================================================
        // Limites et propriétés pour nk_int64 et nk_uint64.

        /**
         * @brief Spécialisation pour nk_int64 (entier signé 64 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [-9223372036854775808, 9223372036854775807]
         *   - digits = 63, digits10 = 18
         */
        template <>
        struct NkNumericLimits<nk_int64> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = true;
            static constexpr nk_bool is_integer = true;

            static constexpr nk_int64 min() noexcept {
                return NKENTSEU_INT64_MIN;
            }

            static constexpr nk_int64 max() noexcept {
                return NKENTSEU_INT64_MAX;
            }

            static constexpr nk_int64 lowest() noexcept {
                return min();
            }

            static constexpr nk_int32 digits = 63;
            static constexpr nk_int32 digits10 = 18;
        };

        /**
         * @brief Spécialisation pour nk_uint64 (entier non-signé 64 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Plage : [0, 18446744073709551615]
         *   - digits = 64, digits10 = 19
         */
        template <>
        struct NkNumericLimits<nk_uint64> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = false;
            static constexpr nk_bool is_integer = true;

            static constexpr nk_uint64 min() noexcept {
                return 0;
            }

            static constexpr nk_uint64 max() noexcept {
                return NKENTSEU_UINT64_MAX;
            }

            static constexpr nk_uint64 lowest() noexcept {
                return min();
            }

            static constexpr nk_int32 digits = 64;
            static constexpr nk_int32 digits10 = 19;
        };

        // ====================================================================
        // SECTION 8 : SPÉCIALISATIONS POUR TYPES FLOTTANTS
        // ====================================================================
        // Limites IEEE 754 pour nk_float32 et nk_float64.

        /**
         * @brief Spécialisation pour nk_float32 (flottant simple précision 32 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Format IEEE 754 binary32
         *   - digits = 24 bits de mantisse (23 explicites + 1 implicite)
         *   - digits10 = 6 chiffres décimaux de précision garantie
         *   - max_digits10 = 9 pour round-trip decimal → float → decimal sans perte
         *   - radix = 2 (représentation binaire)
         *
         * @warning
         *   Les méthodes infinity() et quiet_NaN() ne sont pas constexpr car elles
         *   nécessitent une manipulation bit-level via union (non constexpr en C++11).
         *   En C++20+, ces méthodes pourraient être rendues constexpr avec std::bit_cast.
         *
         * @example
         * @code
         * // Comparaison flottante avec epsilon
         * bool FloatEqual(nk_float32 a, nk_float32 b) {
         *     using Limits = nkentseu::NkNumericLimits<nk_float32>;
         *     return nkentseu::NkAbs(a - b) < Limits::epsilon();
         * }
         *
         * // Détection de valeurs spéciales
         * nk_float32 inf = nkentseu::NkNumericLimits<nk_float32>::infinity();
         * if (x >= inf) { /\* x est infini *\/ }
         *
         * nk_float32 nan = nkentseu::NkNumericLimits<nk_float32>::quiet_NaN();
         * if (x != x) { /\* x est NaN (seul NaN n'est pas égal à lui-même) *\/ }
         * @endcode
         */
        template <>
        struct NkNumericLimits<nk_float32> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = true;
            static constexpr nk_bool is_integer = false;
            static constexpr nk_bool is_exact = false;  ///< Les flottants ne représentent pas toutes les valeurs exactement

            /**
             * @brief Plus petite valeur normalisée positive
             * @return ~1.175494e-38f (2^-126)
             */
            static constexpr nk_float32 min() noexcept {
                return NKENTSEU_FLOAT32_MIN;
            }

            /**
             * @brief Plus grande valeur finie représentable
             * @return ~3.402823e+38f ((2-2^-23) × 2^127)
             */
            static constexpr nk_float32 max() noexcept {
                return NKENTSEU_FLOAT32_MAX;
            }

            /**
             * @brief Plus petite valeur (négative) représentable
             * @return -max()
             */
            static constexpr nk_float32 lowest() noexcept {
                return -NKENTSEU_FLOAT32_MAX;
            }

            /**
             * @brief Epsilon machine : différence entre 1.0 et la prochaine valeur représentable
             * @return ~1.192093e-07f (2^-23)
             * @note Utile pour les comparaisons flottantes avec tolérance
             */
            static constexpr nk_float32 epsilon() noexcept {
                return 1.192092896e-07f;
            }

            /**
             * @brief Infini positif (IEEE 754)
             * @return Valeur représentant +∞
             * @note Représentation bit-level : exposant 0xFF, mantisse 0
             */
            static nk_float32 infinity() noexcept;

            /**
             * @brief NaN silencieux (Not-a-Number)
             * @return Valeur NaN qui ne lève pas d'exception
             * @note Représentation bit-level : exposant 0xFF, mantisse != 0, bit de silence = 1
             */
            static nk_float32 quiet_NaN() noexcept;

            // Métadonnées IEEE 754 binary32
            static constexpr nk_int32 digits = 24;           ///< Bits de mantisse (23 stockés + 1 implicite)
            static constexpr nk_int32 digits10 = 6;          ///< Chiffres décimaux de précision garantie
            static constexpr nk_int32 max_digits10 = 9;      ///< Chiffres pour round-trip decimal sans perte

            static constexpr nk_int32 radix = 2;             ///< Base de représentation (binaire)
            static constexpr nk_int32 min_exponent = -125;   ///< Exposant binaire minimum pour valeurs normalisées
            static constexpr nk_int32 max_exponent = 128;    ///< Exposant binaire maximum
            static constexpr nk_int32 min_exponent10 = -37;  ///< Exposant décimal minimum approximatif
            static constexpr nk_int32 max_exponent10 = 38;   ///< Exposant décimal maximum approximatif
        };

        /**
         * @brief Spécialisation pour nk_float64 (flottant double précision 64 bits)
         * @ingroup NumericLimitsTemplates
         *
         * @note
         *   - Format IEEE 754 binary64
         *   - digits = 53 bits de mantisse (52 explicites + 1 implicite)
         *   - digits10 = 15 chiffres décimaux de précision garantie
         *   - max_digits10 = 17 pour round-trip sans perte
         *
         * @warning
         *   Mêmes limitations constexpr que nk_float32 pour infinity()/quiet_NaN().
         */
        template <>
        struct NkNumericLimits<nk_float64> {
            static constexpr nk_bool is_specialized = true;
            static constexpr nk_bool is_signed = true;
            static constexpr nk_bool is_integer = false;
            static constexpr nk_bool is_exact = false;

            static constexpr nk_float64 min() noexcept {
                return NKENTSEU_FLOAT64_MIN;
            }

            static constexpr nk_float64 max() noexcept {
                return NKENTSEU_FLOAT64_MAX;
            }

            static constexpr nk_float64 lowest() noexcept {
                return -NKENTSEU_FLOAT64_MAX;
            }

            static constexpr nk_float64 epsilon() noexcept {
                return 2.2204460492503131e-16;  ///< 2^-52
            }

            static nk_float64 infinity() noexcept;
            static nk_float64 quiet_NaN() noexcept;

            static constexpr nk_int32 digits = 53;
            static constexpr nk_int32 digits10 = 15;
            static constexpr nk_int32 max_digits10 = 17;

            static constexpr nk_int32 radix = 2;
            static constexpr nk_int32 min_exponent = -1021;
            static constexpr nk_int32 max_exponent = 1024;
            static constexpr nk_int32 min_exponent10 = -307;
            static constexpr nk_int32 max_exponent10 = 308;
        };

        /** @} */ // End of NumericLimitsTemplates

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKLIMITS_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKLIMITS.H
// =============================================================================
// Ce fichier fournit un accès portable aux limites et propriétés des types
// numériques du framework NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Accès direct aux limites des types entiers
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkLimits.h"
    #include <cstdio>

    void PrintIntegerLimits() {
        using namespace nkentseu;

        printf("nk_int8   : [%d, %d]\n",
            NkNumericLimits<nk_int8>::min(),
            NkNumericLimits<nk_int8>::max());

        printf("nk_uint32 : [%u, %u]\n",
            NkNumericLimits<nk_uint32>::min(),
            NkNumericLimits<nk_uint32>::max());

        printf("nk_int64  : [%lld, %lld]\n",
            static_cast<long long>(NkNumericLimits<nk_int64>::min()),
            static_cast<long long>(NkNumericLimits<nk_int64>::max()));
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Comparaison flottante avec epsilon
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkLimits.h"
    #include "NKCore/NkTypeUtils.h"  // Pour NkAbs si nécessaire

    namespace nkentseu {

        // Fonction générique pour comparaison flottante avec tolérance
        template <typename FloatT>
        bool FloatAlmostEqual(FloatT a, FloatT b, FloatT toleranceRatio = 1) {
            using Limits = NkNumericLimits<FloatT>;

            // Si les valeurs sont exactement égales (inclut le cas 0.0 == 0.0)
            if (a == b) {
                return true;
            }

            // Calcul de la tolérance relative basée sur l'epsilon du type
            FloatT epsilon = Limits::epsilon() * toleranceRatio;
            FloatT diff = (a > b) ? (a - b) : (b - a);  // Abs sans dépendance
            FloatT maxVal = (a > b) ? a : b;

            // Comparaison relative : diff < epsilon * max(|a|, |b|)
            return diff < epsilon * maxVal;
        }

        // Utilisation
        void CompareFloats() {
            nk_float32 expected = 0.1f + 0.2f;  // ~0.3000000119
            nk_float32 actual = 0.3f;           // ~0.3000000119 (même représentation)

            if (FloatAlmostEqual(expected, actual)) {
                printf("Values are approximately equal\n");
            }

            // Avec tolérance personnalisée pour calculs accumulés
            nk_float32 sum = 0.0f;
            for (int i = 0; i < 1000; ++i) {
                sum += 0.001f;
            }
            // sum peut être 0.999999 ou 1.000001 à cause des erreurs d'arrondi
            if (FloatAlmostEqual(sum, 1.0f, 10.0f)) {  // Tolérance 10x epsilon
                printf("Accumulated sum is close enough to 1.0\n");
            }
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Détection et gestion des valeurs spéciales flottantes
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkLimits.h"

    namespace nkentseu {

        // Vérification de validité d'un flottant (pas NaN, pas infini)
        template <typename FloatT>
        bool IsValidFloat(FloatT value) {
            using Limits = NkNumericLimits<FloatT>;

            // NaN n'est jamais égal à lui-même (propriété IEEE 754)
            if (value != value) {
                return false;
            }

            // Vérification d'infini par comparaison avec la valeur d'infini
            FloatT inf = Limits::infinity();
            if (value == inf || value == -inf) {
                return false;
            }

            return true;
        }

        // Normalisation sécurisée d'une valeur flottante
        nk_float32 ClampFloat(nk_float32 value, nk_float32 minVal, nk_float32 maxVal) {
            using Limits = NkNumericLimits<nk_float32>;

            // Rejet des valeurs non-finies
            if (!IsValidFloat(value) || !IsValidFloat(minVal) || !IsValidFloat(maxVal)) {
                return 0.0f;  // Fallback safe
            }

            // Clamp manuel sans dépendance à NkClamp pour éviter les problèmes avec NaN
            if (value < minVal) { return minVal; }
            if (value > maxVal) { return maxVal; }
            return value;
        }

        // Utilisation
        void ProcessSensorData(nk_float32* readings, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                readings[i] = ClampFloat(readings[i], -100.0f, 100.0f);
            }
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Code générique avec validation compile-time des types
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkLimits.h"
    #include "NKCore/NkTypeUtils.h"  // Pour NkStaticAssert si nécessaire

    namespace nkentseu {

        // Fonction générique qui ne compile que pour les types entiers supportés
        template <typename IntT>
        IntT SaturatingAdd(IntT a, IntT b) {
            using Limits = NkNumericLimits<IntT>;

            // Vérification compile-time que le type est supporté et entier
            static_assert(Limits::is_specialized, "Type not supported by NkNumericLimits");
            static_assert(Limits::is_integer, "SaturatingAdd requires integer types");

            if (Limits::is_signed) {
                // Addition signée avec détection d'overflow
                if ((b > 0) && (a > Limits::max() - b)) {
                    return Limits::max();  // Overflow positif → saturation
                }
                if ((b < 0) && (a < Limits::min() - b)) {
                    return Limits::min();  // Overflow négatif → saturation
                }
            } else {
                // Addition non-signée avec détection d'overflow
                if (b > Limits::max() - a) {
                    return Limits::max();  // Overflow → saturation
                }
            }

            return static_cast<IntT>(a + b);  // Pas d'overflow, addition normale
        }

        // Utilisation
        void TestSaturatingAdd() {
            // Entiers signés
            nk_int8 a = 100, b = 50;
            nk_int8 result = SaturatingAdd(a, b);  // 127 (saturation, pas 150)

            // Entiers non-signés
            nk_uint16 x = 65000, y = 1000;
            nk_uint16 sum = SaturatingAdd(x, y);  // 65535 (saturation)

            // Ce code ne compilerait PAS (type non-entier) :
            // nk_float32 f = 1.5f;
            // auto bad = SaturatingAdd(f, f);  // Erreur: static_assert failed
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Génération de valeurs aléatoires dans les limites du type
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkLimits.h"
    #include <random>  // Pour std::mt19937, etc.

    namespace nkentseu {

        // Générateur de valeurs aléatoires dans la plage complète d'un type entier
        template <typename IntT>
        IntT RandomInRange(std::mt19937& rng) {
            using Limits = NkNumericLimits<IntT>;

            static_assert(Limits::is_specialized && Limits::is_integer,
                "RandomInRange requires specialized integer types");

            // Distribution uniforme sur toute la plage du type
            std::uniform_int_distribution<IntT> dist(Limits::min(), Limits::max());
            return dist(rng);
        }

        // Version pour flottants avec plage personnalisée
        template <typename FloatT>
        FloatT RandomFloatInRange(std::mt19937& rng, FloatT minVal, FloatT maxVal) {
            using Limits = NkNumericLimits<FloatT>;

            static_assert(Limits::is_specialized && !Limits::is_integer,
                "RandomFloatInRange requires specialized floating-point types");

            // Vérification de validité des bornes
            if (minVal >= maxVal || !IsValidFloat(minVal) || !IsValidFloat(maxVal)) {
                return static_cast<FloatT>(0);  // Fallback
            }

            std::uniform_real_distribution<FloatT> dist(minVal, maxVal);
            return dist(rng);
        }

        // Utilisation
        void GenerateTestData() {
            std::mt19937 rng(42);  // Seed fixe pour reproductibilité

            // Entiers
            nk_int32 randomInt = RandomInRange<nk_int32>(rng);  // [-2^31, 2^31-1]
            nk_uint8 randomByte = RandomInRange<nk_uint8>(rng);  // [0, 255]

            // Flottants
            nk_float32 randomFloat = RandomFloatInRange<nk_float32>(rng, -1.0f, 1.0f);
            nk_float64 randomDouble = RandomFloatInRange<nk_float64>(rng, 0.0, 100.0);
        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================