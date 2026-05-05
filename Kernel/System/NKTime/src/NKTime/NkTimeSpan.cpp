// =============================================================================
// NKTime/NkTimeSpan.cpp
// Implémentation de la classe NkTimeSpan.
//
// Design :
//  - Toutes les méthodes respectent la noexcept-specification du header
//  - Utilisation exclusive des constantes de NkTimeConstants.h
//  - Algorithme de Howard Hinnant pour GetDate() (domaine public)
//  - Formatage ToString() avec gestion des signes et composants optionnels
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
// 3. Headers système nécessaires

#include "pch.h"
#include "NKTime/NkTimeSpan.h"

#include <cstdio>

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE ET USING LOCAL
// -------------------------------------------------------------------------
// Alias local pour accéder aux constantes de conversion.
// Limité à ce fichier .cpp pour éviter la pollution des espaces de noms.

namespace nkentseu {

    // Alias local pour raccourcir l'accès aux constantes (scope fichier uniquement)
    using namespace time;

    // =============================================================================
    //  Constructeurs
    // =============================================================================

    NkTimeSpan::NkTimeSpan() noexcept
        : mTotalNanoseconds(0)
    {
        // Initialisation explicite à zéro pour clarté
        // La liste d'initialisation du membre suffit, corps vide
    }

    NkTimeSpan::NkTimeSpan(
        int64 days,
        int64 hours,
        int64 minutes,
        int64 seconds,
        int64 milliseconds,
        int64 nanoseconds
    ) noexcept
    {
        // Combinaison de tous les composants en nanosecondes totales
        // L'ordre des opérations n'affecte pas le résultat (addition commutative)
        // Attention aux overflow : l'appelant doit garantir que le total tient dans int64
        mTotalNanoseconds =
            days * NS_PER_DAY
            + hours * NS_PER_HOUR
            + minutes * NS_PER_MINUTE
            + seconds * NS_PER_SECOND
            + milliseconds * NS_PER_MILLISECOND
            + nanoseconds;
    }

    // =============================================================================
    //  Fabriques statiques
    // =============================================================================
    // Chaque fabrique convertit l'unité d'entrée en nanosecondes puis délègue
    // au constructeur privé prenant un int64. Simple, efficace, sans duplication.

    NkTimeSpan NkTimeSpan::FromDays(int64 d) noexcept {
        return NkTimeSpan(d * NS_PER_DAY);
    }

    NkTimeSpan NkTimeSpan::FromHours(int64 h) noexcept {
        return NkTimeSpan(h * NS_PER_HOUR);
    }

    NkTimeSpan NkTimeSpan::FromMinutes(int64 m) noexcept {
        return NkTimeSpan(m * NS_PER_MINUTE);
    }

    NkTimeSpan NkTimeSpan::FromSeconds(int64 s) noexcept {
        return NkTimeSpan(s * NS_PER_SECOND);
    }

    NkTimeSpan NkTimeSpan::FromMilliseconds(int64 ms) noexcept {
        return NkTimeSpan(ms * NS_PER_MILLISECOND);
    }

    NkTimeSpan NkTimeSpan::FromNanoseconds(int64 ns) noexcept {
        return NkTimeSpan(ns);
    }

    // =============================================================================
    //  Composants décomposés (Getters)
    // =============================================================================
    // Ces méthodes retournent les composants INDIVIDUELS, pas les totaux.
    // Exemple : une durée de 25h donne GetDays()=1, GetHours()=1 (pas 25).
    // L'implémentation utilise des divisions et modulo successifs.

    int64 NkTimeSpan::GetDays() const noexcept {
        // Division entière : nombre de jours complets dans la durée
        return mTotalNanoseconds / NS_PER_DAY;
    }

    int64 NkTimeSpan::GetHours() const noexcept {
        // Reste après extraction des jours, puis division par heures
        return (mTotalNanoseconds % NS_PER_DAY) / NS_PER_HOUR;
    }

    int64 NkTimeSpan::GetMinutes() const noexcept {
        // Reste après extraction des heures, puis division par minutes
        return (mTotalNanoseconds % NS_PER_HOUR) / NS_PER_MINUTE;
    }

    int64 NkTimeSpan::GetSeconds() const noexcept {
        // Reste après extraction des minutes, puis division par secondes
        return (mTotalNanoseconds % NS_PER_MINUTE) / NS_PER_SECOND;
    }

