// =============================================================================
// NKTime/NkTimeZone.cpp
// Implémentation de la classe NkTimeZone.
//
// Design :
//  - Constantes provenant exclusivement de time (NkTimeConstants.h)
//  - Plus de magic numbers dupliqués dans ce fichier
//  - NkLocalOffsetSecondsForDate : utilise tm_gmtoff si disponible, sinon portable
//  - NkCivilToDays : algorithme de Howard Hinnant (domaine public)
//  - ToLocal/ToUtc(NkTime) : date de référence en paramètre explicite
//
// Thread-safety :
//  - Toutes les méthodes const sont thread-safe (NkTimeZone immuable)
//  - Les appels système (mktime, localtime_r/s) sont protégés par conception
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
#include "NKTime/NkTimeZone.h"

#include <ctime>
#include <cstring>
#include <cstdio>

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE ET USING LOCAL
// -------------------------------------------------------------------------
// Alias local pour accéder aux constantes de conversion.
// Limité à ce fichier .cpp pour éviter la pollution des espaces de noms.

namespace nkentseu {

    // Alias local pour raccourcir l'accès aux constantes (scope fichier uniquement)
    using namespace time;

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE ANONYME — HELPERS INTERNES
    // -------------------------------------------------------------------------
    // Fonctions utilitaires avec linkage interne (non visibles hors de ce fichier).
    // Organisation par catégorie pour une navigation rapide.

    namespace {

        // =====================================================================
        //  SOUS-SECTION 3.1 : Utilitaires chaînes (sans STL)
        // =====================================================================

        /// Convertit un caractère ASCII en majuscule si possible.
        /// @param c Caractère à convertir
        /// @return Caractère en majuscule, ou inchangé si non alphabétique
        static char ToUpperAscii(char c) noexcept {
            return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        }

        /// Compare deux chaînes en ignorant la casse (ASCII uniquement).
        /// @param lhs Chaîne de gauche (NkString)
        /// @param rhs Chaîne de droite (C-string)
        /// @return true si égales en ignorant la casse, false sinon
        static bool EqualsIgnoreCase(const NkString& lhs, const char* rhs) noexcept {
            // Gestion des pointeurs nuls pour sécurité défensive
            if (!rhs) {
                return false;
            }
            const char* l = lhs.CStr();
            if (!l) {
                return false;
            }
            // Comparaison caractère par caractère jusqu'au terminateur
            while (*l && *rhs) {
                if (ToUpperAscii(*l) != ToUpperAscii(*rhs)) {
                    return false;
                }
                ++l;
                ++rhs;
            }
            // Les deux chaînes doivent se terminer simultanément
            return *l == '\0' && *rhs == '\0';
        }

        /// Vérifie si une chaîne commence par un préfixe (ignore casse).
        /// @param val Chaîne à tester
        /// @param prefix Préfixe recherché
        /// @param outRest Pointeur de sortie pour le reste après le préfixe
        /// @return true si le préfixe correspond, false sinon
        static bool StartsWithIgnoreCase(
            const char* val,
            const char* prefix,
            const char** outRest
        ) noexcept {
            // Gestion des pointeurs nuls pour sécurité défensive
            if (!val || !prefix) {
                return false;
            }
            // Comparaison caractère par caractère du préfixe
            while (*prefix) {
                if (!*val || ToUpperAscii(*val) != ToUpperAscii(*prefix)) {
                    return false;
                }
                ++val;
                ++prefix;
            }
            // Si demandé, retourner le pointeur vers le reste de la chaîne
            if (outRest) {
                *outRest = val;
            }
            return true;
        }

        // =====================================================================
        //  SOUS-SECTION 3.2 : Utilitaires système thread-safe
        // =====================================================================

        /// Wrapper thread-safe pour gmtime (multiplateforme).
        /// @param t Timestamp Unix à convertir
        /// @param out Structure tm de sortie
        /// @return true si la conversion a réussi, false sinon
        static bool SafeGmTime(time_t t, tm& out) noexcept {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : gmtime_s retourne errno_t (0 = succès)
                return ::gmtime_s(&out, &t) == 0;
            #else
                // POSIX : gmtime_r retourne nullptr en cas d'erreur
                return ::gmtime_r(&t, &out) != nullptr;
            #endif
        }

