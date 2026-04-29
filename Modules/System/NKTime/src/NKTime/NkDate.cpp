// =============================================================================
// NKTime/NkDate.cpp
// Implémentation de la classe NkDate.
//
// Design :
//  - Validation automatique des plages de dates grégoriennes
//  - Gestion multiplateforme via NKPlatform/NkPlatformDetect.h
//  - Assertions NKENTSEU_ASSERT_MSG pour le debugging en mode développement
//  - Aucune dépendance STL pour compatibilité embarquée
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système

#include "pch.h"
#include "NKTime/NkDate.h"
#include "NKCore/Assert/NkAssert.h"

#include <cstdio>
#include <ctime>

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes de la classe NkDate dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  Constructeurs
    // =============================================================================

    NkDate::NkDate() noexcept {
        // Initialisation via la méthode statique GetCurrent()
        // Garantit que la date par défaut correspond à la date système locale
        *this = GetCurrent();
    }

    NkDate::NkDate(int32 y, int32 m, int32 d)
        : year(y)
        , month(m)
        , day(d)
    {
        // Validation automatique après initialisation des membres
        // Déclenche NKENTSEU_ASSERT_MSG si les valeurs sont hors plage
        Validate();
    }

    // =============================================================================
    //  Validation interne
    // =============================================================================
    // Méthode privée appelée après toute modification des composants de date.
    // Vérifie la cohérence année/mois/jour selon le calendrier grégorien.

    void NkDate::Validate() const {
        // Vérification de la plage annuelle [1601, 30827]
        // Cette plage couvre les dates supportées par les APIs système courantes
        NKENTSEU_ASSERT_MSG(
            year >= 1601 && year <= 30827,
            "Year must be in range [1601, 30827]"
        );

        // Vérification de la plage mensuelle [1, 12]
        NKENTSEU_ASSERT_MSG(
            month >= 1 && month <= 12,
            "Month must be in range [1, 12]"
        );

        // Calcul du nombre maximum de jours pour ce mois/année
        // Prend en compte les années bissextiles pour février
        const int32 maxDay = DaysInMonth(year, month);

        // Vérification de la plage journalière [1, maxDay]
        NKENTSEU_ASSERT_MSG(
            day >= 1 && day <= maxDay,
            "Day invalid for current month/year"
        );
    }

    // =============================================================================
    //  Mutateurs (Setters)
    // =============================================================================
    // Chaque setter met à jour un composant puis re-valide la date complète.
    // Cela garantit que toute modification préserve l'intégrité de l'objet.

    void NkDate::SetYear(int32 y) {
        // Validation de la nouvelle valeur d'année
        NKENTSEU_ASSERT_MSG(
            y >= 1601 && y <= 30827,
            "Year must be in range [1601, 30827]"
        );
        year = y;

        // Re-validation complète : un changement d'année peut invalider le jour
        // Exemple : 29 février 2024 -> année 2023 (non bissextile) = invalide
        Validate();
    }

    void NkDate::SetMonth(int32 m) {
        // Validation de la nouvelle valeur de mois
        NKENTSEU_ASSERT_MSG(
            m >= 1 && m <= 12,
            "Month must be in range [1, 12]"
        );
        month = m;

        // Re-validation complète : un changement de mois peut invalider le jour
        // Exemple : 31 avril n'existe pas (avril a 30 jours)
        Validate();
    }

    void NkDate::SetDay(int32 d) {
        // Mise à jour directe du jour
        // La validation complète vérifie la cohérence avec mois/année
        day = d;
        Validate();
    }

    // =============================================================================
    //  Méthodes statiques
    // =============================================================================

    NkDate NkDate::GetCurrent() {
        // Acquisition du timestamp Unix courant (secondes depuis 1970-01-01)
        time_t now = ::time(nullptr);

        // Structure pour décomposition en composants calendaires
        tm ts = {};

        // -------------------------------------------------------------------------
        // Conversion timestamp -> composants locaux (implémentation multiplateforme)
        // -------------------------------------------------------------------------
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : version thread-safe de localtime
            // Signature : errno_t localtime_s(tm* result, const time_t* source)
            ::localtime_s(&ts, &now);
        #else
            // POSIX (Linux, macOS, BSD) : version thread-safe de localtime
            // Signature : tm* localtime_r(const time_t* source, tm* result)
            ::localtime_r(&now, &ts);
        #endif

        // -------------------------------------------------------------------------
        // Construction de l'objet NkDate depuis les composants décomposés
        // -------------------------------------------------------------------------
        // tm.tm_year : années depuis 1900 -> ajouter 1900 pour obtenir l'année réelle
        // tm.tm_mon  : mois [0, 11] -> ajouter 1 pour obtenir [1, 12]
        // tm.tm_mday : jour du mois [1, 31] -> utilisé tel quel
        return NkDate(
            ts.tm_year + 1900,
            ts.tm_mon + 1,
            ts.tm_mday
        );
    }

    int32 NkDate::DaysInMonth(int32 year, int32 month) {
        // Validation préalable du mois pour sécurité défensive
        NKENTSEU_ASSERT_MSG(
            month >= 1 && month <= 12,
            "Month must be in range [1, 12]"
        );

        // Tableau statique des jours par mois (année non bissextile)
        // Index 0 = janvier, index 11 = décembre
        static const int32 days[12] = {
            31,  // Janvier
            28,  // Février (ajusté ci-dessous si bissextile)
            31,  // Mars
            30,  // Avril
            31,  // Mai
            30,  // Juin
            31,  // Juillet
            31,  // Août
            30,  // Septembre
            31,  // Octobre
            30,  // Novembre
            31   // Décembre
        };

        // Cas spécial : février en année bissextile a 29 jours au lieu de 28
        if (month == 2 && IsLeapYear(year)) {
            return 29;
        }

        // Retour du nombre de jours standard pour ce mois
        // month - 1 pour conversion index [1,12] -> [0,11]
        return days[month - 1];
    }

    bool NkDate::IsLeapYear(int32 year) noexcept {
        // Règle grégorienne de détection des années bissextiles :
        // 1. Divisible par 4 ET
        // 2. (Non divisible par 100 OU divisible par 400)
        //
        // Exemples :
        //   2024 : divisible par 4, non par 100 -> bissextile ✓
        //   1900 : divisible par 4 et 100, non par 400 -> non bissextile ✗
        //   2000 : divisible par 4, 100 et 400 -> bissextile ✓
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    // =============================================================================
    //  Formatage et sérialisation
    // =============================================================================

    NkString NkDate::ToString() const {
        // Buffer local pour le formatage (taille suffisante pour "YYYY-MM-DD\0")
        // 4+1+2+1+2+1 = 11 caractères + null terminator = 12 minimum, 16 pour marge
        char buf[16];

        // Formatage ISO 8601 avec zéros de remplissage :
        // %04d : année sur 4 chiffres (ex: 2024)
        // %02d : mois/jour sur 2 chiffres (ex: 01, 09, 31)
        ::snprintf(
            buf,
            sizeof(buf),
            "%04d-%02d-%02d",
            year,
            month,
            day
        );

        // Construction de la chaîne NKEntseu depuis le buffer C
        return NkString(buf);
    }

    NkString NkDate::GetMonthName() const {
        // Tableau statique des noms de mois en français
        // Index 0 = Janvier, index 11 = Décembre
        // static : allocation unique, partagée entre tous les appels
        static const char* months[12] = {
            "Janvier",
            "Février",
            "Mars",
            "Avril",
            "Mai",
            "Juin",
            "Juillet",
            "Août",
            "Septembre",
            "Octobre",
            "Novembre",
            "Décembre"
        };

        // Conversion month [1,12] -> index [0,11] pour accès au tableau
        return NkString(months[month - 1]);
    }

    // =============================================================================
    //  Opérateurs de comparaison
    // =============================================================================
    // Comparaisons lexicographiques : année d'abord, puis mois, puis jour.
    // Cette sémantique correspond à l'ordre chronologique naturel.

    bool NkDate::operator==(const NkDate& o) const noexcept {
        // Égalité stricte : tous les composants doivent correspondre
        return year == o.year && month == o.month && day == o.day;
    }

    bool NkDate::operator!=(const NkDate& o) const noexcept {
        // Inégalité : déléguée à l'opérateur d'égalité pour cohérence
        return !(*this == o);
    }

    bool NkDate::operator<(const NkDate& o) const noexcept {
        // Comparaison lexicographique pour ordre chronologique
        // 1. Comparer les années d'abord
        if (year != o.year) {
            return year < o.year;
        }
        // 2. Si années égales, comparer les mois
        if (month != o.month) {
            return month < o.month;
        }
        // 3. Si années et mois égaux, comparer les jours
        return day < o.day;
    }

    bool NkDate::operator<=(const NkDate& o) const noexcept {
        // Infériorité ou égalité : déléguée aux opérateurs de base
        return !(o < *this);
    }

    bool NkDate::operator>(const NkDate& o) const noexcept {
        // Supériorité stricte : inverse de l'infériorité
        return (o < *this);
    }

    bool NkDate::operator>=(const NkDate& o) const noexcept {
        // Supériorité ou égalité : inverse de l'infériorité stricte
        return !(*this < o);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Validation :
    - Toutes les modifications de date passent par Validate() pour garantir
      l'intégrité des données. En mode release, NKENTSEU_ASSERT_MSG est désactivé,
      donc la validation n'a pas de coût runtime (mais le comportement est
      indéfini si les préconditions ne sont pas respectées).

    Thread-safety :
    - NkDate::GetCurrent() est thread-safe car elle utilise localtime_s/r.
    - Les instances de NkDate ne sont PAS thread-safe : synchronisation externe
      requise pour l'accès concurrent à une même instance.

    Performance :
    - Les getters sont inline et noexcept pour permettre l'optimisation.
    - DaysInMonth utilise un tableau static pour éviter les allocations.
    - IsLeapYear est noexcept et utilise uniquement des opérations modulo.

    Portabilité :
    - Aucune dépendance STL pour compatibilité avec les environnements embarqués.
    - Gestion conditionnelle des APIs Windows vs POSIX via NKPlatform.
    - Plage d'années [1601, 30827] choisie pour compatibilité avec FILETIME Windows
      et les limites pratiques des systèmes de fichiers courants.

    Extensions futures :
    - Support multi-langue pour GetMonthName() via table de traduction.
    - Opérateurs arithmétiques pour ajout/soustraction de jours.
    - Conversion vers/depuis timestamp Unix pour interopérabilité.
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================