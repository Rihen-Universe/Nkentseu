/**
 * @File    NkTimeZone.cpp
 * @Brief   Implémentation de NkTimeZone.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Notes
 *  Constantes : proviennent de time (NkTimeConstants.h).
 *  Plus de magic numbers dupliqués dans ce fichier.
 *
 *  NkLocalOffsetSecondsForDate :
 *    Utilise tm_gmtoff (Linux/macOS/GCC) quand disponible — un seul mktime.
 *    Sinon : calcul portable mktime + gmtime.
 *
 *  NkCivilToDays : algorithme de Howard Hinnant (domaine public).
 *    Convertit (y, m, d) → jours depuis l'époque Unix, sans appel système.
 *
 *  ToLocal/ToUtc (NkTime) : la date de référence est maintenant un paramètre
 *    explicite (corrige l'effet de bord GetCurrent() dans la version originale).
 */

#include "pch.h"
#include "NKTime/NkTimeZone.h"
#include <ctime>
#include <cstring>
#include <cstdio>

namespace nkentseu {

    using namespace time;

    // =============================================================================
    //  Helpers internes (linkage interne)
    // =============================================================================
    namespace {

        // ── Utilitaires chaînes (sans STL) ────────────────────────────────────────

        static char ToUpperAscii(char c) noexcept {
            return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        }

        static bool EqualsIgnoreCase(const NkString& lhs, const char* rhs) noexcept {
            if (!rhs) return false;
            const char* l = lhs.CStr();
            if (!l) return false;
            while (*l && *rhs) {
                if (ToUpperAscii(*l) != ToUpperAscii(*rhs)) return false;
                ++l; ++rhs;
            }
            return *l == '\0' && *rhs == '\0';
        }

        static bool StartsWithIgnoreCase(const char* val, const char* prefix,
                                        const char** outRest) noexcept {
            if (!val || !prefix) return false;
            while (*prefix) {
                if (!*val || ToUpperAscii(*val) != ToUpperAscii(*prefix)) return false;
                ++val; ++prefix;
            }
            if (outRest) *outRest = val;
            return true;
        }

        // ── Utilitaires système thread-safe ───────────────────────────────────────

        static bool SafeGmTime(time_t t, tm& out) noexcept {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            return ::gmtime_s(&out, &t) == 0;
    #else
            return ::gmtime_r(&t, &out) != nullptr;
    #endif
        }

        static bool SafeLocalTime(time_t t, tm& out) noexcept {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
            return ::localtime_s(&out, &t) == 0;
    #else
            return ::localtime_r(&t, &out) != nullptr;
    #endif
        }

        // ── Algorithme de Howard Hinnant (domaine public) ─────────────────────────
        // Convertit (année, mois, jour) → jours depuis l'époque Unix, sans mktime.

        static int64 CivilToDays(int32 y, int32 m, int32 d) noexcept {
            int64 Y = static_cast<int64>(y) - (m <= 2 ? 1 : 0);
            const int64 era = (Y >= 0 ? Y : Y - 399) / 400;
            const int64 yoe = Y - era * 400;
            const int64 mp  = (m > 2) ? (m - 3) : (m + 9);
            const int64 doy = (153 * mp + 2) / 5 + d - 1;
            const int64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + doe - 719468;
        }

        /// Construit un timestamp UTC depuis date + heure, sans appel système.
        static time_t UtcDateToTimeT(const NkDate& date, int32 h, int32 m, int32 s) noexcept {
            const int64 days = CivilToDays(date.GetYear(), date.GetMonth(), date.GetDay());
            return static_cast<time_t>(
                days * SECONDS_PER_DAY
                + static_cast<int64>(h) * SECONDS_PER_HOUR
                + static_cast<int64>(m) * SECONDS_PER_MINUTE
                + static_cast<int64>(s)
            );
        }

        // ── Offset local pour une date donnée ─────────────────────────────────────