        /// Wrapper thread-safe pour localtime (multiplateforme).
        /// @param t Timestamp Unix à convertir
        /// @param out Structure tm de sortie
        /// @return true si la conversion a réussi, false sinon
        static bool SafeLocalTime(time_t t, tm& out) noexcept {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : localtime_s retourne errno_t (0 = succès)
                return ::localtime_s(&out, &t) == 0;
            #else
                // POSIX : localtime_r retourne nullptr en cas d'erreur
                return ::localtime_r(&t, &out) != nullptr;
            #endif
        }

        // =====================================================================
        //  SOUS-SECTION 3.3 : Algorithme de Howard Hinnant (domaine public)
        // =====================================================================
        // Convertit (année, mois, jour) → jours depuis l'époque Unix (1970-01-01).
        // Référence : http://howardhinnant.github.io/date_algorithms.html

        /// Calcule le nombre de jours depuis l'époque Unix pour une date civile.
        /// @param y Année (ex: 2024)
        /// @param m Mois [1, 12]
        /// @param d Jour [1, 31]
        /// @return Nombre de jours depuis 1970-01-01 (peut être négatif)
        /// @note Algorithme de Howard Hinnant : précis, efficace, sans appel système
        static int64 CivilToDays(int32 y, int32 m, int32 d) noexcept {
            // Ajustement pour traiter janvier/février comme mois 13/14 de l'année précédente
            int64 Y = static_cast<int64>(y) - (m <= 2 ? 1 : 0);

            // Calcul de l'ère (période de 400 ans dans le calendrier grégorien)
            const int64 era = (Y >= 0 ? Y : Y - 399) / 400;

            // Année dans l'ère (0 à 399)
            const int64 yoe = Y - era * 400;

            // Mois priorisé (mars=0, février=11) pour simplifier le calcul des jours
            const int64 mp = (m > 2) ? (m - 3) : (m + 9);

            // Jour dans l'année "priorisée" (0 à 365/366)
            const int64 doy = (153 * mp + 2) / 5 + d - 1;

            // Jour dans l'ère (0 à 146096)
            const int64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

            // Jours totaux depuis l'époque Unix (offset 719468 = 1970-01-01)
            return era * 146097 + doe - 719468;
        }

        /// Construit un timestamp UTC depuis une date et une heure, sans appel système.
        /// @param date Date civile (année, mois, jour)
        /// @param h Heure [0, 23]
        /// @param m Minute [0, 59]
        /// @param s Seconde [0, 59]
        /// @return Timestamp Unix (secondes depuis 1970-01-01 00:00:00 UTC)
        static time_t UtcDateToTimeT(
            const NkDate& date,
            int32 h,
            int32 m,
            int32 s
        ) noexcept {
            // Conversion de la date en jours depuis l'époque Unix
            const int64 days = CivilToDays(
                date.GetYear(),
                date.GetMonth(),
                date.GetDay()
            );

            // Combinaison jours + heure en secondes totales
            return static_cast<time_t>(
                days * SECONDS_PER_DAY
                + static_cast<int64>(h) * SECONDS_PER_HOUR
                + static_cast<int64>(m) * SECONDS_PER_MINUTE
                + static_cast<int64>(s)
            );
        }

        // =====================================================================
        //  SOUS-SECTION 3.4 : Calcul d'offset local pour une date donnée
        // =====================================================================

