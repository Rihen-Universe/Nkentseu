// =============================================================================
// NKLogger/NkLoggerFormatter.h
// Système de formatage des messages de log avec patterns et intégration NkFormat.
//
// Design :
//  - Réutilisation du système NkFormat pour le formatage positionnel
//  - Pattern-based formatting style spdlog pour compatibilité développeur
//  - Tokens de pattern parsés une fois, réutilisés pour chaque message
//  - Support des couleurs ANSI via tokens spéciaux %^ et %$
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Templates sans macros d'export (incompatibles avec l'export DLL)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKLOGGERFORMATTER_H
#define NKENTSEU_NKLOGGERFORMATTER_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour le formatage et les conteneurs.
	// Dépendances projet pour NkString, NkLogMessage et NkFormat.

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKContainers/String/NkStringView.h"
	#include "NKContainers/Sequential/NkVector.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKContainers/String/NkFormat.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace logger pour simplifier l'usage.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// STRUCTURE : NkPatternToken
		// DESCRIPTION : Représentation interne d'un élément de pattern
		// ---------------------------------------------------------------------
		/**
		 * @struct NkPatternToken
		 * @brief Token individuel résultant du parsing d'un pattern de formatage
		 * @ingroup LoggerFormatterTypes
		 *
		 * Chaque token représente soit :
		 *  - Un littéral texte à copier tel quel
		 *  - Un placeholder dynamique à remplacer par une valeur du message
		 *  - Un marqueur de couleur pour l'affichage console
		 *
		 * @note Les tokens sont parsés une fois lors de SetPattern(),
		 *       puis réutilisés pour chaque appel à Format() → performance O(1).
		 *
		 * @example Pattern : "[%Y-%m-%d %L] %v"
		 * @code
		 * Tokens générés :
		 *  1. LITERAL "["
		 *  2. YEAR      → 2024
		 *  3. LITERAL "-"
		 *  4. MONTH     → 01
		 *  5. LITERAL "-"
		 *  6. DAY       → 15
		 *  7. LITERAL " "
		 *  8. LEVEL_SHORT → "INF"
		 *  9. LITERAL "] "
		 * 10. MESSAGE   → "Application started"
		 * @endcode
		 */
		struct NKENTSEU_LOGGER_CLASS_EXPORT NkPatternToken {


			// -----------------------------------------------------------------
			// ÉNUMÉRATION : Type de token
			// -----------------------------------------------------------------
			/**
			 * @enum Type
			 * @brief Catégories de tokens supportées dans les patterns
			 * @ingroup LoggerFormatterTypes
			 *
			 * Préfixe NK_ pour éviter les collisions avec d'autres enums.
			 * Les valeurs sont arbitraires : seul le switch importe.
			 */
			enum class Type : uint8 {
				/// @brief Texte littéral : copié tel quel dans la sortie
				NK_LITERAL = 0,

				/// @brief Année sur 4 chiffres : %Y → 2024
				NK_YEAR = 1,

				/// @brief Mois sur 2 chiffres : %m → 01..12
				NK_MONTH = 2,

				/// @brief Jour du mois sur 2 chiffres : %d → 01..31
				NK_DAY = 3,

				/// @brief Heure (24h) sur 2 chiffres : %H → 00..23
				NK_HOUR = 4,

				/// @brief Minute sur 2 chiffres : %M → 00..59
				NK_MINUTE = 5,

				/// @brief Seconde sur 2 chiffres : %S → 00..59
				NK_SECOND = 6,

				/// @brief Millisecondes sur 3 chiffres : %e → 000..999
				NK_MILLIS = 7,

				/// @brief Microsecondes sur 6 chiffres : %f → 000000..999999
				NK_MICROS = 8,

				/// @brief Niveau de log en texte complet : %l → "info", "error"
				NK_LEVEL = 9,

				/// @brief Niveau de log en code court : %L → "INF", "ERR"
				NK_LEVEL_SHORT = 10,

				/// @brief ID numérique du thread : %t → 12345
				NK_THREAD_ID = 11,

				/// @brief Nom lisible du thread : %T → "MainThread"
				NK_THREAD_NAME = 12,

				/// @brief Chemin du fichier source : %s → "src/main.cpp"
				NK_SOURCE_FILE = 13,

				/// @brief Numéro de ligne source : %# → 42
				NK_SOURCE_LINE = 14,

				/// @brief Nom de la fonction émettrice : %f → "ProcessInput"
				NK_FUNCTION = 15,

				/// @brief Contenu du message de log : %v → "Connection established"
				NK_MESSAGE = 16,

				/// @brief Nom hiérarchique du logger : %n → "nkentseu.network"
				NK_LOGGER_NAME = 17,

				/// @brief Caractère % littéral : %% → %
				NK_PERCENT = 18,

				/// @brief Début de zone colorée : %^ → code ANSI vert
				NK_COLOR_START = 19,

				/// @brief Fin de zone colorée : %$ → code ANSI reset
				NK_COLOR_END = 20
			};


			// -----------------------------------------------------------------
			// MEMBRES DE LA STRUCTURE
			// -----------------------------------------------------------------

			/// @brief Type du token : détermine le comportement de formatage
			Type type;

			/// @brief Valeur associée : utilisée uniquement pour NK_LITERAL
			/// @note Pour les autres types, la valeur est extraite du NkLogMessage
			NkString value;


		}; // struct NkPatternToken


		// ---------------------------------------------------------------------
		// CLASSE : NkLoggerFormatter
		// DESCRIPTION : Formateur de messages de log basé sur des patterns
		// ---------------------------------------------------------------------
		/**
		 * @class NkLoggerFormatter
		 * @brief Formate les messages de log selon un pattern configurable
		 * @ingroup LoggerComponents
		 *
		 * Ce formateur combine :
		 *  - Un parser de pattern style spdlog (%Y, %L, %v, etc.)
		 *  - Le système NkFormat pour le formatage positionnel avancé
		 *  - Un cache de tokens pour éviter le reparsing à chaque log
		 *  - Un support optionnel des couleurs ANSI pour console
		 *
		 * Thread-safety :
		 *  - SetPattern() n'est PAS thread-safe : à appeler avant tout logging
		 *  - Format() EST thread-safe pour lecture concurrente (tokens immuables)
		 *
		 * @note Les patterns sont compilés une fois : overhead initial, puis O(1).
		 *
		 * @example
		 * @code
		 * // Pattern style spdlog avec couleurs
		 * NkLoggerFormatter fmt("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] %v");
		 *
		 * NkLogMessage msg(NkLogLevel::NK_INFO, "Server started");
		 * NkString output = fmt.Format(msg, true);  // avec couleurs
		 * // Résultat : [2024-01-15 14:30:45.123] [INF] Server started
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkLoggerFormatter {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterCtors Constructeurs de NkLoggerFormatter
			 * @brief Initialisation avec pattern par défaut ou personnalisé
			 */

			/**
			 * @brief Constructeur par défaut : utilise NK_DEFAULT_PATTERN
			 * @ingroup FormatterCtors
			 *
			 * @post Pattern initialisé, tokens parsés, prêt pour Format()
			 * @note Pattern par défaut : "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v"
			 */
			NkLoggerFormatter();

			/**
			 * @brief Constructeur avec pattern personnalisé
			 * @ingroup FormatterCtors
			 *
			 * @param pattern Chaîne de format style spdlog à parser
			 * @post Pattern stocké, tokens générés, prêt pour Format()
			 * @note Le parsing est effectué immédiatement : coût O(n) avec n = longueur pattern
			 *
			 * @example
			 * @code
			 * NkLoggerFormatter custom("[%L] %v");  // Format minimal
			 * NkLoggerFormatter debug("%Y-%m-%d %H:%M:%S [%f:%#] %v");  // Avec source
			 * @endcode
			 */
			explicit NkLoggerFormatter(const NkString& pattern);

			/**
			 * @brief Destructeur par défaut
			 * @ingroup FormatterCtors
			 *
			 * @note Libération automatique des NkVector et NkString via RAII
			 */
			~NkLoggerFormatter() = default;


			// -----------------------------------------------------------------
			// CONFIGURATION DU PATTERN
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterConfig Configuration du Pattern
			 * @brief Méthodes pour modifier le pattern de formatage à runtime
			 */

			/**
			 * @brief Définit un nouveau pattern de formatage
			 * @ingroup FormatterConfig
			 *
			 * @param pattern Nouvelle chaîne de format à parser
			 * @post Anciens tokens invalidés, nouveau pattern parsé si différent
			 * @note Coût : O(n) pour le parsing, à éviter dans le chemin critique
			 *
			 * @example
			 * @code
			 * NkLoggerFormatter fmt;
			 * fmt.SetPattern("%L: %v");  // Format compact
			 * // ... logs ...
			 * fmt.SetPattern("[%Y-%m-%d] %v");  // Format avec date
			 * @endcode
			 */
			void SetPattern(const NkString& pattern);

			/**
			 * @brief Obtient le pattern de formatage courant
			 * @ingroup FormatterConfig
			 *
			 * @return Référence constante vers la chaîne de pattern
			 * @note Retour par référence : pas de copie, lecture directe
			 *
			 * @example
			 * @code
			 * const NkString& current = fmt.GetPattern();
			 * Logger::Log(NkLogLevel::NK_DEBUG, "Current pattern: %s", current.CStr());
			 * @endcode
			 */
			const NkString& GetPattern() const;


			// -----------------------------------------------------------------
			// FORMATAGE DES MESSAGES
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterFormat Méthodes de Formatage
			 * @brief Conversion d'un NkLogMessage en chaîne formatée
			 */

			/**
			 * @brief Formate un message de log sans couleurs
			 * @ingroup FormatterFormat
			 *
			 * @param message Message source contenant les données de log
			 * @return Chaîne formatée selon le pattern courant
			 * @note Équivalent à Format(message, false)
			 *
			 * @example
			 * @code
			 * NkLogMessage msg(NkLogLevel::NK_INFO, "Connection established");
			 * NkString output = fmt.Format(msg);
			 * // "[2024-01-15 14:30:45.123] [INF] [default] [12345] -> Connection established"
			 * @endcode
			 */
			NkString Format(const NkLogMessage& message);

			/**
			 * @brief Formate un message de log avec option de couleurs
			 * @ingroup FormatterFormat
			 *
			 * @param message Message source contenant les données de log
			 * @param useColors true pour inclure les codes couleur ANSI, false sinon
			 * @return Chaîne formatée avec ou sans séquences ANSI selon useColors
			 * @note Les tokens %^ et %$ contrôlent les zones colorées dans le pattern
			 *
			 * @example
			 * @code
			 * // Pattern avec marqueurs de couleur : "[%^%L%$] %v"
			 * NkString colored = fmt.Format(msg, true);   // Avec ANSI : [\033[32mINF\033[0m] ...
			 * NkString plain = fmt.Format(msg, false);    // Sans ANSI : [INF] ...
			 * @endcode
			 */
			NkString Format(const NkLogMessage& message, bool useColors);


			// -----------------------------------------------------------------
			// PATTERNS PRÉDÉFINIS (CONSTANTES STATIQUES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterPresets Patterns Prédéfinis
			 * @brief Constantes de patterns courants pour usage immédiat
			 *
			 * Ces patterns sont testés et optimisés pour les cas d'usage courants.
			 * Ils peuvent être utilisés directement ou comme base de personnalisation.
			 */

			/// @brief Pattern par défaut : équilibré entre lisibilité et informations
			/// @ingroup FormatterPresets
			/// @code "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v" @endcode
			static const char* NK_DEFAULT_PATTERN;

			/// @brief Pattern minimal : uniquement le message
			/// @ingroup FormatterPresets
			/// @code "%v" @endcode
			static const char* NK_SIMPLE_PATTERN;

			/// @brief Pattern détaillé : toutes les métadonnées de debug
			/// @ingroup FormatterPresets
			/// @code "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [thread %t] [%s:%# in %f] -> %v" @endcode
			static const char* NK_DETAILED_PATTERN;

			/// @brief Pattern NKENTSEU : avec support des couleurs via %^%L%$
			/// @ingroup FormatterPresets
			/// @code "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%s:%# in %F] -> %v" @endcode
			static const char* NK_NKENTSEU_PATTERN;

			/// @brief Pattern console : couleurs autour du niveau de log uniquement
			/// @ingroup FormatterPresets
			/// @code "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%t] -> %v" @endcode
			static const char* NK_COLOR_PATTERN;

			/// @brief Pattern JSON : pour ingestion dans systèmes de métriques
			/// @ingroup FormatterPresets
			/// @note Les champs sont échappés pour validité JSON
			static const char* NK_JSON_PATTERN;

			/// @brief Pattern court pour production : "12:34:56 INF Message"
			/// @ingroup FormatterPresets
			/// @code "%H:%M:%S %L %v" @endcode
			static const char* NK_SHORT_PATTERN;

			/// @brief Pattern ISO 8601 : "2026-01-01T12:34:56.789Z [INF] Message"
			/// @ingroup FormatterPresets
			/// @code "%Y-%m-%dT%H:%M:%S.%fZ [%L] %v" @endcode
			static const char* NK_ISO8601_PATTERN;

			/// @brief Pattern syslog : "Jan 01 12:34:56 hostname logger[12345]: Message"
			/// @ingroup FormatterPresets
			/// @code "%b %d %H:%M:%S %h %n[%t]: %v" @endcode
			static const char* NK_SYSLOG_PATTERN;


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PROTÉGÉS (POUR HÉRITAGE CONTRÔLÉ)
			// -----------------------------------------------------------------
		protected:


			// -----------------------------------------------------------------
			// MÉTHODES PROTÉGÉES POUR EXTENSION
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterProtected Méthodes Protégées
			 * @brief Points d'extension pour les classes dérivées
			 *
			 * Ces méthodes permettent de personnaliser le comportement
			 * du formateur sans réécrire l'ensemble de la logique.
			 */

			/**
			 * @brief Formate un token individuel vers la chaîne de résultat
			 * @ingroup FormatterProtected
			 *
			 * @param token Token à formater (ex: NK_YEAR, NK_MESSAGE)
			 * @param message Message source contenant les données
			 * @param useColors true pour appliquer les codes couleur si token coloré
			 * @param result Chaîne en construction à laquelle appendre la sortie
			 * @note Méthode virtuelle potentielle pour surcharge dans les dérivées
			 *
			 * @example
			 * @code
			 * // Dans une classe dérivée pour formatage personnalisé
			 * void MyFormatter::FormatToken(const NkPatternToken& token, ...) {
			 *     if (token.type == NkPatternToken::NK_MESSAGE) {
			 *         // Ajouter un préfixe personnalisé aux messages
			 *         result += "[MYAPP] ";
			 *         result += message.message;
			 *     } else {
			 *         // Déléguer au comportement par défaut
			 *         NkLoggerFormatter::FormatToken(token, message, useColors, result);
			 *     }
			 * }
			 * @endcode
			 */
			void FormatToken(
				const NkPatternToken& token,
				const NkLogMessage& message,
				bool useColors,
				NkString& result
			);


			// -----------------------------------------------------------------
			// SECTION 5 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// MÉTHODES PRIVÉES D'IMPLÉMENTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterPrivate Méthodes Privées
			 * @brief Fonctions internes non exposées dans l'API publique
			 */

			/**
			 * @brief Parse une chaîne de pattern en vecteur de tokens
			 * @ingroup FormatterPrivate
			 *
			 * @param pattern Chaîne de format à parser (style spdlog)
			 * @post m_Tokens rempli, m_TokensValid = true
			 * @note Algorithme : parcours caractère par caractère, détection des %
			 *
			 * @example Pattern : "Hello %v from %n"
			 * @code
			 * Tokens générés :
			 *  1. NK_LITERAL "Hello "
			 *  2. NK_MESSAGE  → remplacé par message.message
			 *  3. NK_LITERAL " from "
			 *  4. NK_LOGGER_NAME → remplacé par message.loggerName
			 * @endcode
			 */
			void ParsePattern(const NkString& pattern);

			/**
			 * @brief Formate un entier avec padding et caractère de remplissage
			 * @ingroup FormatterPrivate
			 *
			 * @param value Valeur entière à formater
			 * @param width Largeur minimale de sortie (padding si nécessaire)
			 * @param fillChar Caractère de remplissage (défaut : '0')
			 * @return Chaîne formatée avec padding appliqué
			 * @note Gère les nombres négatifs : signe préservé devant les zéros
			 *
			 * @example
			 * @code
			 * FormatNumber(42, 4)        → "0042"
			 * FormatNumber(7, 3, ' ')    → "  7"
			 * FormatNumber(-5, 4)        → "-005"  (signe avant padding)
			 * @endcode
			 */
			NkString FormatNumber(int value, int width = 2, char fillChar = '0') const;

			/**
			 * @brief Obtient le code couleur ANSI pour un niveau de log donné
			 * @ingroup FormatterPrivate
			 *
			 * @param level Niveau de log pour lequel obtenir la couleur
			 * @return Séquence d'échappement ANSI (ex: "\033[32m" pour vert)
			 * @note Délègue à NkLogLevelToANSIColor() pour cohérence globale
			 *
			 * @example
			 * @code
			 * GetANSIColor(NkLogLevel::NK_ERROR) → "\033[31m"  // Rouge
			 * GetANSIColor(NkLogLevel::NK_INFO)  → "\033[32m"  // Vert
			 * @endcode
			 */
			NkString GetANSIColor(NkLogLevel level) const;

			/**
			 * @brief Obtient le code ANSI de reset pour terminer une zone colorée
			 * @ingroup FormatterPrivate
			 *
			 * @return Séquence "\033[0m" pour réinitialiser les attributs de console
			 * @note À utiliser après chaque %$ dans le pattern
			 *
			 * @example
			 * @code
			 * // Pattern : "[%^%L%$] %v"
			 * // Sortie :  "[\033[32mINF\033[0m] Message"
			 * //           ^^^^^^^^    ^^^^^^^
			 * //           couleur     reset
			 * @endcode
			 */
			NkString GetANSIReset() const;


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES
			// -----------------------------------------------------------------
			/**
			 * @defgroup FormatterMembers Membres de Données
			 * @brief État interne du formateur (non exposé publiquement)
			 */

			/// @brief Chaîne de pattern brute fournie par l'utilisateur
			/// @ingroup FormatterMembers
			/// @note Utilisée pour regeneration des tokens si SetPattern() appelé
			NkString m_Pattern;

			/// @brief Tokens parsés : cache pour éviter le reparsing à chaque Format()
			/// @ingroup FormatterMembers
			/// @note Vecteur pré-alloué : Reserve(pattern.Size() / 2) pour performance
			NkVector<NkPatternToken> m_Tokens;

			/// @brief Indicateur de validité du cache de tokens
			/// @ingroup FormatterMembers
			/// @note false après construction ou SetPattern(), true après ParsePattern()
			bool m_TokensValid;


		}; // class NkLoggerFormatter


		// ---------------------------------------------------------------------
		// TYPE ALIAS POUR GESTION DE MÉMOIRE
		// ---------------------------------------------------------------------
		/**
		 * @typedef FormatterPtr
		 * @brief Pointeur unique vers NkLoggerFormatter pour gestion automatique
		 * @ingroup LoggerTypes
		 *
		 * Alias vers memory::NkUniquePtr<NkLoggerFormatter> pour :
		 *  - Libération automatique via RAII
		 *  - Sémantique de mouvement exclusive
		 *  - Compatibilité avec le système d'allocateur du projet
		 *
		 * @example
		 * @code
		 * // Création avec pattern personnalisé
		 * FormatterPtr fmt = memory::MakeUnique<NkLoggerFormatter>("[%L] %v");
		 *
		 * // Transfert de propriété
		 * void SetFormatter(FormatterPtr&& newFmt) {
		 *     m_formatter = std::move(newFmt);  // Ancien automatiquement libéré
		 * }
		 * @endcode
		 */
		using FormatterPtr = memory::NkUniquePtr<NkLoggerFormatter>;


		// ---------------------------------------------------------------------
		// DOCUMENTATION DES TOKENS DE PATTERN (DOXYGEN TABLE)
		// ---------------------------------------------------------------------
		/**
		 * @defgroup PatternTokens Tokens de Pattern Disponibles
		 * @brief Référence complète des placeholders supportés dans les patterns
		 * @ingroup LoggerFormatter
		 *
		 * | Token | Description | Exemple de sortie |
		 * |-------|-------------|-------------------|
		 * | `%Y` | Année (4 chiffres) | `2026` |
		 * | `%m` | Mois (2 chiffres) | `01` .. `12` |
		 * | `%d` | Jour du mois (2 chiffres) | `01` .. `31` |
		 * | `%H` | Heure (24h, 2 chiffres) | `00` .. `23` |
		 * | `%M` | Minute (2 chiffres) | `00` .. `59` |
		 * | `%S` | Seconde (2 chiffres) | `00` .. `59` |
		 * | `%e` | Millisecondes (3 chiffres) | `000` .. `999` |
		 * | `%f` | Microsecondes (6 chiffres) | `000000` .. `999999` |
		 * | `%l` | Niveau de log (texte complet) | `info`, `error` |
		 * | `%L` | Niveau de log (code court) | `INF`, `ERR` |
		 * | `%t` | ID numérique du thread | `12345` |
		 * | `%T` | Nom du thread (ou ID si vide) | `MainThread` |
		 * | `%s` | Nom du fichier source (sans chemin) | `main.cpp` |
		 * | `%#` | Numéro de ligne source | `42` |
		 * | `%F` | Nom de la fonction émettrice | `ProcessRequest` |
		 * | `%v` | Contenu du message de log | `Connection established` |
		 * | `%n` | Nom hiérarchique du logger | `nkentseu.network` |
		 * | `%%` | Caractère `%` littéral | `%` |
		 * | `%^` | Début de zone colorée (ANSI) | `\033[32m` |
		 * | `%$` | Fin de zone colorée (ANSI) | `\033[0m` |
		 *
		 * @note Les tokens sont sensibles à la casse : `%L` ≠ `%l`
		 * @note Les tokens inconnus sont traités comme littéraux (robustesse)
		 *
		 * @example
		 * @code
		 * // Pattern personnalisé combinant plusieurs tokens
		 * NkLoggerFormatter fmt("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] %v");
		 * // Résultat : [2026-01-15 14:30:45.123] [INF] [nkentseu.app] Server started
		 * @endcode
		 */


	} // namespace nkentseu


