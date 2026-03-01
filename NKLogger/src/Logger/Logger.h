// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Logger.h
// DESCRIPTION: Classe principale du système de logging, similaire à spdlog.
//              Gère les sinks, formatters et fournit une API de logging.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Export.h"
#include "Logger/LogLevel.h"
#include "Logger/Sink.h"
#include "Logger/Formatter.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: Logger
// DESCRIPTION: Classe principale de logging avec support multi-sink
// -------------------------------------------------------------------------
class LOGGER_API Logger {
public:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS ET DESTRUCTEUR
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur de logger avec nom
	 * @param name Nom du logger
	 */
	explicit Logger(const std::string &name);

	/**
	 * @brief Destructeur du logger
	 */
	~Logger();

	// ---------------------------------------------------------------------
	// GESTION DES SINKS
	// ---------------------------------------------------------------------

	/**
	 * @brief Ajoute un sink au logger
	 * @param sink Sink à ajouter (partagé)
	 */
	void AddSink(std::shared_ptr<ISink> sink);

	/**
	 * @brief Supprime tous les sinks du logger
	 */
	void ClearSinks();

	/**
	 * @brief Obtient le nombre de sinks attachés
	 * @return Nombre de sinks
	 */
	size_t GetSinkCount() const;

	// ---------------------------------------------------------------------
	// CONFIGURATION DU FORMATTER
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit le formatter pour tous les sinks
	 * @param formatter Formatter à utiliser
	 */
	void SetFormatter(std::unique_ptr<Formatter> formatter);

	/**
	 * @brief Définit le pattern de formatage
	 * @param pattern Pattern à utiliser
	 */
	void SetPattern(const std::string &pattern);

	// ---------------------------------------------------------------------
	// CONFIGURATION DU NIVEAU DE LOG
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit le niveau de log minimum
	 * @param level Niveau minimum à logger
	 */
	void SetLevel(LogLevel level);

	/**
	 * @brief Obtient le niveau de log courant
	 * @return Niveau de log
	 */
	LogLevel GetLevel() const;

	/**
	 * @brief Vérifie si un niveau devrait être loggé
	 * @param level Niveau à vérifier
	 * @return true si le niveau est >= niveau minimum, false sinon
	 */
	bool ShouldLog(LogLevel level) const;

	// ---------------------------------------------------------------------
	// MÉTHODES DE LOGGING (FORMAT STRING)
	// ---------------------------------------------------------------------

	/**
	 * @brief Log avec format string (style printf)
	 * @param level Niveau de log
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	virtual void Log(LogLevel level, const char *format, ...);

	/**
	 * @brief Log avec format string et informations de source
	 * @param level Niveau de log
	 * @param file Fichier source
	 * @param line Ligne source
	 * @param func Fonction source
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	virtual void Log(LogLevel level, const char *file, int line, const char *func, const char *format, ...);

	/**
	 * @brief Log avec message string et informations de source
	 * @param level Niveau de log
	 * @param file Fichier source
	 * @param line Ligne source
	 * @param func Fonction source
	 * @param message Message à logger
	 */
	void Log(LogLevel level, const char *file, int line, const char *func, const std::string &message);

	/**
	 * @brief Log avec format string, informations de source et va_list
	 * @param level Niveau de log
	 * @param file Fichier source
	 * @param line Ligne source
	 * @param func Fonction source
	 * @param format Format string
	 * @param args Arguments variables (va_list)
	 */
	virtual void Log(LogLevel level, const char *file, int line, const char *func, const char *format, va_list args);

	/**
	 * @brief Log trace avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Trace(const char *format, ...);

	/**
	 * @brief Log debug avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Debug(const char *format, ...);

	/**
	 * @brief Log info avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Info(const char *format, ...);

	/**
	 * @brief Log warning avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Warn(const char *format, ...);

	/**
	 * @brief Log error avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Error(const char *format, ...);

	/**
	 * @brief Log critical avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Critical(const char *format, ...);

	/**
	 * @brief Log fatal avec format string
	 * @param format Format string
	 * @param ... Arguments variables
	 */
	void Fatal(const char *format, ...);

	// ---------------------------------------------------------------------
	// MÉTHODES DE LOGGING (STREAM STYLE)
	// ---------------------------------------------------------------------

	/**
	 * @brief Log avec stream style
	 * @param level Niveau de log
	 * @param message Message à logger
	 */
	void Log(LogLevel level, const std::string &message);

	/**
	 * @brief Log trace avec stream style
	 * @param message Message à logger
	 */
	void Trace(const std::string &message);

	/**
	 * @brief Log debug avec stream style
	 * @param message Message à logger
	 */
	void Debug(const std::string &message);

	/**
	 * @brief Log info avec stream style
	 * @param message Message à logger
	 */
	void Info(const std::string &message);

	/**
	 * @brief Log warning avec stream style
	 * @param message Message à logger
	 */
	void Warn(const std::string &message);

	/**
	 * @brief Log error avec stream style
	 * @param message Message à logger
	 */
	void Error(const std::string &message);

	/**
	 * @brief Log critical avec stream style
	 * @param message Message à logger
	 */
	void Critical(const std::string &message);