        /// Calcule l'offset du fuseau local en secondes pour une date donnée.
        /// @param date Date de référence pour le calcul DST
        /// @return Offset en secondes (positif pour EST, négatif pour WST)
        /// @note Utilise tm_gmtoff si disponible, sinon méthode portable
        static int32 LocalOffsetSecondsForDate(const NkDate& date) noexcept {
            // Initialisation de la structure tm avec la date fournie
            tm localTm = {};
            localTm.tm_year = date.GetYear() - 1900;   // tm_year : années depuis 1900
            localTm.tm_mon = date.GetMonth() - 1;      // tm_mon : [0, 11]
            localTm.tm_mday = date.GetDay();           // tm_mday : [1, 31]
            localTm.tm_hour = 12;                      // Midi : évite les ambiguïtés de minuit/DST
            localTm.tm_isdst = -1;                     // -1 : laisser l'OS déterminer le DST

            // Conversion en timestamp Unix via mktime (interprété comme heure locale)
            const time_t stamp = ::mktime(&localTm);
            if (stamp == static_cast<time_t>(-1)) {
                return 0;  // Échec de conversion : retour offset nul par défaut
            }

            #if defined(_GNU_SOURCE) || defined(__APPLE__) || \
                (defined(__GLIBC__) && !defined(NKENTSEU_PLATFORM_WINDOWS))
                // Plateformes avec tm_gmtoff : offset direct, pas de second mktime
                // tm_gmtoff est en secondes, signe : positif = EST de UTC
                return static_cast<int32>(localTm.tm_gmtoff);
            #else
                // Méthode portable : comparer mktime(local) vs mktime(UTC)
                tm utcTm = {};
                if (!SafeGmTime(stamp, utcTm)) {
                    return 0;  // Échec de conversion GMT
                }
                utcTm.tm_isdst = 0;  // Forcer non-DST pour l'interprétation UTC

                // Interpréter le timestamp UTC comme heure locale via mktime
                const time_t utcAsLocal = ::mktime(&utcTm);
                if (utcAsLocal == static_cast<time_t>(-1)) {
                    return 0;  // Échec de conversion
                }

                // Différence = offset local en secondes
                return static_cast<int32>(stamp - utcAsLocal);
            #endif
        }

        /// Détermine si une date tombe en heure d'été (DST) pour le fuseau local.
        /// @param date Date à tester
        /// @return true si DST actif, false sinon
        /// @note Délègue à tm_isdst via mktime (comportement dépendant de l'OS)
        static bool LocalIsDstForDate(const NkDate& date) noexcept {
            // Initialisation de la structure tm avec la date fournie
            tm localTm = {};
            localTm.tm_year = date.GetYear() - 1900;
            localTm.tm_mon = date.GetMonth() - 1;
            localTm.tm_mday = date.GetDay();
            localTm.tm_hour = 12;  // Midi : évite les ambiguïtés de minuit/DST
            localTm.tm_isdst = -1; // Laisser l'OS déterminer

            // Conversion via mktime pour peupler tm_isdst
            if (::mktime(&localTm) == static_cast<time_t>(-1)) {
                return false;  // Échec : supposer non-DST par défaut
            }

            // tm_isdst > 0 signifie DST actif, == 0 signifie standard, < 0 inconnu
            return localTm.tm_isdst > 0;
        }

        // =====================================================================
        //  SOUS-SECTION 3.5 : Parsing d'offset fixe depuis un nom IANA
        // =====================================================================

        /// Parse un entier positif depuis une plage de caractères.
        /// @param begin Début de la plage (inclusive)
        /// @param end Fin de la plage (exclusive)
        /// @return Valeur entière, ou -1 en cas d'erreur de parsing
        static int32 ParsePositiveInt(const char* begin, const char* end) noexcept {
            // Validation des pointeurs et de la plage
            if (!begin || !end || begin >= end) {
                return -1;
            }
            int32 v = 0;
            // Parcours caractère par caractère
            for (const char* p = begin; p < end; ++p) {
                // Vérification que le caractère est un chiffre
                if (*p < '0' || *p > '9') {
                    return -1;
                }
                // Accumulation de la valeur décimale
                v = v * 10 + (*p - '0');
            }
            return v;
        }

