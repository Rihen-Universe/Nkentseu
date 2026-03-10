// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogLevel.cpp
// DESCRIPTION: Implémentation des fonctions utilitaires pour NkLogLevel.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkLogLevel.h"
#include <cstring>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	namespace {
		inline char NkAsciiToLower(char c) {
			return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
		}

		inline bool NkEqualsIgnoreCase(const char* lhs, const char* rhs) {
			if (!lhs || !rhs) {
				return false;
			}
			while (*lhs && *rhs) {
				if (NkAsciiToLower(*lhs) != NkAsciiToLower(*rhs)) {
					return false;
				}
				++lhs;
				++rhs;
			}
			return (*lhs == '\0' && *rhs == '\0');
		}
	} // namespace

	// -------------------------------------------------------------------------
	// FONCTIONS UTILITAIRES POUR NkLogLevel
	// -------------------------------------------------------------------------

	/**
	 * @brief Convertit un NkLogLevel en chaîne de caractères
	 */
	const char *NkLogLevelToString(NkLogLevel level) {
		switch (level) {
			case NkLogLevel::NK_TRACE:
				return "trace";
			case NkLogLevel::NK_DEBUG:
				return "debug";
			case NkLogLevel::NK_INFO:
				return "info";
			case NkLogLevel::NK_WARN:
				return "warning";
			case NkLogLevel::NK_ERROR:
				return "error";
			case NkLogLevel::NK_CRITICAL:
				return "critical";
			case NkLogLevel::NK_FATAL:
				return "fatal";
			case NkLogLevel::NK_OFF:
				return "off";
			default:
				return "unknown";
		}
	}

	/**
	 * @brief Convertit un NkLogLevel en chaîne courte (3 caractères)
	 */
	const char *NkLogLevelToShortString(NkLogLevel level) {
		switch (level) {
			case NkLogLevel::NK_TRACE:
				return "TRC";
			case NkLogLevel::NK_DEBUG:
				return "DBG";
			case NkLogLevel::NK_INFO:
				return "INF";
			case NkLogLevel::NK_WARN:
				return "WRN";
			case NkLogLevel::NK_ERROR:
				return "ERR";
			case NkLogLevel::NK_CRITICAL:
				return "CRT";
			case NkLogLevel::NK_FATAL:
				return "FTL";
			case NkLogLevel::NK_OFF:
				return "OFF";
			default:
				return "UNK";
		}
	}

	/**
	 * @brief Convertit une chaîne en NkLogLevel
	 */
	NkLogLevel NkStringToLogLevel(const char *str) {
		if (!str)
			return NkLogLevel::NK_INFO;

		if (NkEqualsIgnoreCase(str, "trace"))
			return NkLogLevel::NK_TRACE;
		if (NkEqualsIgnoreCase(str, "debug"))
			return NkLogLevel::NK_DEBUG;
		if (NkEqualsIgnoreCase(str, "info"))
			return NkLogLevel::NK_INFO;
		if (NkEqualsIgnoreCase(str, "warn") || NkEqualsIgnoreCase(str, "warning"))
			return NkLogLevel::NK_WARN;
		if (NkEqualsIgnoreCase(str, "error"))
			return NkLogLevel::NK_ERROR;
		if (NkEqualsIgnoreCase(str, "critical"))
			return NkLogLevel::NK_CRITICAL;
		if (NkEqualsIgnoreCase(str, "fatal"))
			return NkLogLevel::NK_FATAL;
		if (NkEqualsIgnoreCase(str, "off"))
			return NkLogLevel::NK_OFF;

		return NkLogLevel::NK_INFO; // Valeur par défaut
	}

	/**
	 * @brief Convertit une chaîne courte en NkLogLevel
	 */
	NkLogLevel NkShortStringToLogLevel(const char *str) {
		if (!str)
			return NkLogLevel::NK_INFO;

		if (strcmp(str, "TRC") == 0)
			return NkLogLevel::NK_TRACE;
		if (strcmp(str, "DBG") == 0)
			return NkLogLevel::NK_DEBUG;
		if (strcmp(str, "INF") == 0)
			return NkLogLevel::NK_INFO;
		if (strcmp(str, "WRN") == 0)
			return NkLogLevel::NK_WARN;
		if (strcmp(str, "ERR") == 0)
			return NkLogLevel::NK_ERROR;
		if (strcmp(str, "CRT") == 0)
			return NkLogLevel::NK_CRITICAL;
		if (strcmp(str, "FTL") == 0)
			return NkLogLevel::NK_FATAL;
		if (strcmp(str, "OFF") == 0)
			return NkLogLevel::NK_OFF;

		return NkLogLevel::NK_INFO; // Valeur par défaut
	}

	/**
	 * @brief Obtient la couleur ANSI associée à un niveau de log
	 */
	const char *NkLogLevelToANSIColor(NkLogLevel level) {
		switch (level) {
			case NkLogLevel::NK_TRACE:
				return "\033[37m"; // Blanc
			case NkLogLevel::NK_DEBUG:
				return "\033[36m"; // Cyan
			case NkLogLevel::NK_INFO:
				return "\033[32m"; // Vert
			case NkLogLevel::NK_WARN:
				return "\033[33m"; // Jaune
			case NkLogLevel::NK_ERROR:
				return "\033[31m"; // Rouge
			case NkLogLevel::NK_CRITICAL:
				return "\033[35m"; // Magenta
			case NkLogLevel::NK_FATAL:
				return "\033[41m\033[37m"; // Rouge fond, blanc texte
			default:
				return "\033[0m"; // Reset
		}
	}

	/**
	 * @brief Obtient la couleur Windows associée à un niveau de log
	 */
	uint16 NkLogLevelToWindowsColor(NkLogLevel level) {
		switch (level) {
			case NkLogLevel::NK_TRACE:
				return 0x07; // Gris sur noir
			case NkLogLevel::NK_DEBUG:
				return 0x0B; // Cyan clair sur noir
			case NkLogLevel::NK_INFO:
				return 0x0A; // Vert clair sur noir
			case NkLogLevel::NK_WARN:
				return 0x0E; // Jaune sur noir
			case NkLogLevel::NK_ERROR:
				return 0x0C; // Rouge clair sur noir
			case NkLogLevel::NK_CRITICAL:
				return 0x0D; // Magenta clair sur noir
			case NkLogLevel::NK_FATAL:
				return 0x4F; // Blanc sur rouge
			default:
				return 0x07; // Gris sur noir
		}
	}

} // namespace nkentseu
