//
// NkQuat.cpp
// =============================================================================
// Description :
//   Implémentation des fonctions non-template liées aux quaternions.
//   Ce fichier reste minimal car NkQuatT<T> est un template dont
//   l'implémentation doit rester dans le header pour l'instanciation.
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête précompilé (requis en premier)
// =====================================================================
#include "pch.h"

// =====================================================================
// Inclusion de l'en-tête de déclaration du template quaternionique
// =====================================================================
#include "NKMath/NkQuat.h"

// =====================================================================
// Namespace : nkentseu::math
// =====================================================================
namespace nkentseu {
namespace math {

    // ---------------------------------------------------------------------
    // Fonctions utilitaires non-template (espace pour extensions futures)
    // ---------------------------------------------------------------------

    // Exemple : Fonction de debug pour afficher un quaternion formaté
    // void DebugPrintQuaternion(const NkQuatf& quat, const char* label)
    // {
    //     if (label != nullptr) {
    //         std::printf("=== %s ===\n", label);
    //     }
    //     std::printf("  Quaternion: [x=%.4f, y=%.4f, z=%.4f; w=%.4f]\n",
    //         quat.x, quat.y, quat.z, quat.w);
    //     std::printf("  Angle: %.2f°, Axis: (%.3f, %.3f, %.3f)\n",
    //         quat.Angle().Deg(),
    //         quat.Axis().x, quat.Axis().y, quat.Axis().z);
    //     std::printf("  Normalized: %s\n", quat.IsNormalized() ? "yes" : "no");
    // }

    // Exemple : Fonction utilitaire pour interpolation de caméra
    // NkQuatf SmoothLookAt(
    //     const NkVec3f& currentForward,
    //     const NkVec3f& targetDirection,
    //     float32 smoothFactor
    // ) {
    //     NkQuatf currentRot;  // Quaternion depuis forward actuel
    //     NkQuatf targetRot = NkQuatf(NkVec3f(0,0,1), targetDirection);
    //     return currentRot.SLerp(targetRot, smoothFactor);
    // }

    // ---------------------------------------------------------------------
    // Note architecturale : gestion des templates quaternioniques
    // ---------------------------------------------------------------------
    // La structure NkQuatT<T> est entièrement définie dans le fichier .h
    // avec des méthodes inline pour les raisons suivantes :
    //
    // 1. Nature template : le compilateur doit accéder au code source pour
    //    générer les spécialisations pour chaque type T utilisé (float32, float64)
    //
    // 2. Performance : les opérations quaternioniques sont fréquemment appelées
    //    dans les boucles d'animation et de rendu ; l'inlining permet au
    //    compilateur d'optimiser agressivement (vectorisation, élimination)
    //
    // 3. Cohérence projet : alignement avec NK_FORCE_INLINE et les autres
    //    types mathématiques génériques (NkVecT, NkMatT, NkRangeT, etc.)
    //
    // Si vous ajoutez des fonctions non-template (helpers de debug,
    // conversions vers types externes, sérialisation binaire optimisée),
    // placez-les dans ce fichier pour minimiser les dépendances de
    // compilation dans les unités utilisant uniquement les alias concrets.

}  // namespace math
}  // namespace nkentseu