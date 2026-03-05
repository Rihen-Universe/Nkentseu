// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkDailyFileSink.cpp
// DESCRIPTION: Implémentation du sink avec rotation quotidienne.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkDailyFileSink.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
		
	/**
	 * @brief Constructeur avec configuration
	 */
	NkDailyFileSink::NkDailyFileSink(const std::string &filename, int hour, int minute, core::usize maxDays)
		: NkFileSink(filename, false), m_RotationHour(hour), m_RotationMinute(minute), m_MaxDays(maxDays),
		m_LastCheck(std::chrono::system_clock::now()) {

		// Initialiser la date courante
		auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	#ifdef _WIN32
		localtime_s(&m_CurrentDate, &now);
	#else
		localtime_r(&now, &m_CurrentDate);
	#endif
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
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_RotationHour = hour;
		m_RotationMinute = minute;
	}

	/**
	 * @brief Obtient l'heure de rotation
	 */
	int NkDailyFileSink::GetRotationHour() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_RotationHour;
	}

	/**
	 * @brief Obtient la minute de rotation
	 */
	int NkDailyFileSink::GetRotationMinute() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_RotationMinute;
	}

	/**
	 * @brief Définit le nombre maximum de jours à conserver
	 */
	void NkDailyFileSink::SetMaxDays(core::usize maxDays) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_MaxDays = maxDays;
	}

	/**
	 * @brief Obtient le nombre maximum de jours à conserver
	 */
	core::usize NkDailyFileSink::GetMaxDays() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
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
		auto now = std::chrono::system_clock::now();

		// Vérifier seulement toutes les minutes
		if (std::chrono::duration_cast<std::chrono::minutes>(now - m_LastCheck).count() < 1) {
			return;
		}

		m_LastCheck = now;

		// Vérifier si changement de jour
		auto nowTime = std::chrono::system_clock::to_time_t(now);
		std::tm nowDate;
	#ifdef _WIN32
		localtime_s(&nowDate, &nowTime);
	#else
		localtime_r(&nowTime, &nowDate);
	#endif

		if (nowDate.tm_year != m_CurrentDate.tm_year || nowDate.tm_mon != m_CurrentDate.tm_mon ||
			nowDate.tm_mday != m_CurrentDate.tm_mday) {

			// Vérifier l'heure de rotation
			if (nowDate.tm_hour >= m_RotationHour && nowDate.tm_min >= m_RotationMinute) {
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
		std::string rotatedFile = GetFilenameForDate(m_CurrentDate);

		// Renommer le fichier courant
		if (std::filesystem::exists(GetFilename())) {
			std::filesystem::rename(GetFilename(), rotatedFile);
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
	std::string NkDailyFileSink::GetFilenameForDate(const std::tm &date) const {
		std::ostringstream oss;
		oss << GetFilename() << "." << std::setfill('0') << std::setw(4) << (date.tm_year + 1900) << std::setw(2)
			<< (date.tm_mon + 1) << std::setw(2) << date.tm_mday;
		return oss.str();
	}

	/**
	 * @brief Extrait la date d'un nom de fichier
	 */
	std::tm NkDailyFileSink::ExtractDateFromFilename(const std::string &filename) const {
		std::tm date = {};
		// TODO: Parser le nom de fichier
		return date;
	}

	/**
	 * @brief Vérifie si une date est plus ancienne que maxDays
	 */
	bool NkDailyFileSink::IsDateTooOld(const std::tm &date) const {
		// TODO: Comparer avec la date courante
		return false;
	}

} // namespace nkentseu
