// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/LogLevel.cpp
// DESCRIPTION: Implémentation des fonctions utilitaires pour LogLevel.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/LogLevel.h"
#include <cstring>
#include <cctype> // Pour std::tolower

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// FONCTIONS UTILITAIRES POUR LogLevel
// -------------------------------------------------------------------------

/**
 * @brief Convertit un LogLevel en chaîne de caractères
 */
const char *LogLevelToString(LogLevel level) {
	switch (level) {
		case LogLevel::Trace:
			return "trace";
		case LogLevel::Debug:
			return "debug";
		case LogLevel::Info:
			return "info";
		case LogLevel::Warn:
			return "warning";
		case LogLevel::Error:
			return "error";
		case LogLevel::Critical:
			return "critical";
		case LogLevel::Fatal:
			return "fatal";
		case LogLevel::Off:
			return "off";
		default:
			return "unknown";
	}
}

/**
 * @brief Convertit un LogLevel en chaîne courte (3 caractères)
 */
const char *LogLevelToShortString(LogLevel level) {
	switch (level) {
		case LogLevel::Trace:
			return "TRC";
		case LogLevel::Debug:
			return "DBG";
		case LogLevel::Info:
			return "INF";
		case LogLevel::Warn:
			return "WRN";
		case LogLevel::Error:
			return "ERR";
		case LogLevel::Critical:
			return "CRT";
		case LogLevel::Fatal:
			return "FTL";
		case LogLevel::Off:
			return "OFF";
		default:
			return "UNK";
	}
}

/**
 * @brief Convertit une chaîne en LogLevel
 */
LogLevel StringToLogLevel(const char *str) {
	if (!str)
		return LogLevel::Info;

	std::string levelStr(str);
	for (auto &c : levelStr)
		c = std::tolower(c);

	if (levelStr == "trace")
		return LogLevel::Trace;
	if (levelStr == "debug")
		return LogLevel::Debug;
	if (levelStr == "info")
		return LogLevel::Info;
	if (levelStr == "warn" || levelStr == "warning")
		return LogLevel::Warn;
	if (levelStr == "error")
		return LogLevel::Error;
	if (levelStr == "critical")
		return LogLevel::Critical;
	if (levelStr == "fatal")
		return LogLevel::Fatal;
	if (levelStr == "off")
		return LogLevel::Off;

	return LogLevel::Info; // Valeur par défaut
}

/**
 * @brief Convertit une chaîne courte en LogLevel
 */
LogLevel ShortStringToLogLevel(const char *str) {
	if (!str)
		return LogLevel::Info;

	if (strcmp(str, "TRC") == 0)
		return LogLevel::Trace;
	if (strcmp(str, "DBG") == 0)
		return LogLevel::Debug;
	if (strcmp(str, "INF") == 0)
		return LogLevel::Info;
	if (strcmp(str, "WRN") == 0)
		return LogLevel::Warn;
	if (strcmp(str, "ERR") == 0)
		return LogLevel::Error;
	if (strcmp(str, "CRT") == 0)
		return LogLevel::Critical;
	if (strcmp(str, "FTL") == 0)
		return LogLevel::Fatal;
	if (strcmp(str, "OFF") == 0)
		return LogLevel::Off;

	return LogLevel::Info; // Valeur par défaut
}

/**
 * @brief Obtient la couleur ANSI associée à un niveau de log
 */
const char *LogLevelToANSIColor(LogLevel level) {
	switch (level) {
		case LogLevel::Trace:
			return "\033[37m"; // Blanc
		case LogLevel::Debug:
			return "\033[36m"; // Cyan
		case LogLevel::Info:
			return "\033[32m"; // Vert
		case LogLevel::Warn:
			return "\033[33m"; // Jaune
		case LogLevel::Error:
			return "\033[31m"; // Rouge
		case LogLevel::Critical:
			return "\033[35m"; // Magenta
		case LogLevel::Fatal:
			return "\033[41m\033[37m"; // Rouge fond, blanc texte
		default:
			return "\033[0m"; // Reset
	}
}

/**
 * @brief Obtient la couleur Windows associée à un niveau de log
 */
uint16 LogLevelToWindowsColor(LogLevel level) {
	switch (level) {
		case LogLevel::Trace:
			return 0x07; // Gris sur noir
		case LogLevel::Debug:
			return 0x0B; // Cyan clair sur noir
		case LogLevel::Info:
			return 0x0A; // Vert clair sur noir
		case LogLevel::Warn:
			return 0x0E; // Jaune sur noir
		case LogLevel::Error:
			return 0x0C; // Rouge clair sur noir
		case LogLevel::Critical:
			return 0x0D; // Magenta clair sur noir
		case LogLevel::Fatal:
			return 0x4F; // Blanc sur rouge
		default:
			return 0x07; // Gris sur noir
	}
}

} // namespace logger
} // namespace nkentseu