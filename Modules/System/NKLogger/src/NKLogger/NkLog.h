// =============================================================================
// NKLogger/NkLog.h
// Logger par défaut avec API fluide et instance singleton pour usage simplifié.
//
// Design :
//  - Singleton thread-safe via Meyer's singleton (static local)
//  - API fluide chaînable : logger.Named("app").Level(DEBUG).Info("msg {0}", arg)
//  - Héritage de NkLogger : réutilisation de toute l'infrastructure de logging
//  - Intégration de NkFormat pour formatage positionnel : "msg {0:hex}", value
//  - Macro de convenance `logger` pour accès rapide avec métadonnées de source
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

#ifndef NKENTSEU_NKLOG_H
#define NKENTSEU_NKLOG_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour les types de base et le formatage.
	// Dépendances projet pour le logger de base et le formatage positionnel.

	#include "NKCore/NkTypes.h"
	#include "NKCore/NkTraits.h"
	#include "NKContainers/String/NkFormat.h"
	#include "NKContainers/String/NkString.h"
	#include "NKContainers/String/NkStringView.h"
	#include "NKLogger/NkLogger.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLoggerFormatter.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkLog
		// DESCRIPTION : Singleton de logger avec API fluide chaînable
		// ---------------------------------------------------------------------
		/**
		 * @class NkLog
		 * @brief Logger par défaut singleton avec API fluide pour usage simplifié
		 * @ingroup LoggerComponents
		 *
		 * NkLog fournit un point d'accès global au système de logging avec :
		 *  - Singleton thread-safe : instance unique accessible partout
		 *  - API fluide : chaînage de configuration avant chaque log
		 *  - Formatage positionnel : "msg {0:hex}", value via NkFormat
		 *  - Macro de convenance : `logger.Info("msg {0}", arg)` avec source auto
		 *
		 * Architecture :
		 *  - Hérite de NkLogger : réutilisation complète de l'infrastructure
		 *  - Singleton Meyer : static local dans Instance(), thread-safe en C++11+
		 *  - Configuration lazy : Initialize() optionnelle, defaults raisonnables
		 *
		 * Thread-safety :
		 *  - Instance() : thread-safe via static local (C++11 magic statics)
		 *  - Toutes les méthodes : thread-safe via NkLogger::m_Mutex
		 *  - API fluide : chaque appel retourne *this pour chaînage safe
		 *
		 * @note Cette classe est conçue pour être utilisée via la macro `logger`
		 *       ou via NkLog::Instance() pour un contrôle explicite.
		 *
		 * @example Usage basique via macro
		 * @code
		 * #include <NKLogger/NkLog.h>
		 *
		 * // Initialisation optionnelle au startup
		 * nkentseu::NkLog::Initialize("MyApp", "[%L] %v", nkentseu::NkLogLevel::NK_DEBUG);
		 *
		 * // Logging simplifié avec métadonnées de source automatiques
		 * logger.Info("Application started");
		 * logger.Debug("Config loaded: {0} modules", moduleCount);
		 * logger.Error("Connection failed: code={0}", errorCode);
		 *
		 * // Nettoyage optionnel au shutdown
		 * nkentseu::NkLog::Shutdown();
		 * @endcode
		 *
		 * @example API fluide avec chaînage de configuration
		 * @code
		 * // Configuration temporaire pour un bloc de logs
		 * logger.Named("NetworkModule")
		 *       .Level(nkentseu::NkLogLevel::NK_TRACE)
		 *       .Info("Packet received: {0} bytes from {1}", size, sourceIp);
		 *
		 * // Retour à la configuration globale pour le log suivant
		 * logger.Info("Back to global config");
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkLog : public NkLogger {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// MÉTHODES STATIQUES (GESTION DU SINGLETON)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogSingleton Gestion du Singleton
			 * @brief Méthodes statiques pour accès et contrôle de l'instance unique
			 */

			/**
			 * @brief Obtient l'instance singleton du logger par défaut
			 * @return Référence vers l'instance unique de NkLog
			 * @ingroup LogSingleton
			 *
			 * @note Implémentation Meyer's singleton : static local dans la fonction
			 * @note Thread-safe en C++11+ : initialisation garantie une seule fois
			 * @note Lazy initialization : l'instance n'est créée qu'au premier appel
			 *
			 * @example
			 * @code
			 * // Accès explicite à l'instance singleton
			 * nkentseu::NkLog::Instance().Info("Direct singleton access");
			 *
			 * // Équivalent via macro (recommandé)
			 * logger.Info("Macro-based access");
			 * @endcode
			 */
			static NkLog& Instance();

			/**
			 * @brief Initialise le logger par défaut avec configuration personnalisée
			 * @param name Nom identifiant le logger (défaut: "default")
			 * @param pattern Pattern de formatage style spdlog (défaut: NK_DETAILED_PATTERN)
			 * @param level Niveau minimum de log (défaut: NK_INFO)
			 * @ingroup LogSingleton
			 *
			 * @note Méthode optionnelle : des defaults raisonnables sont appliqués si non appelée
			 * @note Thread-safe : synchronisé via le mutex interne de NkLogger
			 * @note Idempotente : peut être appelée multiples fois sans effet secondaire
			 *
			 * @example
			 * @code
			 * // Initialisation au startup de l'application
			 * nkentseu::NkLog::Initialize(
			 *     "MyApp",  // Nom du logger
			 *     "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] %v",  // Pattern avec couleurs
			 *     nkentseu::NkLogLevel::NK_DEBUG  // Niveau verbose pour dev
			 * );
			 * @endcode
			 */
			static void Initialize(
				const NkString& name = "default",
				const NkString& pattern = NkLoggerFormatter::NK_DETAILED_PATTERN,
				NkLogLevel level = NkLogLevel::NK_INFO
			);

			/**
			 * @brief Nettoie et prépare la destruction du logger par défaut
			 * @ingroup LogSingleton
			 *
			 * @post Flush() appelé sur tous les sinks pour persistance des logs
			 * @post ClearSinks() appelé pour libération des références partagées
			 * @note Méthode optionnelle : le destructeur gère automatiquement le cleanup
			 * @note Utile pour un shutdown propre avec garantie de persistance des logs
			 *
			 * @example
			 * @code
			 * // Shutdown propre avant terminaison de l'application
			 * #ifdef NKENTSEU_DEBUG
			 *     nkentseu::NkLog::Shutdown();  // Flush explicite en debug
			 * #endif
			 * // ~NkLog() gère le cleanup automatiquement sinon
			 * @endcode
			 */
			static void Shutdown();


			// -----------------------------------------------------------------
			// API FLUIDE (MÉTHODES CHAÎNABLES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogFluent API Fluide Chaînable
			 * @brief Méthodes retournant *this pour configuration inline avant logging
			 *
			 * Pattern fluide pour configuration temporaire :
			 * @code logger.Named("Module").Level(DEBUG).Info("msg {0}", arg); @endcode
			 *
			 * Chaque méthode modifie l'état du logger et retourne *this,
			 * permettant un chaînage naturel et lisible.
			 *
			 * @note Thread-safety : chaque appel acquiert le mutex interne
			 * @note État persistant : les modifications restent actives pour les appels suivants
			 * @note Pour configuration temporaire : sauvegarder/restaurer l'état si nécessaire
			 */

			/**
			 * @brief Définit le nom du logger et retourne *this pour chaînage
			 * @param name Nouveau nom identifiant le logger
			 * @return Référence vers ce logger pour chaînage fluide
			 * @ingroup LogFluent
			 *
			 * @note Modifie m_Name via SetName() protégé de NkLogger
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour identifier la source des logs dans des applications multi-modules
			 *
			 * @example
			 * @code
			 * // Logging avec nom de module dynamique
			 * logger.Named("Network").Info("Connected to {0}", serverAddress);
			 * logger.Named("Database").Debug("Query executed in {0:.3}ms", elapsedMs);
			 *
			 * // Le nom reste actif pour les appels suivants
			 * logger.Info("This log also shows 'Database' as logger name");
			 * @endcode
			 */
			NkLog& Named(const NkString& name);

			/**
			 * @brief Définit le niveau minimum de log et retourne *this pour chaînage
			 * @param level Nouveau seuil de filtrage des messages
			 * @return Référence vers ce logger pour chaînage fluide
			 * @ingroup LogFluent
			 *
			 * @note Modifie m_Level via SetLevel() de NkLogger
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour augmenter temporairement la verbosité pour debugging
			 *
			 * @example
			 * @code
			 * // Augmenter temporairement la verbosité pour investigation
			 * logger.Level(nkentseu::NkLogLevel::NK_TRACE)
			 *       .Debug("Verbose debug info: {0}", detailedState);
			 *
			 * // Retour au niveau global pour le log suivant
			 * logger.Info("Back to normal verbosity");
			 * @endcode
			 */
			NkLog& Level(NkLogLevel level);

			/**
			 * @brief Définit le pattern de formatage et retourne *this pour chaînage
			 * @param pattern Nouveau pattern style spdlog à appliquer
			 * @return Référence vers ce logger pour chaînage fluide
			 * @ingroup LogFluent
			 *
			 * @note Modifie le formatter via SetPattern() de NkLogger
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour adapter le format des logs selon le contexte
			 *
			 * @example
			 * @code
			 * // Pattern compact pour logs fréquents
			 * logger.Pattern("%L: %v")
			 *       .Info("Frequent status update");
			 *
			 * // Pattern détaillé pour erreurs critiques
			 * logger.Pattern("[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%s:%#] %v")
			 *       .Error("Critical failure with full context");
			 * @endcode
			 */
			NkLog& Pattern(const NkString& pattern);

			/**
			 * @brief Configure les métadonnées de source et retourne *this pour chaînage
			 * @param sourceFile Chemin du fichier source (__FILE__) ou nullptr
			 * @param sourceLine Numéro de ligne source (__LINE__) ou 0
			 * @param functionName Nom de la fonction (__FUNCTION__) ou nullptr
			 * @return Référence vers ce logger pour chaînage fluide
			 * @ingroup LogFluent
			 *
			 * @note Override de NkLogger::Source() avec retour fluide
			 * @note Thread-safe : synchronisé via m_Mutex (mutable dans la base)
			 * @note Les métadonnées sont consommées au prochain appel Log* et réinitialisées
			 *
			 * @example
			 * @code
			 * // Usage typique via la macro `logger` qui inclut Source() automatiquement
			 * // Équivalent manuel :
			 * logger.Source(__FILE__, __LINE__, __func__)
			 *       .Info("Manual source metadata attachment");
			 * @endcode
			 */
			NkLog& Source(const char* sourceFile = nullptr, uint32 sourceLine = 0, const char* functionName = nullptr) override;


			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : FORMATAGE POSITIONNEL (HÉRITÉES + TEMPLATES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogPositional Logging avec Formatage Positionnel
			 * @brief Méthodes héritées de NkLogger avec support NkFormat("{0:props}", args...)
			 *
			 * Ces méthodes sont héritées de NkLogger et pleinement fonctionnelles :
			 *  - Log(level, "format {0}", args...) : méthode principale
			 *  - Trace/Debug/Info/Warn/Error/Critical/Fatal(format, args...) : wrappers typés
			 *
			 * Voir NkLogger.h pour la documentation complète des signatures templates.
			 *
			 * @note Les implémentations templates sont inline dans ce header
			 * @note Utilisent NkFormatIndexed de NKCore/NkFormat.h pour le formatage
			 */

			// Les déclarations templates suivantes sont héritées de NkLogger :
			// template<typename... Args> void Log(NkLogLevel, NkStringView, Args&&...);
			// template<typename... Args> void Trace(NkStringView, Args&&...);
			// ... etc. pour Debug, Info, Warn, Error, Critical, Fatal

			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : STYLE PRINTF ET STREAM (HÉRITÉES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogOtherStyles Autres Styles de Logging (Hérités)
			 * @brief Méthodes printf-style et stream-style héritées de NkLogger
			 *
			 * Style printf (compatibilité legacy) :
			 *  - Logf(level, "format %s", arg...) : via NkFormatf
			 *  - Tracef/Debugf/Infof/etc. : wrappers typés
			 *
			 * Style stream (messages littéraux) :
			 *  - Log(level, "message") : aucun formatage appliqué
			 *  - Trace/Debug/Info/etc. : wrappers typés
			 *
			 * Voir NkLogger.h pour la documentation complète de ces méthodes.
			 */


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION DU SINGLETON)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR (PRIVÉS POUR SINGLETON)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogCtors Constructeurs Privés (Singleton)
			 * @brief Contrôle de l'instanciation unique via constructeurs privés
			 */

			/**
			 * @brief Constructeur privé : initialisation avec configuration par défaut
			 * @ingroup LogCtors
			 *
			 * @param name Nom initial du logger (défaut: "default")
			 * @post Sinks console et fichier ajoutés par défaut
			 * @post Niveau NK_INFO, pattern NK_NKENTSEU_PATTERN configurés
			 * @note Appelable uniquement par Instance() via static local
			 */
			explicit NkLog(const NkString& name = "default");

			/**
			 * @brief Destructeur privé : flush et cleanup automatique
			 * @ingroup LogCtors
			 *
			 * @post Flush() appelé pour persistance des logs en buffer
			 * @note Garantit qu'aucun log n'est perdu à la destruction du singleton
			 */
			~NkLog();


			// -----------------------------------------------------------------
			// RÈGLES DE COPIE (SUPPRIMÉES POUR SINGLETON)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogRules Règles de Copie Supprimées
			 * @brief Prévention de la duplication de l'instance singleton
			 */

			/// @brief Constructeur de copie supprimé : singleton non-copiable
			/// @ingroup LogRules
			NkLog(const NkLog&) = delete;

			/// @brief Opérateur d'affectation supprimé : singleton non-affectable
			/// @ingroup LogRules
			NkLog& operator=(const NkLog&) = delete;


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES STATIQUES PRIVÉES
			// -----------------------------------------------------------------
			/**
			 * @defgroup LogStaticMembers Membres Statiques Privés
			 * @brief État global du singleton (non exposé publiquement)
			 */

			/// @brief Indicateur d'initialisation explicite via Initialize()
			/// @ingroup LogStaticMembers
			/// @note true si Initialize() a été appelé, false sinon (defaults appliqués)
			static bool s_Initialized;


		}; // class NkLog


	} // namespace nkentseu


	// -------------------------------------------------------------------------
	// MACRO DE CONVENANCE POUR ACCÈS RAPIDE AU LOGGER GLOBAL
	// -------------------------------------------------------------------------
	/**
	 * @def logger
	 * @brief Macro d'accès rapide au singleton NkLog avec métadonnées de source automatiques
	 * @ingroup LoggerMacros
	 *
	 * Cette macro simplifie l'usage du logger global en :
	 *  - Accédant à l'instance singleton via NkLog::Instance()
	 *  - Attachant automatiquement __FILE__, __LINE__, __FUNCTION__ via Source()
	 *  - Retournant une référence pour chaînage fluide immédiat
	 *
	 * @note Thread-safe : Instance() utilise static local (C++11 magic statics)
	 * @note Usage typique : logger.Info("msg {0}", arg) → métadonnées incluses automatiquement
	 *
	 * @example
	 * @code
	 * // Usage basique avec métadonnées de source automatiques
	 * logger.Info("User {0} logged in", username);
	 * // → Log message avec __FILE__, __LINE__, __func__ attachés
	 *
	 * // Chaînage fluide après la macro
	 * logger.Named("NetworkModule").Debug("Packet size: {0} bytes", size);
	 *
	 * // Échappement pour usage dans des expressions complexes
	 * (void)logger;  // Pour éviter les warnings "unused variable" si nécessaire
	 * @endcode
	 *
	 * @warning La macro s'expand en une expression : éviter dans des contextes nécessitant un statement
	 * @warning Les métadonnées de source sont consommées au prochain appel Log* : ne pas chaîner plusieurs Source()
	 */
	#define logger ::nkentseu::NkLog::Instance().Source(__FILE__, __LINE__, __func__)


	/**
	 * @def logger_src
	 * @brief Alias de la macro logger pour explicitation (optionnel)
	 * @ingroup LoggerMacros
	 *
	 * @note Strictement équivalent à `logger` : fourni pour lisibilité dans du code explicite
	 * @note Usage : logger_src.Info("msg") → identique à logger.Info("msg")
	 *
	 * @example
	 * @code
	 * // Pour clarifier l'intention dans du code très explicite
	 * logger_src.Error("Explicit source-attached error message");
	 * @endcode
	 */
	#define logger_src logger


