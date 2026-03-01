// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/LogMessage.h
// DESCRIPTION: Structure contenant toutes les informations d'un message de log.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <chrono>
#include "NKCore/NkTypes.h"
#include "Logger/LogLevel.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// STRUCTURE: LogMessage
// DESCRIPTION: Contient toutes les informations d'un message de log
// -------------------------------------------------------------------------
struct LogMessage {
	// ---------------------------------------------------------------------
	// DONNÉES TEMPORELLES
	// ---------------------------------------------------------------------

	/// Timestamp en nanosecondes depuis l'epoch
	uint64 timestamp;

	/// Heure de réception du message
	std::chrono::system_clock::time_point timePoint;

	// ---------------------------------------------------------------------
	// INFORMATIONS DE THREAD
	// ---------------------------------------------------------------------

	/// ID du thread émetteur
	uint32 threadId;

	/// Nom du thread (optionnel)
	std::string threadName;

	// ---------------------------------------------------------------------
	// INFORMATIONS DE LOG
	// ---------------------------------------------------------------------

	/// Niveau de log
	LogLevel level;

	/// Message de log
	std::string message;

	/// Nom du logger
	std::string loggerName;

	// ---------------------------------------------------------------------
	// INFORMATIONS DE SOURCE (optionnelles)
	// ---------------------------------------------------------------------

	/// Fichier source
	std::string sourceFile;

	/// Ligne source
	uint32 sourceLine;

	/// Nom de la fonction
	std::string functionName;

	// ---------------------------------------------------------------------
	// CONSTRUCTEURS
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur par défaut
	 */
	LogMessage();

	/**
	 * @brief Constructeur avec niveau et message
	 * @param lvl Niveau de log
	 * @param msg Message de log
	 * @param logger Nom du logger (optionnel)
	 */
	LogMessage(LogLevel lvl, const std::string &msg, const std::string &logger = "");

	/**
	 * @brief Constructeur avec informations complètes
	 * @param lvl Niveau de log
	 * @param msg Message de log
	 * @param file Fichier source (optionnel)
	 * @param line Ligne source (optionnel)
	 * @param func Fonction source (optionnel)
	 * @param logger Nom du logger (optionnel)
	 */
	LogMessage(LogLevel lvl, const std::string &msg, const std::string &file, uint32 line, const std::string &func,
			   const std::string &logger = "");

	/**
	 * @brief Destructeur
	 */
	~LogMessage() = default;

	// ---------------------------------------------------------------------
	// MÉTHODES UTILITAIRES
	// ---------------------------------------------------------------------

	/**
	 * @brief Réinitialise le message
	 */
	void Reset();

	/**
	 * @brief Vérifie si le message est valide
	 * @return true si valide, false sinon
	 */
	bool IsValid() const;

	/**
	 * @brief Obtient l'heure sous forme de structure tm
	 * @return Structure tm avec l'heure locale
	 */
	std::tm GetLocalTime() const;

	/**
	 * @brief Obtient l'heure sous forme de structure tm (UTC)
	 * @return Structure tm avec l'heure UTC
	 */
	std::tm GetUTCTime() const;

	/**
	 * @brief Obtient le timestamp en millisecondes
	 * @return Timestamp en millisecondes
	 */
	uint64 GetMillis() const;

	/**
	 * @brief Obtient le timestamp en microsecondes
	 * @return Timestamp en microsecondes
	 */
	uint64 GetMicros() const;

	/**
	 * @brief Obtient le timestamp en secondes
	 * @return Timestamp en secondes (double)
	 */
	double GetSeconds() const;
};

} // namespace logger
} // namespace nkentseu