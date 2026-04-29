// =============================================================================
// NKMath/NkFunctions.h
// API mathématique scalaire de base pour le framework NKCore.
//
// Design :
//  - Macro NKENTSEU_MATH_API pour export des symboles non-inline/non-template
//  - Fonctions mathématiques scalaires avec naming Nk* cohérent
//  - Constantes mathématiques et physiques dans namespace dédié
//  - Support float32/float64 avec surcharges explicites
//  - Fonctions inline pour performance (NK_FORCE_INLINE)
//  - Compatibilité avec code legacy via aliases constants/fonctions
//  - Aucune dépendance STL (containers/algorithms) pour portabilité
//  - Intégration avec NKCore/NKCore.h pour types fondamentaux
//
// Intégration :
//  - Utilise NKCore/NkCoreExport.h pour macro d'export NKENTSEU_MATH_API
//  - Utilise NKCore/NKCore.h pour types nk_int32, float32, nk_bool, etc.
//  - Utilise NKPlatform/NkPlatformInline.h pour NKENTSEU_FORCE_INLINE
//  - Compatible avec code C++11+ sans exceptions/RTTI requis
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MATH_NKFUNCTIONS_H
#define NKENTSEU_MATH_NKFUNCTIONS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de base et gestion des conflits de macros.

    #include "NKCore/NKCore.h"
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NKPlatform.h"
    #include "NKMath/NkMathApi.h"
    #include <initializer_list>

    /** @} */ // End of MathExportMacro

    // -------------------------------------------------------------------------
    // SECTION 3 : GESTION DES MACROS LEGACY
    // -------------------------------------------------------------------------
    // Undef des macros fonctionnelles qui entreraient en conflit avec nos fonctions.

    /**
     * @defgroup MathMacroCleanup Nettoyage des Macros Legacy
     * @brief Undef des macros NkMin/NkMax/NkClamp/NkAbs pour éviter les conflits
     * @ingroup MathUtilities
     * @internal
     *
     * NkMacros.h peut définir des macros fonctionnelles (NkMin(x,y), etc.)
     * qui entreraient en conflit avec nos fonctions NkMin<T>(x,y).
     * Ces undef garantissent que les appels résolvent vers nos fonctions.
     *
     * @note
     *   - Les macros sont undef uniquement dans ce scope de compilation
     *   - Le code legacy utilisant les macros doit inclure NkMacros.h après ce fichier
     *   - Préférer les fonctions templates pour type-safety et débogage amélioré
     */
    /** @{ */

    #ifdef NkMin
        #undef NkMin
    #endif
    #ifdef NkMax
        #undef NkMax
    #endif
    #ifdef NkClamp
        #undef NkClamp
    #endif
    #ifdef NkAbs
        #undef NkAbs
    #endif

    /** @} */ // End of MathMacroCleanup

    // -------------------------------------------------------------------------
    // SECTION 4 : MACRO D'INLINE FORCÉ
    // -------------------------------------------------------------------------
    // Mapping vers la macro plateforme pour inlining agressif.

    /**
     * @brief Macro pour forcer l'inlining des fonctions mathématiques
     * @def NK_FORCE_INLINE
     * @ingroup MathUtilities
     * @note Alias vers NKENTSEU_FORCE_INLINE depuis NkPlatformInline.h
     *
     * @example
     * @code
     * NK_FORCE_INLINE float32 NkSquare(float32 x) noexcept {
     *     return x * x;  // Sera inliné même en mode debug si supporté
     * }
     * @endcode
     */
    #ifndef NK_FORCE_INLINE
        #define NK_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #endif

    // -------------------------------------------------------------------------
    // SECTION 5 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms nkentseu::math.

    namespace nkentseu {

        // Indentation niveau 1 : namespace math
        namespace math {

            // ====================================================================
            // SECTION 6 : CONSTANTES MATHÉMATIQUES ET PHYSIQUES
            // ====================================================================
            // Constantes canoniques avec suffixes F/D pour float32/float64.

            /**
             * @defgroup MathConstants Constantes Mathématiques et Physiques
             * @brief Constantes pré-calculées pour calculs numériques et physiques
             * @ingroup MathCore
             *
             * Ce namespace regroupe les constantes mathématiques fondamentales
             * avec une précision adaptée (float32 suffix F, float64 sans suffixe).
             *
             * @note
             *   - Toutes les constantes sont constexpr : évaluées à la compilation
             *   - Suffixes : kPiF (float32), kPi (float64) pour clarté
             *   - Précision : valeurs avec 20+ décimales pour minimiser l'erreur d'arrondi
             *
             * @example
             * @code
             * // Utilisation des constantes
             * float32 circumference = 2.0f * nkentseu::math::constants::kTauF * radius;
             * float64 energy = nkentseu::math::constants::kPlanck * frequency;
             *
             * // Comparaison avec epsilon adapté
             * if (nkentseu::math::NkFabs(a - b) < nkentseu::math::constants::kCompareAbsEpsilonF) {
             *     // a et b sont "égaux" en précision float32
             * }
             * @endcode
             */
            /** @{ */

            namespace constants {

                // ============================================================
                // CONSTANTES MATHÉMATIQUES FONDAMENTALES
                // ============================================================

                /** @brief π (Pi) en précision float32 */
                constexpr float32 kPiF = 3.14159265358979323846f;

                /** @brief π (Pi) en précision float64 */
                constexpr float64 kPi = 3.14159265358979323846;

                /** @brief π/2 en précision float32 */
                constexpr float32 kHalfPiF = 1.57079632679489661923f;

                /** @brief π/2 en précision float64 */
                constexpr float64 kHalfPi = 1.57079632679489661923;

                /** @brief π/4 en précision float32 */
                constexpr float32 kQuarterPiF = 0.78539816339744830961f;

                /** @brief π/4 en précision float64 */
                constexpr float64 kQuarterPi = 0.78539816339744830961;

                /** @brief 2π (Tau) en précision float32 */
                constexpr float32 kTauF = 6.28318530717958647692f;

                /** @brief 2π (Tau) en précision float64 */
                constexpr float64 kTau = 6.28318530717958647692;

                /** @brief 1/π en précision float32 */
                constexpr float32 kInvPiF = 0.31830988618379067154f;

                /** @brief 1/π en précision float64 */
                constexpr float64 kInvPi = 0.31830988618379067154;

                // ============================================================
                // CONSTANTES DE PRÉCISION ET COMPARAISON
                // ============================================================

                /** @brief Epsilon machine float32 (plus petite valeur > 1.0) */
                constexpr float32 kEpsilonF = 1.0e-6f;

                /** @brief Epsilon machine float64 (plus petite valeur > 1.0) */
                constexpr float64 kEpsilonD = 1.0e-15;

                /** @brief Epsilon absolu pour comparaisons float32 */
                constexpr float32 kCompareAbsEpsilonF = 1.0e-6f;

                /** @brief Epsilon absolu pour comparaisons float64 */
                constexpr float64 kCompareAbsEpsilonD = 1.0e-12;

                /** @brief Epsilon relatif pour comparaisons float32 */
                constexpr float32 kCompareRelEpsilonF = 1.0e-5f;

                /** @brief Epsilon relatif pour comparaisons float64 */
                constexpr float64 kCompareRelEpsilonD = 1.0e-12;

                // ============================================================
                // CONSTANTES EXPONENTIELLES ET LOGARITHMIQUES
                // ============================================================

                /** @brief Nombre d'Euler (e) en précision float32 */
                constexpr float32 kEF = 2.71828182845904523536f;

                /** @brief Nombre d'Euler (e) en précision float64 */
                constexpr float64 kE = 2.71828182845904523536;

                /** @brief ln(2) en précision float32 */
                constexpr float32 kLn2F = 0.69314718055994530942f;

                /** @brief ln(2) en précision float64 */
                constexpr float64 kLn2 = 0.69314718055994530942;

                /** @brief ln(10) en précision float32 */
                constexpr float32 kLn10F = 2.30258509299404568402f;

                /** @brief ln(10) en précision float64 */
                constexpr float64 kLn10 = 2.30258509299404568402;

                // ============================================================
                // CONSTANTES DE RACINES CARRÉES
                // ============================================================

                /** @brief √2 en précision float32 */
                constexpr float32 kSqrt2F = 1.41421356237309504880f;

                /** @brief √2 en précision float64 */
                constexpr float64 kSqrt2 = 1.41421356237309504880;

                /** @brief √3 en précision float32 */
                constexpr float32 kSqrt3F = 1.73205080756887729353f;

                /** @brief √3 en précision float64 */
                constexpr float64 kSqrt3 = 1.73205080756887729353;

                // ============================================================
                // CONSTANTES PHYSIQUES ET CHIMIQUES (SI Units)
                // ============================================================

                /** @brief Accélération gravitationnelle standard (m/s²) */
                constexpr float64 kGravity = 9.80665;

                /** @brief Vitesse de la lumière dans le vide (m/s) */
                constexpr float64 kLightSpeed = 299792458.0;

                /** @brief Constante gravitationnelle (m³⋅kg⁻¹⋅s⁻²) */
                constexpr float64 kGravConstant = 6.67430e-11;

                /** @brief Constante de Planck (J⋅s) */
                constexpr float64 kPlanck = 6.62607015e-34;

                /** @brief Constante de Boltzmann (J/K) */
                constexpr float64 kBoltzmann = 1.380649e-23;

                /** @brief Nombre d'Avogadro (mol⁻¹) */
                constexpr float64 kAvogadro = 6.02214076e23;

                /** @brief Constante des gaz parfaits (J⋅mol⁻¹⋅K⁻¹) */
                constexpr float64 kGasConstant = 8.314462618;

            } // namespace constants

            /** @} */ // End of MathConstants

            // ====================================================================
            // SECTION 7 : CONSTANTES DE COMPATIBILITÉ LEGACY
            // ====================================================================
            // Aliases pour code legacy utilisant les anciens noms de constantes.

            /**
             * @defgroup LegacyMathConstants Constantes Legacy pour Compatibilité
             * @brief Aliases vers constants::k* pour migration douce du code legacy
             * @ingroup MathCore
             * @deprecated Préférer nkentseu::math::constants::k* pour nouveau code
             *
             * Ces constantes sont fournies pour éviter de casser le code existant
             * qui utilise les noms legacy (NK_PI_F, PI, etc.). Elles seront
             * potentiellement supprimées dans une future version majeure.
             *
             * @example
             * @code
             * // Ancien code (encore supporté)
             * float area = NK_PI_F * radius * radius;
             *
             * // Nouveau code recommandé
             * float area = nkentseu::math::constants::kPiF * radius * radius;
             * @endcode
             */
            /** @{ */

            // ============================================================
            // ALIASES MATHÉMATIQUES LEGACY
            // ============================================================

            /** @deprecated Utiliser constants::kPiF */
            constexpr float32 NK_PI_F = constants::kPiF;

            /** @deprecated Utiliser constants::kPi */
            constexpr float64 NK_PI_D = constants::kPi;

            /** @deprecated Utiliser constants::kHalfPiF */
            constexpr float32 NK_PI_2_F = constants::kHalfPiF;

            /** @deprecated Utiliser constants::kHalfPi */
            constexpr float64 NK_PI_2_D = constants::kHalfPi;

            /** @deprecated Utiliser constants::kQuarterPiF */
            constexpr float32 NK_PI_4_F = constants::kQuarterPiF;

            /** @deprecated Utiliser constants::kQuarterPi */
            constexpr float64 NK_PI_4_D = constants::kQuarterPi;

            /** @deprecated Utiliser constants::kTauF */
            constexpr float32 NK_2PI_F = constants::kTauF;

            /** @deprecated Utiliser constants::kTau */
            constexpr float64 NK_2PI_D = constants::kTau;

            /** @deprecated Utiliser constants::kInvPiF */
            constexpr float32 NK_INV_PI_F = constants::kInvPiF;

            /** @deprecated Utiliser constants::kInvPi */
            constexpr float64 NK_INV_PI_D = constants::kInvPi;

            /** @deprecated Utiliser constants::kEF */
            constexpr float32 NK_E_F = constants::kEF;

            /** @deprecated Utiliser constants::kE */
            constexpr float64 NK_E_D = constants::kE;

            /** @deprecated Utiliser constants::kLn2F */
            constexpr float32 NK_LN2_F = constants::kLn2F;

            /** @deprecated Utiliser constants::kLn2 */
            constexpr float64 NK_LN2_D = constants::kLn2;

            /** @deprecated Utiliser constants::kLn10F */
            constexpr float32 NK_LN10_F = constants::kLn10F;

            /** @deprecated Utiliser constants::kLn10 */
            constexpr float64 NK_LN10_D = constants::kLn10;

            /** @deprecated Utiliser constants::kCompareAbsEpsilonF */
            constexpr float32 NK_COMPARE_ABS_EPSILON_F = constants::kCompareAbsEpsilonF;

            /** @deprecated Utiliser constants::kCompareAbsEpsilonD */
            constexpr float64 NK_COMPARE_ABS_EPSILON_D = constants::kCompareAbsEpsilonD;

            /** @deprecated Utiliser constants::kCompareRelEpsilonF */
            constexpr float32 NK_COMPARE_REL_EPSILON_F = constants::kCompareRelEpsilonF;

            /** @deprecated Utiliser constants::kCompareRelEpsilonD */
            constexpr float64 NK_COMPARE_REL_EPSILON_D = constants::kCompareRelEpsilonD;

            // ============================================================
            // ALIASES COURTS LEGACY (NOMS SANS PRÉFIXE)
            // ============================================================

            /** @deprecated Utiliser constants::kPiF ou constants::kPi */
            constexpr float32 PI_F = constants::kPiF;

            /** @deprecated Utiliser constants::kPi */
            constexpr float64 PI = constants::kPi;

            // ============================================================
            // ALIASES CONSTANTES PHYSIQUES LEGACY
            // ============================================================

            /** @deprecated Utiliser constants::kLightSpeed */
            constexpr float64 NK_C = constants::kLightSpeed;

            /** @deprecated Utiliser constants::kGravConstant */
            constexpr float64 NK_G = constants::kGravConstant;

            /** @deprecated Utiliser constants::kPlanck */
            constexpr float64 NK_H = constants::kPlanck;

            /** @deprecated Utiliser constants::kBoltzmann */
            constexpr float64 NK_K_B = constants::kBoltzmann;

            /** @deprecated Utiliser constants::kAvogadro */
            constexpr float64 NK_NA = constants::kAvogadro;

            /** @deprecated Utiliser constants::kGasConstant */
            constexpr float64 NK_R = constants::kGasConstant;

            /** @deprecated Utiliser constants::kGravity */
            constexpr float64 NK_G_ACCEL = constants::kGravity;

            /** @} */ // End of LegacyMathConstants

            // ====================================================================
            // SECTION 8 : UTILITAIRES DE CONVERSION D'ANGLES
            // ====================================================================
            // Conversions degrés ↔ radians avec constantes pré-calculées.

            /**
             * @defgroup AngleConversionHelpers Helpers de Conversion d'Angles
             * @brief Fonctions et constantes pour conversions degrés/radians
             * @ingroup MathUtilities
             *
             * Ces utilitaires facilitent les conversions entre degrés et radians,
             * opérations courantes en trigonométrie et graphisme.
             *
             * @note
             *   - DEG_TO_RAD_F/D : facteur de conversion degrés → radians
             *   - RAD_TO_DEG_F/D : facteur de conversion radians → degrés
             *   - Toutes les fonctions sont noexcept et NK_FORCE_INLINE
             *
             * @example
             * @code
             * // Conversion simple
             * float32 angleRad = nkentseu::math::NkRadiansFromDegrees(45.0f);
             * // angleRad == 0.785398... (π/4)
             *
             * // Conversion dans une expression trigonométrique
             * float32 elevationDeg = 30.0f;
             * float32 height = distance * nkentseu::math::NkSin(
             *     nkentseu::math::NkRadiansFromDegrees(elevationDeg)
             * );
             * @endcode
             */
            /** @{ */

            /** @brief Facteur de conversion degrés → radians (float32) */
            constexpr float32 DEG_TO_RAD_F = constants::kPiF / 180.0f;

            /** @brief Facteur de conversion degrés → radians (float64) */
            constexpr float64 DEG_TO_RAD = constants::kPi / 180.0;

            /** @brief Facteur de conversion radians → degrés (float32) */
            constexpr float32 RAD_TO_DEG_F = 180.0f / constants::kPiF;

            /** @brief Facteur de conversion radians → degrés (float64) */
            constexpr float64 RAD_TO_DEG = 180.0 / constants::kPi;

            /**
             * @brief Convertit un angle de degrés en radians (float32)
             * @param degrees Angle en degrés
             * @return Angle équivalent en radians
             * @note Multiplication par DEG_TO_RAD_F : zéro overhead en release
             */
            NK_FORCE_INLINE
            float32 NkRadiansFromDegrees(float32 degrees) noexcept {
                return degrees * DEG_TO_RAD_F;
            }

            /**
             * @brief Convertit un angle de degrés en radians (float64)
             * @param degrees Angle en degrés
             * @return Angle équivalent en radians
             */
            NK_FORCE_INLINE
            float64 NkRadiansFromDegrees(float64 degrees) noexcept {
                return degrees * DEG_TO_RAD;
            }

            /**
             * @brief Convertit un angle de radians en degrés (float32)
             * @param radians Angle en radians
             * @return Angle équivalent en degrés
             */
            NK_FORCE_INLINE
            float32 NkDegreesFromRadians(float32 radians) noexcept {
                return radians * RAD_TO_DEG_F;
            }

            /**
             * @brief Convertit un angle de radians en degrés (float64)
             * @param radians Angle en radians
             * @return Angle équivalent en degrés
             */
            NK_FORCE_INLINE
            float64 NkDegreesFromRadians(float64 radians) noexcept {
                return radians * RAD_TO_DEG;
            }

            // ============================================================
            // ALIASES LEGACY POUR CONVERSIONS D'ANGLES
            // ============================================================

            /**
             * @brief Alias legacy pour NkRadiansFromDegrees (float32)
             * @deprecated Préférer NkRadiansFromDegrees pour clarté sémantique
             */
            NK_FORCE_INLINE
            float32 NkToRadians(float32 degrees) noexcept {
                return NkRadiansFromDegrees(degrees);
            }

            /** @copydoc NkToRadians(float32) */
            NK_FORCE_INLINE
            float64 NkToRadians(float64 degrees) noexcept {
                return NkRadiansFromDegrees(degrees);
            }

            /**
             * @brief Alias legacy pour NkDegreesFromRadians (float32)
             * @deprecated Préférer NkDegreesFromRadians pour clarté sémantique
             */
            NK_FORCE_INLINE
            float32 NkToDegrees(float32 radians) noexcept {
                return NkDegreesFromRadians(radians);
            }

            /** @copydoc NkToDegrees(float32) */
            NK_FORCE_INLINE
            float64 NkToDegrees(float64 radians) noexcept {
                return NkDegreesFromRadians(radians);
            }

            /** @} */ // End of AngleConversionHelpers

            // ====================================================================
            // SECTION 9 : FONCTIONS DE BASE (ABS, MIN, MAX, CLAMP)
            // ====================================================================
            // Opérations fondamentales sur scalaires avec templates génériques.

            /**
             * @defgroup BasicMathFunctions Fonctions Mathématiques de Base
             * @brief Opérations fondamentales : valeur absolue, min, max, clamp
             * @ingroup MathCore
             *
             * Ces fonctions fournissent des opérations mathématiques élémentaires
             * avec support template pour types génériques et surcharges pour
             * float32/float64 avec gestion correcte des signes.
             *
             * @note
             *   - NkAbs/NkFabs : distingués pour entiers (Abs) vs flottants (Fabs)
             *   - NkMin/NkMax : templates pour types comparables + surcharges initializer_list
             *   - NkClamp/NkSaturate : clamp générique et saturation [0,1] shortcut
             *
             * @example
             * @code
             * // Valeur absolue
             * int32 dist = nkentseu::math::NkAbs(position - target);
             * float32 magnitude = nkentseu::math::NkFabs(velocity);
             *
             * // Min/Max avec deux arguments
             * float32 clamped = nkentseu::math::NkMax(0.0f, nkentseu::math::NkMin(1.0f, value));
             *
             * // Min/Max avec liste d'arguments (C++11 initializer_list)
             * float32 minVal = nkentseu::math::NkMin({1.2f, -3.4f, 5.6f, -7.8f});  // -7.8f
             *
             * // Clamp générique
             * int32 index = nkentseu::math::NkClamp(userIndex, 0, arraySize - 1);
             *
             * // Saturation [0,1] pour facteurs de mélange
             * float32 t = nkentseu::math::NkSaturate(alpha);  // Équivalent à Clamp(alpha, 0, 1)
             * @endcode
             */
            /** @{ */

            /**
             * @brief Valeur absolue pour entier signé 32-bit
             * @param v Valeur d'entrée
             * @return |v| (v si v >= 0, -v sinon)
             * @note Évite l'overflow pour INT32_MIN (comportement défini par conception)
             */
            NK_FORCE_INLINE
            int32 NkAbs(int32 v) noexcept {
                return v < 0 ? -v : v;
            }

            /**
             * @brief Valeur absolue pour entier signé 64-bit
             * @param v Valeur d'entrée
             * @return |v|
             * @note Même remarque que pour int32 concernant INT64_MIN
             */
            NK_FORCE_INLINE
            int64 NkAbs(int64 v) noexcept {
                return v < 0 ? -v : v;
            }

            /**
             * @brief Valeur absolue pour flottant 32-bit
             * @param v Valeur d'entrée
             * @return |v| (avec gestion correcte de -0.0 → +0.0)
             * @note Implémentation bitwise possible via NkBits, mais branche simple suffit
             */
            NK_FORCE_INLINE
            float32 NkFabs(float32 v) noexcept {
                return v < 0.0f ? -v : v;
            }

            /**
             * @brief Valeur absolue pour flottant 64-bit
             * @param v Valeur d'entrée
             * @return |v|
             */
            NK_FORCE_INLINE
            float64 NkFabs(float64 v) noexcept {
                return v < 0.0 ? -v : v;
            }

            /**
             * @brief Minimum de deux valeurs de même type
             * @tparam T Type comparable (doit supporter operator<)
             * @param a Première valeur
             * @param b Deuxième valeur
             * @return a si a < b, b sinon
             * @note Template générique : fonctionne pour types numériques, pointers, etc.
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkMin(T a, T b) noexcept {
                return (a < b) ? a : b;
            }

            /**
             * @brief Maximum de deux valeurs de même type
             * @tparam T Type comparable (doit supporter operator>)
             * @param a Première valeur
             * @param b Deuxième valeur
             * @return a si a > b, b sinon
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkMax(T a, T b) noexcept {
                return (a > b) ? a : b;
            }

            /**
             * @brief Minimum d'une liste de valeurs de même type
             * @tparam T Type comparable
             * @param list Liste d'initialisation de valeurs
             * @return Minimum de la liste, ou T{} si liste vide
             * @note
             *   - Complexité O(n) avec n = list.size()
             *   - Retourne T{} (valeur par défaut) si liste vide : comportement défini
             *
             * @example
             * @code
             * float32 minVal = nkentseu::math::NkMin({3.14f, 1.41f, 2.71f, 0.57f});  // 0.57f
             * @endcode
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkMin(std::initializer_list<T> list) noexcept {
                if (list.size() == 0) {
                    return T{};
                }
                auto it = list.begin();
                T minVal = *it++;
                for (; it != list.end(); ++it) {
                    if (*it < minVal) {
                        minVal = *it;
                    }
                }
                return minVal;
            }

            /**
             * @brief Maximum d'une liste de valeurs de même type
             * @tparam T Type comparable
             * @param list Liste d'initialisation de valeurs
             * @return Maximum de la liste, ou T{} si liste vide
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkMax(std::initializer_list<T> list) noexcept {
                if (list.size() == 0) {
                    return T{};
                }
                auto it = list.begin();
                T maxVal = *it++;
                for (; it != list.end(); ++it) {
                    if (*it > maxVal) {
                        maxVal = *it;
                    }
                }
                return maxVal;
            }

            /**
             * @brief Clampe une valeur dans un intervalle [lo, hi]
             * @tparam T Type ordonné (doit supporter operator< et operator>)
             * @param v Valeur à clamer
             * @param lo Borne inférieure inclusive
             * @param hi Borne supérieure inclusive
             * @return lo si v < lo, hi si v > hi, v sinon
             * @note
             *   - Ne vérifie pas que lo <= hi : comportement indéfini si lo > hi
             *   - Préférer NkSaturate pour clamping [0,1] courant en graphisme
             *
             * @example
             * @code
             * // Clamp d'index de tableau
             * int32 safeIndex = nkentseu::math::NkClamp(userIndex, 0, arraySize - 1);
             *
             * // Clamp de paramètre de lerp
             * float32 t = nkentseu::math::NkClamp(interpolationFactor, 0.0f, 1.0f);
             * @endcode
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkClamp(T v, T lo, T hi) noexcept {
                return v < lo ? lo : (v > hi ? hi : v);
            }

            /**
             * @brief Sature une valeur dans l'intervalle [0, 1]
             * @tparam T Type numérique (doit supporter NkClamp)
             * @param v Valeur à saturer
             * @return 0 si v < 0, 1 si v > 1, v sinon
             * @note
             *   - Shortcut courant en graphisme pour facteurs de mélange, UV coords, etc.
             *   - Équivalent à NkClamp(v, static_cast<T>(0), static_cast<T>(1))
             *
             * @example
             * @code
             * // Saturation d'alpha pour blending
             * float32 alpha = nkentseu::math::NkSaturate(rawAlpha);
             *
             * // Saturation de coordonnées de texture
             * vec2 uv = nkentseu::math::NkSaturate(unclampedUV);
             * @endcode
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkSaturate(T v) noexcept {
                return NkClamp<T>(v, static_cast<T>(0), static_cast<T>(1));
            }

            /** @} */ // End of BasicMathFunctions

            // ====================================================================
            // SECTION 10 : COMPARAISONS FLOTTANTES AVEC TOLÉRANCE
            // ====================================================================
            // Comparaisons robustes de flottants avec epsilon absolu/relatif.

            /**
             * @defgroup FloatComparisonFunctions Comparaisons Flottantes Robustes
             * @brief Fonctions pour comparer des flottants avec tolérance d'erreur
             * @ingroup MathUtilities
             *
             * Les comparaisons directes de flottants (a == b) sont fragiles
             * à cause des erreurs d'arrondi. Ces fonctions utilisent des epsilon
             * absolus et relatifs pour des comparaisons "presque égales".
             *
             * @note
             *   - NkIsNearlyZero : teste si |value| <= absEpsilon
             *   - NkNearlyEqual : teste |a-b| <= max(absEpsilon, relEpsilon * max(|a|,|b|))
             *   - Epsilon par défaut : constants::kCompareAbsEpsilon* et kCompareRelEpsilon*
             *
             * @warning
             *   - Ne pas utiliser pour comparer avec zéro exact : préférer NkIsNearlyZero
             *   - Les epsilon doivent être adaptés à l'échelle des valeurs comparées
             *
             * @example
             * @code
             * // Test de quasi-nullité
             * if (nkentseu::math::NkIsNearlyZero(velocity)) {
             *     // Objet considéré comme immobile
             *     StopPhysicsSimulation();
             * }
             *
             * // Comparaison avec tolérance adaptative
             * if (nkentseu::math::NkNearlyEqual(positionA, positionB)) {
             *     // Positions considérées comme identiques
             *     MergeEntities();
             * }
             *
             * // Comparaison avec epsilon personnalisé pour haute précision
             * if (nkentseu::math::NkNearlyEqual(a, b, 1e-10f, 1e-8f)) {
             *     // Comparaison plus stricte que le défaut
             * }
             * @endcode
             */
            /** @{ */

            /**
             * @brief Teste si un float32 est "presque zéro"
             * @param value Valeur à tester
             * @param absEpsilon Seuil absolu de tolérance (défaut: kCompareAbsEpsilonF)
             * @return true si |value| <= absEpsilon
             * @note
             *   - Utile pour tester la convergence d'algorithmes itératifs
             *   - Évite les divisions par zéro ou les sqrt de valeurs négatives petites
             */
            NK_FORCE_INLINE
            nk_bool NkIsNearlyZero(
                float32 value,
                float32 absEpsilon = constants::kCompareAbsEpsilonF
            ) noexcept {
                return NkFabs(value) <= absEpsilon;
            }

            /**
             * @brief Teste si un float64 est "presque zéro"
             * @param value Valeur à tester
             * @param absEpsilon Seuil absolu de tolérance (défaut: kCompareAbsEpsilonD)
             * @return true si |value| <= absEpsilon
             */
            NK_FORCE_INLINE
            nk_bool NkIsNearlyZero(
                float64 value,
                float64 absEpsilon = constants::kCompareAbsEpsilonD
            ) noexcept {
                return NkFabs(value) <= absEpsilon;
            }

            /**
             * @brief Compare deux float32 avec tolérance absolue et relative
             * @param a Première valeur
             * @param b Deuxième valeur
             * @param absEpsilon Seuil absolu de tolérance (défaut: kCompareAbsEpsilonF)
             * @param relEpsilon Seuil relatif de tolérance (défaut: kCompareRelEpsilonF)
             * @return true si a et b sont "presque égaux"
             * @note
             *   - Algorithme : |a-b| <= max(absEpsilon, relEpsilon * max(|a|,|b|))
             *   - Gère correctement les cas où a ou b sont proches de zéro
             *   - Plus robuste que la simple comparaison |a-b| <= epsilon
             */
            NK_FORCE_INLINE
            nk_bool NkNearlyEqual(
                float32 a,
                float32 b,
                float32 absEpsilon = constants::kCompareAbsEpsilonF,
                float32 relEpsilon = constants::kCompareRelEpsilonF
            ) noexcept {
                const float32 diff = NkFabs(a - b);
                if (diff <= absEpsilon) {
                    return true;
                }
                const float32 scale = NkMax(NkFabs(a), NkFabs(b));
                return diff <= (scale * relEpsilon);
            }

            /**
             * @brief Compare deux float64 avec tolérance absolue et relative
             * @param a Première valeur
             * @param b Deuxième valeur
             * @param absEpsilon Seuil absolu de tolérance (défaut: kCompareAbsEpsilonD)
             * @param relEpsilon Seuil relatif de tolérance (défaut: kCompareRelEpsilonD)
             * @return true si a et b sont "presque égaux"
             */
            NK_FORCE_INLINE
            nk_bool NkNearlyEqual(
                float64 a,
                float64 b,
                float64 absEpsilon = constants::kCompareAbsEpsilonD,
                float64 relEpsilon = constants::kCompareRelEpsilonD
            ) noexcept {
                const float64 diff = NkFabs(a - b);
                if (diff <= absEpsilon) {
                    return true;
                }
                const float64 scale = NkMax(NkFabs(a), NkFabs(b));
                return diff <= (scale * relEpsilon);
            }

            /** @} */ // End of FloatComparisonFunctions

            // ====================================================================
            // SECTION 11 : FONCTIONS D'ARRONDI (DÉCLARATIONS EXPORTÉES)
            // ====================================================================
            // Déclarations des fonctions d'arrondi avec macro NKENTSEU_MATH_API.

            /**
             * @defgroup RoundingFunctions Fonctions d'Arrondi
             * @brief Arrondi vers le bas, haut, vers zéro, ou au plus proche
             * @ingroup MathCore
             *
             * Ces fonctions implémentent les opérations d'arrondi standards
             * avec comportement défini pour tous les cas (y compris NaN, Inf).
             *
             * @note
             *   - Implémentations dans NkFunctions.cpp avec export NKENTSEU_MATH_API
             *   - Comportement conforme à IEEE 754 quand possible
             *   - Retourne la valeur inchangée si déjà entière ou non-finie
             *
             * @example
             * @code
             * // Arrondi vers le bas (floor)
             * float32 f1 = nkentseu::math::NkFloor(3.7f);   // 3.0f
             * float32 f2 = nkentseu::math::NkFloor(-3.7f);  // -4.0f
             *
             * // Arrondi vers le haut (ceil)
             * float32 c1 = nkentseu::math::NkCeil(3.2f);    // 4.0f
             * float32 c2 = nkentseu::math::NkCeil(-3.2f);   // -3.0f
             *
             * // Arrondi vers zéro (trunc)
             * float32 t1 = nkentseu::math::NkTrunc(3.9f);   // 3.0f
             * float32 t2 = nkentseu::math::NkTrunc(-3.9f);  // -3.0f
             *
             * // Arrondi au plus proche (round, half-away-from-zero)
             * float32 r1 = nkentseu::math::NkRound(3.5f);   // 4.0f
             * float32 r2 = nkentseu::math::NkRound(-3.5f);  // -4.0f
             * @endcode
             */
            /** @{ */

            /**
             * @brief Arrondi vers l'entier inférieur (floor)
             * @param x Valeur d'entrée
             * @return Plus grand entier <= x
             */
            NKENTSEU_MATH_API float32 NkFloor(float32 x) noexcept;

            /** @copydoc NkFloor(float32) */
            NKENTSEU_MATH_API float64 NkFloor(float64 x) noexcept;

            /**
             * @brief Arrondi vers l'entier supérieur (ceil)
             * @param x Valeur d'entrée
             * @return Plus petit entier >= x
             */
            NKENTSEU_MATH_API float32 NkCeil(float32 x) noexcept;

            /** @copydoc NkCeil(float32) */
            NKENTSEU_MATH_API float64 NkCeil(float64 x) noexcept;

            /**
             * @brief Arrondi vers zéro (truncation)
             * @param x Valeur d'entrée
             * @return Partie entière de x avec signe préservé
             */
            NKENTSEU_MATH_API float32 NkTrunc(float32 x) noexcept;

            /** @copydoc NkTrunc(float32) */
            NKENTSEU_MATH_API float64 NkTrunc(float64 x) noexcept;

            /**
             * @brief Arrondi à l'entier le plus proche (half-away-from-zero)
             * @param x Valeur d'entrée
             * @return Entier le plus proche de x (.5 arrondi vers l'infini en valeur absolue)
             */
            NKENTSEU_MATH_API float32 NkRound(float32 x) noexcept;

            /** @copydoc NkRound(float32) */
            NKENTSEU_MATH_API float64 NkRound(float64 x) noexcept;

            /** @} */ // End of RoundingFunctions

            // ====================================================================
            // SECTION 12 : RACINES, EXPONENTIELLES ET LOGARITHMES (EXPORTÉS)
            // ====================================================================
            // Fonctions de puissance, racines et logarithmes avec export.

            /**
             * @defgroup PowerLogFunctions Fonctions de Puissance et Logarithmes
             * @brief Racines, exponentielles, logarithmes et puissances
             * @ingroup MathCore
             *
             * Ces fonctions couvrent les opérations mathématiques avancées
             * couramment utilisées en calcul numérique, physique et graphisme.
             *
             * @note
             *   - NkSqrt/NkRsqrt : racine carrée et inverse (optimisable via hardware)
             *   - NkExp/NkLog : exponentielle et logarithme naturel
             *   - NkLog10/NkLog2 : wrappers optimisés via NkLog et constantes
             *   - NkPow/NkPowInt : puissance réelle et puissance entière (plus rapide)
             *
             * @warning
             *   - NkSqrt/NkLog : comportement indéfini pour arguments négatifs
             *   - NkRsqrt : retourne Inf pour x=0, NaN pour x<0
             *   - Vérifier les préconditions avant appel pour éviter NaN/Inf
             *
             * @example
             * @code
             * // Distance euclidienne avec sqrt
             * float32 distance = nkentseu::math::NkSqrt(dx*dx + dy*dy + dz*dz);
             *
             * // Normalisation rapide avec rsqrt (1/sqrt(x))
             * float32 invLen = nkentseu::math::NkRsqrt(dotProduct);
             * vec3 normalized = velocity * invLen;
             *
             * // Calcul d'échelle exponentielle
             * float32 decay = nkentseu::math::NkExp(-time * decayRate);
             *
             * // Puissance entière optimisée (exponentiation par carrés)
             * float32 squared = nkentseu::math::NkPowInt(value, 2);  // Plus rapide que Pow(value, 2.0f)
             * @endcode
             */
            /** @{ */

            /**
             * @brief Racine carrée (float32)
             * @param x Valeur d'entrée (doit être >= 0)
             * @return √x
             */
            NKENTSEU_MATH_API float32 NkSqrt(float32 x) noexcept;

            /** @copydoc NkSqrt(float32) */
            NKENTSEU_MATH_API float64 NkSqrt(float64 x) noexcept;

            /**
             * @brief Inverse de la racine carrée (1/√x)
             * @param x Valeur d'entrée (doit être > 0)
             * @return 1/√x
             * @note
             *   - Utile pour normalisation de vecteurs sans division coûteuse
             *   - Peut être optimisé via instruction hardware RSQRTSS/RSQRTPS si disponible
             */
            NKENTSEU_MATH_API float32 NkRsqrt(float32 x) noexcept;

            /** @copydoc NkRsqrt(float32) */
            NKENTSEU_MATH_API float64 NkRsqrt(float64 x) noexcept;

            /**
             * @brief Racine cubique
             * @param x Valeur d'entrée
             * @return ∛x (supporte x négatif)
             */
            NKENTSEU_MATH_API float32 NkCbrt(float32 x) noexcept;

            /** @copydoc NkCbrt(float32) */
            NKENTSEU_MATH_API float64 NkCbrt(float64 x) noexcept;

            /**
             * @brief Exponentielle (e^x)
             * @param x Exposant
             * @return e^x
             */
            NKENTSEU_MATH_API float32 NkExp(float32 x) noexcept;

            /** @copydoc NkExp(float32) */
            NKENTSEU_MATH_API float64 NkExp(float64 x) noexcept;

            /**
             * @brief Logarithme naturel (ln(x))
             * @param x Valeur d'entrée (doit être > 0)
             * @return ln(x)
             */
            NKENTSEU_MATH_API float32 NkLog(float32 x) noexcept;

            /** @copydoc NkLog(float32) */
            NKENTSEU_MATH_API float64 NkLog(float64 x) noexcept;

            /**
             * @brief Logarithme base 10 (log₁₀(x))
             * @param x Valeur d'entrée (doit être > 0)
             * @return log₁₀(x) = ln(x) / ln(10)
             * @note Implémentation optimisée via NkLog et constante NK_LN10_*
             */
            NK_FORCE_INLINE
            float32 NkLog10(float32 x) noexcept {
                return NkLog(x) / NK_LN10_F;
            }

            /** @copydoc NkLog10(float32) */
            NK_FORCE_INLINE
            float64 NkLog10(float64 x) noexcept {
                return NkLog(x) / NK_LN10_D;
            }

            /**
             * @brief Logarithme base 2 (log₂(x))
             * @param x Valeur d'entrée (doit être > 0)
             * @return log₂(x) = ln(x) / ln(2)
             * @note Utile pour calculs de niveau de détail (LOD), arbres binaires, etc.
             */
            NK_FORCE_INLINE
            float32 NkLog2(float32 x) noexcept {
                return NkLog(x) / NK_LN2_F;
            }

            /** @copydoc NkLog2(float32) */
            NK_FORCE_INLINE
            float64 NkLog2(float64 x) noexcept {
                return NkLog(x) / NK_LN2_D;
            }

            /**
             * @brief Puissance réelle (x^y)
             * @param x Base
             * @param y Exposant réel
             * @return x^y = exp(y * ln(x))
             * @note
             *   - Plus lent que NkPowInt pour exposants entiers
             *   - Gère les cas spéciaux : x=0, y<0 → Inf, x<0, y non-entier → NaN
             */
            NKENTSEU_MATH_API float32 NkPow(float32 x, float32 y) noexcept;

            /** @copydoc NkPow(float32,float32) */
            NKENTSEU_MATH_API float64 NkPow(float64 x, float64 y) noexcept;

            /**
             * @brief Puissance entière optimisée (x^n)
             * @param x Base
             * @param n Exposant entier (peut être négatif)
             * @return x^n via exponentiation par carrés (O(log n))
             * @note
             *   - Plus rapide que NkPow(x, static_cast<float>(n)) pour n entier
             *   - Gère n négatif : retourne 1 / PowInt(x, -n)
             *   - Cas n=0 : retourne 1 même si x=0 (convention mathématique)
             *
             * @example
             * @code
             * // Calcul de polynômes ou séries
             * float32 x2 = nkentseu::math::NkPowInt(x, 2);   // x²
             * float32 x3 = nkentseu::math::NkPowInt(x, 3);   // x³
             * float32 invX = nkentseu::math::NkPowInt(x, -1); // 1/x
             * @endcode
             */
            NKENTSEU_MATH_API float32 NkPowInt(float32 x, int32 n) noexcept;

            /** @copydoc NkPowInt(float32,int32) */
            NKENTSEU_MATH_API float64 NkPowInt(float64 x, int32 n) noexcept;

            /** @} */ // End of PowerLogFunctions

            // ====================================================================
            // SECTION 13 : FONCTIONS TRIGONOMÉTRIQUES (EXPORTÉES)
            // ====================================================================
            // Sinus, cosinus, tangente et leurs inverses avec export.

            /**
             * @defgroup TrigFunctions Fonctions Trigonométriques
             * @brief Sinus, cosinus, tangente, arcus et fonctions associées
             * @ingroup MathCore
             *
             * Fonctions trigonométriques standards avec arguments en radians.
             * Toutes les fonctions gèrent correctement les valeurs hors [-π,π]
             * via réduction d'argument interne.
             *
             * @note
             *   - Arguments en radians : utiliser NkRadiansFromDegrees si nécessaire
             *   - NkTan : implémenté via Sin/Cos avec protection division par zéro
             *   - Fonctions arcus : retour dans [-π/2,π/2] pour Asin/Atan, [0,π] pour Acos
             *
             * @example
             * @code
             * // Calcul de position circulaire
             * float32 angle = nkentseu::math::NkRadiansFromDegrees(45.0f);
             * float32 x = radius * nkentseu::math::NkCos(angle);
             * float32 y = radius * nkentseu::math::NkSin(angle);
             *
             * // Calcul d'angle depuis vecteur (Atan2 gère tous les quadrants)
             * float32 angle = nkentseu::math::NkAtan2(dy, dx);
             * float32 angleDeg = nkentseu::math::NkDegreesFromRadians(angle);
             *
             * // Protection contre division par zéro dans Tan
             * float32 tanVal = nkentseu::math::NkTan(nkentseu::math::constants::kHalfPiF);
             * // tanVal == 0.0f (protection, pas Inf)
             * @endcode
             */
            /** @{ */

            /**
             * @brief Sinus (argument en radians)
             * @param x Angle en radians
             * @return sin(x) ∈ [-1, 1]
             */
            NKENTSEU_MATH_API float32 NkSin(float32 x) noexcept;

            /** @copydoc NkSin(float32) */
            NKENTSEU_MATH_API float64 NkSin(float64 x) noexcept;

            /**
             * @brief Cosinus (argument en radians)
             * @param x Angle en radians
             * @return cos(x) ∈ [-1, 1]
             */
            NKENTSEU_MATH_API float32 NkCos(float32 x) noexcept;

            /** @copydoc NkCos(float32) */
            NKENTSEU_MATH_API float64 NkCos(float64 x) noexcept;

            /**
             * @brief Tangente (argument en radians)
             * @param x Angle en radians
             * @return tan(x) = sin(x)/cos(x), ou 0 si cos(x) == 0
             * @note
             *   - Retourne 0 (pas Inf) quand cos(x) == 0 pour éviter propagation de NaN
             *   - Comportement défini pour tous les inputs, même multiples de π/2
             */
            NK_FORCE_INLINE
            float32 NkTan(float32 x) noexcept {
                const float32 c = NkCos(x);
                return c != 0.0f ? NkSin(x) / c : 0.0f;
            }

            /** @copydoc NkTan(float32) */
            NK_FORCE_INLINE
            float64 NkTan(float64 x) noexcept {
                const float64 c = NkCos(x);
                return c != 0.0 ? NkSin(x) / c : 0.0;
            }

            /**
             * @brief Arc tangente (retour en radians)
             * @param x Valeur d'entrée
             * @return atan(x) ∈ [-π/2, π/2]
             */
            NKENTSEU_MATH_API float32 NkAtan(float32 x) noexcept;

            /** @copydoc NkAtan(float32) */
            NKENTSEU_MATH_API float64 NkAtan(float64 x) noexcept;

            /**
             * @brief Arc tangente à deux arguments (quadrant-aware)
             * @param y Coordonnée Y
             * @param x Coordonnée X
             * @return atan2(y,x) ∈ [-π, π] (angle du vecteur (x,y))
             * @note
             *   - Gère correctement tous les quadrants et cas x=0
             *   - Essentiel pour conversion cartésien → polaire
             */
            NKENTSEU_MATH_API float32 NkAtan2(float32 y, float32 x) noexcept;

            /** @copydoc NkAtan2(float32,float32) */
            NKENTSEU_MATH_API float64 NkAtan2(float64 y, float64 x) noexcept;

            /**
             * @brief Arc sinus (retour en radians)
             * @param a Valeur d'entrée (doit être ∈ [-1, 1])
             * @return asin(a) ∈ [-π/2, π/2]
             * @note Comportement indéfini si |a| > 1 : clamer l'input si nécessaire
             */
            NKENTSEU_MATH_API float32 NkAsin(float32 a) noexcept;

            /** @copydoc NkAsin(float32) */
            NKENTSEU_MATH_API float64 NkAsin(float64 a) noexcept;

            /**
             * @brief Arc cosinus (retour en radians)
             * @param a Valeur d'entrée (doit être ∈ [-1, 1])
             * @return acos(a) ∈ [0, π]
             */
            NKENTSEU_MATH_API float32 NkAcos(float32 a) noexcept;

            /** @copydoc NkAcos(float32) */
            NKENTSEU_MATH_API float64 NkAcos(float64 a) noexcept;

            /** @} */ // End of TrigFunctions

            // ====================================================================
            // SECTION 14 : FONCTIONS HYPERBOLIQUES (EXPORTÉES)
            // ====================================================================
            // Sinus, cosinus, tangente hyperboliques avec export.

            /**
             * @defgroup HyperbolicFunctions Fonctions Hyperboliques
             * @brief Sinh, Cosh, Tanh et leurs inverses (si besoin futur)
             * @ingroup MathCore
             *
             * Fonctions hyperboliques utiles en physique relativiste,
             * interpolation avancée, et réseaux de neurones (tanh activation).
             *
             * @note
             *   - Définitions : sinh(x)=(e^x-e^-x)/2, cosh(x)=(e^x+e^-x)/2, tanh=sinh/cosh
             *   - Tanh borné [-1,1] : utile comme fonction d'activation ou saturation douce
             *
             * @example
             * @code
             * // Fonction d'activation tanh pour réseau de neurones
             * float32 activation = nkentseu::math::NkTanh(neuronInput);
             *
             * // Interpolation avec courbure hyperbolique
             * float32 eased = nkentseu::math::NkTanh(t * 3.0f) * 0.5f + 0.5f;
             * @endcode
             */
            /** @{ */

            /**
             * @brief Sinus hyperbolique
             * @param x Valeur d'entrée
             * @return sinh(x) = (e^x - e^-x) / 2
             */
            NKENTSEU_MATH_API float32 NkSinh(float32 x) noexcept;

            /** @copydoc NkSinh(float32) */
            NKENTSEU_MATH_API float64 NkSinh(float64 x) noexcept;

            /**
             * @brief Cosinus hyperbolique
             * @param x Valeur d'entrée
             * @return cosh(x) = (e^x + e^-x) / 2 (toujours >= 1)
             */
            NKENTSEU_MATH_API float32 NkCosh(float32 x) noexcept;

            /** @copydoc NkCosh(float32) */
            NKENTSEU_MATH_API float64 NkCosh(float64 x) noexcept;

            /**
             * @brief Tangente hyperbolique
             * @param x Valeur d'entrée
             * @return tanh(x) = sinh(x)/cosh(x) ∈ [-1, 1]
             * @note
             *   - Fonction sigmoïde centrée en 0, utile pour normalisation
             *   - Approche ±1 asymptotiquement pour |x| grand
             */
            NKENTSEU_MATH_API float32 NkTanh(float32 x) noexcept;

            /** @copydoc NkTanh(float32) */
            NKENTSEU_MATH_API float64 NkTanh(float64 x) noexcept;

            /** @} */ // End of HyperbolicFunctions

            // ====================================================================
            // SECTION 15 : UTILITAIRES FLOTTANTS (EXPORTÉS)
            // ====================================================================
            // Opérations bas-niveau sur la représentation des flottants avec export.

            /**
             * @defgroup FloatUtilityFunctions Utilitaires Flottants Bas-Niveau
             * @brief Opérations sur la représentation IEEE 754 des flottants
             * @ingroup MathUtilities
             *
             * Ces fonctions fournissent un accès aux opérations flottantes
             * de bas niveau : reste flottant, test de finitude, décomposition
             * mantisse/exposant, extraction partie entière/fractionnaire.
             *
             * @note
             *   - NkIsFinite : test bitwise pour NaN/Inf sans dépendance <cmath>
             *   - NkFrexp/NkLdexp : décomposition/recomposition mantisse×2^exp
             *   - NkModf : séparation partie entière/fractionnaire avec pointeur output
             *
             * @example
             * @code
             * // Vérification de validité avant calcul
             * if (!nkentseu::math::NkIsFinite(inputValue)) {
             *     NK_FOUNDATION_LOG_WARNING("Invalid float input");
             *     return defaultValue;
             * }
             *
             * // Décomposition pour affichage scientifique
             * int32 exponent;
             * float32 mantissa = nkentseu::math::NkFrexp(largeValue, &exponent);
             * printf("%.6f × 2^%d\n", mantissa, exponent);
             *
             * // Extraction partie entière pour indexing
             * float32 fractional;
             * int32 index = static_cast<int32>(nkentseu::math::NkModf(position, &fractional));
             * @endcode
             */
            /** @{ */

            /**
             * @brief Reste de division flottante (x - y*trunc(x/y))
             * @param x Dividende
             * @param y Diviseur
             * @return Reste de même signe que x, |result| < |y|
             * @note
             *   - Différent de l'opérateur % entier : gère les flottants
             *   - Utile pour wrapping de coordonnées, animation cyclique, etc.
             */
            NKENTSEU_MATH_API float32 NkFmod(float32 x, float32 y) noexcept;

            /** @copydoc NkFmod(float32,float32) */
            NKENTSEU_MATH_API float64 NkFmod(float64 x, float64 y) noexcept;

            /**
             * @brief Teste si un float32 est fini (ni NaN, ni Inf)
             * @param x Valeur à tester
             * @return true si x est un nombre fini normal/dénormé, false sinon
             * @note
             *   - Implémentation bitwise : vérifie que l'exposant != 0xFF
             *   - Plus rapide que std::isfinite sur certaines plateformes
             *   - Retourne false pour ±0.0 ? Non : ±0.0 est fini (exposant=0, mantisse=0)
             */
            NK_FORCE_INLINE
            nk_bool NkIsFinite(float32 x) noexcept {
                union {
                    float32 value;
                    uint32 bits;
                } data = { x };
                // Exposant = bits 30-23 : 0xFF = NaN ou Inf
                return (data.bits & 0x7F800000u) != 0x7F800000u;
            }

            /**
             * @brief Teste si un float64 est fini (ni NaN, ni Inf)
             * @param x Valeur à tester
             * @return true si x est un nombre fini, false sinon
             */
            NK_FORCE_INLINE
            nk_bool NkIsFinite(float64 x) noexcept {
                union {
                    float64 value;
                    uint64 bits;
                } data = { x };
                // Exposant = bits 62-52 : 0x7FF = NaN ou Inf
                return (data.bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
            }

            /**
             * @brief Décompose un flottant en mantisse normalisée et exposant
             * @param x Valeur à décomposer
             * @param exp Pointeur vers entier pour recevoir l'exposant
             * @return Mantisse m telle que x = m × 2^exp, avec m ∈ [0.5, 1) ou m=0
             * @note
             *   - Si x=0 : retourne 0 et *exp=0
             *   - Utile pour affichage scientifique, normalisation dynamique, etc.
             */
            NKENTSEU_MATH_API float32 NkFrexp(float32 x, int32* exp) noexcept;

            /** @copydoc NkFrexp(float32,int32*) */
            NKENTSEU_MATH_API float64 NkFrexp(float64 x, int32* exp) noexcept;

            /**
             * @brief Reconstruit un flottant depuis mantisse et exposant
             * @param x Mantisse (typiquement sortie de NkFrexp)
             * @param exp Exposant entier
             * @return x × 2^exp
             * @note
             *   - Inverse de NkFrexp : NkLdexp(NkFrexp(x,&e), e) ≈ x (à l'arrondi près)
             *   - Utile pour scaling dynamique de plage de valeurs
             */
            NKENTSEU_MATH_API float32 NkLdexp(float32 x, int32 exp) noexcept;

            /** @copydoc NkLdexp(float32,int32) */
            NKENTSEU_MATH_API float64 NkLdexp(float64 x, int32 exp) noexcept;

            /**
             * @brief Sépare partie entière et fractionnaire d'un flottant
             * @param x Valeur d'entrée
             * @param iptr Pointeur vers flottant pour recevoir la partie entière
             * @return Partie fractionnaire (même signe que x), avec *iptr = trunc(x)
             * @note
             *   - Retourne f tel que x = *iptr + f, avec |f| < 1 et sign(f) = sign(x)
             *   - Utile pour interpolation, génération de bruit, etc.
             */
            NKENTSEU_MATH_API float32 NkModf(float32 x, float32* iptr) noexcept;

            /** @copydoc NkModf(float32,float32*) */
            NKENTSEU_MATH_API float64 NkModf(float64 x, float64* iptr) noexcept;

            /**
             * @brief Structure de résultat pour division entière 64-bit avec reste
             * @struct Ldiv
             * @ingroup MathTypes
             * @note
             *   - quot : quotient de la division euclidienne
             *   - rem : reste (même signe que numerator, |rem| < |denominator|)
             */
            struct Ldiv {
                int64 quot;  ///< Quotient
                int64 rem;   ///< Reste
            };

            /** @brief Alias pour clarté : DivResult64 = Ldiv */
            using DivResult64 = Ldiv;

            /**
             * @brief Division entière 64-bit avec quotient et reste
             * @param numerator Numérateur
             * @param denominator Dénominateur (doit être != 0)
             * @return Structure avec quot = numerator/denominator, rem = numerator%denominator
             * @note
             *   - Comportement défini même pour INT64_MIN / -1 (overflow géré)
             *   - Plus sûr que l'opérateur / et % séparés pour éviter double calcul
             */
            NKENTSEU_MATH_API DivResult64 NkDivI64(int64 numerator, int64 denominator) noexcept;

            /**
             * @brief Alias sémantique pour NkDivI64 (plus clair pour usage modulo)
             * @param numerator Numérateur
             * @param denominator Dénominateur
             * @return Même résultat que NkDivI64
             * @note Préférer ce nom quand on s'intéresse surtout au reste (modulo)
             */
            NK_FORCE_INLINE
            DivResult64 NkDivMod64(int64 numerator, int64 denominator) noexcept {
                return NkDivI64(numerator, denominator);
            }

            /** @} */ // End of FloatUtilityFunctions

            // ====================================================================
            // SECTION 16 : UTILITAIRES ENTIERS ET BITS (EXPORTÉS)
            // ====================================================================
            // PGCD, PPCM, test puissance de 2, comptage de bits, etc. avec export.

            /**
             * @defgroup IntegerBitUtilities Utilitaires Entiers et Manipulation de Bits
             * @brief PGCD, PPCM, puissances de 2, comptage de bits (popcount, clz, ctz)
             * @ingroup MathUtilities
             *
             * Ces fonctions fournissent des opérations courantes sur entiers
             * et bits, avec optimisations hardware quand disponibles.
             *
             * @note
             *   - NkGcd/NkLcm : algorithmes d'Euclide et relation lcm(a,b)=a*b/gcd(a,b)
             *   - NkIsPowerOf2 : test bitwise x && !(x&(x-1))
             *   - NkNextPowerOf2 : arrondi à la puissance de 2 supérieure
             *   - NkClz/NkCtz/NkPopcount : wrappers vers NkBits ou intrinsics compiler
             *
             * @example
             * @code
             * // Simplification de fraction
             * uint64 g = nkentseu::math::NkGcd(numerator, denominator);
             * numerator /= g;
             * denominator /= g;
             *
             * // Alignement à puissance de 2 pour allocation
             * uint64 alignedSize = nkentseu::math::NkNextPowerOf2(requestedSize);
             *
             * // Comptage de bits actifs pour heuristique
             * uint32 activeFlags = nkentseu::math::NkPopcount(featureMask);
             *
             * // Position du bit de poids fort pour normalisation
             * uint32 msbPos = 31 - nkentseu::math::NkClz(value);  // Pour uint32
             * @endcode
             */
            /** @{ */

            /**
             * @brief Plus Grand Commun Diviseur (algorithme d'Euclide)
             * @param a Premier entier (>= 0)
             * @param b Deuxième entier (>= 0)
             * @return PGCD(a,b)
             * @note
             *   - Complexité O(log min(a,b))
             *   - Retourne 0 si a=b=0 (convention)
             */
            NKENTSEU_MATH_API uint64 NkGcd(uint64 a, uint64 b) noexcept;

            /**
             * @brief Plus Petit Commun Multiple via relation avec PGCD
             * @param a Premier entier (>= 0)
             * @param b Deuxième entier (>= 0)
             * @return PPCM(a,b) = (a / gcd(a,b)) * b
             * @note
             *   - Évite l'overflow de a*b en divisant d'abord par gcd
             *   - Retourne 0 si a=0 ou b=0 (convention)
             */
            NK_FORCE_INLINE
            uint64 NkLcm(uint64 a, uint64 b) noexcept {
                return (a / NkGcd(a, b)) * b;
            }

            /**
             * @brief Teste si un entier est une puissance de 2 exacte
             * @param x Valeur à tester (>= 0)
             * @return true si x > 0 et x a exactement un bit à 1
             * @note
             *   - Test bitwise classique : x && !(x & (x-1))
             *   - O(1) : une seule opération AND et comparaison
             *
             * @example
             * @code
             * // Validation de taille de texture (doit être puissance de 2)
             * if (!nkentseu::math::NkIsPowerOf2(textureWidth)) {
             *     NK_FOUNDATION_LOG_WARNING("Texture width must be power of 2");
             * }
             * @endcode
             */
            NK_FORCE_INLINE
            nk_bool NkIsPowerOf2(uint64 x) noexcept {
                return x && !(x & (x - 1));
            }

            /**
             * @brief Arrondit à la prochaine puissance de 2 supérieure ou égale
             * @param x Valeur d'entrée (>= 0)
             * @return Plus petite puissance de 2 >= x, ou 0 si x=0
             * @note
             *   - Algorithme bit-twiddling : propagation du bit de poids fort
             *   - Utile pour alignement mémoire, tailles de texture, hash tables, etc.
             *
             * @example
             * @code
             * // Allocation alignée pour performance cache
             * uint64 bufferSize = nkentseu::math::NkNextPowerOf2(requestedBytes);
             * void* buffer = NkAllocateAligned(bufferSize, bufferSize);
             * @endcode
             */
            NKENTSEU_MATH_API uint64 NkNextPowerOf2(uint64 x) noexcept;

            /**
             * @brief Compte les zéros en tête (Count Leading Zeros) pour uint32
             * @param x Valeur d'entrée
             * @return Nombre de zéros avant le premier bit à 1 (0-32), 32 si x=0
             * @note
             *   - Wrapper vers NkBits::CountLeadingZeros ou intrinsics compiler
             *   - Utile pour normalisation, calcul de log2, compression, etc.
             */
            NKENTSEU_MATH_API uint32 NkClz(uint32 x) noexcept;

            /** @copydoc NkClz(uint32) */
            NKENTSEU_MATH_API uint32 NkClz(uint64 x) noexcept;

            /**
             * @brief Compte les zéros en queue (Count Trailing Zeros) pour uint32
             * @param x Valeur d'entrée
             * @return Nombre de zéros après le dernier bit à 1 (0-32), 32 si x=0
             * @note
             *   - Utile pour trouver le premier bit actif, factorisation de puissances de 2
             */
            NKENTSEU_MATH_API uint32 NkCtz(uint32 x) noexcept;

            /** @copydoc NkCtz(uint32) */
            NKENTSEU_MATH_API uint32 NkCtz(uint64 x) noexcept;

            /**
             * @brief Compte les bits à 1 (Population Count) pour uint32
             * @param x Valeur d'entrée
             * @return Nombre de bits positionnés à 1 (0-32)
             * @note
             *   - Wrapper vers NkBits::CountBits ou instruction POPCNT si disponible
             *   - Utile pour bitmask analysis, compression, cryptographie, etc.
             */
            NKENTSEU_MATH_API uint32 NkPopcount(uint32 x) noexcept;

            /** @copydoc NkPopcount(uint32) */
            NKENTSEU_MATH_API uint32 NkPopcount(uint64 x) noexcept;

            /** @} */ // End of IntegerBitUtilities

            // ====================================================================
            // SECTION 17 : INTERPOLATION ET FONCTIONS DE LISSAGE
            // ====================================================================
            // Lerp, smoothstep, smootherstep pour animation et interpolation.

            /**
             * @defgroup InterpolationFunctions Fonctions d'Interpolation et Lissage
             * @brief Lerp, mix, smoothstep pour interpolation linéaire et easing
             * @ingroup MathUtilities
             *
             * Ces fonctions sont essentielles pour l'animation, l'interpolation
             * de valeurs, et les transitions progressives en graphisme et UI.
             *
             * @note
             *   - NkLerp/NkMix : interpolation linéaire a + (b-a)*t, t ∈ [0,1]
             *   - NkSmoothstep : interpolation cubique Hermite (dérivées nulles aux bords)
             *   - NkSmootherstep : interpolation quintique (dérivées 1 et 2 nulles aux bords)
             *
             * @example
             * @code
             * // Interpolation linéaire de couleur
             * vec3 color = nkentseu::math::NkLerp(colorStart, colorEnd, progress);
             *
             * // Animation avec easing smoothstep (démarrage/arrêt doux)
             * float32 easedT = nkentseu::math::NkSmoothstep(progress);
             * position = NkLerp(startPos, endPos, easedT);
             *
             * // Transition ultra-douce avec smootherstep (pour caméra, etc.)
             * float32 ultraSmooth = nkentseu::math::NkSmootherstep(progress);
             * cameraZoom = NkLerp(zoomMin, zoomMax, ultraSmooth);
             * @endcode
             */
            /** @{ */

            /**
             * @brief Interpolation linéaire entre deux valeurs
             * @tparam T Type supportant +, -, * avec float32
             * @param a Valeur de départ (t=0)
             * @param b Valeur d'arrivée (t=1)
             * @param t Facteur d'interpolation (typiquement ∈ [0,1], non clamped)
             * @return a + (b - a) * t
             * @note
             *   - Fonctionne pour scalaires, vecteurs, couleurs, etc. si opérateurs définis
             *   - t hors [0,1] : extrapolation (comportement défini, pas d'erreur)
             *   - Préférer NkClamp(t,0,1) avant appel si extrapolation non désirée
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkLerp(T a, T b, float32 t) noexcept {
                return a + (b - a) * t;
            }

            /**
             * @brief Alias sémantique pour NkLerp (terminologie graphique)
             * @tparam T Type supportant +, -, * avec float32
             * @param a Valeur de départ
             * @param b Valeur d'arrivée
             * @param t Facteur de mélange ∈ [0,1] typiquement
             * @return Même résultat que NkLerp(a, b, t)
             * @note "Mix" est la terminologie GLSL/shader : utile pour code graphique
             */
            template <typename T>
            NK_FORCE_INLINE
            T NkMix(T a, T b, float32 t) noexcept {
                return NkLerp(a, b, t);
            }

            /**
             * @brief Interpolation cubique Hermite (smoothstep)
             * @param t Valeur d'entrée (typiquement ∈ [0,1])
             * @return 3t² - 2t³ ∈ [0,1], avec dérivée nulle en t=0 et t=1
             * @note
             *   - Courbe en S : démarrage et arrêt progressifs
             *   - Dérivée première continue : pas de "cassure" de vitesse
             *   - Non clamped : valeurs hors [0,1] extrapolées (comportement défini)
             *
             * @example de courbe :
             * @code
             * t=0.0 → 0.0
             * t=0.5 → 0.5
             * t=1.0 → 1.0
             * Dérivée : 6t - 6t² → nulle en 0 et 1, max=1.5 en t=0.5
             * @endcode
             */
            NKENTSEU_MATH_API float32 NkSmoothstep(float32 t) noexcept;

            /** @copydoc NkSmoothstep(float32) */
            NKENTSEU_MATH_API float64 NkSmoothstep(float64 t) noexcept;

            /**
             * @brief Interpolation quintique (smootherstep)
             * @param t Valeur d'entrée (typiquement ∈ [0,1])
             * @return 6t⁵ - 15t⁴ + 10t³ ∈ [0,1], avec dérivées 1 et 2 nulles en 0 et 1
             * @note
             *   - Courbe encore plus douce : démarrage/arrêt très progressifs
             *   - Dérivées première et seconde continues : idéal pour caméra, animations premium
             *   - Plus coûteux que smoothstep (2 multiplications supplémentaires)
             *
             * @example de courbe :
             * @code
             * t=0.0 → 0.0  (dérivée=0, dérivée²=0)
             * t=0.5 → 0.5  (dérivée=1.875, dérivée²=0)
             * t=1.0 → 1.0  (dérivée=0, dérivée²=0)
             * @endcode
             */
            NKENTSEU_MATH_API float32 NkSmootherstep(float32 t) noexcept;

            /** @copydoc NkSmootherstep(float32) */
            NKENTSEU_MATH_API float64 NkSmootherstep(float64 t) noexcept;

            /** @} */ // End of InterpolationFunctions

            // ====================================================================
            // SECTION 18 : UTILITAIRES EXPONENTE/SIGNE FLOTTANTS (EXPORTÉS)
            // ====================================================================
            // Extraction d'exposant et copie de signe bitwise avec export.

            /**
             * @defgroup FloatExponentSignUtils Utilitaires Exposant et Signe Flottants
             * @brief Extraction d'exposant entier et copie de signe bitwise
             * @ingroup MathUtilities
             *
             * Ces fonctions fournissent un accès bas-niveau aux composantes
             * des flottants IEEE 754, utiles pour normalisation, génération
             * de nombres aléatoires, ou opérations bitwise sur flottants.
             *
             * @note
             *   - NkILogb : retourne l'exposant biaisé débiaisé (log2 de la magnitude)
             *   - NkCopysign : copie le bit de signe sans branche (bitwise)
             *
             * @example
             * @code
             * // Normalisation pour génération de bruit
             * int32 exponent;
             * float32 mantissa = nkentseu::math::NkFrexp(value, &exponent);
             * // mantissa ∈ [0.5, 1), value = mantissa * 2^exponent
             *
             * // Copie de signe pour correction directionnelle
             * float32 signedMagnitude = nkentseu::math::NkCopysign(absValue, direction);
             * // Équivalent à : absValue * sign(direction) mais sans branche
             * @endcode
             */
            /** @{ */

            /**
             * @brief Extrait l'exposant entier (log2 de la magnitude)
             * @param x Valeur flottante (doit être fini et != 0)
             * @return Exposant e tel que x = m × 2^e avec m ∈ [1,2)
             * @note
             *   - Équivalent à floor(log2(|x|)) pour x fini non-nul
             *   - Utile pour normalisation, scaling adaptatif, etc.
             */
            NKENTSEU_MATH_API int32 NkILogb(float32 x) noexcept;

            /**
             * @brief Copie le bit de signe d'un flottant à un autre (bitwise)
             * @param number Valeur dont garder la magnitude
             * @param sign Valeur dont copier le bit de signe
             * @return |number| avec le signe de sign
             * @note
             *   - Implémentation bitwise : pas de branche, rapide
             *   - Équivalent à : NkFabs(number) * (sign < 0 ? -1 : 1) mais sans branche
             *   - Gère correctement les cas spéciaux : NaN, Inf, ±0.0
             */
            NKENTSEU_MATH_API float32 NkCopysign(float32 number, float32 sign) noexcept;

            /** @} */ // End of FloatExponentSignUtils

            // ====================================================================
            // SECTION 19 : CONSTANTES LEGACY ADDITIONNELLES
            // ====================================================================
            // Constantes legacy supplémentaires pour compatibilité maximale.

            /**
             * @defgroup LegacyMathConstantsExtra Constantes Legacy Supplémentaires
             * @brief Constantes additionnelles pour code legacy très ancien
             * @ingroup MathCore
             * @deprecated Préférer constants::k* pour tout nouveau code
             *
             * Ces constantes sont conservées uniquement pour compatibilité
             * avec du code très ancien. Elles seront supprimées dans une
             * future version majeure sans avertissement.
             */
            /** @{ */

            /** @deprecated Utiliser constants::kPi */
            constexpr float64 NkPi = constants::kPi;

            /** @deprecated Utiliser constants::kHalfPi */
            constexpr float64 NkPis2 = constants::kHalfPi;

            /** @deprecated Utiliser constants::kTau */
            constexpr float64 NkPi2 = constants::kTau;

            /** @deprecated Utiliser constants::kSqrt2 */
            constexpr float64 NkSqrt2 = constants::kSqrt2;

            /** @deprecated Utiliser constants::kSqrt3 */
            constexpr float64 NkSqrt3 = constants::kSqrt3;

            /** @deprecated Utiliser DEG_TO_RAD */
            constexpr float64 NkDEG2RAD = constants::kPi / 180.0;

            /** @deprecated Utiliser RAD_TO_DEG */
            constexpr float64 NkRAD2DEG = 180.0 / constants::kPi;

            /** @deprecated Constante magique legacy (à documenter/remplacer) */
            constexpr float64 NkPuissanceApprox = 8192.0;

            /** @deprecated Utiliser constants::kCompareAbsEpsilonD */
            constexpr float64 NkEpsilon = constants::kCompareAbsEpsilonD;

            /** @deprecated Utiliser constants::kCompareAbsEpsilonD */
            constexpr float64 NkVectorEpsilon = constants::kCompareAbsEpsilonD;

            /** @deprecated Utiliser constants::kCompareAbsEpsilonD */
            constexpr float64 NkMatrixEpsilon = constants::kCompareAbsEpsilonD;

            /** @deprecated Utiliser constants::kCompareAbsEpsilonD */
            constexpr float64 NkQuatEpsilon = constants::kCompareAbsEpsilonD;

            /** @} */ // End of LegacyMathConstantsExtra

            // ====================================================================
            // SECTION 20 : ENUM NkPrimitiveType
            // ====================================================================
            // Énumération pour identification de type primitif (réflexion, sérialisation).

            /**
             * @defgroup MathTypeTags Tags de Types Primitifs
             * @brief Énumération pour identification de type primitif
             * @ingroup MathReflection
             *
             * Cette énumération permet d'identifier le type primitif d'une
             * valeur à runtime, utile pour la réflexion, la sérialisation,
             * ou les systèmes de scripting intégrés.
             *
             * @note
             *   - N'inclut pas les types composés (vecteurs, matrices, etc.)
             *   - Utiliser avec nkentseu::NkVariant ou système de réflexion pour typage dynamique
             *
             * @example
             * @code
             * // Sérialisation générique basée sur le type primitif
             * void SerializePrimitive(const void* value, nkentseu::math::NkPrimitiveType type) {
             *     switch (type) {
             *         case nkentseu::math::NkPrimitiveType::NK_FLOAT:
             *             WriteFloat(*static_cast<const float32*>(value));
             *             break;
             *         case nkentseu::math::NkPrimitiveType::NK_DOUBLE:
             *             WriteDouble(*static_cast<const float64*>(value));
             *             break;
             *         // ... autres cas ...
             *     }
             * }
             * @endcode
             */
            /** @{ */

            /**
             * @brief Énumération des types primitifs supportés
             * @enum NkPrimitiveType
             * @ingroup MathTypeTags
             *
             * Liste exhaustive des types primitifs C/C++ supportés par
             * les utilitaires mathématiques du framework.
             */
            enum class NkPrimitiveType {
                NK_CHAR,              ///< char (signedness platform-dependent)
                NK_SIGNED_CHAR,       ///< signed char
                NK_UNSIGNED_CHAR,     ///< unsigned char
                NK_SHORT,             ///< short
                NK_UNSIGNED_SHORT,    ///< unsigned short
                NK_INT,               ///< int
                NK_UNSIGNED_INT,      ///< unsigned int
                NK_LONG,              ///< long
                NK_UNSIGNED_LONG,     ///< unsigned long
                NK_LONG_LONG,         ///< long long
                NK_UNSIGNED_LONG_LONG,///< unsigned long long
                NK_FLOAT,             ///< float (float32)
                NK_DOUBLE,            ///< double (float64)
                NK_LONG_DOUBLE,       ///< long double (platform-dependent)
                NK_BOOL               ///< bool
            };

            /** @} */ // End of MathTypeTags

        } // namespace math

    } // namespace nkentseu

#endif // NKENTSEU_MATH_NKFUNCTIONS_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKFUNCTIONS.H
// =============================================================================
// Ce fichier fournit l'API mathématique scalaire de base du framework.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Calculs géométriques de base avec constantes et trigonométrie
// -----------------------------------------------------------------------------
/*
    #include "NKMath/NkFunctions.h"

    void CalculateProjectileTrajectory(
        float32 speed,
        float32 angleDeg,
        float32* outRange,
        float32* outMaxHeight
    ) {
        using namespace nkentseu::math;

        // Conversion degrés → radians
        float32 angleRad = NkRadiansFromDegrees(angleDeg);

        // Décomposition vitesse initiale
        float32 vx = speed * NkCos(angleRad);
        float32 vy = speed * NkSin(angleRad);

        // Physique simplifiée (sans résistance de l'air)
        float32 gravity = constants::kGravity;
        float32 timeToPeak = vy / gravity;
        float32 totalTime = 2.0f * timeToPeak;

        // Résultats
        *outRange = vx * totalTime;
        *outMaxHeight = (vy * vy) / (2.0f * gravity);
    }

    // Usage
    float32 range, height;
    CalculateProjectileTrajectory(50.0f, 45.0f, &range, &height);
    // range ≈ 255.1m, height ≈ 63.8m
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Interpolation et animation avec easing functions
// -----------------------------------------------------------------------------
/*
    #include "NKMath/NkFunctions.h"

    class AnimatedValue {
    public:
        void StartAnimation(float32 start, float32 end, float32 duration) {
            m_start = start;
            m_end = end;
            m_duration = duration;
            m_elapsed = 0.0f;
            m_active = true;
        }

        float32 Update(float32 deltaTime) {
            if (!m_active) {
                return m_end;
            }

            using namespace nkentseu::math;

            m_elapsed = NkMin(m_elapsed + deltaTime, m_duration);
            float32 t = m_elapsed / m_duration;  // t ∈ [0,1]

            // Easing avec smoothstep pour démarrage/arrêt doux
            float32 easedT = NkSmoothstep(t);

            // Interpolation linéaire avec facteur eased
            return NkLerp(m_start, m_end, easedT);
        }

        bool IsActive() const { return m_active && m_elapsed < m_duration; }

    private:
        float32 m_start, m_end, m_duration, m_elapsed;
        bool m_active = false;
    };

    // Usage dans une boucle de jeu
    AnimatedValue opacity;
    opacity.StartAnimation(0.0f, 1.0f, 2.0f);  // Fade-in sur 2 secondes

    while (opacity.IsActive()) {
        float32 currentOpacity = opacity.Update(frameDeltaTime);
        RenderSpriteWithOpacity(sprite, currentOpacity);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Comparaisons flottantes robustes pour tests unitaires
// -----------------------------------------------------------------------------
/*
    #include "NKMath/NkFunctions.h"
    #include <cassert>

    // Macro de test avec comparaison flottante tolérante
    #define ASSERT_FLOAT_NEAR(a, b) \
        assert(nkentseu::math::NkNearlyEqual((a), (b)))

    void TestTrigonometricIdentities() {
        using namespace nkentseu::math;

        // sin²(x) + cos²(x) = 1 (avec tolérance flottante)
        for (float32 angle = 0.0f; angle < constants::kTauF; angle += 0.1f) {
            float32 s = NkSin(angle);
            float32 c = NkCos(angle);
            ASSERT_FLOAT_NEAR(s * s + c * c, 1.0f);
        }

        // tan(x) = sin(x)/cos(x) (avec protection cos=0)
        float32 x = constants::kQuarterPiF;  // π/4
        float32 tanVal = NkTan(x);
        float32 sinOverCos = NkSin(x) / NkCos(x);
        ASSERT_FLOAT_NEAR(tanVal, sinOverCos);

        // atan2 pour tous les quadrants
        ASSERT_FLOAT_NEAR(NkAtan2(1.0f, 0.0f), constants::kHalfPiF);   // +π/2
        ASSERT_FLOAT_NEAR(NkAtan2(-1.0f, 0.0f), -constants::kHalfPiF);  // -π/2
        ASSERT_FLOAT_NEAR(NkAtan2(0.0f, -1.0f), constants::kPiF);       // π
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Optimisations bitwise pour performance critique
// -----------------------------------------------------------------------------
/*
    #include "NKMath/NkFunctions.h"

    // Allocation de texture avec taille puissance de 2
    void* AllocateTexture(uint32 width, uint32 height) {
        using namespace nkentseu::math;

        // Vérification et correction de taille
        if (!NkIsPowerOf2(width)) {
            width = NkNextPowerOf2(width);
            NK_FOUNDATION_LOG_WARNING("Texture width rounded up to %u", width);
        }
        if (!NkIsPowerOf2(height)) {
            height = NkNextPowerOf2(height);
            NK_FOUNDATION_LOG_WARNING("Texture height rounded up to %u", height);
        }

        // Allocation avec alignement sur taille (optimisation cache)
        return NkAllocateAligned(width * height * 4, width);  // 4 bytes/pixel RGBA
    }

    // Comptage de features actives pour LOD décision
    uint32 CalculateDetailLevel(uint64 featureMask) {
        using namespace nkentseu::math;

        // Nombre de features actives
        uint32 activeCount = NkPopcount(featureMask);

        // Position du bit de poids fort pour heuristique supplémentaire
        uint32 highestFeature = 63 - NkClz(featureMask);  // Pour uint64

        // Décision de niveau de détail
        if (activeCount < 4 && highestFeature < 10) {
            return 0;  // Low detail
        } else if (activeCount < 16 && highestFeature < 20) {
            return 1;  // Medium detail
        } else {
            return 2;  // High detail
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Génération de nombres aléatoires normalisés
// -----------------------------------------------------------------------------
/*
    #include "NKMath/NkFunctions.h"

    // Générateur simple de flottants [0,1) via decomposition bitwise
    class SimpleFloatRng {
    public:
        explicit SimpleFloatRng(uint64 seed) : m_state(seed) {}

        // Retourne float32 ∈ [0,1) avec distribution uniforme
        float32 NextFloat() {
            using namespace nkentseu::math;

            // Génération d'entier pseudo-aléatoire simple (LCG)
            m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;

            // Extraction des 23 bits de mantisse + bit implicite pour [1,2)
            uint32 mantissa = static_cast<uint32>(m_state >> 41) & 0x007FFFFF;
            uint32 bits = 0x3F800000u | mantissa;  // Exposant 127 = 1.xxxx

            // Conversion bitwise float et soustraction de 1.0 pour [0,1)
            union { uint32 i; float32 f; } converter = { bits };
            return converter.f - 1.0f;
        }

        // Float dans [min, max)
        float32 NextFloatRange(float32 minVal, float32 maxVal) {
            return NkLerp(minVal, maxVal, NextFloat());
        }

        // Entier dans [0, max) via rejection sampling
        uint32 NextUInt(uint32 max) {
            if (NkIsPowerOf2(max)) {
                // Cas puissance de 2 : masque bitwise rapide
                return static_cast<uint32>(m_state) & (max - 1);
            } else {
                // Rejection sampling pour distribution uniforme
                uint32 limit = 0xFFFFFFFFu - (0xFFFFFFFFu % max);
                uint32 value;
                do {
                    value = static_cast<uint32>(m_state);
                    m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
                } while (value >= limit);
                return value % max;
            }
        }

    private:
        uint64 m_state;
    };

    // Usage
    SimpleFloatRng rng(12345);
    for (int i = 0; i < 1000; ++i) {
        float32 randomValue = rng.NextFloat();  // ∈ [0,1)
        float32 randomAngle = rng.NextFloatRange(0.0f, nkentseu::math::constants::kTauF);
        // ... utilisation ...
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================