// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogger.cpp
// DESCRIPTION: Implémentation de la classe NkLogger principale.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkLogger.h"
#include "NKLogger/NkLogMessage.h"
#include <cstdarg>
#include <chrono>
#include <iostream>
#include <thread>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// IMPLÉMENTATION DE NkLogger
	// -------------------------------------------------------------------------

	/**
	 * @brief Constructeur de logger avec nom
	 * @param name Nom du logger
	 */
	NkLogger::NkLogger(const std::string &name)
		: m_Name(name), m_Level(NkLogLevel::NK_INFO), m_Enabled(true), m_Formatter(std::make_unique<NkFormatter>()) {
	}

	/**
	 * @brief Destructeur du logger
	 */
	NkLogger::~NkLogger() {
		Flush();
	}

	/**
	 * @brief Ajoute un sink au logger
	 * @param sink Sink à ajouter (partagé)
	 */
	void NkLogger::AddSink(std::shared_ptr<NkISink> sink) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Sinks.push_back(sink);
	}

	/**
	 * @brief Supprime tous les sinks du logger
	 */
	void NkLogger::ClearSinks() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Sinks.clear();
	}

	/**
	 * @brief Obtient le nombre de sinks attachés
	 * @return Nombre de sinks
	 */
	core::usize NkLogger::GetSinkCount() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Sinks.size();
	}

	/**
	 * @brief Définit le formatter pour tous les sinks
	 * @param formatter NkFormatter à utiliser
	 */
	void NkLogger::SetFormatter(std::unique_ptr<NkFormatter> formatter) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Formatter = std::move(formatter);
	}

	/**
	 * @brief Définit le pattern de formatage
	 * @param pattern Pattern à utiliser
	 */
	void NkLogger::SetPattern(const std::string &pattern) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Formatter) {
			m_Formatter->SetPattern(pattern);
		}
	}

	/**
	 * @brief Définit le niveau de log minimum
	 * @param level Niveau minimum à logger
	 */
	void NkLogger::SetLevel(NkLogLevel level) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Level = level;
	}

	/**
	 * @brief Obtient le niveau de log courant
	 * @return Niveau de log
	 */
	NkLogLevel NkLogger::GetLevel() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Level;
	}

	/**
	 * @brief Vérifie si un niveau devrait être loggé
	 * @param level Niveau à vérifier
	 * @return true si le niveau est >= niveau minimum, false sinon
	 */
	bool NkLogger::ShouldLog(NkLogLevel level) const {
		if (!m_Enabled)
			return false;
		return level >= m_Level;
	}

	/**
	 * @brief Log interne avec informations de source
	 */
	void NkLogger::LogInternal(NkLogLevel level, const std::string &message, const char *sourceFile, uint32 sourceLine,
							const char *functionName) {
		if (!m_Enabled || level < m_Level)
			return;

		// Création du message de log avec nom du logger
		NkLogMessage msg;
		msg.timestamp =
			std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
				.count();
		msg.threadId = static_cast<uint32>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
		msg.level = level;
		msg.message = message;
		msg.loggerName = m_Name; // Nom du logger ajouté ici

		if (sourceFile)
			msg.sourceFile = sourceFile;
		if (sourceLine > 0)
			msg.sourceLine = sourceLine;
		if (functionName)
			msg.functionName = functionName;

	// Obtention du nom du thread si disponible
	// pthread_getname_np disponible sur Linux et macOS, mais pas Android < API 26
	#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
		char threadName[256] = {0};
		if (pthread_getname_np(pthread_self(), threadName, sizeof(threadName)) == 0) {
			msg.threadName = threadName;
		}
	#endif

		// Formatage du message
		std::string formatted;
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Formatter) {
				formatted = m_Formatter->Format(msg);
			} else {
				formatted = message; // Fallback
			}
		}

		// Envoi à tous les sinks
		std::lock_guard<std::mutex> lock(m_Mutex);
		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->GetFormatter()->SetPattern(m_Formatter->GetPattern());
				sink->Log(msg);
			}
		}
	}

	/**
	 * @brief Formatage variadique
	 */
	std::string NkLogger::FormatString(const char *format, va_list args) {
		// Détermination de la taille nécessaire
		va_list argsCopy;
		va_copy(argsCopy, args);
		int size = vsnprintf(nullptr, 0, format, argsCopy);
		va_end(argsCopy);

		if (size < 0)
			return "";

		// Allocation et formatage
		std::string result(size + 1, '\0');
		vsnprintf(&result[0], result.size(), format, args);
		result.resize(size); // Retirer le null terminator

		return result;
	}

	/**
	 * @brief Log avec format string (style printf)
	 */
	void NkLogger::Logf(NkLogLevel level, const char *format, ...) {
		if (!ShouldLog(level))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(level, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log avec format string et informations de source
	 */
	void NkLogger::Logf(NkLogLevel level, const char *file, int line, const char *func, const char *format, ...) {
		if (!ShouldLog(level))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(level, message, file, line, func);
	}

	/**
	 * @brief Log avec message string et informations de source
	 */
	void NkLogger::Log(NkLogLevel level, const char *file, int line, const char *func, const std::string &message) {
		if (!ShouldLog(level))
			return;
		LogInternal(level, message, file, line, func);
	}

	/**
	 * @brief Log avec format string, informations de source et va_list
	 */
	void NkLogger::Logf(NkLogLevel level, const char *file, int line, const char *func, const char *format, va_list args) {
		if (!ShouldLog(level))
			return;

		std::string message = FormatString(format, args);
		LogInternal(level, message, file, line, func);
	}

	/**
	 * @brief Log trace avec format string
	 */
	void NkLogger::Tracef(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_TRACE))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log debug avec format string
	 */
	void NkLogger::Debugf(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_DEBUG))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log info avec format string
	 */
	void NkLogger::Infof(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_INFO))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log warning avec format string
	 */
	void NkLogger::Warnf(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_WARN))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log error avec format string
	 */
	void NkLogger::Errorf(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_ERROR))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log critical avec format string
	 */
	void NkLogger::Criticalf(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_CRITICAL))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log fatal avec format string
	 */
	void NkLogger::Fatalf(const char *format, ...) {
		if (!ShouldLog(NkLogLevel::NK_FATAL))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log avec stream style
	 */
	void NkLogger::Log(NkLogLevel level, const std::string &message) {
		if (!ShouldLog(level))
			return;
		LogInternal(level, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log trace avec stream style
	 */
	void NkLogger::Trace(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_TRACE))
			return;
		LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log debug avec stream style
	 */
	void NkLogger::Debug(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_DEBUG))
			return;
		LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log info avec stream style
	 */
	void NkLogger::Info(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_INFO))
			return;
		LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log warning avec stream style
	 */
	void NkLogger::Warn(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_WARN))
			return;
		LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log error avec stream style
	 */
	void NkLogger::Error(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_ERROR))
			return;
		LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log critical avec stream style
	 */
	void NkLogger::Critical(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_CRITICAL))
			return;
		LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Log fatal avec stream style
	 */
	void NkLogger::Fatal(const std::string &message) {
		if (!ShouldLog(NkLogLevel::NK_FATAL))
			return;
		LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.c_str(), m_SourceLine, m_FunctionName.c_str());
	}

	/**
	 * @brief Force le flush de tous les sinks
	 */
	void NkLogger::Flush() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->Flush();
			}
		}
	}

	/**
	 * @brief Obtient le nom du logger
	 * @return Nom du logger
	 */
	const std::string &NkLogger::GetName() const {
		return m_Name;
	}

	/**
	 * @brief Vérifie si le logger est actif
	 * @return true si actif, false sinon
	 */
	bool NkLogger::IsEnabled() const {
		return m_Enabled;
	}

	/**
	 * @brief Active ou désactive le logger
	 * @param enabled État d'activation
	 */
	void NkLogger::SetEnabled(bool enabled) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Enabled = enabled;
	}
} // namespace nkentseu
