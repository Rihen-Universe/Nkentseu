// =============================================================================
// NKCore/NkBits.cpp
// Implémentation des fonctions non-template de manipulation de bits.
//
// Design :
//  - Implémentation de NextPowerOfTwo() pour nk_uint32 et nk_uint64
//  - Algorithmes bit-twiddling optimisés sans boucles
//  - Aucune dépendance à des librairies externes
//
// Intégration :
//  - Utilise NKCore/NkBits.h pour déclaration des méthodes
//  - Utilise NKCore/NkTypes.h pour types portables nk_uint32, nk_uint64
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
#include "NkBits.h"

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    // ====================================================================
    // IMPLÉMENTATION : NEXT POWER OF TWO - 32-BIT
    // ====================================================================

    /**
     * @brief Calcule la plus petite puissance de 2 >= value (32-bit)
     * @param value Valeur d'entrée
     * @return Plus petite puissance de 2 >= value, ou 1 si value == 0
     *
     * @note
     *   Algorithme bit-twiddling classique :
     *   1. Décrémenter pour gérer le cas "déjà puissance de 2"
     *   2. Propager le bit de poids fort vers la droite via OR successifs
     *   3. Incrémenter pour obtenir la puissance de 2 suivante
     *
     *   Exemple : value = 1000 (0b1111101000)
     *   - value-- → 999 (0b1111100111)
     *   - Propagation : 0b1111111111
     *   - value++ → 1024 (0b10000000000) ✓
     *
     * @warning
     *   Si value > 0x80000000, le résultat overflow à 0 (comportement défini
     *   pour nk_uint32 : wraparound modulo 2^32). Vérifier les bornes en amont.
     */
    nk_uint32 NkBits::NextPowerOfTwo(nk_uint32 value) {
        if (value == 0) {
            return 1;  // Convention : prochaine puissance de 2 après 0 est 1
        }
        if (IsPowerOfTwo(value)) {
            return value;  // Déjà une puissance de 2 : retour inchangé
        }

        // Algorithme de propagation du bit de poids fort
        value--;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value++;

        return value;
    }

    // ====================================================================
    // IMPLÉMENTATION : NEXT POWER OF TWO - 64-BIT
    // ====================================================================

    /**
     * @brief Calcule la plus petite puissance de 2 >= value (64-bit)
     * @param value Valeur d'entrée
     * @return Plus petite puissance de 2 >= value, ou 1 si value == 0
     *
     * @note
     *   Même algorithme que la version 32-bit, avec étape supplémentaire
     *   pour propager sur les 64 bits complets.
     *
     * @warning
     *   Si value > 0x8000000000000000, overflow à 0. Vérifier les bornes.
     */
    nk_uint64 NkBits::NextPowerOfTwo(nk_uint64 value) {
        if (value == 0) {
            return 1;  // Convention : prochaine puissance de 2 après 0 est 1
        }
        if (IsPowerOfTwo(value)) {
            return value;  // Déjà une puissance de 2 : retour inchangé
        }

        // Algorithme de propagation du bit de poids fort (64-bit)
        value--;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;  // Étape supplémentaire pour 64-bit
        value++;

        return value;
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
//
// Pourquoi NextPowerOfTwo est dans le .cpp et pas inline dans le .h ?
//
// 1. Éviter la duplication de code : l'algorithme est identique pour 32/64-bit
//    à part l'étape >>32, donc une implémentation centrale est plus maintenable.
//
// 2. Réduire la taille du binaire : si appelé depuis plusieurs TU, l'inline
//    dans le .h générerait une copie par TU. Le .cpp permet une seule définition.
//
// 3. Performance : l'algorithme est O(1) avec 5-6 opérations bitwise, donc
//    l'overhead d'appel de fonction est négligeable vs le gain en taille de code.
//
// Alternative template (si vraiment nécessaire) :
// @code
// template <typename T>
// NKENTSEU_FORCE_INLINE static T NextPowerOfTwoTemplate(T value) {
//     if (value == 0) return 1;
//     if (IsPowerOfTwo(value)) return value;
//     value--;
//     value |= value >> 1;
//     value |= value >> 2;
//     value |= value >> 4;
//     value |= value >> 8;
//     value |= value >> 16;
//     if (sizeof(T) > 4) { value |= value >> 32; }
//     value++;
//     return value;
// }
// @endcode
//
// =============================================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================