        static int32 LocalOffsetSecondsForDate(const NkDate& date) noexcept {
            tm localTm = {};
            localTm.tm_year  = date.GetYear() - 1900;
            localTm.tm_mon   = date.GetMonth() - 1;
            localTm.tm_mday  = date.GetDay();
            localTm.tm_hour  = 12; // Midi — évite les ambiguïtés de minuit
            localTm.tm_isdst = -1;

            const time_t stamp = ::mktime(&localTm);
            if (stamp == static_cast<time_t>(-1)) return 0;

    #if defined(_GNU_SOURCE) || defined(__APPLE__) || \
        (defined(__GLIBC__) && !defined(NKENTSEU_PLATFORM_WINDOWS))
            // tm_gmtoff disponible — offset direct, pas de second mktime
            return static_cast<int32>(localTm.tm_gmtoff);
    #else
            // Portable : compare le timestamp UTC interprété comme local
            tm utcTm = {};
            if (!SafeGmTime(stamp, utcTm)) return 0;
            utcTm.tm_isdst = 0;
            const time_t utcAsLocal = ::mktime(&utcTm);
            if (utcAsLocal == static_cast<time_t>(-1)) return 0;
            return static_cast<int32>(stamp - utcAsLocal);
    #endif
        }

        static bool LocalIsDstForDate(const NkDate& date) noexcept {
            tm localTm = {};
            localTm.tm_year  = date.GetYear() - 1900;
            localTm.tm_mon   = date.GetMonth() - 1;
            localTm.tm_mday  = date.GetDay();
            localTm.tm_hour  = 12;
            localTm.tm_isdst = -1;
            if (::mktime(&localTm) == static_cast<time_t>(-1)) return false;
            return localTm.tm_isdst > 0;
        }

        // ── Parsing d'offset fixe ─────────────────────────────────────────────────

        static int32 ParsePositiveInt(const char* begin, const char* end) noexcept {
            if (!begin || !end || begin >= end) return -1;
            int32 v = 0;
            for (const char* p = begin; p < end; ++p) {
                if (*p < '0' || *p > '9') return -1;
                v = v * 10 + (*p - '0');
            }
            return v;
        }

        static bool ParseFixedOffsetSeconds(const NkString& name, int32& outOffset) noexcept {
            const char* rest = nullptr;
            const char* val  = name.CStr();

            if (!StartsWithIgnoreCase(val, "UTC", &rest) &&
                !StartsWithIgnoreCase(val, "GMT", &rest))
                return false;

            if (!rest || *rest == '\0') { outOffset = 0; return true; }

            const int32 sign = (*rest == '+') ? 1 : (*rest == '-') ? -1 : 0;
            if (!sign) return false;
            ++rest;

            const char* hourStart = rest;
            while (*rest >= '0' && *rest <= '9') ++rest;
            const int32 hours = ParsePositiveInt(hourStart, rest);
            if (hours < 0 || hours > 23) return false;

            int32 minutes = 0;
            if (*rest == ':') {
                ++rest;
                const char* minStart = rest;
                while (*rest >= '0' && *rest <= '9') ++rest;
                minutes = ParsePositiveInt(minStart, rest);
                if (minutes < 0 || minutes > 59) return false;
            }
            if (*rest != '\0') return false;
            outOffset = sign * (hours * 3600 + minutes * 60);
            return true;
        }

        /// Replie un total de nanosecondes dans [0, NS_PER_DAY).
        static int64 WrapNsInDay(int64 ns) noexcept {
            int64 w = ns % NS_PER_DAY;
            return (w < 0) ? w + NS_PER_DAY : w;
        }

    } // anonymous namespace

    // =============================================================================
    //  NkTimeZone — Constructeur
    // =============================================================================

    NkTimeZone::NkTimeZone(NkKind kind, const NkString& name, int32 fixedOffsetSeconds) noexcept
        : mKind(kind), mName(name), mFixedOffsetSeconds(fixedOffsetSeconds)
    {}

    // =============================================================================
    //  Fabriques statiques
    // =============================================================================

    NkTimeZone NkTimeZone::GetLocal() noexcept {
        return NkTimeZone(NkKind::NK_LOCAL, NkString("Local"));
    }

    NkTimeZone NkTimeZone::GetUtc() noexcept {
        return NkTimeZone(NkKind::NK_UTC, NkString("UTC"));
    }

    NkTimeZone NkTimeZone::FromName(const NkString& ianaName) noexcept {
        if (ianaName.Empty() || EqualsIgnoreCase(ianaName, "Local"))
            return GetLocal();

        if (EqualsIgnoreCase(ianaName, "UTC")  ||
            EqualsIgnoreCase(ianaName, "GMT")  ||
            EqualsIgnoreCase(ianaName, "Z")    ||
            EqualsIgnoreCase(ianaName, "Etc/UTC"))
            return GetUtc();

        int32 offsetSec = 0;
        if (ParseFixedOffsetSeconds(ianaName, offsetSec)) {
            if (offsetSec == 0) return GetUtc();
            return NkTimeZone(NkKind::NK_FIXED_OFFSET, ianaName, offsetSec);
        }

        // Nom IANA inconnu (pas de base de données) → fallback local
        return NkTimeZone(NkKind::NK_LOCAL, ianaName);
    }

