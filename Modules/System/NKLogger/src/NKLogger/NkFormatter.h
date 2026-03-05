// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkFormatter.h
// DESCRIPTION: Classe de formatage des messages de log avec patterns.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLoggerExport.h"
#include "NKLogger/NkLogMessage.h"
#include <string>
#include <memory>
#include <vector>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// STRUCTURE: PatternToken
	// DESCRIPTION: Token individuel dans un pattern de formatage
	// -------------------------------------------------------------------------
	struct NkPatternToken {
		/// Type du token
		enum class Type {
			NK_LITERAL,	// Texte littéral
			NK_YEAR,		// %Y - Année (4 chiffres)
			NK_MONTH,		// %m - Mois (2 chiffres)
			NK_DAY,		// %d - Jour (2 chiffres)
			NK_HOUR,		// %H - Heure (24h, 2 chiffres)
			NK_MINUTE,		// %M - Minute (2 chiffres)
			NK_SECOND,		// %S - Seconde (2 chiffres)
			NK_MILLIS,		// %e - Millisecondes (3 chiffres)
			NK_MICROS,		// %f - Microsecondes (6 chiffres)
			NK_LEVEL,		// %l - Niveau de log (texte)
			NK_LEVEL_SHORT, // %L - Niveau de log (court)
			NK_THREAD_ID,	// %t - ID du thread
			NK_THREAD_NAME, // %T - Nom du thread
			NK_SOURCE_FILE, // %s - Fichier source
			NK_SOURCE_LINE, // %# - Ligne source
			NK_FUNCTION,	// %f - Fonction
			NK_MESSAGE,	// %v - Message
			NK_LOGGER_NAME, // %n - Nom du logger
			NK_PERCENT,	// %% - Pourcentage littéral
			NK_COLOR_START, // Début de couleur
			NK_COLOR_END	// Fin de couleur
		};

		Type type;
		std::string value; // Pour les tokens littéraux
	};

	// -------------------------------------------------------------------------
	// CLASSE: NkFormatter
	// DESCRIPTION: Formate les messages de log selon un pattern
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkFormatter {
		public:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur par défaut (pattern par défaut)
			 */
			NkFormatter();

			/**
			 * @brief Constructeur avec pattern spécifique
			 * @param pattern Pattern de formatage
			 */
			explicit NkFormatter(const std::string &pattern);

			/**
			 * @brief Destructeur
			 */
			~NkFormatter();

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
			std::string Format(const NkLogMessage &message);

			/**
			 * @brief Formate un message de log avec des couleurs
			 * @param message Message à formater
			 * @param useColors true pour inclure les codes couleur
			 * @return Message formaté
			 */
			std::string Format(const NkLogMessage &message, bool useColors);

			// ---------------------------------------------------------------------
			// PATTERNS PRÉDÉFINIS
			// ---------------------------------------------------------------------

			/**
			 * @brief Pattern par défaut (similaire à spdlog)
			 */
			static const char *NK_DEFAULT_PATTERN;

			/**
			 * @brief Pattern simple (juste le message)
			 */
			static const char *NK_SIMPLE_PATTERN;

			/**
			 * @brief Pattern détaillé (avec toutes les informations)
			 */
			static const char *NK_DETAILED_PATTERN;
			static const char *NK_NKENTSEU_PATTERN;
			/**
			 * @brief Pattern avec couleurs (pour console)
			 */
			static const char *NK_COLOR_PATTERN;

			/**
			 * @brief Pattern JSON (pour traitement automatisé)
			 */
			static const char *NK_JSON_PATTERN;

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
			void FormatToken(const NkPatternToken &token, const NkLogMessage &message, bool useColors, std::string &result);

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
			std::string GetANSIColor(NkLogLevel level) const;

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
			std::vector<NkPatternToken> m_Tokens;

			/// Indicateur si les tokens sont valides
			bool m_TokensValid;
	};

	// -------------------------------------------------------------------------
	// TYPE ALIAS POUR LES FORMATTERS
	// -------------------------------------------------------------------------

	/// Type pour les pointeurs uniques de formatters
	using FormatterPtr = std::unique_ptr<NkFormatter>;

} // namespace nkentseu