// =============================================================================
// NKTime/NKTime.h
// Header d'inclusion unique — importe tout le module NKTime.
//
// Design :
//  - Umbrella header pour une inclusion simplifiée du module NKTime
//  - Ordre d'inclusion respectant les dépendances internes du module
//  - Remplacement de NkTimeExport.h par NkTimeApi.h (nouveau standard)
//  - Aucune logique : uniquement des directives d'inclusion
//
// Règles d'utilisation :
//  - Dans les projets consommateurs : #include "NKTime/NKTime.h" suffit
//  - Dans les fichiers internes au module : inclure les headers spécifiques
//  - Éviter l'inclusion multiple dans un même fichier de traduction
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKTIME_H
#define NKENTSEU_TIME_NKTIME_H

    // -------------------------------------------------------------------------
    // SECTION 1 : API D'EXPORT DU MODULE
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime en premier.
    // Remplace l'ancien NkTimeExport.h selon le nouveau standard du projet.
    // Définit NKENTSEU_TIME_API et les macros de convenance associées.

    #include "NKTime/NkTimeApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONSTANTES DE CONVERSION
    // -------------------------------------------------------------------------
    // Source de vérité unique pour toutes les constantes temporelles.
    // Doit être inclus avant les classes qui l'utilisent (NkTime, NkTimeSpan, etc.).

    #include "NKTime/NkTimeConstants.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : TYPES DE BASE — MESURE ET DURÉE
    // -------------------------------------------------------------------------
    // NkElapsedTime : résultat de mesure (float64, 4 unités précalculées)
    // NkDuration    : durée à spécifier (int64 ns, API mutable)
    // Ces types sont indépendants et n'ont pas de dépendances circulaires.

    #include "NKTime/NkElapsedTime.h"
    #include "NKTime/NkDuration.h"

    // -------------------------------------------------------------------------
    // SECTION 4 : CHRONOMÈTRE ET ORCHESTRATEUR
    // -------------------------------------------------------------------------
    // NkChrono : chronomètre haute précision + primitives sleep/yield
    // NkClock  : orchestrateur de frame pour game loop (utilise NkChrono)
    // NkClock dépend de NkChrono, donc inclusion dans cet ordre.

    #include "NKTime/NkChrono.h"
    #include "NKTime/NkClock.h"

    // -------------------------------------------------------------------------
    // SECTION 5 : COMPOSANTS CALENDRIERS
    // -------------------------------------------------------------------------
    // NkDate     : date grégorienne (année, mois, jour) avec validation
    // NkTimes    : heure quotidienne (HH:MM:SS.mmm.nnnnnn) sans dépendance STL
    // NkTimeSpan : durée signée avec décomposition calendaire (analogue TimeSpan)
    // NkTimeZone : fuseaux horaires et conversions UTC/local avec DST

    #include "NKTime/NkDate.h"
    #include "NKTime/NkTimes.h"
    #include "NKTime/NkTimeSpan.h"
    #include "NKTime/NkTimeZone.h"

