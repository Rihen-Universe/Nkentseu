// =============================================================================
// NKTime/NkTimeSpan.h
// Durée signée avec décomposition calendaire — sans dépendance STL.
//
// Design :
//  - NkTimeSpan représente une durée signée de plusieurs jours, heures, minutes,
//    secondes et nanosecondes. Analogue à .NET System.TimeSpan.
//  - Stockage : un unique int64 en nanosecondes (source de vérité)
//  - GetDays/GetHours/... retournent les composants DÉCOMPOSÉS (pas les totaux)
//
// Distinction NkTimeSpan vs NkDuration :
//  - NkTimeSpan : durée signée avec GetDays/Hours/... et GetDate/GetTime
//                 Usage : intervalles calendaires, différence entre deux dates
//  - NkDuration : durée simple pour API (Sleep, timeout, période de timer)
//                 Usage : NkChrono::Sleep(NkDuration::FromMilliseconds(16))
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions constexpr/inline définies dans le header
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKTIMESPAN_H
#define NKENTSEU_TIME_NKTIMESPAN_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion des constantes de conversion centralisées
    // Inclusion des classes NkTime et NkDate pour l'extraction calendaire
    // Inclusion des types de base NKCore et de la chaîne NKEntseu

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkTimeConstants.h"
    #include "NKTime/NkTimes.h"
    #include "NKTime/NkDate.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // NkTimeSpan représente un intervalle de temps signé avec décomposition
    // calendaire. Utile pour les différences de dates et les durées complexes.

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkTimeSpan {

            public:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.1 : Constructeurs
                // -------------------------------------------------------------

                /// Constructeur par défaut : durée nulle (0 nanoseconde).
                NkTimeSpan() noexcept;

                /**
                 * @brief Construit une durée depuis ses composants décomposés.
                 * @param days         Nombre de jours (peut être négatif)
                 * @param hours        Heures supplémentaires [0-23]
                 * @param minutes      Minutes supplémentaires [0-59]
                 * @param seconds      Secondes supplémentaires [0-59]
                 * @param milliseconds Millisecondes supplémentaires [0-999]
                 * @param nanoseconds  Nanosecondes supplémentaires [0-999999]
                 * @note Les composants sont combinés en un total int64 de nanosecondes.
                 */
                NkTimeSpan(
                    int64 days,
                    int64 hours,
                    int64 minutes,
                    int64 seconds,
                    int64 milliseconds = 0,
                    int64 nanoseconds = 0
                ) noexcept;

                /// Construit depuis un total de nanosecondes.
                /// @param totalNanoseconds Valeur totale en nanosecondes (signée)
                /// @note Source de vérité unique pour toutes les opérations.
                constexpr explicit NkTimeSpan(int64 totalNanoseconds) noexcept
                    : mTotalNanoseconds(totalNanoseconds)
                {
                }

                // Constructeur de copie par défaut (noexcept pour optimisation)
                NkTimeSpan(const NkTimeSpan&) noexcept = default;

                // Opérateur d'affectation par défaut (noexcept pour optimisation)
                NkTimeSpan& operator=(const NkTimeSpan&) noexcept = default;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.2 : Fabriques statiques (Factory Methods)
                // -------------------------------------------------------------
                // Méthodes de construction nommées pour une API expressive.
                // Toutes sont noexcept pour sécurité et performance.

                /// Crée une durée depuis un nombre de jours.
                /// @param days Nombre de jours (signé)
                /// @return Nouvelle instance NkTimeSpan
                NKTIME_NODISCARD static NkTimeSpan FromDays(int64 days) noexcept;

                /// Crée une durée depuis un nombre d'heures.
                /// @param hours Nombre d'heures (signé)
                /// @return Nouvelle instance NkTimeSpan
                NKTIME_NODISCARD static NkTimeSpan FromHours(int64 hours) noexcept;

                /// Crée une durée depuis un nombre de minutes.
                /// @param minutes Nombre de minutes (signé)
                /// @return Nouvelle instance NkTimeSpan
                NKTIME_NODISCARD static NkTimeSpan FromMinutes(int64 minutes) noexcept;

                /// Crée une durée depuis un nombre de secondes.
                /// @param seconds Nombre de secondes (signé)
                /// @return Nouvelle instance NkTimeSpan
                NKTIME_NODISCARD static NkTimeSpan FromSeconds(int64 seconds) noexcept;

                /// Crée une durée depuis un nombre de millisecondes.
                /// @param milliseconds Nombre de millisecondes (signé)
                /// @return Nouvelle instance NkTimeSpan
                NKTIME_NODISCARD static NkTimeSpan FromMilliseconds(int64 milliseconds) noexcept;

                /// Crée une durée depuis un nombre de nanosecondes.
                /// @param nanoseconds Nombre de nanosecondes (signé)
                /// @return Nouvelle instance NkTimeSpan
                NKTIME_NODISCARD static NkTimeSpan FromNanoseconds(int64 nanoseconds) noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.3 : Composants décomposés (Getters)
                // -------------------------------------------------------------
                // Retourne les composants INDIVIDUELS de la durée, pas les totaux.
                // Exemple : FromHours(25).GetDays() == 1, GetHours() == 1 (pas 25)

                /// @return Nombre de jours complets dans cette durée (signé)
                NKTIME_NODISCARD int64 GetDays() const noexcept;

                /// @return Heures résiduelles [0-23] après extraction des jours
                NKTIME_NODISCARD int64 GetHours() const noexcept;

                /// @return Minutes résiduelles [0-59] après extraction des heures
                NKTIME_NODISCARD int64 GetMinutes() const noexcept;

                /// @return Secondes résiduelles [0-59] après extraction des minutes
                NKTIME_NODISCARD int64 GetSeconds() const noexcept;

                /// @return Millisecondes résiduelles [0-999] après extraction des secondes
                NKTIME_NODISCARD int64 GetMilliseconds() const noexcept;

                /// @return Nanosecondes résiduelles [0-999999] après extraction des ms
                NKTIME_NODISCARD int64 GetNanoseconds() const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.4 : Totaux et conversions
                // -------------------------------------------------------------
                // Méthodes retournant la durée totale dans une unité donnée.

                /// @return Durée totale en nanosecondes (valeur interne exacte)
                NKTIME_NODISCARD constexpr int64 ToNanoseconds() const noexcept {
                    return mTotalNanoseconds;
                }

                /// @return Durée totale en secondes (conversion float64)
                NKTIME_NODISCARD double ToSeconds() const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.5 : Opérateurs arithmétiques binaires
                // -------------------------------------------------------------
                // Opérateurs retournant un nouvel objet (sans modifier *this).
                // Tous sont noexcept pour garantir l'absence d'exceptions.

                /// Addition de deux durées.
                /// @param o Durée à ajouter
                /// @return Nouvelle durée résultante
                NKTIME_NODISCARD NkTimeSpan operator+(const NkTimeSpan& o) const noexcept;

                /// Soustraction de deux durées.
                /// @param o Durée à soustraire
                /// @return Nouvelle durée résultante
                NKTIME_NODISCARD NkTimeSpan operator-(const NkTimeSpan& o) const noexcept;

                /// Multiplication par un facteur scalaire.
                /// @param factor Facteur multiplicatif
                /// @return Nouvelle durée résultante
                NKTIME_NODISCARD NkTimeSpan operator*(double factor) const noexcept;

                /// Division par un diviseur scalaire.
                /// @param divisor Diviseur
                /// @return Nouvelle durée résultante
                NKTIME_NODISCARD NkTimeSpan operator/(double divisor) const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.6 : Opérateurs arithmétiques composés
                // -------------------------------------------------------------
                // Opérateurs modifiant l'objet courant (in-place).
                // Retournent *this pour permettre le chaînage d'opérations.

                /// Ajout in-place d'une durée.
                /// @param o Durée à ajouter
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& operator+=(const NkTimeSpan& o) noexcept;

                /// Soustraction in-place d'une durée.
                /// @param o Durée à soustraire
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& operator-=(const NkTimeSpan& o) noexcept;

                /// Multiplication in-place par un scalaire.
                /// @param factor Facteur multiplicatif
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& operator*=(double factor) noexcept;

                /// Division in-place par un scalaire.
                /// @param divisor Diviseur
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& operator/=(double divisor) noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.7 : Méthodes nommées (chaînables)
                // -------------------------------------------------------------
                // Versions nommées des opérateurs pour une API plus expressive.
                // Utiles pour le chaînage fluide et la lisibilité.

                /// Ajoute une durée à cet intervalle.
                /// @param o Durée à ajouter
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& Add(const NkTimeSpan& o) noexcept;

                /// Soustrait une durée de cet intervalle.
                /// @param o Durée à soustraire
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& Subtract(const NkTimeSpan& o) noexcept;

                /// Multiplie cette durée par un facteur.
                /// @param factor Facteur multiplicatif
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& Multiply(double factor) noexcept;

                /// Divise cette durée par un diviseur.
                /// @param divisor Diviseur
                /// @return Référence vers cet objet pour chaînage
                NkTimeSpan& Divide(double divisor) noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.8 : Opérateurs de comparaison
                // -------------------------------------------------------------
                // Comparaisons basées sur la valeur interne en nanosecondes.
                // Toutes les méthodes sont noexcept.

                /// Égalité : vrai si les totaux en ns sont identiques.
                /// @param o Autre durée à comparer
                /// @return true si égales, false sinon
                NKTIME_NODISCARD bool operator==(const NkTimeSpan& o) const noexcept;

                /// Inégalité : inverse de operator==
                /// @param o Autre durée à comparer
                /// @return true si différentes, false sinon
                NKTIME_NODISCARD bool operator!=(const NkTimeSpan& o) const noexcept;

                /// Infériorité stricte : vrai si cette durée est plus courte.
                /// @param o Autre durée à comparer
                /// @return true si *this < o, false sinon
                NKTIME_NODISCARD bool operator<(const NkTimeSpan& o) const noexcept;

                /// Infériorité ou égalité
                /// @param o Autre durée à comparer
                /// @return true si *this <= o, false sinon
                NKTIME_NODISCARD bool operator<=(const NkTimeSpan& o) const noexcept;

                /// Supériorité stricte : vrai si cette durée est plus longue.
                /// @param o Autre durée à comparer
                /// @return true si *this > o, false sinon
                NKTIME_NODISCARD bool operator>(const NkTimeSpan& o) const noexcept;

                /// Supériorité ou égalité
                /// @param o Autre durée à comparer
                /// @return true si *this >= o, false sinon
                NKTIME_NODISCARD bool operator>=(const NkTimeSpan& o) const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.9 : Extraction calendaire
                // -------------------------------------------------------------
                // Méthodes convertissant la durée en composants date/heure.

                /// Extrait la partie horaire de la durée (modulo 24h).
                /// @return Objet NkTime représentant l'heure dans la journée
                /// @note Gère les durées négatives via normalisation
                NKTIME_NODISCARD NkTime GetTime() const noexcept;

                /// Extrait la partie calendaire depuis l'époque Unix.
                /// @return Objet NkDate représentant la date
                /// @note Utilise l'algorithme de Howard Hinnant (domaine public)
                NKTIME_NODISCARD NkDate GetDate() const noexcept;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.10 : Formatage et sérialisation
                // -------------------------------------------------------------

                /// Convertit cette durée en chaîne formatée lisible.
                /// Format : "[±][Xd ][HH:]MM:SS[.mmm[.nnnnnn]]"
                /// @return NkString contenant la représentation textuelle
                NKTIME_NODISCARD NkString ToString() const;

                /// Fonction amie pour formatage via ADL (Argument-Dependent Lookup).
                /// @param ts Objet NkTimeSpan à formater
                /// @return NkString contenant la représentation textuelle
                friend NkString ToString(const NkTimeSpan& ts) {
                    return ts.ToString();
                }

            private:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.11 : Membre privé
                // -------------------------------------------------------------
                // Stockage interne unique : durée totale en nanosecondes (int64 signé).
                // Source de vérité pour toutes les opérations et conversions.

                int64 mTotalNanoseconds = 0;

        }; // class NkTimeSpan

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKTIMESPAN_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTIMESPAN.H
// =============================================================================
// NkTimeSpan représente un intervalle de temps signé avec décomposition
// calendaire. Utile pour les différences de dates et les durées complexes.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création via les fabriques statiques
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"

    void TimeSpanCreationExample() {
        using namespace nkentseu;

        // Création depuis différentes unités
        NkTimeSpan ts1 = NkTimeSpan::FromDays(2);           // 2 jours
        NkTimeSpan ts2 = NkTimeSpan::FromHours(36);         // 1 jour + 12h
        NkTimeSpan ts3 = NkTimeSpan::FromMilliseconds(1500); // 1.5s

        // Création avec composants décomposés
        NkTimeSpan ts4(1, 2, 30, 45, 123, 456);
        // 1 jour, 2h, 30min, 45s, 123ms, 456ns

        // Accès aux composants décomposés (pas aux totaux)
        int64 days = ts2.GetDays();     // 1 (pas 1.5)
        int64 hours = ts2.GetHours();   // 12 (reste après extraction des jours)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Différence entre deux dates
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"
    #include "NKTime/NkDate.h"
    #include "NKCore/Logger/NkLogger.h"

    void DateDifferenceExample() {
        using namespace nkentseu;

        NkDate start(2024, 1, 1);
        NkDate end(2024, 12, 31);

        // Hypothétique : soustraction de dates retourne NkTimeSpan
        // NkTimeSpan diff = end - start;  // ~365 jours

        // Extraction des composants
        // int64 days = diff.GetDays();
        // int64 hours = diff.GetHours();  // 0 si durée exacte en jours

        // Formatage pour affichage
        // NK_LOG_INFO("Duration: {}", diff.ToString());
        // Output: "Duration: 365d 00:00:00"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Arithmétique des intervalles
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"

    void TimeSpanArithmeticExample() {
        using namespace nkentseu;

        // Intervalles de base
        NkTimeSpan workDay = NkTimeSpan::FromHours(8);
        NkTimeSpan breakTime = NkTimeSpan::FromMinutes(45);

        // Addition et soustraction
        NkTimeSpan netWork = workDay - breakTime;  // 7h 15min

        // Opérateurs composés avec chaînage
        NkTimeSpan weekly = NkTimeSpan::Zero();
        weekly.Add(workDay).Add(workDay).Add(workDay)
              .Add(workDay).Add(workDay);  // 5 × 8h = 40h

        // Mise à l'échelle
        NkTimeSpan monthly = weekly * 4.0;  // ~160h
        NkTimeSpan dailyAverage = monthly / 30.0;  // ~5.33h/jour
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Composants décomposés vs totaux
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"
    #include "NKCore/Logger/NkLogger.h"

    void DecompositionExample() {
        using namespace nkentseu;

        // Durée de 25 heures
        NkTimeSpan span = NkTimeSpan::FromHours(25);

        // Composants DÉCOMPOSÉS (ce que retournent les Getters)
        int64 days = span.GetDays();        // 1 (jour complet)
        int64 hours = span.GetHours();      // 1 (reste après extraction des jours)
        // Total affiché : 1 jour et 1 heure

        // Totaux (ce que retournent les To* methods)
        int64 totalHours = span.ToNanoseconds() / time::NS_PER_HOUR;  // 25
        double totalSeconds = span.ToSeconds();  // 90000.0

        NK_LOG_INFO("Decomposed: {}d {}h", days, hours);   // "1d 1h"
        NK_LOG_INFO("Total: {} hours", totalHours);        // "25 hours"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Extraction calendaire (GetDate/GetTime)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"
    #include "NKTime/NkDate.h"
    #include "NKTime/NkTimes.h"

    void CalendarExtractionExample() {
        using namespace nkentseu;

        // Durée depuis l'époque Unix (1970-01-01 00:00:00)
        NkTimeSpan sinceEpoch = NkTimeSpan::FromDays(20000);  // ~54 ans

        // Extraction de la date calendaire
        NkDate date = sinceEpoch.GetDate();
        // date représente environ 2024-09-XX

        // Extraction de l'heure dans la journée
        NkTimeSpan timeOfDay = NkTimeSpan::FromHours(25) + NkTimeSpan::FromMinutes(30);
        NkTime time = timeOfDay.GetTime();
        // time vaut 01:30:00 (25h30 modulo 24h)

        // Utilisation combinée
        NK_LOG_INFO("Date: {}, Time: {}", date.ToString(), time.ToString());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Formatage pour logging et UI
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"
    #include "NKCore/Logger/NkLogger.h"

    void TimeSpanFormattingExample() {
        using namespace nkentseu;

        // Différentes durées avec formatage automatique
        NkTimeSpan ts1 = NkTimeSpan::FromDays(5);
        NkTimeSpan ts2 = NkTimeSpan::FromHours(2) + NkTimeSpan::FromMinutes(30);
        NkTimeSpan ts3 = NkTimeSpan::FromMilliseconds(1234);
        NkTimeSpan ts4 = NkTimeSpan::FromNanoseconds(-500);

        NK_LOG_INFO("ts1: {}", ts1.ToString());  // "5d 00:00:00"
        NK_LOG_INFO("ts2: {}", ts2.ToString());  // "02:30:00"
        NK_LOG_INFO("ts3: {}", ts3.ToString());  // "00:00:01.234"
        NK_LOG_INFO("ts4: {}", ts4.ToString());  // "-0.000000500" ou similaire

        // Formatage via fonction amie (ADL)
        NkString report = NkString("Elapsed: ") + ToString(ts2);
        // report = "Elapsed: 02:30:00"

        // Format adapté pour UI : masquer les composants nuls si désiré
        // (nécessiterait une méthode ToStringCompact supplémentaire)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Gestion des durées négatives
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"
    #include "NKCore/Assert/NkAssert.h"

    void NegativeTimeSpanExample() {
        using namespace nkentseu;

        // Création d'une durée négative
        NkTimeSpan past = NkTimeSpan::FromHours(-5);

        // Les composants reflètent le signe
        NKENTSEU_ASSERT(past.GetDays() <= 0);  // 0 ou négatif
        NKENTSEU_ASSERT(past.ToNanoseconds() < 0);  // Toujours négatif

        // Arithmétique avec signes
        NkTimeSpan future = NkTimeSpan::FromHours(10);
        NkTimeSpan net = future + past;  // 10h + (-5h) = 5h

        // Comparaisons
        if (past < NkTimeSpan::Zero()) {
            // Durée dans le passé
        }

        // GetTime() normalise les négatifs vers [0, 24h)
        NkTime normalized = past.GetTime();  // 19:00:00 (24 - 5)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Intégration avec NkDuration (cas d'usage typique)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeSpan.h"
    #include "NKTime/NkDuration.h"

    namespace nkentseu {

        // Pattern : NkTimeSpan pour les différences, NkDuration pour les APIs
        void ScheduleTaskExample() {
            // Calcul de l'intervalle entre deux dates (NkTimeSpan)
            NkDate startDate(2024, 1, 1);
            NkDate endDate(2024, 6, 1);
            // NkTimeSpan interval = endDate - startDate;  // ~152 jours

            // Conversion vers NkDuration pour une API de timeout
            // NkDuration timeout = NkDuration::FromNanoseconds(
            //     interval.ToNanoseconds());

            // Utilisation dans une API acceptant NkDuration
            // Timer::Start(timeout, []() { /\* callback *\/ });

            // Inverse : durée spécifiée -> extraction calendaire
            NkDuration deadline = NkDuration::FromDays(7);
            // NkTimeSpan asSpan = NkTimeSpan::FromNanoseconds(
            //     deadline.ToNanoseconds());
            // NkDate targetDate = currentDate + asSpan.GetDate();
        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================