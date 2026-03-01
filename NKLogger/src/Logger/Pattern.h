// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Pattern.h
// DESCRIPTION: Patterns de formatage prédéfinis pour le logging.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

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
// PATTERNS DE FORMATAGE PRÉDÉFINIS
// -------------------------------------------------------------------------

/**
 * @brief Pattern par défaut (similaire à spdlog)
 * Format: [2026-01-01 12:34:56.789] [INF] [12345] Message
 */
extern const char *DEFAULT_PATTERN;

/**
 * @brief Pattern simple (juste le message)
 * Format: Message
 */
extern const char *SIMPLE_PATTERN;

/**
 * @brief Pattern détaillé (avec toutes les informations)
 * Format: [2026-01-01 12:34:56.789] [INF] [logger] [thread 12345] [file.cpp:42 in function] Message
 */
extern const char *DETAILED_PATTERN;

/**
 * @brief Pattern avec couleurs (pour console)
 * Format: [2026-01-01 12:34:56.789] [INF] [12345] Message (INF en couleur)
 */
extern const char *COLOR_PATTERN;

/**
 * @brief Pattern JSON (pour traitement automatisé)
 * Format:
 * {"time":"2026-01-01T12:34:56.789000Z","level":"info","thread":12345,"logger":"default","file":"file.cpp","line":42,"function":"function","message":"Message"}
 */
extern const char *JSON_PATTERN;

/**
 * @brief Pattern court (pour logs de production)
 * Format: 12:34:56 INF Message
 */
extern const char *SHORT_PATTERN;

/**
 * @brief Pattern avec heure ISO 8601
 * Format: 2026-01-01T12:34:56.789Z [INF] Message
 */
extern const char *ISO8601_PATTERN;

/**
 * @brief Pattern pour les logs système
 * Format: Jan 01 12:34:56 hostname logger[12345]: Message
 */
extern const char *SYSLOG_PATTERN;

// -------------------------------------------------------------------------
// DOCUMENTATION DES TOKENS DE PATTERN
// -------------------------------------------------------------------------

/**
 * TOKENS DISPONIBLES:
 *
 * %Y - Année (4 chiffres, ex: 2026)
 * %m - Mois (2 chiffres, ex: 01)
 * %d - Jour (2 chiffres, ex: 01)
 * %H - Heure (24h, 2 chiffres, ex: 14)
 * %M - Minute (2 chiffres, ex: 30)
 * %S - Seconde (2 chiffres, ex: 45)
 * %e - Millisecondes (3 chiffres, ex: 123)
 * %f - Microsecondes (6 chiffres, ex: 123456)
 * %l - Niveau de log (texte complet, ex: "info")
 * %L - Niveau de log (3 lettres, ex: "INF")
 * %t - ID du thread (nombre)
 * %T - Nom du thread (si disponible, sinon ID)
 * %s - Fichier source (nom seulement)
 * %# - Ligne source (nombre)
 * %F - Fonction source
 * %v - Message de log
 * %n - Nom du logger
 * %% - Caractère '%' littéral
 * %^ - Début de couleur (ANSI)
 * %$ - Fin de couleur (ANSI)
 */

} // namespace logger
} // namespace nkentseu