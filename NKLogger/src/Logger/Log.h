// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/NkentseuLogger.h
// DESCRIPTION: Logger par défaut avec API fluide pour un usage simplifié.
//              Fournit une instance singleton et des méthodes chaînables.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Logger.h"
#include <memory>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: NkentseuLogger
// DESCRIPTION: Logger par défaut singleton avec API fluide
// -------------------------------------------------------------------------
class LOGGER_API NkentseuLogger : public Logger {
public:
	// ---------------------------------------------------------------------
	// MÉTHODES STATIQUES (SINGLETON)
	// ---------------------------------------------------------------------

	/**
	 * @brief Obtient l'instance singleton du logger par défaut
	 * @return Référence à l'instance du logger par défaut
	 */
	static NkentseuLogger &Instance();

	/**
	 * @brief Initialise le logger par défaut
	 * @param name Nom du logger (optionnel, "default" par défaut)
	 * @param pattern Pattern de formatage (optionnel)
	 * @param level Niveau de log (optionnel, Info par défaut)
	 */
	static void Initialize(const std::string &name = "default", const std::string &pattern = Formatter::DEFAULT_PATTERN,
						   LogLevel level = LogLevel::Info);

	/**
	 * @brief Nettoie et détruit le logger par défaut
	 */
	static void Shutdown();

	// ---------------------------------------------------------------------
	// API FLUIDE
	// ---------------------------------------------------------------------

	/**
	 * @brief Configure les informations de source pour le prochain log
	 * @param file Fichier source
	 * @param line Ligne source
	 * @param function Fonction source
	 * @return Objet SourceContext pour chaînage
	 */
	// SourceContext Source(const char* file, int line, const char* function) const;

	/**
	 * @brief Configure le nom du logger (retourne *this pour chaînage)
	 */
	NkentseuLogger &Named(const std::string &name);

	/**
	 * @brief Configure le niveau de log (retourne *this pour chaînage)
	 */
	NkentseuLogger &Level(LogLevel level);

	/**
	 * @brief Configure le pattern (retourne *this pour chaînage)
	 */
	NkentseuLogger &Pattern(const std::string &pattern);

	virtual NkentseuLogger &Source(const char *sourceFile = nullptr, uint32 sourceLine = 0,
								   const char *functionName = nullptr) override;

private:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS PRIVÉS (SINGLETON)
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur privé
	 */
	explicit NkentseuLogger(const std::string &name = "default");

	/**
	 * @brief Destructeur privé
	 */
	~NkentseuLogger();

	/**
	 * @brief Constructeur de copie supprimé
	 */
	NkentseuLogger(const NkentseuLogger &) = delete;

	/**
	 * @brief Opérateur d'affectation supprimé
	 */
	NkentseuLogger &operator=(const NkentseuLogger &) = delete;

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE
	// ---------------------------------------------------------------------

	/// Indicateur d'initialisation
	static bool s_Initialized;
};

} // namespace logger
} // namespace nkentseu

// -----------------------------------------------------------------------------
// MACRO PRINCIPALE POUR UN USAGE SIMPLIFIÉ
// -----------------------------------------------------------------------------

/// Macro pour obtenir le logger par défaut avec informations de source
#define logs nkentseu::logger::NkentseuLogger::Instance().Source(__FILE__, __LINE__, __FUNCTION__)

/// Macros raccourcis pour un usage direct (sans informations de source)
#define NK_LOG_TRACE(...) nkentseu::logger::NkentseuLogger::Instance().Trace(__VA_ARGS__)
#define NK_LOG_DEBUG(...) nkentseu::logger::NkentseuLogger::Instance().Debug(__VA_ARGS__)
#define NK_LOG_INFO(...) nkentseu::logger::NkentseuLogger::Instance().Info(__VA_ARGS__)
#define NK_LOG_WARN(...) nkentseu::logger::NkentseuLogger::Instance().Warn(__VA_ARGS__)
#define NK_LOG_ERROR(...) nkentseu::logger::NkentseuLogger::Instance().Error(__VA_ARGS__)
#define NK_LOG_CRITICAL(...) nkentseu::logger::NkentseuLogger::Instance().Critical(__VA_ARGS__)
#define NK_LOG_FATAL(...) nkentseu::logger::NkentseuLogger::Instance().Fatal(__VA_ARGS__)