#endif // NKENTSEU_TIME_NKTIME_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTIME.H
// =============================================================================
// Ce header umbrella permet d'importer tout le module NKTime en une seule
// inclusion. Idéal pour les projets consommateurs qui utilisent plusieurs
// composants du module.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Inclusion simplifiée pour un projet consommateur
// -----------------------------------------------------------------------------
/*
    // Dans un fichier de votre application :
    #include "NKTime/NKTime.h"  // Une seule ligne pour tout le module

    namespace myapp {

        void GameLoop() {
            using namespace nkentseu;

            // Tous les types du module sont disponibles :
            NkClock clock;                          // Orchestrateur de frame
            NkChrono timer;                         // Chronomètre haute précision
            NkDuration timeout = NkDuration::FromSeconds(5);  // Durée à spécifier
            NkDate today = NkDate::GetCurrent();    // Date courante
            NkTime now = NkTime::GetCurrent();      // Heure courante
            NkTimeZone tz = NkTimeZone::GetLocal(); // Fuseau local

            while (IsRunning()) {
                const auto& t = clock.Tick();

                // Mise à jour avec time scale
                // UpdateLogic(t.Scaled());

                // Sleep si nécessaire
                if (t.fps > 120.f) {
                    NkChrono::SleepMilliseconds(1);
                }
            }
        }

    } // namespace myapp
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Inclusion sélective pour les fichiers internes au module
// -----------------------------------------------------------------------------
/*
    // Dans un fichier d'implémentation interne au module NKTime :
    // Préférer l'inclusion spécifique pour réduire les dépendances et
    // accélérer la compilation incrémentale.

    #include "NKTime/NkChrono.h"      // Uniquement ce qui est nécessaire
    #include "NKTime/NkTimeConstants.h"

    // Éviter :
    // #include "NKTime/NKTime.h"  // Trop lourd pour un fichier interne

    namespace nkentseu {

        // Implémentation utilisant uniquement NkChrono et les constantes
        void InternalTimingFunction() {
            NkChrono chrono;
            // ... travail ...
            auto elapsed = chrono.Elapsed();
            // Utilisation de time::NS_PER_SECOND, etc.
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Pattern de précompilation avec l'umbrella header
// -----------------------------------------------------------------------------
/*
    // Dans pch.h (Precompiled Header) du projet consommateur :

    #pragma once

    // Headers système
    #include <cstdint>
    #include <cstdio>

    // Headers NKEntseu — module NKTime via umbrella
    #include "NKTime/NKTime.h"

    // Autres modules si nécessaires
    // #include "NKCore/NKCore.h"
    // #include "NKPlatform/NKPlatform.h"

    // Dans les fichiers sources du projet :
    // #include "pch.h"  // Toujours en premier
    // ... code utilisant les types NKTime ...
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Éviter les inclusions multiples dans un même TU
// -----------------------------------------------------------------------------
/*
    // ❌ À éviter : inclusion redondante dans un même fichier de traduction

    #include "NKTime/NKTime.h"      // Inclut déjà NkChrono.h
    #include "NKTime/NkChrono.h"    // Redondant, inutile

    // ✅ Préférer : une seule inclusion umbrella OU les spécifiques

    // Option A : Umbrella pour simplicité
    #include "NKTime/NKTime.h"

    // Option B : Spécifiques pour contrôle fin des dépendances
    #include "NKTime/NkChrono.h"
    #include "NKTime/NkClock.h"
    #include "NKTime/NkDuration.h"

    // Les guards #ifndef dans chaque header empêchent les inclusions multiples,
    // mais éviter la redondance améliore la clarté et la vitesse de compilation.
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans un header public de bibliothèque
// -----------------------------------------------------------------------------
/*
    // Dans MyLibrary/Public/TimeUtils.h :

    #pragma once

    // Forward declarations pour minimiser les dépendances transitives
    namespace nkentseu {
        class NkDuration;      // Forward decl seulement
        class NkElapsedTime;   // Forward decl seulement
    }

    namespace mylib {

        // API publique utilisant les types NKTime par référence/pointeur
        // pour éviter d'exposer les headers NKTime aux consommateurs
        class Timer {
        public:
            void Start();
            void Stop();

            // Retour par valeur : nécessite l'inclusion complète dans le .cpp
            nkentseu::NkElapsedTime GetElapsed() const;

        private:
            // Membre privé : définition complète nécessaire dans le .cpp
            // Donc le .cpp inclura "NKTime/NKTime.h" ou "NKTime/NkChrono.h"
            void* mImpl;  // PImpl pour cacher l'implémentation
        };

    } // namespace mylib

    // Dans MyLibrary/Src/TimeUtils.cpp :
    #include "pch.h"
    #include "NKTime/NKTime.h"  // Inclusion complète uniquement dans le .cpp
    #include "MyLibrary/Public/TimeUtils.h"

    // Implémentation avec accès complet aux types NKTime...
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Configuration CMake avec l'umbrella header
// -----------------------------------------------------------------------------
/*
    # Dans CMakeLists.txt du projet consommateur :

    cmake_minimum_required(VERSION 3.15)
    project(MyApp VERSION 1.0.0)

    # Trouver le package NKTime (si installé via install(TARGETS))
    find_package(NKTime REQUIRED)

    add_executable(myapp src/main.cpp)

    # Liaison avec la cible NKTime::NKTime
    target_link_libraries(myapp PRIVATE NKTime::NKTime)

    # L'umbrella header est automatiquement disponible via les include directories
    # Définis par la cible NKTime::NKTime dans sa configuration d'export

    # Dans src/main.cpp :
    // #include "NKTime/NKTime.h"  // Disponible grâce à target_link_libraries

    # Alternative : inclusion manuelle des directories si pas de package config
    # target_include_directories(myapp PRIVATE
    #     ${NKTime_INCLUDE_DIRS}  # Variable définie par FindNKTime.cmake
    # )
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Tests unitaires avec l'umbrella header
// -----------------------------------------------------------------------------
/*
    // Dans tests/NkTimeTests.cpp :

    #include "pch.h"
    #include "NKTime/NKTime.h"        // Tous les types disponibles
    #include "NKCore/Assert/NkAssert.h"
    #include <gtest/gtest.h>          // Framework de tests

    TEST(NkTimeModule, UmbrellaHeaderIncludesAll) {
        using namespace nkentseu;

        // Vérifier que tous les types principaux sont accessibles
        NkChrono chrono;
        NkClock clock;
        NkDuration duration = NkDuration::FromMilliseconds(100);
        NkElapsedTime elapsed = chrono.Elapsed();
        NkDate date(2024, 12, 25);
        NkTime time(14, 30, 0);
        NkTimeSpan span = NkTimeSpan::FromHours(2);
        NkTimeZone tz = NkTimeZone::GetUtc();

        // Tests basiques de compilation et d'instanciation
        EXPECT_NO_THROW(chrono.Reset());
        EXPECT_NO_THROW(clock.Tick());
        EXPECT_TRUE(duration.ToMilliseconds() == 100);
        EXPECT_TRUE(date.GetYear() == 2024);
        EXPECT_TRUE(time.GetHour() == 14);
        EXPECT_TRUE(span.GetHours() == 2);
        EXPECT_TRUE(tz.GetKind() == NkTimeZone::NkKind::NK_UTC);
    }

    // L'umbrella header simplifie l'écriture des tests en évitant
    // d'avoir à gérer manuellement l'ordre des inclusions.
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Migration depuis l'ancien NkTimeExport.h
// -----------------------------------------------------------------------------
/*
    // Ancien code (à migrer) :
    // #include "NKTime/NkTimeExport.h"  // Obsolète

    // Nouveau code (standard actuel) :
    // #include "NKTime/NkTimeApi.h"     // Via l'umbrella ou directement

    // Si votre code incluait explicitement NkTimeExport.h :
    // 1. Remplacer par #include "NKTime/NKTime.h" pour l'umbrella
    // 2. OU remplacer par #include "NKTime/NkTimeApi.h" pour l'API seule

    // Les macros d'export sont maintenant centralisées dans NkTimeApi.h :
    // - NKENTSEU_TIME_API pour les fonctions libres
    // - NKENTSEU_TIME_CLASS_EXPORT pour les classes/structs
    // - NKENTSEU_TIME_API_INLINE, etc. pour les variantes inline

    // Migration guide :
    // Ancien                                    → Nouveau
    // -----------------------------------------------------------
    // NKTIME_API                                → NKENTSEU_TIME_API
    // NKTIME_CLASS_EXPORT                       → NKENTSEU_TIME_CLASS_EXPORT
    // #include "NKTime/NkTimeExport.h"          → #include "NKTime/NkTimeApi.h"
    // (ou via umbrella)                         → #include "NKTime/NKTime.h"
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================