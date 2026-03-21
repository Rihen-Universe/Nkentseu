// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkDailyFileSink.cpp
// DESCRIPTION: Implémentation du sink avec rotation quotidienne.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkDailyFileSink.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <cstring>
#if !defined(_WIN32)
#   include <time.h>
#   include <dirent.h>
#else
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	namespace {
		static bool NkIsDigit(const char ch) {
			return ch >= '0' && ch <= '9';
		}

		static bool NkHasPrefix(const NkString& value, const NkString& prefix) {
			if (value.Size() < prefix.Size()) {
				return false;
			}
			for (usize i = 0; i < prefix.Size(); ++i) {
				if (value[i] != prefix[i]) {
					return false;
				}
			}
			return true;
		}

		static bool NkIsPathSeparator(const char ch) {
			return ch == '/' || ch == '\\';
		}

		static NkString NkJoinPath(const NkString& directory, const NkString& filename) {
			if (directory.Empty() || directory == ".") {
				if (directory == ".") {
					NkString path = directory;
					path.PushBack('/');
					path.Append(filename);
					return path;
				}
				return filename;
			}

			NkString path = directory;
			if (!NkIsPathSeparator(path[path.Size() - 1])) {
				path.PushBack('/');
			}
			path.Append(filename);
			return path;
		}

		static bool NkParseYmd8(const char* digits, tm& outDate) {
			if (digits == nullptr) {
				return false;
			}
			for (int i = 0; i < 8; ++i) {
				if (!NkIsDigit(digits[i])) {
					return false;
				}
			}

			const int year = (digits[0] - '0') * 1000 + (digits[1] - '0') * 100 +
			                 (digits[2] - '0') * 10 + (digits[3] - '0');
			const int month = (digits[4] - '0') * 10 + (digits[5] - '0');
			const int day = (digits[6] - '0') * 10 + (digits[7] - '0');
			if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
				return false;
			}

			tm candidate {};
			candidate.tm_year = year - 1900;
			candidate.tm_mon = month - 1;
			candidate.tm_mday = day;
			candidate.tm_hour = 0;
			candidate.tm_min = 0;
			candidate.tm_sec = 0;
			candidate.tm_isdst = -1;
			if (::mktime(&candidate) == static_cast<time_t>(-1)) {
				return false;
			}

			outDate = candidate;
			return true;
		}

		static bool NkExtractRotatedDate(
			const NkString& entryName,
			const NkString& baseName,
			tm& outDate) {
			const usize requiredSize = baseName.Size() + 1 + 8;
			if (entryName.Size() != requiredSize) {
				return false;
			}
			if (!NkHasPrefix(entryName, baseName)) {
				return false;
			}

			const usize dotPos = baseName.Size();
			if (entryName[dotPos] != '.') {
				return false;
			}

			char digits[9];
			for (int i = 0; i < 8; ++i) {
				digits[i] = entryName[dotPos + 1 + static_cast<usize>(i)];
			}
			digits[8] = '\0';
			return NkParseYmd8(digits, outDate);
		}

		static uint64 NkNowNs() {
#if defined(_WIN32)
			FILETIME fileTime{};
			::GetSystemTimeAsFileTime(&fileTime);

			ULARGE_INTEGER ticks{};
			ticks.LowPart = fileTime.dwLowDateTime;
			ticks.HighPart = fileTime.dwHighDateTime;

			constexpr uint64 kWinToUnixEpochOffsetNs = 11644473600ULL * 1000000000ULL;
			const uint64 nowNs = static_cast<uint64>(ticks.QuadPart) * 100ULL; // 100ns ticks
			return (nowNs >= kWinToUnixEpochOffsetNs) ? (nowNs - kWinToUnixEpochOffsetNs) : 0ULL;
#else
			timespec ts{};
			if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
				return (static_cast<uint64>(ts.tv_sec) * 1000000000ULL) + static_cast<uint64>(ts.tv_nsec);
			return 0;
#endif
		}

		static bool NkFileExists(const NkString& path) {
			struct stat info {};
			return ::stat(path.CStr(), &info) == 0;
		}
	}
		
	/**
	 * @brief Constructeur avec configuration
	 */
	NkDailyFileSink::NkDailyFileSink(const NkString &filename, int hour, int minute, usize maxDays)
		: NkFileSink(filename, false), m_RotationHour(hour), m_RotationMinute(minute), m_MaxDays(maxDays), m_LastCheck(0) {

		// Initialiser la date courante
		const time_t now = ::time(nullptr);
	#ifdef _WIN32
		localtime_s(&m_CurrentDate, &now);
	#else
		localtime_r(&now, &m_CurrentDate);
	#endif
		m_LastCheck = NkNowNs();
	}

	/**
	 * @brief Destructeur
	 */
	NkDailyFileSink::~NkDailyFileSink() {
	}

	/**
	 * @brief Logge un message avec vérification de rotation quotidienne
	 */
	void NkDailyFileSink::Log(const NkLogMessage &message) {
		NkFileSink::Log(message);
	}

	/**
	 * @brief Définit l'heure de rotation
	 */
	void NkDailyFileSink::SetRotationTime(int hour, int minute) {
		loggersync::NkScopedLock lock(m_Mutex);
		m_RotationHour = hour;
		m_RotationMinute = minute;
	}

	/**
	 * @brief Obtient l'heure de rotation
	 */
	int NkDailyFileSink::GetRotationHour() const {
		loggersync::NkScopedLock lock(m_Mutex);
		return m_RotationHour;
	}

	/**
	 * @brief Obtient la minute de rotation
	 */
	int NkDailyFileSink::GetRotationMinute() const {
		loggersync::NkScopedLock lock(m_Mutex);
		return m_RotationMinute;
	}

	/**
	 * @brief Définit le nombre maximum de jours à conserver
	 */
	void NkDailyFileSink::SetMaxDays(usize maxDays) {
		loggersync::NkScopedLock lock(m_Mutex);
		m_MaxDays = maxDays;
	}

	/**
	 * @brief Obtient le nombre maximum de jours à conserver
	 */
	usize NkDailyFileSink::GetMaxDays() const {
		loggersync::NkScopedLock lock(m_Mutex);
		return m_MaxDays;
	}

	/**
	 * @brief Force la rotation du fichier
	 */
	bool NkDailyFileSink::Rotate() {
		loggersync::NkScopedLock lock(m_Mutex);
		PerformRotation();
		return true;
	}

	/**
	 * @brief Vérifie et effectue la rotation si nécessaire
	 */
	void NkDailyFileSink::CheckRotation() {
		const uint64 nowNs = NkNowNs();

		// Vérifier seulement toutes les minutes
		if (m_LastCheck > 0 && nowNs > m_LastCheck && (nowNs - m_LastCheck) < 60000000000ULL) {
			return;
		}

		m_LastCheck = nowNs;

		// Vérifier si changement de jour
		const time_t nowTime = static_cast<time_t>(nowNs / 1000000000ULL);
		tm nowDate{};
	#ifdef _WIN32
		localtime_s(&nowDate, &nowTime);
	#else
		localtime_r(&nowTime, &nowDate);
	#endif

		if (nowDate.tm_year != m_CurrentDate.tm_year || nowDate.tm_mon != m_CurrentDate.tm_mon ||
			nowDate.tm_mday != m_CurrentDate.tm_mday) {

			// Vérifier l'heure de rotation
			const bool isAtRotationTime =
				(nowDate.tm_hour > m_RotationHour) ||
				(nowDate.tm_hour == m_RotationHour && nowDate.tm_min >= m_RotationMinute);
			if (isAtRotationTime) {
				PerformRotation();
				m_CurrentDate = nowDate;
			}
		}
	}

	/**
	 * @brief Effectue la rotation quotidienne
	 */
	void NkDailyFileSink::PerformRotation() {
		CloseUnlocked();

		// Générer le nom de fichier avec la date
		const NkString rotatedFile = GetFilenameForDate(m_CurrentDate);

		// Renommer le fichier courant
		const NkString currentFile = GetFilenameUnlocked();
		if (NkFileExists(currentFile)) {
			(void)::rename(currentFile.CStr(), rotatedFile.CStr());
		}

		// Nettoyer les anciens fichiers
		if (m_MaxDays > 0) {
			CleanOldFiles();
		}

		(void)OpenUnlocked();
	}

	/**
	 * @brief Nettoie les anciens fichiers
	 */
	void NkDailyFileSink::CleanOldFiles() {
		if (m_MaxDays == 0) {
			return;
		}

		const NkString currentFile = GetFilenameUnlocked();
		const usize slashPos = currentFile.FindLastOf("/\\");
		const NkString directory = (slashPos == NkString::npos)
			? NkString(".")
			: currentFile.SubStr(0, slashPos);
		const NkString baseName = (slashPos == NkString::npos)
			? currentFile
			: currentFile.SubStr(slashPos + 1);
		if (baseName.Empty()) {
			return;
		}

#if defined(_WIN32)
		NkString pattern = directory;
		if (!pattern.Empty() && !NkIsPathSeparator(pattern[pattern.Size() - 1])) {
			pattern.PushBack('\\');
		}
		pattern.Append("*");

		WIN32_FIND_DATAA findData {};
		HANDLE handle = ::FindFirstFileA(pattern.CStr(), &findData);
		if (handle == INVALID_HANDLE_VALUE) {
			return;
		}

		do {
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				continue;
			}

			const NkString entryName(findData.cFileName);
			tm entryDate {};
			if (!NkExtractRotatedDate(entryName, baseName, entryDate)) {
				continue;
			}
			if (!IsDateTooOld(entryDate)) {
				continue;
			}

			const NkString fullPath = NkJoinPath(directory, entryName);
			(void)::remove(fullPath.CStr());
		} while (::FindNextFileA(handle, &findData) != 0);

		(void)::FindClose(handle);
