// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/DailyFileSink.h
// DESCRIPTION: Sink pour fichiers avec rotation quotidienne.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Sinks/FileSink.h"
#include <chrono>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: DailyFileSink
// DESCRIPTION: Sink avec rotation quotidienne de fichiers
// -------------------------------------------------------------------------
class LOGGER_API DailyFileSink : public FileSink {
public:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur avec configuration
	 * @param filename Chemin du fichier (peut contenir des patterns de date)
	 * @param hour Heure de rotation (0-23)
	 * @param minute Minute de rotation (0-59)
	 * @param maxDays Nombre maximum de jours à conserver (0 = illimité)
	 */
	DailyFileSink(const std::string &filename, int hour = 0, int minute = 0, size_t maxDays = 0);

	/**
	 * @brief Destructeur
	 */
	~DailyFileSink() override;

	// ---------------------------------------------------------------------
	// IMPLÉMENTATION DE FileSink
	// ---------------------------------------------------------------------

	/**
	 * @brief Logge un message avec vérification de rotation quotidienne
	 */
	void Log(const LogMessage &message) override;

	// ---------------------------------------------------------------------
	// CONFIGURATION DE LA ROTATION QUOTIDIENNE
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit l'heure de rotation
	 * @param hour Heure (0-23)
	 * @param minute Minute (0-59)
	 */
	void SetRotationTime(int hour, int minute);

	/**
	 * @brief Obtient l'heure de rotation
	 * @return Heure de rotation
	 */
	int GetRotationHour() const;

	/**
	 * @brief Obtient la minute de rotation
	 * @return Minute de rotation
	 */
	int GetRotationMinute() const;

	/**
	 * @brief Définit le nombre maximum de jours à conserver
	 * @param maxDays Nombre de jours (0 = illimité)
	 */
	void SetMaxDays(size_t maxDays);

	/**
	 * @brief Obtient le nombre maximum de jours à conserver
	 * @return Nombre de jours
	 */
	size_t GetMaxDays() const;

	/**
	 * @brief Force la rotation du fichier
	 * @return true si rotation réussie, false sinon
	 */
	bool Rotate();

private:
	// ---------------------------------------------------------------------
	// MÉTHODES PRIVÉES
	// ---------------------------------------------------------------------

	/**
	 * @brief Vérifie et effectue la rotation si nécessaire
	 */
	void CheckRotation() override;

	/**
	 * @brief Effectue la rotation quotidienne
	 */
	void PerformRotation();

	/**
	 * @brief Nettoie les anciens fichiers
	 */
	void CleanOldFiles();

	/**
	 * @brief Génère le nom de fichier pour une date donnée
	 * @param date Date
	 * @return Nom de fichier
	 */
	std::string GetFilenameForDate(const std::tm &date) const;

	/**
	 * @brief Extrait la date d'un nom de fichier
	 * @param filename Nom de fichier
	 * @return Structure tm avec la date
	 */
	std::tm ExtractDateFromFilename(const std::string &filename) const;

	/**
	 * @brief Vérifie si une date est plus ancienne que maxDays
	 * @param date Date à vérifier
	 * @return true si trop ancienne, false sinon
	 */
	bool IsDateTooOld(const std::tm &date) const;

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Heure de rotation (0-23)
	int m_RotationHour;

	/// Minute de rotation (0-59)
	int m_RotationMinute;

	/// Nombre maximum de jours à conserver
	size_t m_MaxDays;

	/// Date du fichier courant
	std::tm m_CurrentDate;

	/// Dernière vérification de rotation
	std::chrono::system_clock::time_point m_LastCheck;
};

} // namespace logger
} // namespace nkentseu