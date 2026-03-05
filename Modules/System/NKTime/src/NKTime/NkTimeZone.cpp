/**
* @File NkTimeZone.cpp
* @Description Implémentation de la classe NkTimeZone
*/

#include "pch.h"
#include "NkTimeZone.h"
#include <ctime>

namespace nkentseu {

    // NkTimeZone NkTimeZone::GetLocal() {
    //     return NkTimeZone("Local");
    // }

    // NkTime NkTimeZone::ToLocal(const NkTime& utcTime) const {
    //     std::time_t t = utcTime.ToTimeT();
    //     std::tm localTm;
    //
    //     #if defined(NKENTSEU_PLATFORM_WINDOWS)
    //         localtime_s(&localTm, &t);
    //     #else
    //         localtime_r(&t, &localTm);
    //     #endif

    //     return NkTime(localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
    // }

    // NkTimeSpan NkTimeZone::GetUtcOffset(const NkDate& date) const {
    //     std::tm tm = {0};
    //     tm.tm_year = date.GetYear() - 1900;
    //     tm.tm_mon = date.GetMonth() - 1;
    //     tm.tm_mday = date.GetDay();
    //
    //     std::mktime(&tm);
    //     return NkTimeSpan(0, static_cast<int64>(tm.tm_gmtoff / 3600), 0, 0);
    // }

    // bool NkTimeZone::IsDaylightSavingTime(const NkDate& date) const {
    //     std::tm tm = {0};
    //     tm.tm_year = date.GetYear() - 1900;
    //     tm.tm_mon = date.GetMonth() - 1;
    //     tm.tm_mday = date.GetDay();
    //
    //     std::mktime(&tm);
    //     return tm.tm_isdst > 0;
    // }
} // namespace nkentseu (fin du namespace)


// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
