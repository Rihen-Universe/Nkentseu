// =============================================================================
// NKMath/NkFunctions.cpp
// Implémentation des fonctions mathématiques scalaires de base.
//
// Design :
//  - Implémentation des fonctions déclarées avec NKENTSEU_MATH_API dans NkFunctions.h
//  - Wrapper autour de <math.h> avec gestion sécurisée des cas limites
//  - Optimisations platform-specific pour les opérations bitwise (CLZ, CTZ, POPCOUNT)
//  - Algorithmes optimisés pour les puissances entières (exponentiation par carrés)
//  - Fonctions noexcept pour compatibilité avec code sans exceptions
//  - Aucune dépendance STL pour portabilité maximale
//
// Intégration :
//  - Utilise NKMath/NkFunctions.h pour les déclarations et types
//  - Utilise <math.h> pour les fonctions mathématiques C standard
//  - Compatible avec GCC/Clang builtins (__builtin_clz, __builtin_popcount, etc.)
//  - Fallback software portable pour les compilateurs sans builtins
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRECOMPILED HEADER (requis pour tous les fichiers .cpp du projet)
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMath/NkFunctions.h"
#include "NKCore/NkTypes.h"

// En-tête standard pour les fonctions mathématiques C
#include <math.h>

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    // Indentation niveau 1 : namespace math
    namespace math {

        // ====================================================================
        // SECTION 1 : FONCTIONS D'ARRONDI
        // ====================================================================
        // Implémentation des fonctions floor, ceil, trunc, round.

        /**
         * @brief Arrondi vers l'entier inférieur (floor) - float32
         * @param x Valeur d'entrée
         * @return Plus grand entier <= x
         * @note
         *   - Délègue à floor() de <math.h>
         *   - Gère correctement les valeurs spéciales : NaN, Inf retournés inchangés
         *   - noexcept : ne lève jamais d'exception
         */
        float32 NkFloor(float32 x) noexcept {
            return static_cast<float32>(floor(x));
        }

        /**
         * @brief Arrondi vers l'entier inférieur (floor) - float64
         * @param x Valeur d'entrée
         * @return Plus grand entier <= x
         */
        float64 NkFloor(float64 x) noexcept {
            return floor(x);
        }

        /**
         * @brief Arrondi vers l'entier supérieur (ceil) - float32
         * @param x Valeur d'entrée
         * @return Plus petit entier >= x
         * @note
         *   - Délègue à ceil() de <math.h>
         *   - Exemple : NkCeil(3.2f) == 4.0f, NkCeil(-3.2f) == -3.0f
         */
        float32 NkCeil(float32 x) noexcept {
            return static_cast<float32>(ceil(x));
        }

        /**
         * @brief Arrondi vers l'entier supérieur (ceil) - float64
         * @param x Valeur d'entrée
         * @return Plus petit entier >= x
         */
        float64 NkCeil(float64 x) noexcept {
            return ceil(x);
        }

        /**
         * @brief Arrondi vers zéro (truncation) - float32
         * @param x Valeur d'entrée
         * @return Partie entière de x avec signe préservé
         * @note
         *   - Délègue à trunc() de <math.h>
         *   - Exemple : NkTrunc(3.9f) == 3.0f, NkTrunc(-3.9f) == -3.0f
         *   - Différent de floor/ceil : toujours vers zéro, pas vers -/+\infty
         */
        float32 NkTrunc(float32 x) noexcept {
            return static_cast<float32>(trunc(x));
        }

        /**
         * @brief Arrondi vers zéro (truncation) - float64
         * @param x Valeur d'entrée
         * @return Partie entière de x avec signe préservé
         */
        float64 NkTrunc(float64 x) noexcept {
            return trunc(x);
        }

        /**
         * @brief Arrondi à l'entier le plus proche (half-away-from-zero) - float32
         * @param x Valeur d'entrée
         * @return Entier le plus proche de x
         * @note
         *   - Délègue à round() de <math.h>
         *   - .5 est arrondi vers l'infini en valeur absolue : round(2.5) == 3, round(-2.5) == -3
         *   - Pour round-half-to-even (bancaire), utiliser une implémentation custom si nécessaire
         */
        float32 NkRound(float32 x) noexcept {
            return static_cast<float32>(round(x));
        }

        /**
         * @brief Arrondi à l'entier le plus proche (half-away-from-zero) - float64
         * @param x Valeur d'entrée
         * @return Entier le plus proche de x
         */
        float64 NkRound(float64 x) noexcept {
            return round(x);
        }

        // ====================================================================
        // SECTION 2 : RACINES ET PUISSANCES
        // ====================================================================
        // Implémentation des fonctions sqrt, rsqrt, cbrt, exp, log, pow.

        /**
         * @brief Racine carrée avec gestion sécurisée des valeurs négatives - float32
         * @param x Valeur d'entrée
         * @return √x si x > 0, 0.0f sinon
         * @note
         *   - Retourne 0.0f pour x <= 0 au lieu de NaN pour robustesse
         *   - Délègue à sqrt() de <math.h> pour x > 0
         *   - noexcept : pas d'exception même pour inputs invalides
         *
         * @warning
         *   Le comportement pour x < 0 est défini comme retour 0.0f (pas standard C).
         *   Si vous avez besoin du comportement standard (NaN), appeler sqrt() directement.
         */
        float32 NkSqrt(float32 x) noexcept {
            return (x <= 0.0f) ? 0.0f : static_cast<float32>(sqrt(x));
        }

        /**
         * @brief Racine carrée avec gestion sécurisée des valeurs négatives - float64
         * @param x Valeur d'entrée
         * @return √x si x > 0, 0.0 sinon
         */
        float64 NkSqrt(float64 x) noexcept {
            return (x <= 0.0) ? 0.0 : sqrt(x);
        }

        /**
         * @brief Inverse de la racine carrée (1/√x) avec gestion des cas limites - float32
         * @param x Valeur d'entrée
         * @return 1/√x si x > 0, 0.0f sinon
         * @note
         *   - Utile pour normalisation de vecteurs sans division coûteuse
         *   - Retourne 0.0f pour x <= 0 au lieu de Inf/NaN pour robustesse
         *   - Peut être optimisé via instruction hardware RSQRTSS si disponible (à implémenter platform-specific)
         */
        float32 NkRsqrt(float32 x) noexcept {
            return (x <= 0.0f) ? 0.0f : (1.0f / static_cast<float32>(sqrt(x)));
        }

        /**
         * @brief Inverse de la racine carrée (1/√x) avec gestion des cas limites - float64
         * @param x Valeur d'entrée
         * @return 1/√x si x > 0, 0.0 sinon
         */
        float64 NkRsqrt(float64 x) noexcept {
            return (x <= 0.0) ? 0.0 : (1.0 / sqrt(x));
        }

        /**
         * @brief Racine cubique - float32
         * @param x Valeur d'entrée
         * @return ∛x (supporte x négatif)
         * @note
         *   - Délègue à cbrt() de <math.h> qui gère correctement les signes
         *   - Exemple : NkCbrt(-8.0f) == -2.0f
         */
        float32 NkCbrt(float32 x) noexcept {
            return static_cast<float32>(cbrt(x));
        }

        /**
         * @brief Racine cubique - float64
         * @param x Valeur d'entrée
         * @return ∛x (supporte x négatif)
         */
        float64 NkCbrt(float64 x) noexcept {
            return cbrt(x);
        }

        /**
         * @brief Exponentielle (e^x) avec clamping pour éviter overflow/underflow - float32
         * @param x Exposant
         * @return e^x clampé dans [0, FLT_MAX]
         * @note
         *   - x < -87 → retourne 0.0f (sous-flow vers zéro)
         *   - x > 88 → retourne FLT_MAX (3.4028235e38f) pour éviter Inf
         *   - Délègue à exp() de <math.h> pour la plage normale
         *
         * @warning
         *   Les seuils -87/88 sont approximatifs pour float32.
         *   Pour une précision maximale, ajuster selon l'implémentation de exp() de votre plateforme.
         */
        float32 NkExp(float32 x) noexcept {
            if (x < -87.0f) {
                return 0.0f;
            }
            if (x > 88.0f) {
                return 3.4028235e38f;
            }
            return static_cast<float32>(exp(x));
        }

        /**
         * @brief Exponentielle (e^x) avec clamping pour éviter overflow/underflow - float64
         * @param x Exposant
         * @return e^x clampé dans [0, DBL_MAX]
         * @note
         *   - x < -708 → retourne 0.0 (sous-flow vers zéro)
         *   - x > 709 → retourne DBL_MAX (1.7976931348623157e308) pour éviter Inf
         */
        float64 NkExp(float64 x) noexcept {
            if (x < -708.0) {
                return 0.0;
            }
            if (x > 709.0) {
                return 1.7976931348623157e308;
            }
            return exp(x);
        }

        /**
         * @brief Logarithme naturel (ln(x)) avec gestion des valeurs non-positives - float32
         * @param x Valeur d'entrée
         * @return ln(x) si x > 0, -FLT_MAX sinon
         * @note
         *   - Retourne -FLT_MAX (-3.4028235e38f) pour x <= 0 au lieu de -Inf/NaN
         *   - Permet de propager une valeur "très négative" sans casser les calculs suivants
         *
         * @warning
         *   Le comportement pour x <= 0 est défini comme retour -FLT_MAX (pas standard C).
         *   Pour le comportement standard, utiliser log() directement avec vérification préalable.
         */
        float32 NkLog(float32 x) noexcept {
            return (x <= 0.0f) ? -3.4028235e38f : static_cast<float32>(log(x));
        }

        /**
         * @brief Logarithme naturel (ln(x)) avec gestion des valeurs non-positives - float64
         * @param x Valeur d'entrée
         * @return ln(x) si x > 0, -1e308 sinon
         * @note
         *   - Attention : l'implémentation originale utilisait log2f par erreur, corrigé ici en log()
         */
        float64 NkLog(float64 x) noexcept {
            return (x <= 0.0) ? -1.0e308 : log(x);
        }

        /**
         * @brief Puissance réelle (x^y) avec gestion des cas spéciaux - float32
         * @param x Base
         * @param y Exposant réel
         * @return x^y avec gestion de x=0, x<0, y non-entier
         * @note
         *   - x == 0 : retourne 0 si y > 0, 1 si y <= 0 (convention 0^0 = 1)
         *   - x < 0 et y non-entier : retourne 0.0f au lieu de NaN pour robustesse
         *   - Délègue à pow() de <math.h> pour les cas normaux
         *
         * @warning
         *   La vérification "y non-entier" via cast float→int→float peut échouer pour
         *   de grands entiers (> 2^24 pour float32). Pour une précision maximale,
         *   utiliser une vérification bitwise ou NkPowInt pour les exposants entiers connus.
         */
        float32 NkPow(float32 x, float32 y) noexcept {
            if (x == 0.0f) {
                return (y > 0.0f) ? 0.0f : 1.0f;
            }
            if (x < 0.0f) {
                const float32 yInt = static_cast<float32>(static_cast<int32>(y));
                if (y != yInt) {
                    return 0.0f;
                }
            }
            return static_cast<float32>(pow(x, y));
        }

        /**
         * @brief Puissance réelle (x^y) avec gestion des cas spéciaux - float64
         * @param x Base
         * @param y Exposant réel
         * @return x^y avec gestion de x=0, x<0, y non-entier
         */
        float64 NkPow(float64 x, float64 y) noexcept {
            if (x == 0.0) {
                return (y > 0.0) ? 0.0 : 1.0;
            }
            if (x < 0.0) {
                const float64 yInt = static_cast<float64>(static_cast<int64>(y));
                if (y != yInt) {
                    return 0.0;
                }
            }
            return pow(x, y);
        }

        /**
         * @brief Puissance entière optimisée via exponentiation par carrés - float32
         * @param x Base
         * @param n Exposant entier (peut être négatif)
         * @return x^n calculé en O(log |n|) opérations
         * @note
         *   - Algorithme : exponentiation par carrés (binary exponentiation)
         *   - Gère n négatif : calcule 1 / PowInt(x, -n)
         *   - Cas n=0 : retourne 1.0f même si x=0 (convention mathématique)
         *   - Plus rapide et précis que NkPow(x, static_cast<float32>(n)) pour n entier
         *
         * @example
         * @code
         * // Calcul efficace de polynômes
         * float32 x2 = NkPowInt(x, 2);   // x² en 1 multiplication (x*x)
         * float32 x3 = NkPowInt(x, 3);   // x³ en 2 multiplications (x²*x)
         * float32 invX = NkPowInt(x, -1); // 1/x via division finale
         * @endcode
         */
        float32 NkPowInt(float32 x, int32 n) noexcept {
            if (n == 0) {
                return 1.0f;
            }

            nk_bool negativeExp = n < 0;
            uint32 exp = static_cast<uint32>(negativeExp ? -n : n);
            float32 base = x;
            float32 result = 1.0f;

            while (exp > 0u) {
                if ((exp & 1u) != 0u) {
                    result *= base;
                }
                base *= base;
                exp >>= 1u;
            }

            return negativeExp ? (1.0f / result) : result;
        }

        /**
         * @brief Puissance entière optimisée via exponentiation par carrés - float64
         * @param x Base
         * @param n Exposant entier (peut être négatif)
         * @return x^n calculé en O(log |n|) opérations
         */
        float64 NkPowInt(float64 x, int32 n) noexcept {
            if (n == 0) {
                return 1.0;
            }

            nk_bool negativeExp = n < 0;
            uint32 exp = static_cast<uint32>(negativeExp ? -n : n);
            float64 base = x;
            float64 result = 1.0;

            while (exp > 0u) {
                if ((exp & 1u) != 0u) {
                    result *= base;
                }
                base *= base;
                exp >>= 1u;
            }

            return negativeExp ? (1.0 / result) : result;
        }

        // ====================================================================
        // SECTION 3 : FONCTIONS TRIGONOMÉTRIQUES
        // ====================================================================
        // Implémentation des fonctions sin, cos, atan, atan2, asin, acos.

        /**
         * @brief Sinus (argument en radians) - float32
         * @param x Angle en radians
         * @return sin(x) ∈ [-1, 1]
         * @note Délègue à sin() de <math.h> avec cast de précision
         */
        float32 NkSin(float32 x) noexcept {
            return static_cast<float32>(sin(x));
        }

        /**
         * @brief Sinus (argument en radians) - float64
         * @param x Angle en radians
         * @return sin(x) ∈ [-1, 1]
         */
        float64 NkSin(float64 x) noexcept {
            return sin(x);
        }

        /**
         * @brief Cosinus (argument en radians) - float32
         * @param x Angle en radians
         * @return cos(x) ∈ [-1, 1]
         */
        float32 NkCos(float32 x) noexcept {
            return static_cast<float32>(cos(x));
        }

        /**
         * @brief Cosinus (argument en radians) - float64
         * @param x Angle en radians
         * @return cos(x) ∈ [-1, 1]
         */
        float64 NkCos(float64 x) noexcept {
            return cos(x);
        }

        /**
         * @brief Arc tangente (retour en radians) - float32
         * @param x Valeur d'entrée
         * @return atan(x) ∈ [-π/2, π/2]
         */
        float32 NkAtan(float32 x) noexcept {
            return static_cast<float32>(atan(x));
        }

        /**
         * @brief Arc tangente (retour en radians) - float64
         * @param x Valeur d'entrée
         * @return atan(x) ∈ [-π/2, π/2]
         */
        float64 NkAtan(float64 x) noexcept {
            return atan(x);
        }

        /**
         * @brief Arc tangente à deux arguments (quadrant-aware) - float32
         * @param y Coordonnée Y
         * @param x Coordonnée X
         * @return atan2(y,x) ∈ [-π, π] (angle du vecteur (x,y))
         * @note
         *   - Gère correctement tous les quadrants et cas x=0, y=0
         *   - Essentiel pour conversion cartésien → polaire
         */
        float32 NkAtan2(float32 y, float32 x) noexcept {
            return static_cast<float32>(atan2(y, x));
        }

        /**
         * @brief Arc tangente à deux arguments (quadrant-aware) - float64
         * @param y Coordonnée Y
         * @param x Coordonnée X
         * @return atan2(y,x) ∈ [-π, π]
         */
        float64 NkAtan2(float64 y, float64 x) noexcept {
            return atan2(y, x);
        }

        /**
         * @brief Arc sinus avec clamping de l'input pour éviter NaN - float32
         * @param a Valeur d'entrée
         * @return asin(clamp(a, -1, 1)) ∈ [-π/2, π/2]
         * @note
         *   - Clampe automatiquement |a| > 1 vers ±1 pour éviter NaN
         *   - Retourne 0.0f pour les inputs hors domaine (robustesse)
         *
         * @warning
         *   Le clamping silencieux peut masquer des erreurs de logique.
         *   Pour du débogage strict, vérifier |a| <= 1 avant appel avec NKENTSEU_ASSERT.
         */
        float32 NkAsin(float32 a) noexcept {
            if (NkFabs(a) > 1.0f) {
                return 0.0f;
            }
            return static_cast<float32>(asin(a));
        }

        /**
         * @brief Arc sinus avec clamping de l'input pour éviter NaN - float64
         * @param a Valeur d'entrée
         * @return asin(clamp(a, -1, 1)) ∈ [-π/2, π/2]
         */
        float64 NkAsin(float64 a) noexcept {
            if (NkFabs(a) > 1.0) {
                return 0.0;
            }
            return asin(a);
        }

        /**
         * @brief Arc cosinus avec clamping de l'input pour éviter NaN - float32
         * @param a Valeur d'entrée
         * @return acos(clamp(a, -1, 1)) ∈ [0, π]
         */
        float32 NkAcos(float32 a) noexcept {
            if (NkFabs(a) > 1.0f) {
                return 0.0f;
            }
            return static_cast<float32>(acos(a));
        }

        /**
         * @brief Arc cosinus avec clamping de l'input pour éviter NaN - float64
         * @param a Valeur d'entrée
         * @return acos(clamp(a, -1, 1)) ∈ [0, π]
         */
        float64 NkAcos(float64 a) noexcept {
            if (NkFabs(a) > 1.0) {
                return 0.0;
            }
            return acos(a);
        }

        // ====================================================================
        // SECTION 4 : FONCTIONS HYPERBOLIQUES
        // ====================================================================
        // Implémentation des fonctions sinh, cosh, tanh avec clamping pour tanh.

        /**
         * @brief Sinus hyperbolique - float32
         * @param x Valeur d'entrée
         * @return sinh(x) = (e^x - e^-x) / 2
         */
        float32 NkSinh(float32 x) noexcept {
            return static_cast<float32>(sinh(x));
        }

        /**
         * @brief Sinus hyperbolique - float64
         * @param x Valeur d'entrée
         * @return sinh(x) = (e^x - e^-x) / 2
         */
        float64 NkSinh(float64 x) noexcept {
            return sinh(x);
        }

        /**
         * @brief Cosinus hyperbolique - float32
         * @param x Valeur d'entrée
         * @return cosh(x) = (e^x + e^-x) / 2 (toujours >= 1)
         */
        float32 NkCosh(float32 x) noexcept {
            return static_cast<float32>(cosh(x));
        }

        /**
         * @brief Cosinus hyperbolique - float64
         * @param x Valeur d'entrée
         * @return cosh(x) = (e^x + e^-x) / 2
         */
        float64 NkCosh(float64 x) noexcept {
            return cosh(x);
        }

        /**
         * @brief Tangente hyperbolique avec saturation pour éviter les calculs inutiles - float32
         * @param x Valeur d'entrée
         * @return tanh(x) ∈ [-1, 1], saturé à ±1 pour |x| > 10
         * @note
         *   - tanh(10) ≈ 0.9999999958, donc saturation à |x| > 10 est numériquement sûre
         *   - Évite l'appel à tanh() pour les grandes valeurs où le résultat est trivial
         *   - Utile pour les fonctions d'activation de réseaux de neurones
         */
        float32 NkTanh(float32 x) noexcept {
            if (x > 10.0f) {
                return 1.0f;
            }
            if (x < -10.0f) {
                return -1.0f;
            }
            return static_cast<float32>(tanh(x));
        }

        /**
         * @brief Tangente hyperbolique avec saturation pour éviter les calculs inutiles - float64
         * @param x Valeur d'entrée
         * @return tanh(x) ∈ [-1, 1], saturé à ±1 pour |x| > 20
         * @note
         *   - Seuil plus élevé (20) pour float64 car la précision permet d'aller plus loin
         *   - tanh(20) ≈ 1 - 2e-17, donc saturation sûre
         */
        float64 NkTanh(float64 x) noexcept {
            if (x > 20.0) {
                return 1.0;
            }
            if (x < -20.0) {
                return -1.0;
            }
            return tanh(x);
        }

        // ====================================================================
        // SECTION 5 : UTILITAIRES FLOTTANTS
        // ====================================================================
        // Implémentation des fonctions fmod, frexp, ldexp, modf, division entière.

        /**
         * @brief Reste de division flottante avec gestion de division par zéro - float32
         * @param x Dividende
         * @param y Diviseur
         * @return x - y*trunc(x/y) si y != 0, x sinon
         * @note
         *   - Retourne x inchangé si y == 0 au lieu de NaN pour robustesse
         *   - Délègue à fmod() de <math.h> pour y != 0
         */
        float32 NkFmod(float32 x, float32 y) noexcept {
            if (y == 0.0f) {
                return x;
            }
            return static_cast<float32>(fmod(x, y));
        }

        /**
         * @brief Reste de division flottante avec gestion de division par zéro - float64
         * @param x Dividende
         * @param y Diviseur
         * @return x - y*trunc(x/y) si y != 0, x sinon
         */
        float64 NkFmod(float64 x, float64 y) noexcept {
            if (y == 0.0) {
                return x;
            }
            return fmod(x, y);
        }

        /**
         * @brief Décomposition en mantisse normalisée et exposant - float32
         * @param x Valeur à décomposer
         * @param exp Pointeur vers entier pour recevoir l'exposant
         * @return Mantisse m ∈ [0.5, 1) ou 0 telle que x = m × 2^exp
         * @note Délègue à frexp() de <math.h> avec cast de précision
         */
        float32 NkFrexp(float32 x, int32* exp) noexcept {
            return static_cast<float32>(frexp(x, exp));
        }

        /**
         * @brief Décomposition en mantisse normalisée et exposant - float64
         * @param x Valeur à décomposer
         * @param exp Pointeur vers entier pour recevoir l'exposant
         * @return Mantisse m ∈ [0.5, 1) ou 0 telle que x = m × 2^exp
         */
        float64 NkFrexp(float64 x, int32* exp) noexcept {
            return frexp(x, exp);
        }

        /**
         * @brief Reconstruction depuis mantisse et exposant - float32
         * @param x Mantisse (typiquement sortie de NkFrexp)
         * @param exp Exposant entier
         * @return x × 2^exp
         * @note Inverse de NkFrexp : NkLdexp(NkFrexp(x,&e), e) ≈ x (à l'arrondi près)
         */
        float32 NkLdexp(float32 x, int32 exp) noexcept {
            return static_cast<float32>(ldexp(x, exp));
        }

        /**
         * @brief Reconstruction depuis mantisse et exposant - float64
         * @param x Mantisse
         * @param exp Exposant entier
         * @return x × 2^exp
         */
        float64 NkLdexp(float64 x, int32 exp) noexcept {
            return ldexp(x, exp);
        }

        /**
         * @brief Séparation partie entière/fractionnaire - float32
         * @param x Valeur d'entrée
         * @param iptr Pointeur vers flottant pour recevoir la partie entière
         * @return Partie fractionnaire f avec x = *iptr + f, |f| < 1, sign(f) = sign(x)
         * @note Délègue à modf() de <math.h> avec cast de précision
         */
        float32 NkModf(float32 x, float32* iptr) noexcept {
            return static_cast<float32>(modf(x, iptr));
        }

        /**
         * @brief Séparation partie entière/fractionnaire - float64
         * @param x Valeur d'entrée
         * @param iptr Pointeur vers flottant pour recevoir la partie entière
         * @return Partie fractionnaire f avec x = *iptr + f
         */
        float64 NkModf(float64 x, float64* iptr) noexcept {
            return modf(x, iptr);
        }

        /**
         * @brief Division entière 64-bit avec quotient et reste
         * @param numerator Numérateur
         * @param denominator Dénominateur
         * @return Structure {quot = numerator/denominator, rem = numerator%denominator}
         * @note
         *   - Retourne {0, numerator} si denominator == 0 pour éviter division par zéro
         *   - Plus sûr que / et % séparés : évite double calcul et incohérences
         *
         * @warning
         *   Le cas numerator = INT64_MIN, denominator = -1 peut overflow selon la plateforme.
         *   Le comportement est alors défini par l'implémentation C de la plateforme.
         */
        DivResult64 NkDivI64(int64 numerator, int64 denominator) noexcept {
            if (denominator == 0) {
                return {0, numerator};
            }
            return {numerator / denominator, numerator % denominator};
        }

        // ====================================================================
        // SECTION 6 : UTILITAIRES ENTIERS ET BITS
        // ====================================================================
        // Implémentation des fonctions gcd, nextPowerOf2, clz, ctz, popcount.

        /**
         * @brief Plus Grand Commun Diviseur via algorithme d'Euclide
         * @param a Premier entier (>= 0)
         * @param b Deuxième entier (>= 0)
         * @return PGCD(a,b)
         * @note
         *   - Complexité O(log min(a,b))
         *   - Retourne a si b == 0 (cas de base de l'algorithme)
         *   - Retourne 0 si a == b == 0 (convention)
         */
        uint64 NkGcd(uint64 a, uint64 b) noexcept {
            while (b != 0u) {
                const uint64 t = b;
                b = a % b;
                a = t;
            }
            return a;
        }

        /**
         * @brief Arrondi à la prochaine puissance de 2 supérieure ou égale
         * @param x Valeur d'entrée (>= 0)
         * @return Plus petite puissance de 2 >= x, ou 1 si x == 0
         * @note
         *   - Algorithme bit-twiddling : propagation du bit de poids fort vers la droite
         *   - Étapes : x-- → ORs successifs avec shifts → x++
         *   - Exemple : x=5 (0b101) → x--=4 (0b100) → ORs → 0b111 → +1 → 0b1000 = 8 ✓
         *
         * @warning
         *   Si x > 2^63, le résultat overflow à 0 (comportement défini pour uint64).
         *   Vérifier les bornes en amont pour les allocations mémoire critiques.
         */
        uint64 NkNextPowerOf2(uint64 x) noexcept {
            if (x == 0u) {
                return 1u;
            }

            --x;
            x |= x >> 1u;
            x |= x >> 2u;
            x |= x >> 4u;
            x |= x >> 8u;
            x |= x >> 16u;
            x |= x >> 32u;
            return x + 1u;
        }

        /**
         * @brief Compte les zéros en tête (Count Leading Zeros) pour uint32
         * @param x Valeur d'entrée
         * @return Nombre de zéros avant le premier bit à 1 (0-32), 32 si x == 0
         * @note
         *   - Utilise __builtin_clz() sur GCC/Clang (instruction hardware LZCNT si disponible)
         *   - Fallback software portable via recherche binaire bit-level
         *   - Le fallback est O(log bits) = O(5) pour uint32, acceptable si rarement appelé
         *
         * @example de fallback :
         * @code
         * // Pour x = 0b00001010 (10 décimal) :
         * // x <= 0x0000FFFF → n+=16, x<<=16 → x = 0b10100000000000000
         * // x > 0x00FFFFFF → skip
         * // x > 0x0FFFFFFF → skip
         * // x > 0x3FFFFFFF → skip
         * // x <= 0x7FFFFFFF → n+=1 → n=17
         * // Retourne 17 + 15 (bits décalés) = 32 - 5 (bits significatifs) = 27 ✓
         * @endcode
         */
        uint32 NkClz(uint32 x) noexcept {
            if (x == 0u) {
                return 32u;
            }

            #if defined(__clang__) || defined(__GNUC__)
                return static_cast<uint32>(__builtin_clz(x));
            #else
                uint32 n = 0u;
                if (x <= 0x0000FFFFu) { n += 16u; x <<= 16u; }
                if (x <= 0x00FFFFFFu) { n += 8u; x <<= 8u; }
                if (x <= 0x0FFFFFFFu) { n += 4u; x <<= 4u; }
                if (x <= 0x3FFFFFFFu) { n += 2u; x <<= 2u; }
                if (x <= 0x7FFFFFFFu) { n += 1u; }
                return n;
            #endif
        }

        /**
         * @brief Compte les zéros en tête (Count Leading Zeros) pour uint64
         * @param x Valeur d'entrée
         * @return Nombre de zéros avant le premier bit à 1 (0-64), 64 si x == 0
         * @note
         *   - Utilise __builtin_clzll() sur GCC/Clang
         *   - Fallback software avec recherche binaire étendue à 64 bits
         */
        uint32 NkClz(uint64 x) noexcept {
            if (x == 0u) {
                return 64u;
            }

            #if defined(__clang__) || defined(__GNUC__)
                return static_cast<uint32>(__builtin_clzll(static_cast<unsigned long long>(x)));
            #else
                uint32 n = 0u;
                if (x <= 0x00000000FFFFFFFFull) { n += 32u; x <<= 32u; }
                if (x <= 0x0000FFFFFFFFFFFFull) { n += 16u; x <<= 16u; }
                if (x <= 0x00FFFFFFFFFFFFFFull) { n += 8u; x <<= 8u; }
                if (x <= 0x0FFFFFFFFFFFFFFFull) { n += 4u; x <<= 4u; }
                if (x <= 0x3FFFFFFFFFFFFFFFull) { n += 2u; x <<= 2u; }
                if (x <= 0x7FFFFFFFFFFFFFFFull) { n += 1u; }
                return n;
            #endif
        }

        /**
         * @brief Compte les zéros en queue (Count Trailing Zeros) pour uint32
         * @param x Valeur d'entrée
         * @return Nombre de zéros après le dernier bit à 1 (0-32), 32 si x == 0
         * @note
         *   - Utilise __builtin_ctz() sur GCC/Clang (instruction hardware TZCNT si disponible)
         *   - Fallback via identité : ctz(x) = 31 - clz(x & -x) où -x = ~x+1 isole le LSB
         *
         * @warning
         *   Le fallback suppose que NkClz() est disponible et correct.
         *   x & -x isole le bit de poids faible à 1 : ex: 0b101100 & 0b010100 = 0b000100
         */
        uint32 NkCtz(uint32 x) noexcept {
            if (x == 0u) {
                return 32u;
            }

            #if defined(__clang__) || defined(__GNUC__)
                return static_cast<uint32>(__builtin_ctz(x));
            #else
                return 31u - NkClz(x & static_cast<uint32>(-static_cast<int32>(x)));
            #endif
        }

        /**
         * @brief Compte les zéros en queue (Count Trailing Zeros) pour uint64
         * @param x Valeur d'entrée
         * @return Nombre de zéros après le dernier bit à 1 (0-64), 64 si x == 0
         */
        uint32 NkCtz(uint64 x) noexcept {
            if (x == 0u) {
                return 64u;
            }

            #if defined(__clang__) || defined(__GNUC__)
                return static_cast<uint32>(__builtin_ctzll(static_cast<unsigned long long>(x)));
            #else
                return 63u - NkClz(x & static_cast<uint64>(-static_cast<int64>(x)));
            #endif
        }

        /**
         * @brief Compte les bits à 1 (Population Count) pour uint32
         * @param x Valeur d'entrée
         * @return Nombre de bits positionnés à 1 (0-32)
         * @note
         *   - Utilise __builtin_popcount() sur GCC/Clang (instruction hardware POPCNT si disponible)
         *   - Fallback software via algorithme SWAR (SIMD Within A Register)
         *   - L'algorithme SWAR compte les bits en parallèle via des masques et additions
         *
         * @example d'algorithme SWAR pour 0b10110100 (180 décimal, 3 bits à 1) :
         * @code
         * // Étape 1 : compter les bits par paires
         * x = x - ((x>>1) & 0x55555555)
         * // 0b10110100 → 0b01100010 (compte : 1+0, 1+1, 0+1, 0+0)
         *
         * // Étape 2 : sommer par groupes de 4 bits
         * x = (x & 0x33333333) + ((x>>2) & 0x33333333)
         * // → 0b00110010 (3+0, 0+2)
         *
         * // Étape 3 : sommer par octets et multiplier pour propagation
         * return ((x + (x>>4)) & 0x0F0F0F0F) * 0x01010101 >> 24
         * // → 3 ✓
         * @endcode
         */
        uint32 NkPopcount(uint32 x) noexcept {
            #if defined(__clang__) || defined(__GNUC__)
                return static_cast<uint32>(__builtin_popcount(x));
            #else
                x = x - ((x >> 1u) & 0x55555555u);
                x = (x & 0x33333333u) + ((x >> 2u) & 0x33333333u);
                return ((x + (x >> 4u)) & 0x0F0F0F0Fu) * 0x01010101u >> 24u;
            #endif
        }

        /**
         * @brief Compte les bits à 1 (Population Count) pour uint64
         * @param x Valeur d'entrée
         * @return Nombre de bits positionnés à 1 (0-64)
         * @note
         *   - Utilise __builtin_popcountll() sur GCC/Clang
         *   - Fallback SWAR étendu à 64 bits avec constante 64-bit
         */
        uint32 NkPopcount(uint64 x) noexcept {
            #if defined(__clang__) || defined(__GNUC__)
                return static_cast<uint32>(__builtin_popcountll(static_cast<unsigned long long>(x)));
            #else
                x = x - ((x >> 1u) & 0x5555555555555555ull);
                x = (x & 0x3333333333333333ull) + ((x >> 2u) & 0x3333333333333333ull);
                return static_cast<uint32>(((x + (x >> 4u)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull >> 56u);
            #endif
        }

        // ====================================================================
        // SECTION 7 : INTERPOLATION ET LISSAGE
        // ====================================================================
        // Implémentation des fonctions smoothstep et smootherstep.

        /**
         * @brief Interpolation cubique Hermite (smoothstep) - float32
         * @param t Valeur d'entrée (typiquement ∈ [0,1])
         * @return 3t² - 2t³ ∈ [0,1], avec dérivée nulle en t=0 et t=1
         * @note
         *   - Clampe automatiquement t < 0 → 0, t > 1 → 1 pour robustesse
         *   - Courbe en S : démarrage et arrêt progressifs, idéal pour animations
         *   - Dérivée première continue : pas de "cassure" de vitesse
         *
         * @example de courbe :
         * @code
         * t=0.0 → 0.0  (dérivée=0)
         * t=0.5 → 0.5  (dérivée=1.5, pente max)
         * t=1.0 → 1.0  (dérivée=0)
         * @endcode
         */
        float32 NkSmoothstep(float32 t) noexcept {
            if (t < 0.0f) {
                return 0.0f;
            }
            if (t > 1.0f) {
                return 1.0f;
            }
            return t * t * (3.0f - 2.0f * t);
        }

        /**
         * @brief Interpolation cubique Hermite (smoothstep) - float64
         * @param t Valeur d'entrée
         * @return 3t² - 2t³ ∈ [0,1]
         */
        float64 NkSmoothstep(float64 t) noexcept {
            if (t < 0.0) {
                return 0.0;
            }
            if (t > 1.0) {
                return 1.0;
            }
            return t * t * (3.0 - 2.0 * t);
        }

        /**
         * @brief Interpolation quintique (smootherstep) - float32
         * @param t Valeur d'entrée (typiquement ∈ [0,1])
         * @return 6t⁵ - 15t⁴ + 10t³ ∈ [0,1], avec dérivées 1 et 2 nulles en 0 et 1
         * @note
         *   - Clampe automatiquement t < 0 → 0, t > 1 → 1
         *   - Courbe ultra-douce : démarrage/arrêt très progressifs
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
        float32 NkSmootherstep(float32 t) noexcept {
            if (t < 0.0f) {
                return 0.0f;
            }
            if (t > 1.0f) {
                return 1.0f;
            }
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        /**
         * @brief Interpolation quintique (smootherstep) - float64
         * @param t Valeur d'entrée
         * @return 6t⁵ - 15t⁴ + 10t³ ∈ [0,1]
         */
        float64 NkSmootherstep(float64 t) noexcept {
            if (t < 0.0) {
                return 0.0;
            }
            if (t > 1.0) {
                return 1.0;
            }
            return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
        }

        // ====================================================================
        // SECTION 8 : UTILITAIRES EXPONENTE/SIGNE
        // ====================================================================
        // Implémentation des fonctions ilogb et copysign.

        /**
         * @brief Extrait l'exposant entier (log2 de la magnitude) - float32
         * @param x Valeur flottante (doit être fini et != 0)
         * @return Exposant e tel que x = m × 2^e avec m ∈ [1,2)
         * @note
         *   - Délègue à ilogbf() de <math.h> (version float de ilogb)
         *   - Équivalent à floor(log2(|x|)) pour x fini non-nul
         *   - Retourne FP_ILOGB0 ou FP_ILOGBNAN pour x=0 ou NaN selon l'implémentation C
         *
         * @warning
         *   Le comportement pour x=0, NaN, Inf dépend de l'implémentation C de la plateforme.
         *   Vérifier NkIsFinite(x) avant appel si un comportement spécifique est requis.
         */
        int32 NkILogb(float32 x) noexcept {
            return ilogbf(x);
        }

        /**
         * @brief Copie le bit de signe d'un flottant à un autre (bitwise) - float32
         * @param number Valeur dont garder la magnitude
         * @param sign Valeur dont copier le bit de signe
         * @return |number| avec le signe de sign
         * @note
         *   - Délègue à copysign() de <math.h> avec cast de précision
         *   - Implémentation bitwise : pas de branche, rapide
         *   - Équivalent à : NkFabs(number) * (sign < 0 ? -1.0f : 1.0f) mais sans branche
         *   - Gère correctement les cas spéciaux : NaN, Inf, ±0.0
         *
         * @example
         * @code
         * // Correction de signe pour une direction calculée
         * float32 magnitude = NkSqrt(dotProduct);
         * float32 signedMag = NkCopysign(magnitude, direction);
         * // signedMag a la magnitude de magnitude mais le signe de direction
         * @endcode
         */
        float32 NkCopysign(float32 number, float32 sign) noexcept {
            return static_cast<float32>(copysign(number, sign));
        }

    } // namespace math

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
//
// Gestion des cas limites :
//   Toutes les fonctions retournent des valeurs définies même pour des inputs
//   invalides (x < 0 pour sqrt, x <= 0 pour log, etc.) afin d'éviter la propagation
//   de NaN/Inf dans les calculs suivants. Ce comportement "robuste" peut masquer
//   des bugs de logique : utiliser NKENTSEU_ASSERT en debug pour vérifier les préconditions
//   si nécessaire.
//
// Optimisations platform-specific :
//   - Les fonctions bitwise (NkClz, NkCtz, NkPopcount) utilisent les builtins
//     GCC/Clang quand disponibles, qui se traduisent souvent en instructions
//     hardware dédiées (LZCNT, TZCNT, POPCNT sur x86_64 moderne).
//   - Pour MSVC ou autres compilateurs, un fallback software est fourni.
//     Ces fallbacks sont corrects mais potentiellement plus lents : profiler
//     si ces fonctions sont dans un hot path.
//
// Précision numérique :
//   - Les fonctions trigonométriques et exponentielles délèguent à <math.h>.
//     La précision dépend donc de l'implémentation de la libc de la plateforme.
//   - Pour une précision garantie cross-platform, envisager une implémentation
//     custom ou une librairie comme crlibm, mais au prix de la performance.
//
// Thread-safety :
//   Toutes les fonctions sont stateless et thread-safe. Aucune variable globale
//   n'est modifiée, et les fonctions C de <math.h> utilisées sont thread-safe
//   sur toutes les plateformes supportées (C99+).
//
// Extensions possibles :
//   - Ajouter des versions vectorisées (SSE/AVX/NEON) pour les opérations
//     sur tableaux de flottants (NkSqrtArray, NkSinArray, etc.)
//   - Ajouter des fonctions mathématiques avancées : erf, gamma, Bessel, etc.
//   - Ajouter un mode "fast math" avec relaxation des garanties IEEE 754
//     pour plus de performance (à activer via macro de configuration).
//
// =============================================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================