        /// Parse un offset fixe depuis un nom de fuseau (ex: "UTC+5:30").
        /// @param name Nom du fuseau à parser
        /// @param outOffset Variable de sortie pour l'offset en secondes
        /// @return true si le parsing a réussi, false sinon
        static bool ParseFixedOffsetSeconds(
            const NkString& name,
            int32& outOffset
        ) noexcept {
            const char* rest = nullptr;
            const char* val = name.CStr();

            // Vérification du préfixe "UTC" ou "GMT" (ignore casse)
            if (!StartsWithIgnoreCase(val, "UTC", &rest)
                && !StartsWithIgnoreCase(val, "GMT", &rest)) {
                return false;
            }

            // Cas sans suffixe : "UTC" ou "GMT" = offset 0
            if (!rest || *rest == '\0') {
                outOffset = 0;
                return true;
            }

            // Détection du signe (+ ou -)
            const int32 sign = (*rest == '+') ? 1 : (*rest == '-') ? -1 : 0;
            if (!sign) {
                return false;  // Signe manquant ou invalide
            }
            ++rest;  // Avancer après le signe

            // Parsing des heures
            const char* hourStart = rest;
            while (*rest >= '0' && *rest <= '9') {
                ++rest;
            }
            const int32 hours = ParsePositiveInt(hourStart, rest);
            if (hours < 0 || hours > 23) {
                return false;  // Heures hors plage valide
            }

            // Parsing optionnel des minutes (après ':')
            int32 minutes = 0;
            if (*rest == ':') {
                ++rest;  // Avancer après ':'
                const char* minStart = rest;
                while (*rest >= '0' && *rest <= '9') {
                    ++rest;
                }
                minutes = ParsePositiveInt(minStart, rest);
                if (minutes < 0 || minutes > 59) {
                    return false;  // Minutes hors plage valide
                }
            }

            // Vérification de fin de chaîne (pas de caractères supplémentaires)
            if (*rest != '\0') {
                return false;
            }

            // Calcul de l'offset total en secondes
            outOffset = sign * (hours * 3600 + minutes * 60);
            return true;
        }

        /// Replie un total de nanosecondes dans l'intervalle [0, NS_PER_DAY).
        /// @param ns Valeur en nanosecondes (peut être négative ou > 1 jour)
        /// @return Valeur normalisée dans [0, NS_PER_DAY)
        /// @note Utile pour les conversions d'heure qui "bouclent" sur 24h
        static int64 WrapNsInDay(int64 ns) noexcept {
            // Modulo pour ramener dans [0, NS_PER_DAY) ou (-NS_PER_DAY, 0)
            int64 w = ns % NS_PER_DAY;
            // Correction pour les valeurs négatives
            return (w < 0) ? w + NS_PER_DAY : w;
        }

    } // namespace anonymous

    // =============================================================================
    //  NkTimeZone — Constructeur
    // =============================================================================

    NkTimeZone::NkTimeZone(
        NkKind kind,
        const NkString& name,
        int32 fixedOffsetSeconds
    ) noexcept
        : mKind(kind)
        , mName(name)
        , mFixedOffsetSeconds(fixedOffsetSeconds)
    {
        // Initialisation via liste de membres : efficace et clair
        // Pas de logique supplémentaire nécessaire dans le corps
    }

    // =============================================================================
    //  Fabriques statiques
    // =============================================================================

    NkTimeZone NkTimeZone::GetLocal() noexcept {
        // Construction du fuseau système local avec nom descriptif
        return NkTimeZone(NkKind::NK_LOCAL, NkString("Local"));
    }

    NkTimeZone NkTimeZone::GetUtc() noexcept {
        // Construction du fuseau UTC fixe (offset = 0)
        return NkTimeZone(NkKind::NK_UTC, NkString("UTC"));
    }

    NkTimeZone NkTimeZone::FromName(const NkString& ianaName) noexcept {
        // Gestion des cas spéciaux : chaîne vide ou "Local" → fuseau local
        if (ianaName.Empty() || EqualsIgnoreCase(ianaName, "Local")) {
            return GetLocal();
        }

        // Gestion des alias UTC : "UTC", "GMT", "Z", "Etc/UTC" → fuseau UTC
        if (EqualsIgnoreCase(ianaName, "UTC")
            || EqualsIgnoreCase(ianaName, "GMT")
            || EqualsIgnoreCase(ianaName, "Z")
            || EqualsIgnoreCase(ianaName, "Etc/UTC")) {
            return GetUtc();
        }

        // Tentative de parsing d'un offset fixe (ex: "UTC+5:30")
        int32 offsetSec = 0;
        if (ParseFixedOffsetSeconds(ianaName, offsetSec)) {
            // Offset nul → retourne UTC pour cohérence
            if (offsetSec == 0) {
                return GetUtc();
            }
            // Offset non nul → fuseau à offset fixe
            return NkTimeZone(NkKind::NK_FIXED_OFFSET, ianaName, offsetSec);
        }

        // Nom IANA inconnu (pas de base de données) → fallback vers local
        // Note : Dans une version future avec base IANA, charger les données ici
        return NkTimeZone(NkKind::NK_LOCAL, ianaName);
    }

