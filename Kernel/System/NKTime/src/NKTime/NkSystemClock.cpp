// =============================================================================
// NkSystemClock.cpp — Heure murale. Contrat et distinction monotone/mural :
//                     voir NkSystemClock.h.
// =============================================================================

#include "pch.h"
#include "NKTime/NkSystemClock.h"
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

namespace nkentseu {

	namespace {

		// Écrit un entier sur `chiffres` positions, zéros en tête, sans passer
		// par `snprintf` — c'est précisément ce qu'on retire du moteur.
		// Rend le nombre de caractères écrits.
		usize EcrirePadded(char *out, int32 valeur, int32 chiffres) noexcept {
			if (valeur < 0)
				valeur = 0;
			for (int32 i = chiffres - 1; i >= 0; --i) {
				out[i] = (char)('0' + (valeur % 10));
				valeur /= 10;
			}
			return (usize)chiffres;
		}

	} // namespace

	int64 NkSystemClock::UnixSeconds() noexcept {
		return UnixMilliseconds() / 1000;
	}

	int64 NkSystemClock::UnixMilliseconds() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		// FILETIME compte les intervalles de 100 ns depuis le 1er janvier 1601.
		// L'écart avec l'époque Unix est une constante connue : 11 644 473 600
		// secondes. On la pose en clair plutôt qu'en nombre magique.
		FILETIME ft{};
		::GetSystemTimeAsFileTime(&ft);
		ULARGE_INTEGER li;
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		const int64 cent_ns = (int64)li.QuadPart;
		const int64 SECONDES_1601_A_1970 = 11644473600LL;
		return (cent_ns / 10000LL) - (SECONDES_1601_A_1970 * 1000LL);
#else
		struct timespec ts;
		if (::clock_gettime(CLOCK_REALTIME, &ts) != 0)
			return 0;
		return (int64)ts.tv_sec * 1000LL + (int64)(ts.tv_nsec / 1000000L);
#endif
	}

	NkSystemDateTime NkSystemClock::LocalNow() noexcept {
		NkSystemDateTime d{};
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		SYSTEMTIME st{};
		::GetLocalTime(&st);
		d.year = (int32)st.wYear;
		d.month = (int32)st.wMonth;
		d.day = (int32)st.wDay;
		d.hour = (int32)st.wHour;
		d.minute = (int32)st.wMinute;
		d.second = (int32)st.wSecond;
		d.millisecond = (int32)st.wMilliseconds;
		d.valid = (d.year > 1600);
#else
		const int64 ms = UnixMilliseconds();
		time_t t = (time_t)(ms / 1000);
		struct tm tmv;
		if (::localtime_r(&t, &tmv) == nullptr)
			return d;
		d.year = tmv.tm_year + 1900;
		d.month = tmv.tm_mon + 1;
		d.day = tmv.tm_mday;
		d.hour = tmv.tm_hour;
		d.minute = tmv.tm_min;
		d.second = tmv.tm_sec;
		d.millisecond = (int32)(ms % 1000);
		d.valid = true;
#endif
		return d;
	}

	NkSystemDateTime NkSystemClock::UtcNow() noexcept {
		NkSystemDateTime d{};
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		SYSTEMTIME st{};
		::GetSystemTime(&st);
		d.year = (int32)st.wYear;
		d.month = (int32)st.wMonth;
		d.day = (int32)st.wDay;
		d.hour = (int32)st.wHour;
		d.minute = (int32)st.wMinute;
		d.second = (int32)st.wSecond;
		d.millisecond = (int32)st.wMilliseconds;
		d.valid = (d.year > 1600);
#else
		const int64 ms = UnixMilliseconds();
		time_t t = (time_t)(ms / 1000);
		struct tm tmv;
		if (::gmtime_r(&t, &tmv) == nullptr)
			return d;
		d.year = tmv.tm_year + 1900;
		d.month = tmv.tm_mon + 1;
		d.day = tmv.tm_mday;
		d.hour = tmv.tm_hour;
		d.minute = tmv.tm_min;
		d.second = tmv.tm_sec;
		d.millisecond = (int32)(ms % 1000);
		d.valid = true;
#endif
		return d;
	}

	bool NkSystemClock::StampCompact(char *out, usize outSize, bool utc) noexcept {
		// 8 + 1 + 6 + 1 = 16 octets, terminateur compris.
		if (out == nullptr || outSize < 16)
			return false;

		const NkSystemDateTime d = utc ? UtcNow() : LocalNow();
		if (!d.valid) {
			out[0] = '\0'; // vide plutôt qu'à moitié écrit
			return false;
		}

		usize k = 0;
		k += EcrirePadded(out + k, d.year, 4);
		k += EcrirePadded(out + k, d.month, 2);
		k += EcrirePadded(out + k, d.day, 2);
		out[k++] = '_';
		k += EcrirePadded(out + k, d.hour, 2);
		k += EcrirePadded(out + k, d.minute, 2);
		k += EcrirePadded(out + k, d.second, 2);
		out[k] = '\0';
		return true;
	}

} // namespace nkentseu