    int64 NkTimeSpan::GetMilliseconds() const noexcept {
        // Reste après extraction des secondes, puis division par millisecondes
        return (mTotalNanoseconds % NS_PER_SECOND) / NS_PER_MILLISECOND;
    }

    int64 NkTimeSpan::GetNanoseconds() const noexcept {
        // Reste final : nanosecondes résiduelles après extraction des millisecondes
        return mTotalNanoseconds % NS_PER_MILLISECOND;
    }

    double NkTimeSpan::ToSeconds() const noexcept {
        // Conversion vers double pour précision flottante
        // Utiliser static_cast pour éviter les warnings de conversion implicite
        return static_cast<double>(mTotalNanoseconds)
             / static_cast<double>(NS_PER_SECOND);
    }

    // =============================================================================
    //  Opérateurs arithmétiques
    // =============================================================================
    // Implémentation directe via la valeur interne en nanosecondes.
    // Toutes les méthodes sont noexcept : pas d'exceptions, pas d'allocation.

    NkTimeSpan NkTimeSpan::operator+(const NkTimeSpan& o) const noexcept {
        return NkTimeSpan(mTotalNanoseconds + o.mTotalNanoseconds);
    }

    NkTimeSpan NkTimeSpan::operator-(const NkTimeSpan& o) const noexcept {
        return NkTimeSpan(mTotalNanoseconds - o.mTotalNanoseconds);
    }

    NkTimeSpan NkTimeSpan::operator*(double f) const noexcept {
        // Conversion vers double pour la multiplication, puis retour en int64
        // Attention : perte de précision possible pour les très grandes valeurs
        return NkTimeSpan(static_cast<int64>(
            static_cast<double>(mTotalNanoseconds) * f
        ));
    }

    NkTimeSpan NkTimeSpan::operator/(double d) const noexcept {
        // Protection contre la division par zéro : retourne zéro
        if (d == 0.0) {
            return NkTimeSpan(0LL);
        }
        // Division flottante puis conversion vers int64
        return NkTimeSpan(static_cast<int64>(
            static_cast<double>(mTotalNanoseconds) / d
        ));
    }

    NkTimeSpan& NkTimeSpan::operator+=(const NkTimeSpan& o) noexcept {
        // Délégation à la méthode nommée Add pour cohérence
        return Add(o);
    }

    NkTimeSpan& NkTimeSpan::operator-=(const NkTimeSpan& o) noexcept {
        // Délégation à la méthode nommée Subtract pour cohérence
        return Subtract(o);
    }

    NkTimeSpan& NkTimeSpan::operator*=(double f) noexcept {
        // Délégation à la méthode nommée Multiply pour cohérence
        return Multiply(f);
    }

    NkTimeSpan& NkTimeSpan::operator/=(double d) noexcept {
        // Délégation à la méthode nommée Divide pour cohérence
        return Divide(d);
    }

    NkTimeSpan& NkTimeSpan::Add(const NkTimeSpan& o) noexcept {
        // Modification in-place : ajout direct de la valeur interne
        mTotalNanoseconds += o.mTotalNanoseconds;
        return *this;
    }

    NkTimeSpan& NkTimeSpan::Subtract(const NkTimeSpan& o) noexcept {
        // Modification in-place : soustraction directe de la valeur interne
        mTotalNanoseconds -= o.mTotalNanoseconds;
        return *this;
    }

    NkTimeSpan& NkTimeSpan::Multiply(double f) noexcept {
        // Modification in-place : multiplication avec conversion flottante
        mTotalNanoseconds = static_cast<int64>(
            static_cast<double>(mTotalNanoseconds) * f
        );
        return *this;
    }

    NkTimeSpan& NkTimeSpan::Divide(double d) noexcept {
        // Protection contre la division par zéro : pas de modification si d==0
        if (d != 0.0) {
            mTotalNanoseconds = static_cast<int64>(
                static_cast<double>(mTotalNanoseconds) / d
            );
        }
        return *this;
    }

    // =============================================================================
    //  Opérateurs de comparaison
    // =============================================================================
    // Comparaisons déléguées à la valeur interne int64 pour simplicité et performance.
    // Toutes les méthodes sont noexcept et constexpr-compatible.

