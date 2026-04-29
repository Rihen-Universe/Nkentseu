//
// NkVec.cpp
// =============================================================================
// Description :
//   Implémentation des fonctions non-template liées aux vecteurs.
//   Ce fichier reste minimal car la majorité du code est template et inline
//   dans le header pour permettre l'instanciation à la compilation.
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#include "NKMath/NkVec.h"

// =============================================================================
// Namespace : nkentseu::math
// =============================================================================
namespace nkentseu {
namespace math {

    // ---------------------------------------------------------------------
    // Fonctions utilitaires non-template (exemples pour extension future)
    // ---------------------------------------------------------------------

    // Exemple : Fonction de debug qui pourrait être ajoutée
    // void DebugPrintVector(const NkVec3f& vector, const char* label)
    // {
    //     if (label != nullptr) {
    //         std::printf("%s: ", label);
    //     }
    //     std::printf("(%.3f, %.3f, %.3f)\n", vector.x, vector.y, vector.z);
    // }

    // ---------------------------------------------------------------------
    // Note importante sur l'architecture template
    // ---------------------------------------------------------------------
    // Toutes les méthodes des structures NkVec2T, NkVec3T, NkVec4T sont
    // définies inline dans le fichier .h car :
    //
    // 1. Ce sont des templates : le compilateur doit voir l'implémentation
    //    pour générer le code pour chaque type T instancié.
    //
    // 2. Ces méthodes sont courtes et fréquemment appelées : inline permet
    //    au compilateur d'optimiser les appels (évite le overhead de fonction).
    //
    // 3. Le système NK_FORCE_INLINE assure une cohérence avec les autres
    //    modules mathématiques du projet.
    //
    // Si vous ajoutez des fonctions non-template (ex: helpers de debug,
    // conversions vers types externes), placez-les ici pour réduire les
    // dépendances de compilation.

} // namespace math
} // namespace nkentseu