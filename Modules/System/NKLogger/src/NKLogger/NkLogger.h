// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogger.h
// DESCRIPTION: Classe principale du système de logging, similaire à spdlog.
//              Gère les sinks, formatters et fournit une API de logging.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLoggerExport.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkSink.h"
#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkTextFormat.h"
#include "NKLogger/NkSync.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKContainers/String/NkStringView.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKMemory/NkUniquePtr.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// CLASSE: NkLogger
	// DESCRIPTION: Classe principale de logging avec support multi-sink
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkLogger {
	public:
		// ---------------------------------------------------------------------
		// CONSTRUCTEURS ET DESTRUCTEUR
		// ---------------------------------------------------------------------

		/**
		 * @brief Constructeur de logger avec nom
		 * @param name Nom du logger
		 */
		explicit NkLogger(const NkString &name);

		/**
		 * @brief Destructeur du logger
		 */
		~NkLogger();

		// ---------------------------------------------------------------------
		// GESTION DES SINKS
		// ---------------------------------------------------------------------

		/**
		 * @brief Ajoute un sink au logger
		 * @param sink Sink à ajouter (partagé)
		 */
		void AddSink(memory::NkSharedPtr<NkISink> sink);

		/**
		 * @brief Supprime tous les sinks du logger
		 */
		void ClearSinks();

		/**
		 * @brief Obtient le nombre de sinks attachés
		 * @return Nombre de sinks
		 */
		usize GetSinkCount() const;

		// ---------------------------------------------------------------------
		// CONFIGURATION DU FORMATTER
		// ---------------------------------------------------------------------

		/**
		 * @brief Définit le formatter pour tous les sinks
		 * @param formatter NkFormatter à utiliser
		 */
		void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter);

		/**
		 * @brief Définit le pattern de formatage
		 * @param pattern Pattern à utiliser
		 */
		void SetPattern(const NkString &pattern);

		// ---------------------------------------------------------------------
		// CONFIGURATION DU NIVEAU DE LOG
		// ---------------------------------------------------------------------

		/**
		 * @brief Définit le niveau de log minimum
		 * @param level Niveau minimum à logger
		 */
		void SetLevel(NkLogLevel level);

		/**
		 * @brief Obtient le niveau de log courant
		 * @return Niveau de log
		 */
		NkLogLevel GetLevel() const;

		/**
		 * @brief Vérifie si un niveau devrait être loggé
		 * @param level Niveau à vérifier
		 * @return true si le niveau est >= niveau minimum, false sinon
		 */
		bool ShouldLog(NkLogLevel level) const;

		// ---------------------------------------------------------------------
		// MÉTHODES DE LOGGING (C-STYLE / printf)
		// ---------------------------------------------------------------------

		/**
		 * @brief Log avec format string (style printf)
		 * @param level Niveau de log
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		virtual void Logf(NkLogLevel level, const char *format, ...);

		/**
		 * @brief Log avec format string et informations de source
		 * @param level Niveau de log
		 * @param file Fichier source
		 * @param line Ligne source
		 * @param func Fonction source
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		virtual void Logf(NkLogLevel level, const char *file, int line, const char *func, const char *format, ...);

		/**
		 * @brief Log avec message string et informations de source
		 * @param level Niveau de log
		 * @param file Fichier source
		 * @param line Ligne source
		 * @param func Fonction source
		 * @param message Message à logger
		 */
		void Log(NkLogLevel level, const char *file, int line, const char *func, const NkString &message);

		/**
		 * @brief Log avec format string, informations de source et va_list
		 * @param level Niveau de log
		 * @param file Fichier source
		 * @param line Ligne source
		 * @param func Fonction source
		 * @param format Format string
		 * @param args Arguments variables (va_list)
		 */
		virtual void Logf(NkLogLevel level, const char *file, int line, const char *func, const char *format, va_list args);

		/**
		 * @brief Log trace avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Tracef(const char *format, ...);

		/**
		 * @brief Log debug avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Debugf(const char *format, ...);

		/**
		 * @brief Log info avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Infof(const char *format, ...);

		/**
		 * @brief Log warning avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Warnf(const char *format, ...);

		/**
		 * @brief Log error avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Errorf(const char *format, ...);

		/**
		 * @brief Log critical avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Criticalf(const char *format, ...);

		/**
		 * @brief Log fatal avec format string
		 * @param format Format string
		 * @param ... Arguments variables
		 */
		void Fatalf(const char *format, ...);

		// ---------------------------------------------------------------------
		// MÉTHODES DE LOGGING (STREAM STYLE)
		// ---------------------------------------------------------------------

		/**
		 * @brief Log avec stream style
		 * @param level Niveau de log
		 * @param message Message à logger
		 */
		void Log(NkLogLevel level, const NkString &message);

		/**
		 * @brief Log trace avec stream style
		 * @param message Message à logger
		 */
		void Trace(const NkString &message);

		/**
		 * @brief Log debug avec stream style
		 * @param message Message à logger
		 */
		void Debug(const NkString &message);

		/**
		 * @brief Log info avec stream style
		 * @param message Message à logger
		 */
		void Info(const NkString &message);

		/**
		 * @brief Log warning avec stream style
		 * @param message Message à logger
		 */
		void Warn(const NkString &message);

		/**
		 * @brief Log error avec stream style
		 * @param message Message à logger
		 */
		void Error(const NkString &message);

		/**
		 * @brief Log critical avec stream style
		 * @param message Message à logger
		 */
		void Critical(const NkString &message);

		/**
		 * @brief Log fatal avec stream style
		 * @param message Message à logger
		 */
		void Fatal(const NkString &message);

		// ---------------------------------------------------------------------
		// MÉTHODES DE LOGGING (INDEXED FORMAT STYLE: {i:p}) - API PRINCIPALE
		// ---------------------------------------------------------------------

		/**
		 * @brief Log typé avec format indexé ({i:p})
		 * @param level Niveau de log
		 * @param format Chaîne de format
		 * @param args Arguments typés
		 */
		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Log(NkLogLevel level, NkStringView format, Args&&... args) {
			if (!ShouldLog(level))
				return;
			NkString message = NkFormatIndexed(format, traits::NkForward<Args>(args)...);
			LogInternal(level, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Trace(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_TRACE, format, traits::NkForward<Args>(args)...);
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Debug(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_DEBUG, format, traits::NkForward<Args>(args)...);
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Info(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_INFO, format, traits::NkForward<Args>(args)...);
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Warn(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_WARN, format, traits::NkForward<Args>(args)...);
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Error(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_ERROR, format, traits::NkForward<Args>(args)...);
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Critical(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_CRITICAL, format, traits::NkForward<Args>(args)...);
		}

		template <typename... Args, typename traits::NkEnableIf_t<(sizeof...(Args) > 0), int> = 0>
		void Fatal(NkStringView format, Args&&... args) {
			Log(NkLogLevel::NK_FATAL, format, traits::NkForward<Args>(args)...);
		}

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
		const NkString &GetName() const;

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

		virtual NkLogger &Source(const char *sourceFile = nullptr, uint32 sourceLine = 0,
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
		void LogInternal(NkLogLevel level, const NkString &message, const char *sourceFile = nullptr,
						uint32 sourceLine = 0, const char *functionName = nullptr);

		// ---------------------------------------------------------------------
		// VARIABLES MEMBRE PRIVÉES
		// ---------------------------------------------------------------------

		/// Nom du logger
		NkString m_Name;

		/// Niveau de log minimum
		NkLogLevel m_Level;

		/// Indicateur d'activation
		bool m_Enabled;

	protected:
		void SetName(const NkString &name) {
			m_Name = name;
		}
		/// Mutex pour la synchronisation thread-safe
		mutable threading::NkMutex m_Mutex;

		/// Liste des sinks attachés
		NkVector<memory::NkSharedPtr<NkISink>> m_Sinks;

		/// NkFormatter pour le formatting
		memory::NkUniquePtr<NkFormatter> m_Formatter;

		/**
		 * @brief Formatage variadique
		 * @param format Format string
		 * @param args Arguments variables
		 * @return Chaîne formatée
		 */
		NkString FormatString(const char *format, va_list args);

		NkString m_SourceFile;
		uint32 m_SourceLine;
		NkString m_FunctionName;
	};

	// -------------------------------------------------------------------------
	// MACROS DE LOGGING PRATIQUES
	// -------------------------------------------------------------------------

	#define NK_LOG_TRACE_F(logger, ...)                                                                                       \
		if ((logger)->ShouldLog(NkLogLevel::NK_TRACE))                                                                          \
		(logger)->Logf(NkLogLevel::NK_TRACE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_DEBUG_F(logger, ...)                                                                                       \
		if ((logger)->ShouldLog(NkLogLevel::NK_DEBUG))                                                                          \
		(logger)->Logf(NkLogLevel::NK_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_INFO_F(logger, ...)                                                                                        \
		if ((logger)->ShouldLog(NkLogLevel::NK_INFO))                                                                           \
		(logger)->Logf(NkLogLevel::NK_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_WARN_F(logger, ...)                                                                                        \
		if ((logger)->ShouldLog(NkLogLevel::NK_WARN))                                                                           \
		(logger)->Logf(NkLogLevel::NK_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_ERROR_F(logger, ...)                                                                                       \
		if ((logger)->ShouldLog(NkLogLevel::NK_ERROR))                                                                          \
		(logger)->Logf(NkLogLevel::NK_ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_CRITICAL_F(logger, ...)                                                                                    \
		if ((logger)->ShouldLog(NkLogLevel::NK_CRITICAL))                                                                       \
		(logger)->Logf(NkLogLevel::NK_CRITICAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_FATAL_F(logger, ...)                                                                                       \
		if ((logger)->ShouldLog(NkLogLevel::NK_FATAL))                                                                          \
		(logger)->Logf(NkLogLevel::NK_FATAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	// Alias de compatibilité
	#define NK_LOG_TRACE(logger, ...) NK_LOG_TRACE_F(logger, __VA_ARGS__)
	#define NK_LOG_DEBUG(logger, ...) NK_LOG_DEBUG_F(logger, __VA_ARGS__)
	#define NK_LOG_INFO(logger, ...) NK_LOG_INFO_F(logger, __VA_ARGS__)
	#define NK_LOG_WARN(logger, ...) NK_LOG_WARN_F(logger, __VA_ARGS__)
	#define NK_LOG_ERROR(logger, ...) NK_LOG_ERROR_F(logger, __VA_ARGS__)
	#define NK_LOG_CRITICAL(logger, ...) NK_LOG_CRITICAL_F(logger, __VA_ARGS__)
	#define NK_LOG_FATAL(logger, ...) NK_LOG_FATAL_F(logger, __VA_ARGS__)

	#define NK_LOG_FLUSH(logger) (logger)->Flush()

	// -------------------------------------------------------------------------
	// MACROS AVEC INFORMATIONS DE SOURCE
	// -------------------------------------------------------------------------

	#define NK_LOG_TRACE_SRC(logger, ...)                                                                                    \
		(logger)->Logf(NkLogLevel::NK_TRACE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_DEBUG_SRC(logger, ...)                                                                                    \
		(logger)->Logf(NkLogLevel::NK_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_INFO_SRC(logger, ...)                                                                                     \
		(logger)->Logf(NkLogLevel::NK_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_WARN_SRC(logger, ...)                                                                                     \
		(logger)->Logf(NkLogLevel::NK_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_ERROR_SRC(logger, ...)                                                                                    \
		(logger)->Logf(NkLogLevel::NK_ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_CRITICAL_SRC(logger, ...)                                                                                 \
		(logger)->Logf(NkLogLevel::NK_CRITICAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

	#define NK_LOG_FATAL_SRC(logger, ...)                                                                                    \
		(logger)->Logf(NkLogLevel::NK_FATAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

} // namespace nkentseu
