// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sink.h
// DESCRIPTION: Interface de base pour tous les sinks de logging.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Export.h"
#include "Logger/LogMessage.h"
#include "Logger/Formatter.h"
#include <memory>
#include <string>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: ISink
// DESCRIPTION: Interface de base pour tous les sinks de logging
// -------------------------------------------------------------------------
class LOGGER_API ISink {
public:
	/**
	 * @brief Destructeur virtuel
	 */
	virtual ~ISink() = default;

	// ---------------------------------------------------------------------
	// MÉTHODES VIRTUELLES PURES
	// ---------------------------------------------------------------------

	/**
	 * @brief Logge un message
	 * @param message Message à logger
	 */
	virtual void Log(const LogMessage &message) = 0;

	/**
	 * @brief Force l'écriture des données en attente
	 */
	virtual void Flush() = 0;

	/**
	 * @brief Définit le formatter pour ce sink
	 * @param formatter Formatter à utiliser
	 */
	virtual void SetFormatter(std::unique_ptr<Formatter> formatter) = 0;

	/**
	 * @brief Définit le pattern de formatage
	 * @param pattern Pattern à utiliser
	 */
	virtual void SetPattern(const std::string &pattern) = 0;

	/**
	 * @brief Obtient le formatter courant
	 * @return Pointeur vers le formatter
	 */
	virtual Formatter *GetFormatter() const = 0;

	/**
	 * @brief Obtient le pattern courant
	 * @return Pattern de formatage
	 */
	virtual std::string GetPattern() const = 0;

	// ---------------------------------------------------------------------
	// MÉTHODES VIRTUELLES (OPTIONNELLES)
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit le niveau minimum de log pour ce sink
	 * @param level Niveau minimum
	 */
	virtual void SetLevel(LogLevel level) {
		m_Level = level;
	}

	/**
	 * @brief Obtient le niveau minimum de log
	 * @return Niveau minimum
	 */
	virtual LogLevel GetLevel() const {
		return m_Level;
	}

	/**
	 * @brief Vérifie si un niveau devrait être loggé
	 * @param level Niveau à vérifier
	 * @return true si le niveau est >= niveau minimum, false sinon
	 */
	virtual bool ShouldLog(LogLevel level) const {
		return level >= m_Level;
	}

	/**
	 * @brief Active ou désactive le sink
	 * @param enabled État d'activation
	 */
	virtual void SetEnabled(bool enabled) {
		m_Enabled = enabled;
	}

	/**
	 * @brief Vérifie si le sink est activé
	 * @return true si activé, false sinon
	 */
	virtual bool IsEnabled() const {
		return m_Enabled;
	}

	/**
	 * @brief Obtient le nom du sink
	 * @return Nom du sink
	 */
	virtual std::string GetName() const {
		return m_Name;
	}

	/**
	 * @brief Définit le nom du sink
	 * @param name Nom du sink
	 */
	virtual void SetName(const std::string &name) {
		m_Name = name;
	}

protected:
	/// Niveau minimum de log
	LogLevel m_Level = LogLevel::Trace;

	/// Indicateur d'activation
	bool m_Enabled = true;

	/// Nom du sink
	std::string m_Name;
};

// -------------------------------------------------------------------------
// TYPE ALIAS POUR LES SINKS
// -------------------------------------------------------------------------

/// Type pour les pointeurs partagés de sinks
using SinkPtr = std::shared_ptr<ISink>;

/// Type pour les pointeurs uniques de sinks
using SinkUniquePtr = std::unique_ptr<ISink>;

} // namespace logger
} // namespace nkentseu