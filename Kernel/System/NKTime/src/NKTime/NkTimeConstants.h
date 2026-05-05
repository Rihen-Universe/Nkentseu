// =============================================================================
// NKTime/NkTimeConstants.h
// Constantes de conversion temporelle — source de vérité unique du module NKTime.
//
// Design :
//  - Centralisation UNIQUE de toutes les constantes de conversion temporelle
//  - Élimination des duplications précédentes dans NkDuration.cpp, NkTime.h, etc.
//  - Utilisation de constexpr pour évaluation à la compilation
//  - Types explicites (int32/int64) via NKCore/NkTypes.h pour portabilité
//
// Règles d'utilisation :
//  - Dans les .h  : inclure directement ce fichier
//  - Dans les .cpp: l'inclusion transite via le header correspondant
//  - JAMAIS de "using namespace nkentseu::time" dans les headers publics
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKTIMECONSTANTS_H
#define NKENTSEU_TIME_NKTIMECONSTANTS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des types de base définis par NKCore pour garantir la cohérence
    // des tailles de types sur toutes les plateformes supportées.

    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL NKENTSEU
    // -------------------------------------------------------------------------
    // Toutes les constantes temps sont regroupées dans le namespace nkentseu::time
    // pour éviter les conflits de nommage et organiser logiquement le code.

    namespace nkentseu {

        // Namespace interne pour les constantes de conversion temporelle
        namespace time {

            // -----------------------------------------------------------------
            // SOUS-SECTION 2.1 : Facteurs de conversion montants (ns → unité)
            // -----------------------------------------------------------------
            // Ces constantes permettent de convertir des nanosecondes vers
            // des unités de temps plus grandes. Toutes sont exprimées en int64
            // pour supporter les grandes valeurs sans overflow.

            // Nanosecondes dans une microseconde : 1'000
            static constexpr int64 NS_PER_MICROSECOND = 1'000LL;

            // Nanosecondes dans une milliseconde : 1'000'000
            static constexpr int64 NS_PER_MILLISECOND = 1'000'000LL;

            // Nanosecondes dans une seconde : 1'000'000'000
            static constexpr int64 NS_PER_SECOND = 1'000'000'000LL;

            // Nanosecondes dans une minute : 60 secondes
            static constexpr int64 NS_PER_MINUTE = 60LL * NS_PER_SECOND;

            // Nanosecondes dans une heure : 3'600 secondes
            static constexpr int64 NS_PER_HOUR = 3'600LL * NS_PER_SECOND;

            // Nanosecondes dans un jour : 86'400 secondes
            static constexpr int64 NS_PER_DAY = 86'400LL * NS_PER_SECOND;

            // -----------------------------------------------------------------
            // SOUS-SECTION 2.2 : Facteurs de conversion en secondes
            // -----------------------------------------------------------------
            // Constantes utilitaires pour les calculs basés sur les secondes.
            // Utiles pour les conversions intermédiaires et les validations.

            // Secondes dans une minute
            static constexpr int64 SECONDS_PER_MINUTE = 60LL;

            // Secondes dans une heure
            static constexpr int64 SECONDS_PER_HOUR = 3'600LL;

            // Secondes dans un jour
            static constexpr int64 SECONDS_PER_DAY = 86'400LL;

            // -----------------------------------------------------------------
            // SOUS-SECTION 2.3 : Bornes des composants de NkTime
            // -----------------------------------------------------------------
            // Ces constantes définissent les plages valides pour chaque composant
            // temporel de la classe NkTime. Elles sont utilisées pour la validation
            // et la normalisation des valeurs temporelles.

            // Heures dans un jour : plage [0, 23]
            static constexpr int32 HOURS_PER_DAY = 24;

            // Minutes dans une heure : plage [0, 59]
            static constexpr int32 MINUTES_PER_HOUR = 60;

            // Secondes dans une minute (version int32) : plage [0, 59]
            static constexpr int32 SECONDS_PER_MINUTE_I32 = 60;

            // Millisecondes dans une seconde : plage [0, 999]
            static constexpr int32 MILLISECONDS_PER_SECOND = 1'000;

            // Microsecondes dans une milliseconde : plage [0, 999]
            static constexpr int32 MICROSECONDS_PER_MILLISECOND = 1'000;

            // Nanosecondes dans une milliseconde : plage [0, 999'999]
            // Utilisé pour extraire la partie nanoseconde résiduelle
            static constexpr int32 NANOSECONDS_PER_MILLISECOND = 1'000'000;

        } // namespace time

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKTIMECONSTANTS_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTIMECONSTANTS.H
// =============================================================================
// Ce fichier fournit les constantes de conversion temporelle centralisées
// pour le module NKTime. Toutes les constantes sont constexpr pour une
// évaluation à la compilation et une optimisation maximale.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Conversion simple de nanosecondes vers millisecondes
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeConstants.h"

    void ConvertTimeExample() {
        using namespace nkentseu::time;

        // Valeur en nanosecondes
        int64 nanos = 1'500'000'000LL;

        // Conversion vers millisecondes utilisant les constantes
        int64 millis = nanos / NS_PER_MILLISECOND;  // Résultat : 1500

        // Conversion vers secondes
        int64 seconds = nanos / NS_PER_SECOND;  // Résultat : 1
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Validation d'entrée utilisateur avec les bornes
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeConstants.h"
    #include "NKCore/Assert/NkAssert.h"

    bool ValidateTimeInput(int32 hour, int32 minute, int32 second) {
        using namespace nkentseu::time;

        // Vérification des plages valides utilisant les constantes centralisées
        if (hour < 0 || hour >= HOURS_PER_DAY) {
            return false;
        }
        if (minute < 0 || minute >= MINUTES_PER_HOUR) {
            return false;
        }
        if (second < 0 || second >= SECONDS_PER_MINUTE_I32) {
            return false;
        }

        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Calcul de durée totale en nanosecondes
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeConstants.h"

    int64 ComputeTotalNanoseconds(int32 hours, int32 minutes, int32 seconds) {
        using namespace nkentseu::time;

        // Calcul utilisant les facteurs de conversion
        int64 totalNs = 0;
        totalNs += static_cast<int64>(hours) * NS_PER_HOUR;
        totalNs += static_cast<int64>(minutes) * NS_PER_MINUTE;
        totalNs += static_cast<int64>(seconds) * NS_PER_SECOND;

        return totalNs;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Formatage manuel d'un timestamp
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeConstants.h"
    #include <cstdio>

    void FormatTimestamp(int64 totalNanoseconds) {
        using namespace nkentseu::time;

        // Extraction des composants depuis les nanosecondes totales
        int32 nanos = static_cast<int32>(totalNanoseconds % NANOSECONDS_PER_MILLISECOND);
        totalNanoseconds /= NANOSECONDS_PER_MILLISECOND;

        int32 millis = static_cast<int32>(totalNanoseconds % MILLISECONDS_PER_SECOND);
        totalNanoseconds /= MILLISECONDS_PER_SECOND;

        int32 secs = static_cast<int32>(totalNanoseconds % SECONDS_PER_MINUTE_I32);
        totalNanoseconds /= SECONDS_PER_MINUTE_I32;

        int32 mins = static_cast<int32>(totalNanoseconds % MINUTES_PER_HOUR);
        totalNanoseconds /= MINUTES_PER_HOUR;

        int32 hrs = static_cast<int32>(totalNanoseconds % HOURS_PER_DAY);

        // Affichage formaté
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer),
                      "%02d:%02d:%02d.%03d.%06d",
                      hrs, mins, secs, millis, nanos);
        // buffer contient maintenant "HH:MM:SS.mmm.nnnnnn"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans une classe avec normalisation
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeConstants.h"

    namespace nkentseu {

        class TimeComponent {
        public:
            void SetHours(int32 hours) {
                // Normalisation automatique utilisant les constantes
                m_hours = hours % time::HOURS_PER_DAY;
                if (m_hours < 0) {
                    m_hours += time::HOURS_PER_DAY;
                }
            }

            void AddMinutes(int32 minutes) {
                using namespace time;

                // Conversion et propagation des dépassements
                int64 totalMinutes = m_minutes + minutes;
                m_minutes = static_cast<int32>(totalMinutes % MINUTES_PER_HOUR);

                int32 hourOverflow = static_cast<int32>(totalMinutes / MINUTES_PER_HOUR);
                SetHours(m_hours + hourOverflow);
            }

        private:
            int32 m_hours = 0;
            int32 m_minutes = 0;
        };

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================