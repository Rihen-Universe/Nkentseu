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
#if !defined(_WIN32)
#   include <time.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	namespace {
		static uint64 NkNowNs() {
			timespec ts{};
#if defined(_WIN32)
			if (::timespec_get(&ts, TIME_UTC) == TIME_UTC)
#else
			if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
#endif
				return (static_cast<uint64>(ts.tv_sec) * 1000000000ULL) + static_cast<uint64>(ts.tv_nsec);
			return 0;
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
		logger_sync::NkScopedLock lock(m_Mutex);
		m_RotationHour = hour;
		m_RotationMinute = minute;
	}

	/**
	 * @brief Obtient l'heure de rotation
	 */
	int NkDailyFileSink::GetRotationHour() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_RotationHour;
	}

	/**
	 * @brief Obtient la minute de rotation
	 */
	int NkDailyFileSink::GetRotationMinute() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_RotationMinute;
	}

	/**
	 * @brief Définit le nombre maximum de jours à conserver
	 */
	void NkDailyFileSink::SetMaxDays(usize maxDays) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_MaxDays = maxDays;
	}

	/**
	 * @brief Obtient le nombre maximum de jours à conserver
	 */
	usize NkDailyFileSink::GetMaxDays() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_MaxDays;
	}

	/**
	 * @brief Force la rotation du fichier
	 */
	bool NkDailyFileSink::Rotate() {
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
		Close();

		// Générer le nom de fichier avec la date
		const NkString rotatedFile = GetFilenameForDate(m_CurrentDate);

		// Renommer le fichier courant
		const NkString currentFile = GetFilename();
		if (NkFileExists(currentFile)) {
			(void)::rename(currentFile.CStr(), rotatedFile.CStr());
		}

		// Nettoyer les anciens fichiers
		if (m_MaxDays > 0) {
			CleanOldFiles();
		}

		Open();
	}

	/**
	 * @brief Nettoie les anciens fichiers
	 */
	void NkDailyFileSink::CleanOldFiles() {
		// TODO: Implémenter le nettoyage des fichiers plus anciens que maxDays
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
		NkString file = GetFilename();
		if (written > 0) {
			file.Append(suffix, static_cast<usize>(written));
		}
		return file;
	}

	/**
	 * @brief Extrait la date d'un nom de fichier
	 */
	tm NkDailyFileSink::ExtractDateFromFilename(const NkString &filename) const {
		tm date{};
		// TODO: Parser le nom de fichier
		return date;
	}

	/**
	 * @brief Vérifie si une date est plus ancienne que maxDays
	 */
	bool NkDailyFileSink::IsDateTooOld(const tm &date) const {
		// TODO: Comparer avec la date courante
		return false;
	}

} // namespace nkentseu
