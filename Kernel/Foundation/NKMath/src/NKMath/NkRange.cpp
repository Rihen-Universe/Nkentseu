//
// NkRange.cpp
// =============================================================================
// Description :
//   Implémentation des fonctions non-template liées aux intervalles NkRangeT.
//   Ce fichier est minimal car NkRangeT est un template dont l'implémentation
//   reste dans le header pour permettre l'instanciation à la compilation.
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#include "NKMath/NkRange.h"

// =============================================================================
// Namespace : nkentseu::math
// =============================================================================
namespace nkentseu {
namespace math {

    // ---------------------------------------------------------------------
    // Fonctions utilitaires non-template (espace pour extensions futures)
    // ---------------------------------------------------------------------

    // Exemple : Fonction de validation d'intervalle pour debugging
    // bool ValidateRange(const NkRange& range, const char* context)
    // {
    //     if (range.GetMin() > range.GetMax()) {
    //         NK_LOG_ERROR("Invalid range in %s: min=%f > max=%f",
    //             context, range.GetMin(), range.GetMax());
    //         return false;
    //     }
    //     return true;
    // }

    // ---------------------------------------------------------------------
    // Note architecturale : gestion des templates
    // ---------------------------------------------------------------------
    // La structure NkRangeT<T> est entièrement définie dans le fichier .h
    // avec des méthodes inline pour les raisons suivantes :
    //
    // 1. Nature template : le compilateur doit accéder au code source pour
    //    générer les spécialisations pour chaque type T utilisé (float, int, etc.)
    //
    // 2. Performance : les méthodes courtes (GetMin, Contains, Clamp...) sont
    //    candidates à l'inlining pour éliminer le overhead d'appel de fonction
    //
    // 3. Cohérence projet : alignement avec NK_FORCE_INLINE et les autres
    //    types mathématiques génériques (NkVecT, NkRangeT, etc.)
    //
    // Si vous ajoutez des fonctions non-template (helpers de debug, conversions
    // vers types externes, sérialisation binaire), placez-les dans ce fichier
    // pour minimiser les dépendances de compilation dans les unités utilisant
    // uniquement les alias concrets (NkRange, NkRangeInt, etc.)

} // namespace math
} // namespace nkentseu