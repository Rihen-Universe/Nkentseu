// =============================================================================
// NKMath/NkLegacySystem.h
// Couche de compatibilité pour les en-têtes mathématiques hérités.
//
// Design :
//  - Centralisation des dépendances attendues par le code legacy NKMath
//  - Inclusion des modules fondamentaux : NKCore, NKPlatform, NKContainers
//  - Import des en-têtes C++ standards couramment utilisés en mathématiques
//  - Aucune logique métier : ce header sert uniquement de pont d'inclusion
//  - Indépendance totale : ne définit pas de nouveaux types ou fonctions
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MATH_NKLEGACYSYSTEM_H
#define NKENTSEU_MATH_NKLEGACYSYSTEM_H

    // -------------------------------------------------------------------------
    // SECTION 1 : INCLUSIONS DES MODULES FONDAMENTAUX
    // -------------------------------------------------------------------------
    // Ces headers fournissent les types, macros et conteneurs de base
    // attendus par les fichiers NKMath hérités.

    #include "NKCore/NKCore.h"              // Types fondamentaux, assertions, traits
    #include "NKPlatform/NKPlatform.h"      // Détection plateforme, exports API, SIMD
    #include "NKContainers/NKContainers.h"  // Conteneurs génériques (Vector, Map, Pair)

    // -------------------------------------------------------------------------
    // SECTION 2 : INCLUSIONS DES EN-TÊTES C++ STANDARDS
    // -------------------------------------------------------------------------
    // Bibliothèques standard couramment utilisées dans le code mathématique legacy.

    #include <algorithm>    // std::min, std::max, std::clamp, algorithmes génériques
    #include <cmath>        // std::sin, std::cos, std::sqrt, fonctions mathématiques
    #include <cstdlib>      // std::abs, std::rand, utilitaires C standard
    #include <iostream>     // std::cout, std::cerr, flux d'E/S pour débogage
    #include <string>       // std::string, manipulation de chaînes de caractères

#endif // NKENTSEU_MATH_NKLEGACYSYSTEM_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Inclusion dans un fichier legacy NKMath
    // ---------------------------------------------------
    // Fichier : LegacyVector3.h
    #include "NKMath/NkLegacySystem.h"

    // Tous les types et fonctions standards sont maintenant disponibles :
    // - nkentseu::usize, float32, NkAssert (via NKCore)
    // - NKENTSEU_PLATFORM_*, NKENTSEU_ALIGN_* (via NKPlatform)
    // - nkentseu::NkVector, NkPair (via NKContainers)
    // - std::min, std::sqrt, std::string, etc. (via STL)

    class LegacyVector3 {
    public:
        float32 x, y, z;

        float32 Length() const {
            return std::sqrt(x*x + y*y + z*z);  // std::sqrt disponible
        }

        LegacyVector3 Clamped(float32 minVal, float32 maxVal) const {
            return {
                std::clamp(x, minVal, maxVal),  // std::clamp disponible
                std::clamp(y, minVal, maxVal),
                std::clamp(z, minVal, maxVal)
            };
        }
    };


    // Exemple 2 : Migration progressive vers la nouvelle API
    // -------------------------------------------------------
    // Étape 1 : Code legacy utilisant NkLegacySystem.h
    #include "NKMath/NkLegacySystem.h"

    void LegacyFunction() {
        nkentseu::NkVector<float32> values;  // Via NKContainers
        values.PushBack(1.0f);
        values.PushBack(2.0f);

        float32 maxVal = *std::max_element(values.Begin(), values.End());
        NKENTSEU_ASSERT(maxVal > 0.0f);  // Via NKCore
    }

    // Étape 2 : Code moderne utilisant les headers spécifiques
    #include "NKMath/Vector3.h"           // API mathématique moderne
    #include "NKContainers/Sequential/NkVector.h"  // Header spécifique
    #include "NKCore/Assert/NkAssert.h"   // Header spécifique

    void ModernFunction() {
        nkentseu::NkVector<float32> values;
        values.PushBack(1.0f);

        // Utilisation de l'algorithme NKMath si disponible, sinon STL
        float32 maxVal = nkentseu::math::Max(values.Data(), values.Size());
        NKENTSEU_ASSERT(maxVal > 0.0f);
    }


    // Exemple 3 : Configuration CMake pour gérer la compatibilité
    // ------------------------------------------------------------
    // CMakeLists.txt
    cmake_minimum_required(VERSION 3.15)
    project(MyProject)

    # Option pour activer/désactiver le support legacy
    option(ENABLE_MATH_LEGACY "Utiliser la couche de compatibilité NKMath" ON)

    if(ENABLE_MATH_LEGACY)
        # Le code legacy inclut NkLegacySystem.h
        target_compile_definitions(myapp PRIVATE NKENTSEU_MATH_LEGACY_ENABLED)
    else()
        # Le code moderne doit inclure les headers spécifiques
        target_compile_definitions(myapp PRIVATE NKENTSEU_MATH_MODERN_ONLY)
    endif()

    target_link_libraries(myapp PRIVATE NKMath::NKMath)


    // Exemple 4 : Débogage avec les flux standards
    // ---------------------------------------------
    #include "NKMath/NkLegacySystem.h"
    #include <iomanip>

    void DebugPrintVector3(const LegacyVector3& v) {
        // std::cout et std::iomanip disponibles via NkLegacySystem.h
        std::cout << std::fixed << std::setprecision(3)
                  << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")\n";
    }

    // Note : Pour le logging en production, préférer NKCore/NkLogger.h
    // NkLegacySystem.h est destiné au débogage et à la compatibilité.


    // Exemple 5 : Utilisation conditionnelle selon la plateforme
    // -----------------------------------------------------------
    #include "NKMath/NkLegacySystem.h"

    void PlatformSpecificMath() {
        #if NKENTSEU_PLATFORM_WINDOWS
            // Code spécifique Windows utilisant <cmath> et <algorithm>
            float32 result = std::fmax(0.0f, ComputeValue());
        #elif NKENTSEU_PLATFORM_LINUX
            // Code spécifique Linux
            float32 result = std::clamp(ComputeValue(), 0.0f, 1.0f);
        #endif

        // Conteneur disponible via NKContainers
        nkentseu::NkVector<float32> results;
        results.Reserve(100);
    }
*/

// =============================================================================
// NOTES DE MIGRATION
// =============================================================================
/*
    NkLegacySystem.h est un header de transition. Pour migrer vers la nouvelle API :

    1. Remplacer #include "NKMath/NkLegacySystem.h" par :
       - Les headers spécifiques nécessaires (ex: "NKMath/Vector3.h")
       - Les headers NKCore/NKPlatform/NKContainers individuels si requis

    2. Remplacer les appels STL par les équivalents NKMath quand disponibles :
       - std::sqrt -> nkentseu::math::Sqrt
       - std::clamp -> nkentseu::math::Clamp
       - etc.

    3. Utiliser NKCore/NkLogger.h au lieu de <iostream> pour le logging production

    4. Supprimer NkLegacySystem.h une fois toute la migration effectuée
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================