    bool NkTimeSpan::operator==(const NkTimeSpan& o) const noexcept {
        return mTotalNanoseconds == o.mTotalNanoseconds;
    }

    bool NkTimeSpan::operator!=(const NkTimeSpan& o) const noexcept {
        return !(*this == o);
    }

    bool NkTimeSpan::operator<(const NkTimeSpan& o) const noexcept {
        return mTotalNanoseconds < o.mTotalNanoseconds;
    }

    bool NkTimeSpan::operator<=(const NkTimeSpan& o) const noexcept {
        return !(o < *this);
    }

    bool NkTimeSpan::operator>(const NkTimeSpan& o) const noexcept {
        return (o < *this);
    }

    bool NkTimeSpan::operator>=(const NkTimeSpan& o) const noexcept {
        return !(*this < o);
    }

    // =============================================================================
    //  Extraction calendaire : GetTime
    // =============================================================================
    // Extrait la partie horaire (modulo 24h) de la durée.
    // Gère les durées négatives en normalisant vers [0, NS_PER_DAY).

    NkTime NkTimeSpan::GetTime() const noexcept {
        // Extraction du reste modulo un jour pour obtenir la partie < 24h
        int64 ns = mTotalNanoseconds % NS_PER_DAY;

        // Normalisation des valeurs négatives vers l'intervalle [0, 24h)
        // Exemple : -1h devient 23h dans la représentation horaire
        if (ns < 0) {
            ns += NS_PER_DAY;
        }

        // Décomposition en composants horaires
        const int32 h = static_cast<int32>(ns / NS_PER_HOUR);
        ns %= NS_PER_HOUR;

        const int32 m = static_cast<int32>(ns / NS_PER_MINUTE);
        ns %= NS_PER_MINUTE;

        const int32 s = static_cast<int32>(ns / NS_PER_SECOND);
        ns %= NS_PER_SECOND;

        const int32 ms = static_cast<int32>(ns / NS_PER_MILLISECOND);
        const int32 nano = static_cast<int32>(ns % NS_PER_MILLISECOND);

        // Construction et retour de l'objet NkTime
        return NkTime(h, m, s, ms, nano);
    }

    // =============================================================================
    //  Extraction calendaire : GetDate (algorithme de Howard Hinnant)
    // =============================================================================
    // Convertit la durée en une date calendaire depuis l'époque Unix (1970-01-01).
    // Algorithme de Howard Hinnant (domaine public) : efficace et précis.
    // Référence : http://howardhinnant.github.io/date_algorithms.html

    NkDate NkTimeSpan::GetDate() const noexcept {
        // Conversion des nanosecondes vers jours entiers
        int64 days = mTotalNanoseconds / NS_PER_DAY;

        // Offset pour aligner sur l'époque de l'algorithme (1970-01-01)
        // 719468 = nombre de jours entre 0000-03-01 et 1970-01-01
        days += 719468LL;

        // -------------------------------------------------------------------------
        // Algorithme de conversion jours -> année/mois/jour (Howard Hinnant)
        // -------------------------------------------------------------------------

        // Calcul de l'ère (période de 400 ans dans le calendrier grégorien)
        const int64 era = (days >= 0 ? days : days - 146096) / 146097;

        // Jour dans l'ère (0 à 146096)
        const int64 doe = days - era * 146097;

        // Année dans l'ère (0 à 399)
        const int64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;

        // Année absolue
        const int64 y = yoe + era * 400;

        // Jour dans l'année (0 à 365/366)
        const int64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);

        // Mois priorisé (0 à 11, mars=0, février=11)
        const int64 mp = (5 * doy + 2) / 153;

        // Jour du mois (1 à 31)
        const int32 day = static_cast<int32>(doy - (153 * mp + 2) / 5 + 1);

        // Mois calendaire (1 à 12)
        const int32 month = static_cast<int32>(mp < 10 ? mp + 3 : mp - 9);

        // Année calendaire (ajustement pour janvier/février)
        const int32 year = static_cast<int32>(y + (month <= 2 ? 1 : 0));

