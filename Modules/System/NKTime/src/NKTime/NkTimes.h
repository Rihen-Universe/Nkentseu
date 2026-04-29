// =============================================================================
// NKTime/NkTimes.h
// Temps quotidien avec précision nanoseconde — sans dépendance STL.
//
// Design :
//  - Stockage de 5 composants : heure [0-23], minute [0-59], seconde [0-59],
//    milliseconde [0-999], nanoseconde [0-999999]
//  - Constantes de conversion provenant de NkTimeConstants.h (source unique)
//  - Opérateurs arithmétiques via conversion int64 (total en ns) pour cohérence
//  - Aucune dépendance à la STL pour compatibilité embarquée et performance
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions inline définies dans le header
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKTIMES_H
#define NKENTSEU_TIME_NKTIMES_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion des constantes de conversion centralisées
    // Inclusion des détections plateforme et types de base

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkTimeConstants.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"

    // Inclusion de <ctime> pour les fonctions time_t et tm
    #include <ctime>

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkTime représente un instant dans la journée avec une précision
    // nanoseconde. Elle est conçue pour être légère, copiable et comparable.

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkTime {

            public:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.1 : Constructeurs
                // -------------------------------------------------------------

                /// Initialise tous les composants à zéro (représente minuit).
                /// @note Constructeur par défaut noexcept pour performance.
                NkTime() noexcept;

                /**
                 * @brief Constructeur avec paramètres explicites pour chaque composant.
                 * @param hour        Heure [0-23]
                 * @param minute      Minute [0-59]
                 * @param second      Seconde [0-59]
                 * @param millisecond Milliseconde [0-999] (défaut : 0)
                 * @param nanosecond  Nanoseconde [0-999999] (défaut : 0)
                 * @note Les valeurs hors plage sont normalisées automatiquement.
                 */
                NkTime(
                    int32 hour,
                    int32 minute,
                    int32 second,
                    int32 millisecond = 0,
                    int32 nanosecond = 0
                );

                /// Construit depuis un total de nanosecondes écoulées depuis minuit.
                /// @param totalNanoseconds Nanosecondes totales depuis 00:00:00
                /// @note Conversion automatique vers les 5 composants temporels.
                explicit NkTime(int64 totalNanoseconds) noexcept;

                // Constructeur de copie par défaut (noexcept pour optimisation)
                NkTime(const NkTime&) noexcept = default;

                // Opérateur d'affectation par défaut (noexcept pour optimisation)
                NkTime& operator=(const NkTime&) noexcept = default;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.2 : Accesseurs (Getters)
                // -------------------------------------------------------------
                // Toutes les méthodes d'accès sont const, noexcept et inline
                // pour permettre l'optimisation par le compilateur.

                /// @return Heure courante [0-23]
                NKTIME_NODISCARD int32 GetHour() const noexcept {
                    return mHour;
                }

                /// @return Minute courante [0-59]
                NKTIME_NODISCARD int32 GetMinute() const noexcept {
                    return mMinute;
                }

                /// @return Seconde courante [0-59]
                NKTIME_NODISCARD int32 GetSecond() const noexcept {
                    return mSecond;
                }

                /// @return Milliseconde courante [0-999]
                NKTIME_NODISCARD int32 GetMillisecond() const noexcept {
                    return mMillisecond;
                }

                /// @return Nanoseconde courante [0-999999]
                NKTIME_NODISCARD int32 GetNanosecond() const noexcept {
                    return mNanosecond;
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.3 : Mutateurs (Setters)
                // -------------------------------------------------------------
                // Les mutateurs valident et normalisent les valeurs d'entrée.
                // Ils utilisent NKENTSEU_ASSERT_MSG pour le debugging en mode dev.

                /// Définit l'heure. Doit être dans [0, 23].
                /// @param hour Valeur de l'heure à définir
                void SetHour(int32 hour);

                /// Définit la minute. Doit être dans [0, 59].
                /// @param minute Valeur de la minute à définir
                void SetMinute(int32 minute);

                /// Définit la seconde. Doit être dans [0, 59].
                /// @param second Valeur de la seconde à définir
                void SetSecond(int32 second);

                /// Définit la milliseconde. Doit être dans [0, 999].
                /// @param millis Valeur de la milliseconde à définir
                void SetMillisecond(int32 millis);

                /// Définit la nanoseconde. Doit être dans [0, 999999].
                /// @param nano Valeur de la nanoseconde à définir
                void SetNanosecond(int32 nano);

                // -------------------------------------------------------------
                // SOUS-SECTION 3.4 : Opérateurs arithmétiques
                // -------------------------------------------------------------
                // Permettent d'ajouter ou soustraire des durées temporelles.
                // L'implémentation passe par la conversion en int64 (ns totaux)
                // pour garantir la cohérence des calculs et gérer les dépassements.

                /// Ajoute une durée à cet instant. Gère les dépassements de jour.
                /// @param o Durée à ajouter
                /// @return Référence vers cet objet pour chaînage
                NkTime& operator+=(const NkTime& o) noexcept;

                /// Soustrait une durée de cet instant. Gère les valeurs négatives.
                /// @param o Durée à soustraire
                /// @return Référence vers cet objet pour chaînage
                NkTime& operator-=(const NkTime& o) noexcept;

                /// Retourne un nouvel instant résultant de l'addition.
                /// @param o Durée à ajouter
                /// @return Nouvel objet NkTime avec le résultat
                NKTIME_NODISCARD NkTime operator+(const NkTime& o) const noexcept;

                /// Retourne un nouvel instant résultant de la soustraction.
                /// @param o Durée à soustraire
                /// @return Nouvel objet NkTime avec le résultat
                NKTIME_NODISCARD NkTime operator-(const NkTime& o) const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.5 : Opérateurs de comparaison
                // -------------------------------------------------------------
                // Comparaisons basées sur la valeur totale en nanosecondes.
                // Toutes les méthodes sont noexcept pour garantir l'absence d'exceptions.

                /// Égalité : vrai si tous les composants sont identiques.
                /// @param o Autre objet à comparer
                /// @return true si égaux, false sinon
                NKTIME_NODISCARD bool operator==(const NkTime& o) const noexcept;

                /// Inégalité : inverse de operator==
                /// @param o Autre objet à comparer
                /// @return true si différents, false sinon
                NKTIME_NODISCARD bool operator!=(const NkTime& o) const noexcept;

                /// Infériorité stricte : vrai si cet instant est avant l'autre.
                /// @param o Autre objet à comparer
                /// @return true si *this < o, false sinon
                NKTIME_NODISCARD bool operator<(const NkTime& o) const noexcept;

                /// Infériorité ou égalité
                /// @param o Autre objet à comparer
                /// @return true si *this <= o, false sinon
                NKTIME_NODISCARD bool operator<=(const NkTime& o) const noexcept;

                /// Supériorité stricte : vrai si cet instant est après l'autre.
                /// @param o Autre objet à comparer
                /// @return true si *this > o, false sinon
                NKTIME_NODISCARD bool operator>(const NkTime& o) const noexcept;

                /// Supériorité ou égalité
                /// @param o Autre objet à comparer
                /// @return true si *this >= o, false sinon
                NKTIME_NODISCARD bool operator>=(const NkTime& o) const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.6 : Conversions de type explicites
                // -------------------------------------------------------------
                // Permettent de convertir un NkTime vers des types numériques.
                // Toutes sont explicit pour éviter les conversions implicites accidentelles.

                /// Conversion vers float : secondes avec précision milliseconde.
                /// @return Valeur en secondes (partie décimale : millisecondes)
                explicit operator float() const noexcept;

                /// Conversion vers double : secondes avec précision nanoseconde.
                /// @return Valeur en secondes (partie décimale : nanosecondes)
                explicit operator double() const noexcept;

                /// Conversion vers int64 : nanosecondes totales depuis minuit.
                /// @return Nombre total de nanosecondes écoulées depuis 00:00:00
                explicit operator int64() const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.7 : Méthodes statiques utilitaires
                // -------------------------------------------------------------

                /// Obtient l'heure système locale actuelle avec précision nanoseconde.
                /// @return Objet NkTime représentant l'heure courante
                /// @note Thread-safe : utilise les fonctions système appropriées
                NKTIME_NODISCARD static NkTime GetCurrent();

                // -------------------------------------------------------------
                // SOUS-SECTION 3.8 : Formatage et sérialisation
                // -------------------------------------------------------------

                /// Convertit cet instant en chaîne formatée ISO 8601 étendu.
                /// Format : "HH:MM:SS.mmm.nnnnnn"
                /// @return NkString contenant la représentation textuelle
                NKTIME_NODISCARD NkString ToString() const;

                /// Fonction amie pour formatage via ADL (Argument-Dependent Lookup)
                /// @param t Objet NkTime à formater
                /// @return NkString contenant la représentation textuelle
                friend NkString ToString(const NkTime& t) {
                    return t.ToString();
                }

            private:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.9 : Membres privés
                // -------------------------------------------------------------
                // Stockage des 5 composants temporels. Initialisés à zéro.
                // L'ordre des membres est optimisé pour l'alignement mémoire.

                int32 mHour = 0;                    // Heure [0-23]
                int32 mMinute = 0;                  // Minute [0-59]
                int32 mSecond = 0;                  // Seconde [0-59]
                int32 mMillisecond = 0;             // Milliseconde [0-999]
                int32 mNanosecond = 0;              // Nanoseconde [0-999999]

                // -------------------------------------------------------------
                // SOUS-SECTION 3.10 : Méthodes internes privées
                // -------------------------------------------------------------
                // Ces méthodes ne font pas partie de l'API publique.
                // Elles sont utilisées pour la validation et normalisation interne.

                /// Normalise tous les composants dans leurs plages valides.
                /// @note Gère les dépassements par modulo et propagation
                void Normalize() noexcept;

                /// Valide que tous les composants sont dans leurs plages.
                /// @note Utilise NKENTSEU_ASSERT_MSG en mode debug uniquement
                void Validate() const;

        }; // class NkTime

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKTIMES_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTIMES.H
// =============================================================================
// La classe NkTime permet de manipuler des instants dans la journée avec
// une précision nanoseconde, sans dépendance à la STL pour une portabilité
// maximale et des performances optimales.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création et accès aux composants
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"

    void TimeAccessExample() {
        using namespace nkentseu;

        // Création avec constructeur paramétré
        NkTime time(14, 30, 45, 123, 456789);  // 14:30:45.123.456789

        // Accès aux composants via les getters (inline, noexcept)
        int32 h = time.GetHour();           // 14
        int32 m = time.GetMinute();         // 30
        int32 s = time.GetSecond();         // 45
        int32 ms = time.GetMillisecond();   // 123
        int32 ns = time.GetNanosecond();    // 456789

        // Modification via les setters (avec validation)
        time.SetHour(15);
        time.SetMinute(0);
        // time.SetHour(25);  // Déclencherait un assert en mode debug
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Arithmétique temporelle
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"

    void TimeArithmeticExample() {
        using namespace nkentseu;

        // Création de deux instants
        NkTime start(10, 0, 0);   // 10:00:00.000.000000
        NkTime duration(2, 30, 0); // 2h 30min de durée

        // Addition : calcul de l'heure de fin
        NkTime end = start + duration;  // 12:30:00.000.000000

        // Soustraction : calcul de durée écoulée
        NkTime elapsed = end - start;   // 2:30:00.000.000000

        // Opérateurs composés pour mise à jour in-place
        NkTime counter(0, 0, 0);
        counter += duration;  // counter vaut maintenant 2:30:00
        counter -= NkTime(0, 15, 0);  // counter vaut maintenant 2:15:00
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Comparaisons et tri
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"
    #include <vector>
    #include <algorithm>

    void TimeComparisonExample() {
        using namespace nkentseu;

        std::vector<NkTime> schedule;
        schedule.emplace_back(9, 0, 0);
        schedule.emplace_back(14, 30, 0);
        schedule.emplace_back(11, 15, 0);

        // Tri chronologique utilisant operator<
        std::sort(schedule.begin(), schedule.end());

        // Recherche d'un créneau
        NkTime target(11, 15, 0);
        if (std::find(schedule.begin(), schedule.end(), target) != schedule.end()) {
            // Créneau trouvé
        }

        // Vérification d'ordre
        if (schedule[0] < schedule[1]) {
            // Premier événement avant le second
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Conversions numériques et formatage
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"
    #include <iostream>

    void TimeConversionExample() {
        using namespace nkentseu;

        NkTime time(1, 30, 45, 500, 123456);

        // Conversion vers nanosecondes totales
        int64 totalNs = static_cast<int64>(time);
        // totalNs = 1*3600 + 30*60 + 45 seconds + 500ms + 123456ns

        // Conversion vers secondes (double précision)
        double totalSec = static_cast<double>(time);
        // totalSec ≈ 5445.500123456

        // Conversion vers secondes (simple précision)
        float totalSecF = static_cast<float>(time);

        // Formatage en chaîne lisible
        NkString formatted = time.ToString();
        // formatted = "01:30:45.500.123456"

        // Formatage via fonction amie (ADL)
        NkString alsoFormatted = ToString(time);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Obtention de l'heure système courante
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"
    #include "NKCore/Logger/NkLogger.h"

    void GetCurrentTimeExample() {
        using namespace nkentseu;

        // Récupération de l'heure locale actuelle (thread-safe)
        NkTime now = NkTime::GetCurrent();

        // Logging de l'heure courante
        NK_LOG_INFO("Current time: {}", now.ToString());

        // Utilisation dans une boucle de jeu
        NkTime frameStart = NkTime::GetCurrent();

        // ... logique de frame ...

        NkTime frameEnd = NkTime::GetCurrent();
        double frameDuration = static_cast<double>(frameEnd - frameStart);

        NK_LOG_DEBUG("Frame duration: {} seconds", frameDuration);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Gestion des dépassements et normalisation
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"

    void TimeNormalizationExample() {
        using namespace nkentseu;

        // Construction avec valeurs hors plage : normalisation automatique
        NkTime time(25, 70, 80);
        // Résultat normalisé : 02:11:20 (dépassements propagés)

        // Addition avec dépassement de jour
        NkTime late(23, 45, 0);
        NkTime added = late + NkTime(1, 30, 0);
        // added vaut 01:15:00 (jour suivant, mais NkTime ne gère pas les dates)

        // Pour gérer les dates, utiliser NkDateTime (module séparé)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Intégration avec d'autres modules NKEntseu
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimes.h"
    #include "NKCore/Config/NkConfig.h"
    #include "NKPlatform/NkPlatformDetect.h"

    namespace nkentseu {

        class TimeAwareComponent {
        public:
            TimeAwareComponent()
                : m_lastUpdate(NkTime::GetCurrent())
                , m_updateInterval(0, 0, 1)  // 1 seconde
            {
                // Initialisation avec heure courante
            }

            bool ShouldUpdate() {
                NkTime now = NkTime::GetCurrent();
                NkTime elapsed = now - m_lastUpdate;

                // Comparaison avec l'intervalle configuré
                return elapsed >= m_updateInterval;
            }

            void MarkUpdated() {
                m_lastUpdate = NkTime::GetCurrent();
            }

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Code spécifique Windows utilisant les macros de détection
            void PlatformSpecificSync() {
                // Synchronisation avec le timer Windows...
            }
        #endif

        private:
            NkTime m_lastUpdate;
            NkTime m_updateInterval;
        };

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================