    // =============================================================================
    //  Conversions — NkTime
    // =============================================================================

    NkTime NkTimeZone::ToLocal(const NkTime& utcTime, const NkDate& refDate) const noexcept {
        const int64 offsetNs = GetUtcOffset(refDate).ToNanoseconds();
        return NkTime(WrapNsInDay(static_cast<int64>(utcTime) + offsetNs));
    }

    NkTime NkTimeZone::ToUtc(const NkTime& localTime, const NkDate& refDate) const noexcept {
        const int64 offsetNs = GetUtcOffset(refDate).ToNanoseconds();
        return NkTime(WrapNsInDay(static_cast<int64>(localTime) - offsetNs));
    }

    // =============================================================================
    //  Conversions — NkDate
    // =============================================================================

    NkDate NkTimeZone::ToLocal(const NkDate& utcDate) const noexcept {
        if (mKind == NkKind::NK_UTC) return utcDate;

        const time_t utcStamp = UtcDateToTimeT(utcDate, 0, 0, 0);

        if (mKind == NkKind::NK_FIXED_OFFSET) {
            tm shifted = {};
            if (!SafeGmTime(static_cast<time_t>(utcStamp + mFixedOffsetSeconds), shifted))
                return utcDate;
            return NkDate(shifted.tm_year + 1900, shifted.tm_mon + 1, shifted.tm_mday);
        }

        // Local
        tm localTm = {};
        if (!SafeLocalTime(utcStamp, localTm)) return utcDate;
        return NkDate(localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
    }

    NkDate NkTimeZone::ToUtc(const NkDate& localDate) const noexcept {
        if (mKind == NkKind::NK_UTC) return localDate;

        if (mKind == NkKind::NK_FIXED_OFFSET) {
            const time_t localStamp = UtcDateToTimeT(localDate, 0, 0, 0);
            tm utcTm = {};
            if (!SafeGmTime(static_cast<time_t>(localStamp - mFixedOffsetSeconds), utcTm))
                return localDate;
            return NkDate(utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday);
        }

        // Local
        tm localTm = {};
        localTm.tm_year  = localDate.GetYear() - 1900;
        localTm.tm_mon   = localDate.GetMonth() - 1;
        localTm.tm_mday  = localDate.GetDay();
        localTm.tm_isdst = -1;
        const time_t stamp = ::mktime(&localTm);
        if (stamp == static_cast<time_t>(-1)) return localDate;

        tm utcTm = {};
        if (!SafeGmTime(stamp, utcTm)) return localDate;
        return NkDate(utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday);
    }

    // =============================================================================
    //  Informations
    // =============================================================================

    const NkString& NkTimeZone::GetName() const noexcept { return mName; }
    NkTimeZone::NkKind NkTimeZone::GetKind() const noexcept { return mKind; }
    int32 NkTimeZone::GetFixedOffsetSeconds() const noexcept { return mFixedOffsetSeconds; }

    NkTimeSpan NkTimeZone::GetUtcOffset(const NkDate& date) const noexcept {
        if (mKind == NkKind::NK_UTC)
            return NkTimeSpan(0LL);
        if (mKind == NkKind::NK_FIXED_OFFSET)
            return NkTimeSpan(0LL, 0LL, 0LL, static_cast<int64>(mFixedOffsetSeconds));
        return NkTimeSpan(0LL, 0LL, 0LL,
                        static_cast<int64>(LocalOffsetSecondsForDate(date)));
    }

    bool NkTimeZone::IsDaylightSavingTime(const NkDate& date) const noexcept {
        if (mKind != NkKind::NK_LOCAL) return false;
        return LocalIsDstForDate(date);
    }

    // =============================================================================
    //  Comparaisons
    // =============================================================================

    bool NkTimeZone::operator==(const NkTimeZone& o) const noexcept {
        return mKind == o.mKind
            && mFixedOffsetSeconds == o.mFixedOffsetSeconds
            && mName == o.mName;
    }
    bool NkTimeZone::operator!=(const NkTimeZone& o) const noexcept { return !(*this == o); }

} // namespace nkentseu