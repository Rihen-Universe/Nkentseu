// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogMessage.cpp
// DESCRIPTION: Implémentation de la structure NkLogMessage.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkLogMessage.h"
#include <chrono>
#include <ctime>
#include <thread>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// IMPLÉMENTATION DE NkLogMessage
	// -------------------------------------------------------------------------

	/**
	 * @brief Constructeur par défaut
	 */
	NkLogMessage::NkLogMessage()
		: timestamp(0), timePoint(std::chrono::system_clock::now()), threadId(0), sourceLine(0), level(NkLogLevel::NK_INFO) {
		// Conversion du time_point en timestamp nanosecondes
		timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(timePoint.time_since_epoch()).count();

		// Obtention de l'ID du thread
		threadId = static_cast<uint32>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}

	/**
	 * @brief Constructeur avec niveau et message
	 */
	NkLogMessage::NkLogMessage(NkLogLevel lvl, const std::string &msg, const std::string &logger) : NkLogMessage() {
		level = lvl;
		message = msg;
		if (!logger.empty()) {
			loggerName = logger;
		}
	}

	/**
	 * @brief Constructeur avec informations complètes
	 */
	NkLogMessage::NkLogMessage(NkLogLevel lvl, const std::string &msg, const std::string &file, uint32 line,
						const std::string &func, const std::string &logger)
		: NkLogMessage(lvl, msg, logger) {
		if (!file.empty())
			sourceFile = file;
		if (line > 0)
			sourceLine = line;
		if (!func.empty())
			functionName = func;
	}

	/**
	 * @brief Réinitialise le message
	 */
	void NkLogMessage::Reset() {
		timestamp = 0;
		timePoint = std::chrono::system_clock::now();
		threadId = 0;
		threadName.clear();
		level = NkLogLevel::NK_INFO;
		message.clear();
		loggerName.clear();
		sourceFile.clear();
		sourceLine = 0;
		functionName.clear();

		// Recalcul du timestamp
		timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(timePoint.time_since_epoch()).count();

		// Recalcul de l'ID du thread
		threadId = static_cast<uint32>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}

	/**
	 * @brief Vérifie si le message est valide
	 */
	bool NkLogMessage::IsValid() const {
		return !message.empty() && timestamp > 0;
	}

	/**
	 * @brief Obtient l'heure sous forme de structure tm
	 */
	std::tm NkLogMessage::GetLocalTime() const {
		auto time = std::chrono::system_clock::to_time_t(timePoint);
		std::tm localTime;

	#ifdef _WIN32
		localtime_s(&localTime, &time);
	#else
		localtime_r(&time, &localTime);
	#endif

		return localTime;
	}

	/**
	 * @brief Obtient l'heure sous forme de structure tm (UTC)
	 */
	std::tm NkLogMessage::GetUTCTime() const {
		auto time = std::chrono::system_clock::to_time_t(timePoint);
		std::tm utcTime;

	#ifdef _WIN32
		gmtime_s(&utcTime, &time);
	#else
		gmtime_r(&time, &utcTime);
	#endif

		return utcTime;
	}

	/**
	 * @brief Obtient le timestamp en millisecondes
	 */
	uint64 NkLogMessage::GetMillis() const {
		return timestamp / 1000000ULL;
	}

	/**
	 * @brief Obtient le timestamp en microsecondes
	 */
	uint64 NkLogMessage::GetMicros() const {
		return timestamp / 1000ULL;
	}

	/**
	 * @brief Obtient le timestamp en secondes
	 */
	double NkLogMessage::GetSeconds() const {
		return static_cast<double>(timestamp) / 1000000000.0;
	}

} // namespace nkentseu