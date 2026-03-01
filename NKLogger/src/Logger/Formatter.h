// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Formatter.h
// DESCRIPTION: Classe de formatage des messages de log avec patterns.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Export.h"
#include "Logger/LogMessage.h"
#include <string>
#include <memory>
#include <vector>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// STRUCTURE: PatternToken
// DESCRIPTION: Token individuel dans un pattern de formatage
// -------------------------------------------------------------------------
struct PatternToken {
	/// Type du token
	enum class Type {
		Literal,	// Texte littéral
		Year,		// %Y - Année (4 chiffres)
		Month,		// %m - Mois (2 chiffres)
		Day,		// %d - Jour (2 chiffres)
		Hour,		// %H - Heure (24h, 2 chiffres)
		Minute,		// %M - Minute (2 chiffres)
		Second,		// %S - Seconde (2 chiffres)
		Millis,		// %e - Millisecondes (3 chiffres)
		Micros,		// %f - Microsecondes (6 chiffres)
		Level,		// %l - Niveau de log (texte)
		LevelShort, // %L - Niveau de log (court)
		ThreadId,	// %t - ID du thread
		ThreadName, // %T - Nom du thread
		SourceFile, // %s - Fichier source
		SourceLine, // %# - Ligne source
		Function,	// %f - Fonction
		Message,	// %v - Message
		LoggerName, // %n - Nom du logger
		Percent,	// %% - Pourcentage littéral
		ColorStart, // Début de couleur
		ColorEnd	// Fin de couleur
	};

	Type type;
	std::string value; // Pour les tokens littéraux
};

// -------------------------------------------------------------------------
// CLASSE: Formatter
// DESCRIPTION: Formate les messages de log selon un pattern
// -------------------------------------------------------------------------
class LOGGER_API Formatter {
public:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS ET DESTRUCTEUR
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur par défaut (pattern par défaut)
	 */
	Formatter();

	/**
	 * @brief Constructeur avec pattern spécifique
	 * @param pattern Pattern de formatage
	 */
	explicit Formatter(const std::string &pattern);

	/**
	 * @brief Destructeur
	 */
	~Formatter();

	// ---------------------------------------------------------------------
	// CONFIGURATION DU PATTERN
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit le pattern de formatage
	 * @param pattern Pattern à utiliser
	 */
	void SetPattern(const std::string &pattern);

	/**
	 * @brief Obtient le pattern courant
	 * @return Pattern de formatage
	 */
	const std::string &GetPattern() const;

	// ---------------------------------------------------------------------
	// FORMATAGE
	// ---------------------------------------------------------------------

	/**
	 * @brief Formate un message de log
	 * @param message Message à formater
	 * @return Message formaté
	 */
	std::string Format(const LogMessage &message);

	/**
	 * @brief Formate un message de log avec des couleurs
	 * @param message Message à formater
	 * @param useColors true pour inclure les codes couleur
	 * @return Message formaté
	 */
	std::string Format(const LogMessage &message, bool useColors);

	// ---------------------------------------------------------------------
	// PATTERNS PRÉDÉFINIS
	// ---------------------------------------------------------------------

	/**
	 * @brief Pattern par défaut (similaire à spdlog)
	 */
	static const char *DEFAULT_PATTERN;

	/**
	 * @brief Pattern simple (juste le message)
	 */
	static const char *SIMPLE_PATTERN;

	/**
	 * @brief Pattern détaillé (avec toutes les informations)
	 */
	static const char *DETAILED_PATTERN;

	/**
	 * @brief Pattern avec couleurs (pour console)
	 */
	static const char *COLOR_PATTERN;

	/**
	 * @brief Pattern JSON (pour traitement automatisé)
	 */
	static const char *JSON_PATTERN;

private:
	// ---------------------------------------------------------------------
	// MÉTHODES PRIVÉES
	// ---------------------------------------------------------------------

	/**
	 * @brief Parse le pattern en tokens
	 * @param pattern Pattern à parser
	 */
	void ParsePattern(const std::string &pattern);

	/**
	 * @brief Formate un token individuel
	 * @param token Token à formater
	 * @param message Message source
	 * @param useColors true pour inclure les couleurs
	 * @param result Résultat en construction
	 */
	void FormatToken(const PatternToken &token, const LogMessage &message, bool useColors, std::string &result);

	/**
	 * @brief Formate un nombre avec padding
	 * @param value Valeur à formater
	 * @param width Largeur minimum
	 * @param fillChar Caractère de remplissage
	 * @return Chaîne formatée
	 */
	std::string FormatNumber(int value, int width = 2, char fillChar = '0') const;

	/**
	 * @brief Obtient le code couleur ANSI pour un niveau de log
	 * @param level Niveau de log
	 * @return Code couleur ANSI
	 */
	std::string GetANSIColor(LogLevel level) const;

	/**
	 * @brief Obtient le code de fin de couleur ANSI
	 * @return Code de fin de couleur
	 */
	std::string GetANSIReset() const;

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Pattern de formatage
	std::string m_Pattern;

	/// Tokens parsés
	std::vector<PatternToken> m_Tokens;

	/// Indicateur si les tokens sont valides
	bool m_TokensValid;
};

// -------------------------------------------------------------------------
// TYPE ALIAS POUR LES FORMATTERS
// -------------------------------------------------------------------------

/// Type pour les pointeurs uniques de formatters
using FormatterPtr = std::unique_ptr<Formatter>;

} // namespace logger
} // namespace nkentseu