#else
		DIR* dir = ::opendir(directory.CStr());
		if (dir == nullptr) {
			return;
		}

		for (dirent* entry = ::readdir(dir); entry != nullptr; entry = ::readdir(dir)) {
			if (entry->d_name[0] == '\0') {
				continue;
			}

			const NkString entryName(entry->d_name);
			tm entryDate {};
			if (!NkExtractRotatedDate(entryName, baseName, entryDate)) {
				continue;
			}
			if (!IsDateTooOld(entryDate)) {
				continue;
			}

			const NkString fullPath = NkJoinPath(directory, entryName);
			(void)::remove(fullPath.CStr());
		}

		(void)::closedir(dir);
#endif
	}

	/**
	 * @brief Génère le nom de fichier pour une date donnée
	 */
	NkString NkDailyFileSink::GetFilenameForDate(const tm &date) const {
		char suffix[32];
		const int written = ::snprintf(
			suffix,
			sizeof(suffix),
			".%04d%02d%02d",
			date.tm_year + 1900,
			date.tm_mon + 1,
			date.tm_mday);
		NkString file = GetFilenameUnlocked();
		if (written > 0) {
			file.Append(suffix, static_cast<usize>(written));
		}
		return file;
	}

	/**
	 * @brief Extrait la date d'un nom de fichier
	 */
	tm NkDailyFileSink::ExtractDateFromFilename(const NkString &filename) const {
		tm date {};
		date.tm_year = -1;
		date.tm_mon = 0;
		date.tm_mday = 0;
		date.tm_hour = 0;
		date.tm_min = 0;
		date.tm_sec = 0;
		date.tm_isdst = -1;

		if (filename.Size() < 9) {
			return date;
		}

		const usize dotPos = filename.Size() - 9;
		if (filename[dotPos] != '.') {
			return date;
		}

		char digits[9];
		for (int i = 0; i < 8; ++i) {
			digits[i] = filename[dotPos + 1 + static_cast<usize>(i)];
		}
		digits[8] = '\0';

		tm parsed {};
		if (NkParseYmd8(digits, parsed)) {
			return parsed;
		}
		return date;
	}

	/**
	 * @brief Vérifie si une date est plus ancienne que maxDays
	 */
	bool NkDailyFileSink::IsDateTooOld(const tm &date) const {
		if (m_MaxDays == 0 || date.tm_year < 0) {
			return false;
		}

		const time_t now = ::time(nullptr);
		tm nowDate {};
#ifdef _WIN32
		localtime_s(&nowDate, &now);
#else
		localtime_r(&now, &nowDate);
#endif
		nowDate.tm_hour = 0;
		nowDate.tm_min = 0;
		nowDate.tm_sec = 0;
		nowDate.tm_isdst = -1;

		time_t threshold = ::mktime(&nowDate);
		if (threshold == static_cast<time_t>(-1)) {
			return false;
		}
		threshold -= static_cast<time_t>(m_MaxDays) * 86400;

		tm candidate = date;
		candidate.tm_hour = 0;
		candidate.tm_min = 0;
		candidate.tm_sec = 0;
		candidate.tm_isdst = -1;
		const time_t candidateTime = ::mktime(&candidate);
		if (candidateTime == static_cast<time_t>(-1)) {
			return false;
		}

		return candidateTime < threshold;
	}

} // namespace nkentseu