        // Construction et retour de l'objet NkDate
        return NkDate(year, month, day);
    }

    // =============================================================================
    //  Formatage : ToString
    // =============================================================================
    // Format : "[±][Xd ][HH:]MM:SS[.mmm[.nnnnnn]]"
    // Exemples :
    //   "5d 02:30:45"          -> 5 jours, 2h, 30min, 45s
    //   "02:30:45.123"         -> 2h, 30min, 45s, 123ms
    //   "02:30:45.123.456789"  -> avec nanosecondes
    //   "-01:30:00"            -> durée négative

    NkString NkTimeSpan::ToString() const {
        // Calcul de la valeur absolue pour l'extraction des composants
        // Le signe est géré séparément pour un préfixe '-' si nécessaire
        const int64 absNs = mTotalNanoseconds < 0 ? -mTotalNanoseconds : mTotalNanoseconds;

        // Variable de reste pour l'extraction successive des composants
        int64 rem = absNs;

        // Extraction des composants par divisions/modulo successifs
        const int64 days = rem / NS_PER_DAY;
        rem %= NS_PER_DAY;

        const int64 hours = rem / NS_PER_HOUR;
        rem %= NS_PER_HOUR;

        const int64 mins = rem / NS_PER_MINUTE;
        rem %= NS_PER_MINUTE;

        const int64 secs = rem / NS_PER_SECOND;
        rem %= NS_PER_SECOND;

        const int64 ms = rem / NS_PER_MILLISECOND;
        const int64 ns = rem % NS_PER_MILLISECOND;

        // Buffer local pour le formatage (taille généreuse pour sécurité)
        // Format max : "-999999999d 23:59:59.999.999999\0" = ~40 chars
        char buf[160];

        // Offset courant dans le buffer pour écriture séquentielle
        int off = 0;

        // Préfixe de signe pour les durées négatives
        if (mTotalNanoseconds < 0) {
            buf[off++] = '-';
        }

        // Composant jours : affiché uniquement si non nul
        if (days > 0) {
            off += ::snprintf(
                buf + off,
                sizeof(buf) - static_cast<size_t>(off),
                "%lldj ",
                static_cast<long long>(days)
            );
        }

        // Composant horaire : toujours affiché avec format HH:MM:SS
        off += ::snprintf(
            buf + off,
            sizeof(buf) - static_cast<size_t>(off),
            "%02lld:%02lld:%02lld",
            static_cast<long long>(hours),
            static_cast<long long>(mins),
            static_cast<long long>(secs)
        );

        // Composant milliseconde : affiché si non nul ou si nanosecondes présentes
        if (ms > 0 || ns > 0) {
            off += ::snprintf(
                buf + off,
                sizeof(buf) - static_cast<size_t>(off),
                ".%03lld",
                static_cast<long long>(ms)
            );

            // Composant nanoseconde : affiché uniquement si non nul
            if (ns > 0) {
                ::snprintf(
                    buf + off,
                    sizeof(buf) - static_cast<size_t>(off),
                    ".%06lld",
                    static_cast<long long>(ns)
                );
            }
        }

        // Construction de la chaîne NKEntseu depuis le buffer C
        return NkString(buf);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Précision et overflow :
    ----------------------
    - Toutes les opérations arithmétiques utilisent int64 pour le stockage.
    - Les conversions vers double (pour * et /) peuvent perdre en précision
      pour les très grandes valeurs (> 2^53 ns ≈ 104 jours).
    - L'appelant doit garantir que les opérations ne provoquent pas d'overflow.

    Gestion des signes :
    -------------------
    - GetDays/GetHours/etc. retournent des valeurs avec le signe de la durée.
    - GetTime() normalise les durées négatives vers [0, 24h) pour affichage.
    - ToString() préserve le signe avec un préfixe '-' si nécessaire.

    Algorithme GetDate() :
    ---------------------
    - Basé sur l'algorithme de Howard Hinnant (domaine public).
    - Précis pour tout le calendrier grégorien (1582 à +∞).
    - Gère correctement les années bissextiles et les siècles.
    - Référence : http://howardhinnant.github.io/date_algorithms.html

    Performance :
    ------------
    - Getters décomposés : opérations entières simples, très rapides.
    - ToString : allocation unique du buffer sur la pile, pas d'heap.
    - Aucune allocation dynamique dans les méthodes critiques.

    Portabilité :
    ------------
    - %lld pour int64 : standard C99, supporté par tous les compilateurs cibles.
    - static_cast<long long> : garantit la correspondance de type pour printf.
    - Aucune dépendance STL ou système spécifique.
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================