#endif // NKENTSEU_NKLOG_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOG.H
// =============================================================================
// Ce fichier fournit le logger singleton global avec API fluide.
// Il combine simplicité d'usage et puissance du système de logging complet.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Initialisation et usage basique via macro
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>

	int main() {
		// Initialisation optionnelle au startup
		nkentseu::NkLog::Initialize(
			"MyApp",
			"[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] %v",  // Pattern avec couleurs
			nkentseu::NkLogLevel::NK_DEBUG  // Verbose pour développement
		);

		// Logging simplifié via macro avec métadonnées de source automatiques
		logger.Info("Application started v{0}.{1}", MAJOR_VERSION, MINOR_VERSION);
		logger.Debug("Config loaded from {0}", configPath);
		logger.Warn("Deprecated API used in module {0}", moduleName);

		// Shutdown propre optionnel
		nkentseu::NkLog::Shutdown();
		return 0;
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : API fluide avec chaînage de configuration temporaire
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>

	void ProcessNetworkRequest(const nkentseu::NkString& endpoint, int statusCode) {
		// Configuration temporaire pour ce bloc de logs
		logger.Named("Network")
		      .Level(nkentseu::NkLogLevel::NK_TRACE)
		      .Pattern("[%L] %v")  // Pattern compact pour logs fréquents
		      .Trace("Request to {0} started", endpoint);

		// ... traitement de la requête ...

		if (statusCode >= 400) {
			logger.Named("Network")  // Nom persistant depuis le chaînage précédent
			      .Level(nkentseu::NkLogLevel::NK_ERROR)
			      .Error("Request failed: {0} returned {1}", endpoint, statusCode);
		}

		// Retour implicite à la configuration globale pour le log suivant
		logger.Info("Request processing completed");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Formatage positionnel avancé avec NkFormat
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>

	void LogSystemMetrics(float cpuUsage, nk_uint64 memoryBytes, const nkentseu::NkString& hostname) {
		// Formatage positionnel avec propriétés
		logger.Info(
			"Metrics for {0}: CPU={1:.1}%, RAM={2:w=10} bytes",
			hostname,
			cpuUsage,
			memoryBytes
		);

		// Formatage hexadécimal avec padding pour addresses mémoire
		logger.Debug(
			"Memory map: stack=0x{0:hex16}, heap=0x{1:hex16}",
			stackPointer,
			heapPointer
		);

		// Échappement des accolades pour JSON ou templates
		logger.Debug(
			"Payload: {{\"host\": \"{0}\", \"cpu\": {1:.2}}}",
			hostname,
			cpuUsage
		);

		// Combinaison de types avec formatage spécifique
		logger.Trace(
			"Flags: {0:bin}, Priority: {1:w=2}, Tag: {2:upper}",
			flagBits,
			priority,
			tagString
		);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Logging conditionnel avec filtrage précoce
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>

	void ExpensiveDebugOperation() {
		// Filtrage précoce manuel pour éviter un calcul coûteux
		if (logger.ShouldLog(nkentseu::NkLogLevel::NK_TRACE)) {
			auto debugData = ComputeExpensiveDebugSnapshot();
			logger.Trace("Debug snapshot: {0}", debugData);
		}

		// Les méthodes Log* font déjà ce filtrage automatiquement :
		logger.Debug("This message is skipped if level > DEBUG");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Intégration avec macros de logging personnalisées
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>

	// Macro personnalisée pour logging avec contexte métier
	#define LOG_WITH_USER(logger, user, level, fmt, ...) \
		(logger).Named("User:" + user).Log(level, "[User:{0}] " fmt, user, ##__VA_ARGS__)

	void UserAction(const nkentseu::NkString& userId, const nkentseu::NkString& action) {
		// Logging avec contexte utilisateur injecté automatiquement
		LOG_WITH_USER(logger, userId, nkentseu::NkLogLevel::NK_INFO,
			"Performed action: {0}", action);

		// Équivalent explicite sans macro
		logger.Named("User:" + userId)
		      .Info("[User:{0}] Performed action: {1}", userId, action);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Gestion dynamique de la configuration via fichier
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>
	#include <NKCore/NkConfig.h>

	void ApplyLoggingConfigFromFile() {
		// Lecture de la configuration depuis fichier/env/CLI
		auto config = nkentseu::core::Config::GetSection("logging");

		// Application dynamique des paramètres
		nkentseu::NkLog::Initialize(
			config.GetString("name", "default"),
			config.GetString("pattern", nkentseu::NkLoggerFormatter::NK_DEFAULT_PATTERN),
			nkentseu::NkLogLevelFromString(config.GetString("level", "info"))
		);

		// Logging de confirmation
		logger.Info("Logging configuration applied: name={0}, level={1}",
			nkentseu::NkLog::Instance().GetName(),
			nkentseu::NkLogLevelToString(nkentseu::NkLog::Instance().GetLevel()));
	}
*/


// -----------------------------------------------------------------------------
// Exemple 7 : Testing unitaire avec capture des logs
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLog.h>
	#include <NKLogger/Sinks/NkMemorySink.h>  // Sink fictif pour tests
	#include <gtest/gtest.h>

	TEST(LoggerTest, InfoLevelLogs) {
		// Setup : sink mémoire pour capture des outputs
		auto memorySink = nkentseu::memory::MakeShared<nkentseu::NkMemorySink>();
		auto& globalLogger = nkentseu::NkLog::Instance();
		globalLogger.AddSink(memorySink);
		globalLogger.SetLevel(nkentseu::NkLogLevel::NK_INFO);

		// Execution : logging via macro
		logger.Info("Test message with {0}", 42);

		// Verification : inspection du sink mémoire
		const auto& captured = memorySink->GetMessages();
		ASSERT_EQ(captured.Size(), 1);
		EXPECT_EQ(captured[0].message, "Test message with 42");
		EXPECT_EQ(captured[0].level, nkentseu::NkLogLevel::NK_INFO);

		// Cleanup : retirer le sink de test
		globalLogger.ClearSinks();
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. SINGLETON ET THREAD-SAFETY :
	   - Instance() utilise static local : thread-safe en C++11+ (magic statics)
	   - Toutes les méthodes héritées de NkLogger sont thread-safe via m_Mutex
	   - L'API fluide retourne *this : safe car chaque appel acquiert le mutex

	2. INITIALISATION LAZY VS EXPLICITE :
	   - Defaults raisonnables appliqués si Initialize() non appelée
	   - Initialize() est idempotente : peut être appelée multiples fois
	   - Pour contrôle total : appeler Initialize() explicitement au startup

	3. MACRO `logger` ET MÉTADONNÉES DE SOURCE :
	   - La macro attache automatiquement __FILE__, __LINE__, __func__
	   - Ces métadonnées sont consommées au prochain appel Log* puis réinitialisées
	   - Ne pas chaîner plusieurs Source() sans Log* intermédiaire

	4. FORMATAGE POSITIONNEL VIA NKFORMAT :
	   - Préférer le style positionnel "msg {0}" pour le nouveau code
	   - Type-safe, extensible via NkToString<T>, meilleure expérience IDE
	   - Voir NKCore/NkFormat.h pour la documentation complète des propriétés

	5. PERFORMANCE ET FILTRAGE PRÉCOCE :
	   - Toutes les méthodes Log* font un filtrage précoce via ShouldLog()
	   - Pour calculs coûteux avant logging : vérifier ShouldLog() manuellement
	   - Exemple : if (logger.ShouldLog(DEBUG)) { auto data = Expensive(); logger.Debug(...); }

	6. EXTENSIBILITÉ FUTURES :
	   - NkLog hérite de NkLogger : toute extension de la base profite au singleton
	   - Pour features spécifiques au singleton : ajouter des méthodes dans NkLog uniquement
	   - Documenter clairement le contrat de chaque méthode dans les commentaires Doxygen

	7. TESTING ET MOCKING :
	   - Utiliser NkMemorySink (ou équivalent) pour capturer les logs en tests
	   - Tester les cas limites : logger non-initialisé, sinks vides, niveaux filtrés
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================