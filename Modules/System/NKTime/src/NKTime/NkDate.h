// =============================================================================
// NKTime/NkDate.h
// Date grégorienne validée — sans dépendance STL.
//
// Design :
//  - Représentation d'une date calendaire avec validation automatique des plages
//  - Plages valides : year [1601, 30827], month [1, 12], day [1, DaysInMonth]
//  - Aucune dépendance STL pour compatibilité embarquée et performance
//  - Thread-safe pour les méthodes statiques d'acquisition système
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

#ifndef NKENTSEU_TIME_NKDATE_H
#define NKENTSEU_TIME_NKDATE_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion des détections plateforme et types de base
    // Inclusion de la chaîne NKEntseu pour le formatage

    #include "NKTime/NkTimeApi.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"

    // Inclusion de <ctime> pour les fonctions time_t et tm
    #include <ctime>

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkDate représente une date du calendrier grégorien.
    // Elle inclut une validation automatique pour garantir l'intégrité des données.

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkDate {

            public:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.1 : Constructeurs
                // -------------------------------------------------------------

                /// Initialise avec la date système locale courante.
                /// @note Appel interne à GetCurrent() pour initialisation cohérente.
                NkDate() noexcept;

                /**
                 * @brief Construit une date spécifique avec validation automatique.
                 * @param year  Année valide dans [1601, 30827]
                 * @param month Mois valide dans [1, 12]
                 * @param day   Jour valide dans [1, DaysInMonth(year, month)]
                 * @note Déclenche NKENTSEU_ASSERT_MSG si les paramètres sont hors plage.
                 */
                NkDate(int32 year, int32 month, int32 day);

                // Constructeur de copie par défaut (noexcept pour optimisation)
                NkDate(const NkDate&) noexcept = default;

                // Opérateur d'affectation par défaut (noexcept pour optimisation)
                NkDate& operator=(const NkDate&) noexcept = default;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.2 : Accesseurs (Getters)
                // -------------------------------------------------------------
                // Toutes les méthodes d'accès sont const, noexcept et inline
                // pour permettre l'optimisation par le compilateur.

                /// @return Année courante [1601, 30827]
                NKTIME_NODISCARD int32 GetYear() const noexcept {
                    return year;
                }

                /// @return Mois courant [1, 12]
                NKTIME_NODISCARD int32 GetMonth() const noexcept {
                    return month;
                }

                /// @return Jour courant [1, 31 selon le mois]
                NKTIME_NODISCARD int32 GetDay() const noexcept {
                    return day;
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.3 : Mutateurs (Setters)
                // -------------------------------------------------------------
                // Les mutateurs valident les valeurs d'entrée via Validate().
                // Ils utilisent NKENTSEU_ASSERT_MSG pour le debugging en mode dev.

                /// Définit l'année. Doit être dans [1601, 30827].
                /// @param year Nouvelle valeur de l'année
                /// @note Re-valide la date complète après modification.
                void SetYear(int32 year);

                /// Définit le mois. Doit être dans [1, 12].
                /// @param month Nouvelle valeur du mois
                /// @note Re-valide la date complète après modification.
                void SetMonth(int32 month);

                /// Définit le jour. Doit être valide pour le mois/année courants.
                /// @param day Nouvelle valeur du jour
                /// @note Re-valide la date complète après modification.
                void SetDay(int32 day);

                // -------------------------------------------------------------
                // SOUS-SECTION 3.4 : Méthodes statiques utilitaires
                // -------------------------------------------------------------
                // Méthodes de classe fournissant des fonctionnalités communes
                // sans nécessiter d'instance de NkDate.

                /// Obtient la date système locale actuelle.
                /// @return Objet NkDate représentant la date courante
                /// @note Thread-safe : utilise localtime_r/localtime_s selon plateforme.
                NKTIME_NODISCARD static NkDate GetCurrent();

                /// Calcule le nombre de jours dans un mois donné.
                /// @param year Année de référence (pour détection année bissextile)
                /// @param month Mois [1, 12]
                /// @return Nombre de jours dans ce mois (28-31)
                /// @note Tient compte des années bissextiles pour février.
                NKTIME_NODISCARD static int32 DaysInMonth(int32 year, int32 month);

                /// Détermine si une année est bissextile selon le calendrier grégorien.
                /// @param year Année à tester
                /// @return true si bissextile, false sinon
                /// @note Règle : (y%4==0 && y%100!=0) || (y%400==0)
                NKTIME_NODISCARD static bool IsLeapYear(int32 year) noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.5 : Formatage et sérialisation
                // -------------------------------------------------------------
                // Méthodes de conversion vers des représentations textuelles.

                /// Convertit cette date en chaîne formatée ISO 8601.
                /// Format : "YYYY-MM-DD" avec zéros de remplissage.
                /// @return NkString contenant la représentation textuelle
                NKTIME_NODISCARD NkString ToString() const;

                /// Obtient le nom du mois en français.
                /// @return NkString contenant "Janvier", "Février", etc.
                /// @note Noms localisés en français uniquement (extension possible).
                NKTIME_NODISCARD NkString GetMonthName() const;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.6 : Opérateurs de comparaison
                // -------------------------------------------------------------
                // Comparaisons chronologiques basées sur l'ordre année/mois/jour.
                // Toutes les méthodes sont noexcept pour garantir l'absence d'exceptions.

                /// Égalité : vrai si année, mois et jour sont identiques.
                /// @param o Autre objet à comparer
                /// @return true si égaux, false sinon
                NKTIME_NODISCARD bool operator==(const NkDate& o) const noexcept;

                /// Inégalité : inverse de operator==
                /// @param o Autre objet à comparer
                /// @return true si différents, false sinon
                NKTIME_NODISCARD bool operator!=(const NkDate& o) const noexcept;

                /// Infériorité stricte : vrai si cette date est antérieure.
                /// @param o Autre objet à comparer
                /// @return true si *this < o, false sinon
                NKTIME_NODISCARD bool operator<(const NkDate& o) const noexcept;

                /// Infériorité ou égalité
                /// @param o Autre objet à comparer
                /// @return true si *this <= o, false sinon
                NKTIME_NODISCARD bool operator<=(const NkDate& o) const noexcept;

                /// Supériorité stricte : vrai si cette date est postérieure.
                /// @param o Autre objet à comparer
                /// @return true si *this > o, false sinon
                NKTIME_NODISCARD bool operator>(const NkDate& o) const noexcept;

                /// Supériorité ou égalité
                /// @param o Autre objet à comparer
                /// @return true si *this >= o, false sinon
                NKTIME_NODISCARD bool operator>=(const NkDate& o) const noexcept;

                /// Fonction amie pour formatage via ADL (Argument-Dependent Lookup).
                /// @param d Objet NkDate à formater
                /// @return NkString contenant la représentation textuelle
                friend NkString ToString(const NkDate& d) {
                    return d.ToString();
                }

            private:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.7 : Membres privés
                // -------------------------------------------------------------
                // Stockage des 3 composants de la date. Initialisés à 1970-01-01.
                // L'ordre des membres est optimisé pour l'alignement mémoire.

                int32 year = 1970;    // Année [1601, 30827]
                int32 month = 1;      // Mois [1, 12]
                int32 day = 1;        // Jour [1, 31 selon mois]

                // -------------------------------------------------------------
                // SOUS-SECTION 3.8 : Méthodes internes privées
                // -------------------------------------------------------------
                // Ces méthodes ne font pas partie de l'API publique.
                // Elles sont utilisées pour la validation interne des données.

                /// Valide que tous les composants sont dans leurs plages valides.
                /// @note Utilise NKENTSEU_ASSERT_MSG en mode debug uniquement.
                /// @note Vérifie aussi la cohérence jour/mois/année (ex: 30 février).
                void Validate() const;

        }; // class NkDate

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKDATE_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKDATE.H
// =============================================================================
// La classe NkDate permet de manipuler des dates du calendrier grégorien
// avec validation automatique, sans dépendance à la STL pour une portabilité
// maximale et des performances optimales.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création et accès aux composants
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"

    void DateAccessExample() {
        using namespace nkentseu;

        // Création avec date spécifique
        NkDate date(2024, 12, 25);  // 25 décembre 2024

        // Accès aux composants via les getters (inline, noexcept)
        int32 y = date.GetYear();   // 2024
        int32 m = date.GetMonth();  // 12
        int32 d = date.GetDay();    // 25

        // Modification via les setters (avec validation)
        date.SetYear(2025);
        date.SetMonth(1);
        date.SetDay(1);
        // date.SetDay(32);  // Déclencherait un assert en mode debug
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Date courante et méthodes statiques
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"
    #include "NKCore/Logger/NkLogger.h"

    void CurrentDateExample() {
        using namespace nkentseu;

        // Obtention de la date système locale actuelle
        NkDate today = NkDate::GetCurrent();

        // Logging de la date courante
        NK_LOG_INFO("Today is: {}", today.ToString());

        // Utilisation des méthodes statiques utilitaires
        int32 year = today.GetYear();
        bool isLeap = NkDate::IsLeapYear(year);
        int32 daysInFeb = NkDate::DaysInMonth(year, 2);

        NK_LOG_DEBUG("Year {}: {} (February has {} days)",
                     year,
                     isLeap ? "leap" : "common",
                     daysInFeb);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Comparaisons et tris de dates
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"
    #include <vector>
    #include <algorithm>

    void DateComparisonExample() {
        using namespace nkentseu;

        std::vector<NkDate> events;
        events.emplace_back(2024, 1, 15);
        events.emplace_back(2024, 3, 8);
        events.emplace_back(2024, 2, 20);

        // Tri chronologique utilisant operator<
        std::sort(events.begin(), events.end());

        // Recherche d'une date spécifique
        NkDate target(2024, 2, 20);
        auto it = std::find(events.begin(), events.end(), target);
        if (it != events.end()) {
            NK_LOG_INFO("Event found on: {}", it->ToString());
        }

        // Vérification de plages de dates
        NkDate start(2024, 1, 1);
        NkDate end(2024, 12, 31);
        for (const auto& event : events) {
            if (event >= start && event <= end) {
                // Événement dans la plage annuelle 2024
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Formatage et localisation
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"
    #include <iostream>

    void DateFormattingExample() {
        using namespace nkentseu;

        NkDate date(2024, 7, 14);

        // Formatage ISO 8601 standard
        NkString iso = date.ToString();
        // iso = "2024-07-14"

        // Nom du mois en français
        NkString monthName = date.GetMonthName();
        // monthName = "Juillet"

        // Formatage personnalisé via fonction amie (ADL)
        NkString alsoFormatted = ToString(date);

        // Affichage lisible
        std::printf("Date: %s (%s)\n", iso.c_str(), monthName.c_str());
        // Output: Date: 2024-07-14 (Juillet)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Validation automatique et gestion des erreurs
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"
    #include "NKCore/Assert/NkAssert.h"

    void DateValidationExample() {
        using namespace nkentseu;

        // Cas valides : construction sans erreur
        NkDate d1(2024, 2, 29);  // OK : 2024 est bissextile
        NkDate d2(2023, 2, 28);  // OK : jour valide pour février non-bissextile

        // Cas invalides : déclenchent NKENTSEU_ASSERT_MSG en mode debug
        // En release, le comportement est indéfini (pas de vérification)

        #if defined(NKENTSEU_DEBUG)
            // Année hors plage
            // NkDate invalid1(1500, 1, 1);   // Assert: Year must be in [1601, 30827]

            // Mois hors plage
            // NkDate invalid2(2024, 13, 1);  // Assert: Month must be in [1, 12]

            // Jour invalide pour le mois
            // NkDate invalid3(2023, 2, 30);  // Assert: Day invalid for current month/year

            // 29 février sur année non-bissextile
            // NkDate invalid4(2023, 2, 29);  // Assert: Day invalid...
        #endif

        // Modification avec re-validation automatique
        NkDate mutableDate(2024, 1, 15);
        mutableDate.SetMonth(2);      // OK : janvier -> février
        mutableDate.SetDay(29);       // OK : 2024 est bissextile
        // mutableDate.SetYear(2023); // Assert: 29/02/2023 n'existe pas
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Calcul de durée entre deux dates (simplifié)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"

    namespace nkentseu {

        // Fonction utilitaire pour calculer la différence en jours (approximative)
        // Note : Pour des calculs précis, utiliser NkDateTime ou une bibliothèque dédiée
        int32 ApproximateDaysBetween(const NkDate& start, const NkDate& end) {
            // Conversion simplifiée : jours totaux depuis une époque de référence
            auto ToDays = [](const NkDate& d) -> int64 {
                int64 days = static_cast<int64>(d.GetYear()) * 365;
                days += static_cast<int64>(d.GetMonth()) * 30;  // Approximation
                days += d.GetDay();
                return days;
            };

            int64 diff = ToDays(end) - ToDays(start);
            return static_cast<int32>(diff);
        }

        void DateArithmeticExample() {
            NkDate start(2024, 1, 1);
            NkDate end(2024, 12, 31);

            int32 days = ApproximateDaysBetween(start, end);
            NK_LOG_INFO("Approximate duration: {} days", days);
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Intégration avec NkTime pour datetime complet
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDate.h"
    #include "NKTime/NkTimes.h"

    namespace nkentseu {

        // Structure composite pour date + heure (non fournie par défaut)
        struct DateTime {
            NkDate date;
            NkTime time;

            DateTime() noexcept = default;

            DateTime(const NkDate& d, const NkTime& t)
                : date(d)
                , time(t)
            {
            }

            // Formatage combiné : "YYYY-MM-DD HH:MM:SS.mmm.nnnnnn"
            NkString ToString() const {
                return date.ToString() + " " + time.ToString();
            }

            // Comparaison lexicographique date puis heure
            bool operator<(const DateTime& other) const {
                if (date != other.date) {
                    return date < other.date;
                }
                return time < other.time;
            }
        };

        void DateTimeExample() {
            NkDate today = NkDate::GetCurrent();
            NkTime now = NkTime::GetCurrent();

            DateTime current(today, now);
            NK_LOG_INFO("Current datetime: {}", current.ToString());
        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================