#endif // NKENTSEU_NKLOGGERFORMATTER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOGGERFORMATTER.H
// =============================================================================
// Ce fichier fournit le système de formatage des logs avec patterns.
// Il combine la simplicité du style spdlog avec la puissance de NkFormat.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Usage basique avec pattern par défaut
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>

	void LogStartup() {
		// Formateur avec pattern par défaut
		nkentseu::NkLoggerFormatter fmt;  // Pattern : "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v"

		// Message simple
		nkentseu::NkLogMessage msg(nkentseu::NkLogLevel::NK_INFO, "Application v2.1.0 started");
		nkentseu::NkString output = fmt.Format(msg);

		// Résultat : [2024-01-15 14:30:45.123] [INF] [default] [12345] -> Application v2.1.0 started
		Console::WriteLine(output);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Pattern personnalisé avec couleurs console
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>

	void SetupColoredLogging() {
		// Pattern avec marqueurs de couleur autour du niveau de log
		// %^ = début couleur, %$ = fin couleur, %L = niveau court
		nkentseu::NkLoggerFormatter coloredFmt("[%^%L%$] %v");

		nkentseu::NkLogMessage info(nkentseu::NkLogLevel::NK_INFO, "Server listening on port 8080");
		nkentseu::NkLogMessage error(nkentseu::NkLogLevel::NK_ERROR, "Database connection failed");

		// Formatage avec couleurs ANSI (pour terminaux compatibles)
		nkentseu::NkString infoOut = coloredFmt.Format(info, true);
		nkentseu::NkString errorOut = coloredFmt.Format(error, true);

		// Sortie : [INF] Server... (vert) et [ERR] Database... (rouge)
		Console::WriteColored(infoOut);
		Console::WriteColored(errorOut);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Formatage JSON pour ingestion dans système de métriques
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>
	#include <NKLogger/NkLogSink.h>

	class JsonLogSink : public nkentseu::NkLogSink {
	public:
		JsonLogSink() : m_formatter(nkentseu::NkLoggerFormatter::NK_JSON_PATTERN) {}

		void Emit(const nkentseu::NkLogMessage& message) override {
			// Formatage en JSON structuré
			nkentseu::NkString json = m_formatter.Format(message, false);  // Pas de couleurs en JSON

			// Écriture vers fichier ou réseau
			m_output.Write(json.CStr(), json.Length());
		}

	private:
		nkentseu::NkLoggerFormatter m_formatter;
		FileOutput m_output;
	};

	// Pattern JSON généré :
	// {"time":"2024-01-15T14:30:45.123Z","level":"info","thread":12345,
	//  "logger":"nkentseu.network","file":"server.cpp","line":42,
	//  "function":"HandleRequest","message":"Connection established"}
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Formatage positionnel avancé via NkFormat dans les littéraux
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>
	#include <NKCore/NkFormat.h>

	void LogWithDynamicContent() {
		// Le pattern peut contenir des littéraux formatés via NkFormat
		// Note : le parsing du pattern ne résout pas {0}, c'est du texte littéral
		// Pour du formatage dynamique, construire le message avec NkFormat d'abord

		int errorCode = 404;
		const char* resource = "/api/users";

		// Formatage du message avec NkFormat AVANT création du NkLogMessage
		nkentseu::NkString dynamicMsg = nkentseu::NkFormat(
			"Resource {0} not found (error {1:hex})",
			resource,
			errorCode
		);

		nkentseu::NkLogMessage msg(nkentseu::NkLogLevel::NK_WARN, dynamicMsg, "HttpHandler");
		nkentseu::NkLoggerFormatter fmt("[%L] %v");

		nkentseu::NkString output = fmt.Format(msg);
		// Résultat : [WRN] Resource /api/users not found (error 194)
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Changement dynamique de pattern à runtime (configuration)
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>
	#include <NKCore/NkConfig.h>

	class ConfigurableLogger {
	public:
		void Initialize() {
			// Lecture du pattern depuis configuration
			const char* pattern = nkentseu::core::Config::GetString(
				"logging.pattern",
				nkentseu::NkLoggerFormatter::NK_DEFAULT_PATTERN
			);

			m_formatter.SetPattern(pattern);

			// Lecture de l'option couleurs
			m_useColors = nkentseu::core::Config::GetBool(
				"logging.console.colors",
				true  // Défaut : couleurs activées
			);
		}

		void Log(nkentseu::NkLogLevel level, const char* message) {
			nkentseu::NkLogMessage msg(level, message);
			nkentseu::NkString output = m_formatter.Format(msg, m_useColors);
			Console::WriteLine(output);
		}

		// Permet de recharger la configuration sans redémarrage
		void ReloadConfig() {
			Initialize();  // Re-lit pattern et options
		}

	private:
		nkentseu::NkLoggerFormatter m_formatter;
		bool m_useColors = true;
	};
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Héritage pour formatage personnalisé
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>

	namespace myapp {
	namespace logging {

		// Formateur dérivé pour ajouter un préfixe d'application
		class AppFormatter : public nkentseu::NkLoggerFormatter {
		public:
			AppFormatter(const nkentseu::NkString& appPrefix)
				: nkentseu::NkLoggerFormatter()
				, m_appPrefix(appPrefix) {}

		protected:
			// Surcharge du formatage des tokens MESSAGE pour préfixage
			void FormatToken(
				const nkentseu::NkPatternToken& token,
				const nkentseu::NkLogMessage& message,
				bool useColors,
				nkentseu::NkString& result
			) override {
				if (token.type == nkentseu::NkPatternToken::Type::NK_MESSAGE) {
					// Ajout du préfixe d'application avant le message
					result += "[";
					result += m_appPrefix;
					result += "] ";
					result += message.message;
				} else {
					// Délégation au comportement par défaut pour les autres tokens
					nkentseu::NkLoggerFormatter::FormatToken(
						token, message, useColors, result);
				}
			}

		private:
			nkentseu::NkString m_appPrefix;
		};

	} // namespace logging
	} // namespace myapp

	// Usage :
	// myapp::logging::AppFormatter fmt("MyApp");
	// fmt.Format(msg) → "[MyApp] Message original"
*/


// -----------------------------------------------------------------------------
// Exemple 7 : Performance - Pré-allocation et réutilisation
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLoggerFormatter.h>

	void HighFrequencyLogging() {
		// Création unique du formateur (parsing pattern une seule fois)
		static nkentseu::NkLoggerFormatter fmt("[%L] %v");

		// Buffer de résultat réutilisé pour éviter les allocations
		static nkentseu::NkString outputBuffer;
		outputBuffer.Clear();

		// Boucle de logging haute fréquence
		for (int i = 0; i < 10000; ++i) {
			nkentseu::NkLogMessage msg(
				nkentseu::NkLogLevel::NK_DEBUG,
				nkentseu::NkString::Format("Iteration %d", i)
			);

			// Formatage dans le buffer réutilisé
			outputBuffer = fmt.Format(msg, false);

			// Traitement du résultat (écriture, envoi réseau, etc.)
			ProcessLogOutput(outputBuffer);
		}
	}

	// Bonnes pratiques :
	// - Réutiliser NkLoggerFormatter : parsing pattern = coût O(n) une fois
	// - Réutiliser NkString de sortie : évite allocations heap en boucle
	// - Désactiver couleurs si non nécessaires : évite concaténation de séquences ANSI
*/


// =============================================================================
// NOTES DE MAINTENANCE ET EXTENSIBILITÉ
// =============================================================================
/*
	1. AJOUT D'UN NOUVEAU TOKEN DE PATTERN :
	   - Ajouter l'entrée dans NkPatternToken::Type avec valeur unique
	   - Implémenter le cas correspondant dans ParsePattern() (détection du %)
	   - Implémenter le cas dans FormatToken() (extraction valeur depuis NkLogMessage)
	   - Documenter le nouveau token dans la table d'équivalence Doxygen
	   - Ajouter un test unitaire pour validation du parsing et du formatage

	2. PERSONNALISATION DES COULEURS :
	   - GetANSIColor() délègue à NkLogLevelToANSIColor() pour cohérence
	   - Pour des thèmes personnalisés : surcharger via classe dérivée
	   - Conserver les codes par défaut pour compatibilité cross-projet

	3. SUPPORT DE NOUVELLES PLATEFORMES DE CONSOLE :
	   - Les codes ANSI sont standards pour Unix/Linux/macOS
	   - Pour Windows : le backend console doit convertir ANSI → Win32 API
	   - Envisager un NkConsoleColorHelper pour abstraction multiplateforme

	4. OPTIMISATIONS FUTURES :
	   - Pré-compiler les patterns fréquents en bytecode pour formatage plus rapide
	   - Ajouter un mode "fast path" pour patterns simples (%v uniquement)
	   - Support du formatage positionnel {0} directement dans les patterns (fusion NkFormat)

	5. SÉCURITÉ DES PATTERNS UTILISATEUR :
	   - Le parser est robuste aux patterns mal formés (fallback littéral)
	   - Pas d'exécution de code : les tokens sont purement déclaratifs
	   - Limiter la longueur maximale de pattern pour éviter DoS par parsing
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================