    // =============================================================================
    //  Conversions — NkTime (avec paramètre refDate explicite)
    // =============================================================================

    NkTime NkTimeZone::ToLocal(
        const NkTime& utcTime,
        const NkDate& refDate
    ) const noexcept {
        // Obtention de l'offset UTC pour la date de référence (gère DST)
        const int64 offsetNs = GetUtcOffset(refDate).ToNanoseconds();

        // Application de l'offset et normalisation dans [0, 24h)
        return NkTime(WrapNsInDay(static_cast<int64>(utcTime) + offsetNs));
    }

    NkTime NkTimeZone::ToUtc(
        const NkTime& localTime,
        const NkDate& refDate
    ) const noexcept {
        // Obtention de l'offset UTC pour la date de référence (gère DST)
        const int64 offsetNs = GetUtcOffset(refDate).ToNanoseconds();

        // Soustraction de l'offset et normalisation dans [0, 24h)
        return NkTime(WrapNsInDay(static_cast<int64>(localTime) - offsetNs));
    }

    // =============================================================================
    //  Conversions — NkDate (changement de jour potentiel)
    // =============================================================================

    NkDate NkTimeZone::ToLocal(const NkDate& utcDate) const noexcept {
        // Cas trivial : UTC vers UTC = identité
        if (mKind == NkKind::NK_UTC) {
            return utcDate;
        }

        // Conversion de la date UTC en timestamp Unix à minuit
        const time_t utcStamp = UtcDateToTimeT(utcDate, 0, 0, 0);

        // Cas offset fixe : ajout simple de l'offset puis conversion
        if (mKind == NkKind::NK_FIXED_OFFSET) {
            tm shifted = {};
            if (!SafeGmTime(static_cast<time_t>(utcStamp + mFixedOffsetSeconds), shifted)) {
                return utcDate;  // Échec : retour de la date originale
            }
            return NkDate(
                shifted.tm_year + 1900,
                shifted.tm_mon + 1,
                shifted.tm_mday
            );
        }

        // Cas local : conversion via localtime (gère DST automatiquement)
        tm localTm = {};
        if (!SafeLocalTime(utcStamp, localTm)) {
            return utcDate;  // Échec : retour de la date originale
        }
        return NkDate(
            localTm.tm_year + 1900,
            localTm.tm_mon + 1,
            localTm.tm_mday
        );
    }

    NkDate NkTimeZone::ToUtc(const NkDate& localDate) const noexcept {
        // Cas trivial : UTC vers UTC = identité
        if (mKind == NkKind::NK_UTC) {
            return localDate;
        }

        // Cas offset fixe : soustraction simple de l'offset
        if (mKind == NkKind::NK_FIXED_OFFSET) {
            const time_t localStamp = UtcDateToTimeT(localDate, 0, 0, 0);
            tm utcTm = {};
            if (!SafeGmTime(static_cast<time_t>(localStamp - mFixedOffsetSeconds), utcTm)) {
                return localDate;  // Échec : retour de la date originale
            }
            return NkDate(
                utcTm.tm_year + 1900,
                utcTm.tm_mon + 1,
                utcTm.tm_mday
            );
        }

        // Cas local : conversion via mktime + gmtime
        tm localTm = {};
        localTm.tm_year = localDate.GetYear() - 1900;
        localTm.tm_mon = localDate.GetMonth() - 1;
        localTm.tm_mday = localDate.GetDay();
        localTm.tm_isdst = -1;  // Laisser l'OS déterminer le DST

        // Conversion en timestamp Unix (interprété comme heure locale)
        const time_t stamp = ::mktime(&localTm);
        if (stamp == static_cast<time_t>(-1)) {
            return localDate;  // Échec : retour de la date originale
        }

        // Conversion du timestamp en UTC via gmtime
        tm utcTm = {};
        if (!SafeGmTime(stamp, utcTm)) {
            return localDate;  // Échec : retour de la date originale
        }
        return NkDate(
            utcTm.tm_year + 1900,
            utcTm.tm_mon + 1,
            utcTm.tm_mday
        );
    }

    // =============================================================================
    //  Informations — Accesseurs
    // =============================================================================

    const NkString& NkTimeZone::GetName() const noexcept {
        // Retour par référence constante : zéro copie, zéro allocation
        return mName;
    }

