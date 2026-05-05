//
// NkRandom.cpp
// =============================================================================
// Description :
//   Implémentation des fonctions non-template de la classe NkRandom.
//   Ce fichier est minimal car la plupart des méthodes sont inline
//   dans le header pour des raisons de performance et de simplicité.
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête précompilé (requis en premier)
// =====================================================================
#include "pch.h"

// =====================================================================
// Inclusion de l'en-tête de déclaration de la classe
// =====================================================================
#include "NKMath/NkRandom.h"

// =====================================================================
// Namespace : nkentseu::math
// =====================================================================
namespace nkentseu {
namespace math {

    // ---------------------------------------------------------------------
    // Fonctions utilitaires non-template (espace pour extensions futures)
    // ---------------------------------------------------------------------

    // Exemple : Fonction de debug pour afficher une séquence de valeurs
    // void DebugPrintRandomSequence(size_t count)
    // {
    //     std::printf("=== Random Sequence (%zu values) ===\n", count);
    //     for (size_t i = 0; i < count; ++i) {
    //         std::printf("  [%zu] %.6f\n", i, NkRand.NextFloat32());
    //     }
    // }

    // Exemple : Fonction utilitaire pour générer un seed cryptographique
    // (si rand() n'est pas suffisant pour l'usage)
    // uint32 GenerateSecureSeed()
    // {
    //     // Sur les plateformes supportant C++11+, utiliser std::random_device
    //     #if __cplusplus >= 201103L
    //         std::random_device rd;
    //         return rd();
    //     #else
    //         // Fallback : combinaison de time() et d'adresse mémoire
    //         return static_cast<uint32>(time(nullptr)) ^ reinterpret_cast<uintptr_t>(&GenerateSecureSeed);
    //     #endif
    // }

    // ---------------------------------------------------------------------
    // Note architecturale : pourquoi tout est inline dans le header
    // ---------------------------------------------------------------------
    // La classe NkRandom est conçue comme un singleton header-only pour :
    //
    // 1. Performance : les méthodes de génération sont appelées très fréquemment
    //    dans les boucles de jeu ; l'inlining évite le overhead d'appel de fonction
    //
    // 2. Simplicité : pas de fichier .cpp à maintenir pour des méthodes triviales
    //
    // 3. Flexibilité : les templates (NextVec2, NextMat4, etc.) doivent être
    //    définis dans le header pour être instanciés dans les unités de compilation
    //
    // Si vous ajoutez des fonctions non-template complexes (générateurs alternatifs,
    // sérialisation de l'état du RNG, etc.), placez-les dans ce fichier pour
    // réduire les dépendances de compilation.

}  // namespace math
}  // namespace nkentseu