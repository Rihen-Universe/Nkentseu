// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/LogLevel.h
// DESCRIPTION: Définition des niveaux de log et fonctions utilitaires.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include "NKCore/NkTypes.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// ÉNUMÉRATION: LogLevel
// DESCRIPTION: Niveaux de log disponibles, similaires à spdlog
// -------------------------------------------------------------------------
enum class LogLevel : uint8 {
	/// Niveau trace - messages de trace très détaillés
	Trace = 0,

	/// Niveau debug - messages de débogage
	Debug = 1,

	/// Niveau info - messages informatifs normaux
	Info = 2,

	/// Niveau warn - messages d'avertissement
	Warn = 3,

	/// Niveau error - messages d'erreur
	Error = 4,

	/// Niveau critical - messages critiques
	Critical = 5,

	/// Niveau fatal - messages fatals (arrêt de l'application)
	Fatal = 6,

	/// Désactivation complète du logging
	Off = 7
};

// -------------------------------------------------------------------------
// FONCTIONS UTILITAIRES POUR LogLevel
// -------------------------------------------------------------------------

/**
 * @brief Convertit un LogLevel en chaîne de caractères
 * @param level Niveau de log à convertir
 * @return Chaîne représentant le niveau de log
 */
const char *LogLevelToString(LogLevel level);

/**
 * @brief Convertit un LogLevel en chaîne courte (3 caractères)
 * @param level Niveau de log à convertir
 * @return Chaîne courte représentant le niveau de log
 */
const char *LogLevelToShortString(LogLevel level);

/**
 * @brief Convertit une chaîne en LogLevel
 * @param str Chaîne à convertir
 * @return LogLevel correspondant, LogLevel::Info par défaut
 */
LogLevel StringToLogLevel(const char *str);

/**
 * @brief Convertit une chaîne courte en LogLevel
 * @param str Chaîne courte à convertir
 * @return LogLevel correspondant, LogLevel::Info par défaut
 */
LogLevel ShortStringToLogLevel(const char *str);

/**
 * @brief Obtient la couleur ANSI associée à un niveau de log
 * @param level Niveau de log
 * @return Code couleur ANSI (pour console)
 */
const char *LogLevelToANSIColor(LogLevel level);

/**
 * @brief Obtient la couleur Windows associée à un niveau de log
 * @param level Niveau de log
 * @return Code couleur Windows (pour console Windows)
 */
uint16 LogLevelToWindowsColor(LogLevel level);

} // namespace logger
} // namespace nkentseu