    NkTimeZone::NkKind NkTimeZone::GetKind() const noexcept {
        // Retour direct de l'enum : opération triviale
        return mKind;
    }

    int32 NkTimeZone::GetFixedOffsetSeconds() const noexcept {
        // Retour direct de l'offset fixe (0 pour NK_LOCAL et NK_UTC)
        return mFixedOffsetSeconds;
    }

    NkTimeSpan NkTimeZone::GetUtcOffset(const NkDate& date) const noexcept {
        // Cas UTC : offset toujours nul
        if (mKind == NkKind::NK_UTC) {
            return NkTimeSpan(0LL);
        }

        // Cas offset fixe : conversion de secondes vers NkTimeSpan
        if (mKind == NkKind::NK_FIXED_OFFSET) {
            return NkTimeSpan(
                0LL,  // days
                0LL,  // hours
                0LL,  // minutes
                static_cast<int64>(mFixedOffsetSeconds)  // seconds
            );
        }

        // Cas local : calcul dynamique via helper (gère DST)
        return NkTimeSpan(
            0LL,  // days
            0LL,  // hours
            0LL,  // minutes
            static_cast<int64>(LocalOffsetSecondsForDate(date))  // seconds
        );
    }

    bool NkTimeZone::IsDaylightSavingTime(const NkDate& date) const noexcept {
        // DST n'existe que pour le fuseau local
        if (mKind != NkKind::NK_LOCAL) {
            return false;
        }
        // Délégation au helper qui utilise tm_isdst via mktime
        return LocalIsDstForDate(date);
    }

    // =============================================================================
    //  Comparaisons
    // =============================================================================

    bool NkTimeZone::operator==(const NkTimeZone& o) const noexcept {
        // Égalité complète : kind, offset ET nom doivent correspondre
        // Note : Deux fuseaux avec même offset mais noms différents ne sont pas égaux
        return mKind == o.mKind
            && mFixedOffsetSeconds == o.mFixedOffsetSeconds
            && mName == o.mName;
    }

    bool NkTimeZone::operator!=(const NkTimeZone& o) const noexcept {
        // Inégalité : déléguée à l'opérateur d'égalité pour cohérence
        return !(*this == o);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Thread-safety :
    --------------
    - NkTimeZone est immuable après construction : toutes les méthodes const
      sont thread-safe sans synchronisation externe.
    - Les appels système (mktime, localtime_r/s, gmtime_r/s) utilisent les
      versions thread-safe selon la plateforme.
    - Les helpers dans le namespace anonyme ont un état local uniquement.

    Gestion du DST (Daylight Saving Time) :
    --------------------------------------
    - Pour NK_LOCAL : le DST est déterminé par l'OS via tm_isdst après mktime.
    - La date de référence est cruciale : 15 juillet vs 15 janvier peuvent
      donner des offsets différents dans les zones avec DST.
    - Pour NK_UTC et NK_FIXED_OFFSET : DST n'existe pas, offset constant.

    Algorithme CivilToDays (Howard Hinnant) :
    ----------------------------------------
    - Précis pour tout le calendrier grégorien (1582 à +∞).
    - Gère correctement les années bissextiles et les siècles.
    - Sans appel système : rapide et déterministe.
    - Référence complète : http://howardhinnant.github.io/date_algorithms.html

    Portabilité des offsets :
    ------------------------
    - tm_gmtoff (GNU/BSD) : disponible sur Linux/macOS, retour direct.
    - Méthode portable (mktime + gmtime) : fonctionne partout mais plus lente.
    - La détection compile-time choisit la meilleure méthode disponible.

    Parsing des noms de fuseaux :
    ----------------------------
    - Supporte "UTC", "GMT", "Z", "Etc/UTC" comme alias de UTC.
    - Parse "UTC+HH", "UTC+HH:MM", "GMT-HH", etc. pour les offsets fixes.
    - Sans base de données IANA complète, les noms inconnus fallback vers Local.
    - Extension future : intégrer une base tzdata pour support IANA complet.

    Performance :
    ------------
    - GetUtcOffset : O(1) pour UTC/Fixed, O(1) avec mktime pour Local.
    - ToLocal/ToUtc : conversions arithmétiques simples + normalisation.
    - Aucune allocation dynamique dans les chemins critiques.
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================