	/**
	 * @brief Log fatal avec stream style
	 * @param message Message à logger
	 */
	void Fatal(const std::string &message);

	// ---------------------------------------------------------------------
	// UTILITAIRES
	// ---------------------------------------------------------------------

	/**
	 * @brief Force le flush de tous les sinks
	 */
	virtual void Flush();

	/**
	 * @brief Obtient le nom du logger
	 * @return Nom du logger
	 */
	const std::string &GetName() const;

	/**
	 * @brief Vérifie si le logger est actif
	 * @return true si actif, false sinon
	 */
	bool IsEnabled() const;

	/**
	 * @brief Active ou désactive le logger
	 * @param enabled État d'activation
	 */
	void SetEnabled(bool enabled);

	virtual Logger &Source(const char *sourceFile = nullptr, uint32 sourceLine = 0,
						   const char *functionName = nullptr) {
		m_SourceFile = sourceFile ? sourceFile : "";
		m_SourceLine = sourceLine;
		m_FunctionName = functionName ? functionName : "";
		return *this;
	}

private:
	// ---------------------------------------------------------------------
	// MÉTHODES PRIVÉES
	// ---------------------------------------------------------------------

	/**
	 * @brief Log interne avec informations de source
	 * @param level Niveau de log
	 * @param message Message à logger
	 * @param sourceFile Fichier source (optionnel)
	 * @param sourceLine Ligne source (optionnel)
	 * @param functionName Fonction source (optionnel)
	 */
	void LogInternal(LogLevel level, const std::string &message, const char *sourceFile = nullptr,
					 uint32 sourceLine = 0, const char *functionName = nullptr);

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Nom du logger
	std::string m_Name;

	/// Niveau de log minimum
	LogLevel m_Level;

	/// Indicateur d'activation
	bool m_Enabled;

protected:
	void SetName(const std::string &name) {
		m_Name = name;
	}
	/// Mutex pour la synchronisation thread-safe
	mutable std::mutex m_Mutex;

	/// Liste des sinks attachés
	std::vector<std::shared_ptr<ISink>> m_Sinks;

	/// Formatter pour le formatting
	std::unique_ptr<Formatter> m_Formatter;

	/**
	 * @brief Formatage variadique
	 * @param format Format string
	 * @param args Arguments variables
	 * @return Chaîne formatée
	 */
	std::string FormatString(const char *format, va_list args);

	std::string m_SourceFile;
	uint32 m_SourceLine;
	std::string m_FunctionName;
};

// -------------------------------------------------------------------------
// MACROS DE LOGGING PRATIQUES
// -------------------------------------------------------------------------

#define LOG_TRACE(logger, ...)                                                                                         \
	if ((logger)->ShouldLog(LogLevel::Trace))                                                                          \
	(logger)->Log(LogLevel::Trace, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_DEBUG(logger, ...)                                                                                         \
	if ((logger)->ShouldLog(LogLevel::Debug))                                                                          \
	(logger)->Log(LogLevel::Debug, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_INFO(logger, ...)                                                                                          \
	if ((logger)->ShouldLog(LogLevel::Info))                                                                           \
	(logger)->Log(LogLevel::Info, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_WARN(logger, ...)                                                                                          \
	if ((logger)->ShouldLog(LogLevel::Warn))                                                                           \
	(logger)->Log(LogLevel::Warn, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_ERROR(logger, ...)                                                                                         \
	if ((logger)->ShouldLog(LogLevel::Error))                                                                          \
	(logger)->Log(LogLevel::Error, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_CRITICAL(logger, ...)                                                                                      \
	if ((logger)->ShouldLog(LogLevel::Critical))                                                                       \
	(logger)->Log(LogLevel::Critical, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_FATAL(logger, ...)                                                                                         \
	if ((logger)->ShouldLog(LogLevel::Fatal))                                                                          \
	(logger)->Log(LogLevel::Fatal, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_FLUSH(logger) (logger)->Flush()

// -------------------------------------------------------------------------
// MACROS AVEC INFORMATIONS DE SOURCE
// -------------------------------------------------------------------------

#define LOG_TRACE_SRC(logger, ...)                                                                                     \
	(logger)->LogInternal(LogLevel::Trace, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

#define LOG_DEBUG_SRC(logger, ...)                                                                                     \
	(logger)->LogInternal(LogLevel::Debug, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

#define LOG_INFO_SRC(logger, ...)                                                                                      \
	(logger)->LogInternal(LogLevel::Info, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

#define LOG_WARN_SRC(logger, ...)                                                                                      \
	(logger)->LogInternal(LogLevel::Warn, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

#define LOG_ERROR_SRC(logger, ...)                                                                                     \
	(logger)->LogInternal(LogLevel::Error, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

#define LOG_CRITICAL_SRC(logger, ...)                                                                                  \
	(logger)->LogInternal(LogLevel::Critical, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

#define LOG_FATAL_SRC(logger, ...)                                                                                     \
	(logger)->LogInternal(LogLevel::Fatal, (logger)->FormatString(__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__)

} // namespace logger
} // namespace nkentseu