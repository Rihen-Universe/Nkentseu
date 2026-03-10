// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogLevel.h
// DESCRIPTION: Définition des niveaux de log et fonctions utilitaires.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKCore/NkTypes.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// ÉNUMÉRATION: NkLogLevel
	// DESCRIPTION: Niveaux de log disponibles, similaires à spdlog
	// -------------------------------------------------------------------------
	enum class NkLogLevel : uint8 {
		/// Niveau trace - messages de trace très détaillés
		NK_TRACE = 0,

		/// Niveau debug - messages de débogage
		NK_DEBUG = 1,

		/// Niveau info - messages informatifs normaux
		NK_INFO = 2,

		/// Niveau warn - messages d'avertissement
		NK_WARN = 3,

		/// Niveau error - messages d'erreur
		NK_ERROR = 4,

		/// Niveau critical - messages critiques
		NK_CRITICAL = 5,

		/// Niveau fatal - messages fatals (arrêt de l'application)
		NK_FATAL = 6,

		/// Désactivation complète du logging
		NK_OFF = 7
	};

	// -------------------------------------------------------------------------
	// FONCTIONS UTILITAIRES POUR NkLogLevel
	// -------------------------------------------------------------------------

	/**
	 * @brief Convertit un NkLogLevel en chaîne de caractères
	 * @param level Niveau de log à convertir
	 * @return Chaîne représentant le niveau de log
	 */
	const char *NkLogLevelToString(NkLogLevel level);

	/**
	 * @brief Convertit un NkLogLevel en chaîne courte (3 caractères)
	 * @param level Niveau de log à convertir
	 * @return Chaîne courte représentant le niveau de log
	 */
	const char *NkLogLevelToShortString(NkLogLevel level);

	/**
	 * @brief Convertit une chaîne en NkLogLevel
	 * @param str Chaîne à convertir
	 * @return NkLogLevel correspondant, NkLogLevel::Info par défaut
	 */
	NkLogLevel NkStringToLogLevel(const char *str);

	/**
	 * @brief Convertit une chaîne courte en NkLogLevel
	 * @param str Chaîne courte à convertir
	 * @return NkLogLevel correspondant, NkLogLevel::Info par défaut
	 */
	NkLogLevel NkShortStringToLogLevel(const char *str);

	/**
	 * @brief Obtient la couleur ANSI associée à un niveau de log
	 * @param level Niveau de log
	 * @return Code couleur ANSI (pour console)
	 */
	const char *NkLogLevelToANSIColor(NkLogLevel level);

	/**
	 * @brief Obtient la couleur Windows associée à un niveau de log
	 * @param level Niveau de log
	 * @return Code couleur Windows (pour console Windows)
	 */
	uint16 NkLogLevelToWindowsColor(NkLogLevel level);
} // namespace nkentseu
