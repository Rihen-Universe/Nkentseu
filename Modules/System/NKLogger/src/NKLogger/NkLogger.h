// =============================================================================
// NKLogger/NkLogger.h
// Classe principale du système de logging, similaire à spdlog.
//
// Design :
//  - API unifiée avec support multi-sink et formatage configurable
//  - Intégration du système NkFormat/NkFormatf pour tout le formatage
//  - Méthodes de logging typées : Log(level, "msg {0}", args...) via NkFormat
//  - Méthodes printf-style : Logf(level, "msg %d", args...) via NkFormatf
//  - Méthodes stream-style : Log(level, "message littéral")
//  - Synchronisation thread-safe via NKThreading/NkMutex
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

#ifndef NKENTSEU_NKLOGGER_H
#define NKENTSEU_NKLOGGER_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour le formatage variadique et les types de base.
	// Dépendances projet pour le logging, le formatage et la synchronisation.

	#include <cstdarg>

	#include "NKCore/NkTypes.h"
	#include "NKCore/NkTraits.h"
	#include "NKContainers/String/NkFormat.h"
	#include "NKContainers/String/NkString.h"
	#include "NKContainers/String/NkStringView.h"
	#include "NKContainers/Sequential/NkVector.h"
	#include "NKMemory/NkSharedPtr.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKThreading/NkMutex.h"
	#include "NKLogger/NkLoggerApi.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkSink.h"
	#include "NKLogger/NkLoggerFormatter.h"
	#include "NKLogger/NkLogMessage.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkLogger
		// DESCRIPTION : Classe principale de logging avec support multi-sink
		// ---------------------------------------------------------------------
		/**
		 * @class NkLogger
		 * @brief Classe principale du système de logging avec support multi-sink
		 * @ingroup LoggerComponents
		 *
		 * NkLogger est le point d'entrée central pour émettre des messages de log
		 * vers une ou plusieurs destinations (sinks) configurables.
		 *
		 * Fonctionnalités principales :
		 *  - Multi-sink : envoi simultané vers console, fichier, réseau, etc.
		 *  - Formatage flexible : patterns style spdlog ou formatage positionnel NkFormat
		 *  - Filtrage par niveau : contrôle granulaire de la verbosité
		 *  - Thread-safe : synchronisation interne pour usage concurrent
		 *  - Source tracking : capture automatique de fichier/ligne/fonction
		 *  - Sanitization : nettoyage UTF-8 pour sécurité des outputs
		 *
		 * Styles de logging supportés :
		 *  @b Formatage positionnel (recommandé) :
		 *  @code logger.Info("User {0} logged in from {1}", username, ip); @endcode
		 *
		 *  @b Style printf (compatibilité legacy) :
		 *  @code logger.Infof("User %s logged in from %s", username.CStr(), ip.CStr()); @endcode
		 *
		 *  @b Stream-style (messages littéraux) :
		 *  @code logger.Info("Simple message without formatting"); @endcode
		 *
		 * Architecture :
		 *  - Pattern Composite : agrège plusieurs NkISink pour diffusion
		 *  - Pattern Strategy : chaque sink définit son comportement d'écriture
		 *  - RAII : gestion automatique des ressources via smart pointers
		 *
		 * Thread-safety :
		 *  - Toutes les méthodes publiques sont thread-safe
		 *  - Synchronisation via nkentseu::threading::NkMutex interne
		 *  - Les sinks partagés doivent être thread-safe individuellement
		 *
		 * @note Cette classe est conçue pour être utilisée via des instances nommées
		 *       ou via le logger global (si fourni par le module).
		 *
		 * @example Usage basique
		 * @code
		 * nkentseu::NkLogger appLogger("MyApp");
		 * appLogger.AddSink(nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>());
		 * appLogger.SetLevel(nkentseu::NkLogLevel::NK_INFO);
		 *
		 * appLogger.Info("Application started");
		 * appLogger.Debug("Config loaded: {0} modules", moduleCount);
		 * appLogger.Error("Connection failed: code={0}", errorCode);
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkLogger {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerCtors Constructeurs de NkLogger
			 * @brief Initialisation avec configuration par défaut
			 */

			/**
			 * @brief Constructeur avec nom identifiant le logger
			 * @ingroup LoggerCtors
			 *
			 * @param name Nom lisible pour identification dans les logs
			 * @post Niveau par défaut : NK_INFO, formatter par défaut configuré
			 * @post Logger activé (m_Enabled = true), liste de sinks vide
			 * @note Le nom est utilisé dans le champ loggerName des NkLogMessage
			 *
			 * @example
			 * @code
			 * // Logger nommé pour un module spécifique
			 * nkentseu::NkLogger networkLogger("nkentseu.network");
			 * networkLogger.SetPattern("[%n] %v");  // Affiche le nom dans les logs
			 * @endcode
			 */
			explicit NkLogger(const NkString& name);

			/**
			 * @brief Destructeur : flush et cleanup des ressources
			 * @ingroup LoggerCtors
			 *
			 * @post Flush() appelé sur tous les sinks pour persistance des logs
			 * @post ClearSinks() appelé pour libération des références partagées
			 * @note Garantit qu'aucun log en buffer n'est perdu à la destruction
			 */
			~NkLogger();


			// -----------------------------------------------------------------
			// GESTION DES SINKS (DESTINATIONS DE LOG)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerSinks Gestion des Sinks
			 * @brief Ajout, suppression et interrogation des destinations de log
			 */

			/**
			 * @brief Ajoute un sink à la liste de destinations de ce logger
			 * @ingroup LoggerSinks
			 *
			 * @param sink Pointeur partagé vers le sink à ajouter
			 * @post Le sink est ajouté à m_Sinks et recevra tous les futurs messages
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Ownership partagé : le sink peut être utilisé par plusieurs loggers
			 *
			 * @example
			 * @code
			 * // Ajout d'un sink console pour développement
			 * auto consoleSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>();
			 * consoleSink->SetLevel(nkentseu::NkLogLevel::NK_DEBUG);
			 * logger.AddSink(consoleSink);
			 *
			 * // Ajout d'un sink fichier pour production
			 * auto fileSink = nkentseu::memory::MakeShared<nkentseu::NkFileSink>("app.log");
			 * fileSink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
			 * logger.AddSink(fileSink);
			 * @endcode
			 */
			void AddSink(memory::NkSharedPtr<NkISink> sink);

			/**
			 * @brief Supprime tous les sinks attachés à ce logger
			 * @ingroup LoggerSinks
			 *
			 * @post m_Sinks vidé, plus aucun message ne sera écrit jusqu'à nouvel ajout
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Les sinks partagés ne sont pas détruits (comptage de références)
			 *
			 * @example
			 * @code
			 * // Désactiver temporairement tout le logging
			 * logger.ClearSinks();
			 * // ... section silencieuse ...
			 * logger.AddSink(restoredSink);  // Réactiver
			 * @endcode
			 */
			void ClearSinks();

			/**
			 * @brief Obtient le nombre de sinks actuellement attachés
			 * @return Nombre de sinks dans la liste interne
			 * @ingroup LoggerSinks
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour debugging ou validation de configuration
			 *
			 * @example
			 * @code
			 * if (logger.GetSinkCount() == 0) {
			 *     nkentseu::NkLogger::Global().Warn("No sinks configured!");
			 * }
			 * @endcode
			 */
			usize GetSinkCount() const;


			// -----------------------------------------------------------------
			// CONFIGURATION DU FORMATAGE
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerFormat Configuration du Formatage
			 * @brief Définition du pattern ou formatter pour tous les sinks
			 */

			/**
			 * @brief Définit le formatter utilisé par tous les sinks de ce logger
			 * @ingroup LoggerFormat
			 *
			 * @param formatter Pointeur unique vers le nouveau formatter à adopter
			 * @post Transfert de propriété : le logger devient propriétaire du formatter
			 * @post Tous les sinks recevront ce formatter via propagation
			 * @note Thread-safe : synchronisé via m_Mutex
			 *
			 * @example
			 * @code
			 * // Formatter personnalisé avec pattern complexe
			 * auto fmt = nkentseu::memory::MakeUnique<nkentseu::NkLoggerFormatter>(
			 *     "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] %v"
			 * );
			 * logger.SetFormatter(std::move(fmt));
			 * @endcode
			 */
			void SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> formatter);

			/**
			 * @brief Définit le pattern de formatage via création interne de formatter
			 * @ingroup LoggerFormat
			 *
			 * @param pattern Chaîne de pattern style spdlog à parser
			 * @post Crée ou met à jour m_Formatter avec le nouveau pattern
			 * @post Le pattern est propagé aux sinks via SetPattern()
			 * @note Méthode de convenance : équivalent à SetFormatter(MakeUnique<NkLoggerFormatter>(pattern))
			 *
			 * @example
			 * @code
			 * // Pattern minimal pour production
			 * logger.SetPattern("%L: %v");
			 *
			 * // Pattern détaillé pour debugging
			 * logger.SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%s:%#] %v");
			 *
			 * // Pattern avec couleurs pour console
			 * logger.SetPattern("[%^%L%$] %v");
			 * @endcode
			 */
			void SetPattern(const NkString& pattern);


			// -----------------------------------------------------------------
			// CONFIGURATION DU NIVEAU DE LOG
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerLevel Configuration du Niveau de Log
			 * @brief Contrôle de la verbosité et filtrage des messages
			 */

			/**
			 * @brief Définit le niveau minimum de log pour ce logger
			 * @ingroup LoggerLevel
			 *
			 * @param level Niveau minimum : seuls les messages >= level seront traités
			 * @post m_Level mis à jour, filtrage appliqué dans ShouldLog()
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Niveau par défaut : NK_INFO (équilibre production/debugging)
			 *
			 * @example
			 * @code
			 * // Développement : tous les niveaux pour investigation
			 * #ifdef NKENTSEU_DEBUG
			 *     logger.SetLevel(nkentseu::NkLogLevel::NK_TRACE);
			 * #else
			 *     logger.SetLevel(nkentseu::NkLogLevel::NK_WARN);
			 * #endif
			 * @endcode
			 */
			void SetLevel(NkLogLevel level);

			/**
			 * @brief Obtient le niveau minimum de log configuré
			 * @return NkLogLevel représentant le seuil de filtrage actuel
			 * @ingroup LoggerLevel
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour affichage de configuration ou validation
			 *
			 * @example
			 * @code
			 * nkentseu::NkLogLevel current = logger.GetLevel();
			 * printf("Current log level: %s\n", nkentseu::NkLogLevelToString(current));
			 * @endcode
			 */
			NkLogLevel GetLevel() const;

			/**
			 * @brief Vérifie si un niveau de log donné devrait être traité
			 * @param level Niveau à évaluer
			 * @return true si level >= m_Level ET logger activé, false sinon
			 * @ingroup LoggerLevel
			 *
			 * @note Méthode clé pour le filtrage précoce : éviter tout traitement inutile
			 * @note Utilisée en début de chaque méthode Log* pour short-circuit
			 * @note Thread-safe : lecture safe de m_Level et m_Enabled
			 *
			 * @example
			 * @code
			 * // Filtrage manuel avant construction de message coûteux
			 * if (logger.ShouldLog(nkentseu::NkLogLevel::NK_DEBUG)) {
			 *     auto debugInfo = ExpensiveDebugComputation();
			 *     logger.Debug("Debug info: {0}", debugInfo);
			 * }
			 * @endcode
			 */
			bool ShouldLog(NkLogLevel level) const;


			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : FORMATAGE POSITIONNEL (API PRINCIPALE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerLogPositional Logging avec Formatage Positionnel
			 * @brief Méthodes typées utilisant NkFormat("{index:props}", args...)
			 *
			 * Style recommandé pour le nouveau code : type-safe, extensible,
			 * et compatible avec l'extension utilisateur via NkToString<T>.
			 *
			 * Syntaxe des placeholders :
			 *  - {0} : insère le premier argument avec formatage par défaut
			 *  - {1:hex} : insère le second argument en hexadécimal
			 *  - {2:.3} : insère le troisième avec 3 décimales (flottants)
			 *  - {{ et }} : échappement pour afficher { et } littéraux
			 *
			 * Voir NKCore/NkFormat.h pour la documentation complète des propriétés.
			 */

			/**
			 * @brief Log typé avec formatage positionnel et arguments variadiques
			 * @tparam Args Types des arguments à formater (déduits automatiquement)
			 * @param level Niveau de log pour ce message
			 * @param format Chaîne de format avec placeholders {index:props}
			 * @param args Pack d'arguments à insérer aux positions indiquées
			 * @ingroup LoggerLogPositional
			 *
			 * @note SFINAE : désactivé si sizeof...(Args) == 0 pour éviter l'ambiguïté
			 *       avec la méthode Log(level, const NkString&) stream-style
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Formatage via NkFormatIndexed() → NkFormat() unifié
			 * @note Source info : utilise les valeurs de Source() si configurées
			 *
			 * @example
			 * @code
			 * // Logging basique avec arguments
			 * logger.Info(nkentseu::NkLogLevel::NK_INFO, "User {0} logged in", username);
			 *
			 * // Formatage avancé avec propriétés
			 * logger.Debug("Address: 0x{0:hex8}, Count: {1:w=5}", ptr, count);
			 *
			 * // Échappement des accolades littérales
			 * logger.Info("JSON: {{\"id\": {0}}}", objectId);
			 * @endcode
			 */
			template<typename... Args>
			void Log(NkLogLevel level, NkStringView format, Args&&... args);

			/**
			 * @brief Log trace avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_TRACE, format, args...)
			 * @note Niveau le plus verbeux : pour debugging très détaillé
			 *
			 * @example
			 * @code
			 * logger.Trace("Function entry: {0}({1})", __func__, param);
			 * logger.Trace("Buffer dump: {0:bin}", byteValue);
			 * @endcode
			 */
			template<typename... Args>
			void Trace(NkStringView format, Args&&... args);

			/**
			 * @brief Log debug avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_DEBUG, format, args...)
			 * @note Pour informations de débogage technique
			 *
			 * @example
			 * @code
			 * logger.Debug("Cache hit ratio: {0:.2}%", hitRatio * 100.0);
			 * logger.Debug("Request headers: {0}", headersJson);
			 * @endcode
			 */
			template<typename... Args>
			void Debug(NkStringView format, Args&&... args);

			/**
			 * @brief Log info avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_INFO, format, args...)
			 * @note Pour messages informatifs normaux (niveau par défaut)
			 *
			 * @example
			 * @code
			 * logger.Info("Server started on port {0}", port);
			 * logger.Info("Loaded {0} plugins from {1}", pluginCount, pluginPath);
			 * @endcode
			 */
			template<typename... Args>
			void Info(NkStringView format, Args&&... args);

			/**
			 * @brief Log warning avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_WARN, format, args...)
			 * @note Pour avertissements non bloquants nécessitant attention
			 *
			 * @example
			 * @code
			 * logger.Warn("Deprecated API used: {0}, use {1} instead", oldApi, newApi);
			 * logger.Warn("Memory usage at {0:.1}%", memoryUsagePercent);
			 * @endcode
			 */
			template<typename... Args>
			void Warn(NkStringView format, Args&&... args);

			/**
			 * @brief Log error avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_ERROR, format, args...)
			 * @note Pour erreurs récupérables nécessitant intervention
			 *
			 * @example
			 * @code
			 * logger.Error("Failed to connect to {0}: {1}", host, errorMessage);
			 * logger.Error("Invalid config value for {0}: {1}", key, value);
			 * @endcode
			 */
			template<typename... Args>
			void Error(NkStringView format, Args&&... args);

			/**
			 * @brief Log critical avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_CRITICAL, format, args...)
			 * @note Pour erreurs critiques impactant une fonctionnalité majeure
			 *
			 * @example
			 * @code
			 * logger.Critical("Database connection lost: {0}", dbError);
			 * logger.Critical("Authentication service unreachable: {0}", serviceUrl);
			 * @endcode
			 */
			template<typename... Args>
			void Critical(NkStringView format, Args&&... args);

			/**
			 * @brief Log fatal avec formatage positionnel
			 * @tparam Args Types des arguments (déduits automatiquement)
			 * @param format Chaîne de format avec placeholders
			 * @param args Arguments à formater
			 * @ingroup LoggerLogPositional
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_FATAL, format, args...)
			 * @note Pour erreurs fatales entraînant l'arrêt de l'application
			 * @note Appelera Flush() sur tous les sinks avant terminaison
			 *
			 * @example
			 * @code
			 * logger.Fatal("Unrecoverable error: {0}", fatalReason);
			 * logger.Fatal("Assertion failed: {0} at {1}:{2}", condition, file, line);
			 * @endcode
			 */
			template<typename... Args>
			void Fatal(NkStringView format, Args&&... args);


			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : STYLE PRINTF (COMPATIBILITÉ LEGACY)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerLogPrintf Logging Style printf
			 * @brief Méthodes utilisant NkFormatf("%spec", args...) pour compatibilité
			 *
			 * Style recommandé uniquement pour :
			 *  - Migration de code existant utilisant printf
			 *  - Interopérabilité avec des APIs C attendent des format strings
			 *  - Cas où le formatage positionnel n'apporte pas de valeur ajoutée
			 *
			 * Pour le nouveau code, préférer le formatage positionnel (section précédente).
			 */

			/**
			 * @brief Log avec format string style printf et arguments variadiques
			 * @param level Niveau de log pour ce message
			 * @param format Chaîne de format style printf (%s, %d, %.2f, etc.)
			 * @param ... Arguments variadiques correspondant aux spécificateurs
			 * @ingroup LoggerLogPrintf
			 *
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Formatage via FormatString() → NkFormatf() unifié
			 * @note Source info : utilise les valeurs de Source() si configurées
			 * @note Thread-safe : synchronisé via m_Mutex pour l'écriture aux sinks
			 *
			 * @example
			 * @code
			 * logger.Logf(nkentseu::NkLogLevel::NK_INFO, "User %s logged in", username.CStr());
			 * logger.Logf(nkentseu::NkLogLevel::NK_DEBUG, "Address: 0x%08X, Count: %5d", ptr, count);
			 * @endcode
			 */
			template<typename... Args>
			void Logf(NkLogLevel level, const char* format, Args&&... args) {
				if (!ShouldLog(level)) return;
				
				// Utilisation directe de NkPrintf de NkFormat.h
				NkString formattedMessage = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				
				LogInternal(level, formattedMessage, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			/**
			 * @brief Log printf-style avec informations de source explicites
			 * @param level Niveau de log pour ce message
			 * @param file Chemin du fichier source (__FILE__)
			 * @param line Numéro de ligne source (__LINE__)
			 * @param func Nom de la fonction source (__FUNCTION__)
			 * @param format Chaîne de format style printf
			 * @param ... Arguments variadiques
			 * @ingroup LoggerLogPrintf
			 *
			 * @note Permet de spécifier manuellement les métadonnées de source
			 * @note Utile pour les wrappers de logging ou macros personnalisées
			 * @note Écrase temporairement les valeurs de Source() pour cet appel
			 *
			 * @example
			 * @code
			 * // Wrapper personnalisé avec métadonnées enrichies
			 * void MyLogWrapper(nkentseu::NkLogLevel level, const char* format, ...) {
			 *     va_list args;
			 *     va_start(args, format);
			 *     logger.Logf(level, __FILE__, __LINE__, __func__, format, args);
			 *     va_end(args);
			 * }
			 * @endcode
			 */
			template<typename... Args>
			void Logf(NkLogLevel level, const char* file, int line, const char* func, const char* format, Args&&... args) {
				if (!ShouldLog(level)) return;

				NkString formattedMessage;

				if constexpr (sizeof...(Args) == 0) {
					// Pas d'arguments : utiliser directement la chaîne
					formattedMessage = NkString(format);
				} else {
					formattedMessage = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				}
				
				LogInternal(level, formattedMessage, file, line, func);
			}

            // -------------------------------------------------------------------------
            // MÉTHODE : Méthodes *f (Tracef, Debugf, etc.)
            // DESCRIPTION : Wrappers typés vers Logf pour convenance d'usage
            // -------------------------------------------------------------------------
			template<typename... Args>
			void Tracef(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_TRACE)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			template<typename... Args>
			void Debugf(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_DEBUG)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			template<typename... Args>
			void Infof(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_INFO)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			template<typename... Args>
			void Warnf(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_WARN)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			template<typename... Args>
			void Errorf(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_ERROR)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			template<typename... Args>
			void Criticalf(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_CRITICAL)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			template<typename... Args>
			void Fatalf(const char* format, Args&&... args) {
				if (!ShouldLog(NkLogLevel::NK_FATAL)) return;
				NkString message = NkPrintf(NkStringView(format), std::forward<Args>(args)...);
				LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
			}

			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : STREAM-STYLE (MESSAGES LITTÉRAUX)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerLogStream Logging Stream-Style
			 * @brief Méthodes pour messages simples sans formatage
			 *
			 * Pour les cas où aucun formatage n'est nécessaire :
			 *  - Messages statiques prédéfinis
			 *  - Messages déjà formatés en amont
			 *  - Performance critique : éviter l'overhead du parsing de format
			 */

			/**
			 * @brief Log avec message littéral et niveau explicite
			 * @param level Niveau de log pour ce message
			 * @param message Chaîne NkString contenant le message déjà formaté
			 * @ingroup LoggerLogStream
			 *
			 * @note Aucun formatage appliqué : le message est utilisé tel quel
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Source info : utilise les valeurs de Source() si configurées
			 *
			 * @example
			 * @code
			 * logger.Log(nkentseu::NkLogLevel::NK_INFO, "Application shutdown initiated");
			 * logger.Log(nkentseu::NkLogLevel::NK_ERROR, preformattedErrorMessage);
			 * @endcode
			 */
			void Log(NkLogLevel level);

			/**
			 * @brief Log trace avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_TRACE, message)
			 * @note Pour debugging très détaillé sans formatage
			 */
			void Trace();

			/**
			 * @brief Log debug avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_DEBUG, message)
			 * @note Pour informations de débogage sans formatage
			 */
			void Debug();

			/**
			 * @brief Log info avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_INFO, message)
			 * @note Pour messages informatifs sans formatage
			 */
			void Info();

			/**
			 * @brief Log warning avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_WARN, message)
			 * @note Pour avertissements sans formatage
			 */
			void Warn();

			/**
			 * @brief Log error avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_ERROR, message)
			 * @note Pour erreurs sans formatage
			 */
			void Error();

			/**
			 * @brief Log critical avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_CRITICAL, message)
			 * @note Pour erreurs critiques sans formatage
			 */
			void Critical();

			/**
			 * @brief Log fatal avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_FATAL, message)
			 * @note Pour erreurs fatales sans formatage
			 */
			void Fatal();

			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : STREAM-STYLE (MESSAGES LITTÉRAUX)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerLogStream Logging Stream-Style
			 * @brief Méthodes pour messages simples sans formatage
			 *
			 * Pour les cas où aucun formatage n'est nécessaire :
			 *  - Messages statiques prédéfinis
			 *  - Messages déjà formatés en amont
			 *  - Performance critique : éviter l'overhead du parsing de format
			 */

			/**
			 * @brief Log avec message littéral et niveau explicite
			 * @param level Niveau de log pour ce message
			 * @param message Chaîne NkString contenant le message déjà formaté
			 * @ingroup LoggerLogStream
			 *
			 * @note Aucun formatage appliqué : le message est utilisé tel quel
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Source info : utilise les valeurs de Source() si configurées
			 *
			 * @example
			 * @code
			 * logger.Log(nkentseu::NkLogLevel::NK_INFO, "Application shutdown initiated");
			 * logger.Log(nkentseu::NkLogLevel::NK_ERROR, preformattedErrorMessage);
			 * @endcode
			 */
			void Logf(NkLogLevel level);

			/**
			 * @brief Log trace avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_TRACE, message)
			 * @note Pour debugging très détaillé sans formatage
			 */
			void Tracef();

			/**
			 * @brief Log debug avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_DEBUG, message)
			 * @note Pour informations de débogage sans formatage
			 */
			void Debugf();

			/**
			 * @brief Log info avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_INFO, message)
			 * @note Pour messages informatifs sans formatage
			 */
			void Infof();

			/**
			 * @brief Log warning avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_WARN, message)
			 * @note Pour avertissements sans formatage
			 */
			void Warnf();

			/**
			 * @brief Log error avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_ERROR, message)
			 * @note Pour erreurs sans formatage
			 */
			void Errorf();

			/**
			 * @brief Log critical avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_CRITICAL, message)
			 * @note Pour erreurs critiques sans formatage
			 */
			void Criticalf();

			/**
			 * @brief Log fatal avec message littéral
			 * @param message Chaîne NkString contenant le message
			 * @ingroup LoggerLogStream
			 *
			 * @note Équivalent à Log(NkLogLevel::NK_FATAL, message)
			 * @note Pour erreurs fatales sans formatage
			 */
			void Fatalf();


			// -----------------------------------------------------------------
			// MÉTHODES DE LOGGING : CAPTURE DE SOURCE CODE
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerSource Capture des Métadonnées de Source
			 * @brief Configuration manuelle de fichier/ligne/fonction pour les logs
			 *
			 * Pattern fluide pour associer des métadonnées de source aux logs :
			 * @code logger.Source(__FILE__, __LINE__, __func__).Info("msg {0}", arg); @endcode
			 *
			 * Les métadonnées sont stockées temporairement et appliquées
			 * au prochain appel de Log*, puis réinitialisées.
			 */

			/**
			 * @brief Configure les métadonnées de source pour le prochain log
			 * @param sourceFile Chemin du fichier source (__FILE__) ou nullptr
			 * @param sourceLine Numéro de ligne source (__LINE__) ou 0
			 * @param functionName Nom de la fonction (__FUNCTION__) ou nullptr
			 * @return Référence vers ce logger pour chaînage fluide
			 * @ingroup LoggerSource
			 *
			 * @note Les valeurs nullptr/0 sont converties en chaînes vides/0 internes
			 * @note Thread-safe : écriture protégée via m_Mutex (mutable)
			 * @note Les métadonnées sont consommées au prochain appel Log* et réinitialisées
			 *
			 * @example
			 * @code
			 * // Usage typique via macros
			 * #define MY_LOG_INFO(fmt, ...) \
			 *     logger.Source(__FILE__, __LINE__, __func__).Info(fmt, ##__VA_ARGS__)
			 *
			 * MY_LOG_INFO("Processing item {0}", itemId);
			 * // Les métadonnées __FILE__, __LINE__, __func__ sont attachées au message
			 * @endcode
			 */
			virtual NkLogger& Source(const char* sourceFile = nullptr, uint32 sourceLine = 0, const char* functionName = nullptr);


			// -----------------------------------------------------------------
			// UTILITAIRES ET MÉTHODES DE CONTRÔLE
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerUtils Méthodes Utilitaires
			 * @brief Opérations de contrôle, flush et interrogation du logger
			 */

			/**
			 * @brief Force l'écriture immédiate des buffers de tous les sinks
			 * @ingroup LoggerUtils
			 *
			 * @post Flush() appelé sur chaque sink de m_Sinks
			 * @note Critique pour : crash recovery, debugging temps-réel, audits
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Appelée automatiquement dans le destructeur ~NkLogger()
			 *
			 * @example
			 * @code
			 * // Avant une opération critique : garantir la persistance des logs
			 * logger.Flush();
			 * PerformCriticalOperation();
			 *
			 * // Après un message fatal : flush avant terminaison
			 * logger.Fatal("Unrecoverable error");
			 * logger.Flush();  // Optionnel : déjà fait dans ~NkLogger()
			 * @endcode
			 */
			virtual void Flush();

			/**
			 * @brief Obtient le nom identifiant ce logger
			 * @return Référence constante vers la chaîne de nom interne
			 * @ingroup LoggerUtils
			 *
			 * @note Retour par référence : pas de copie, lecture efficace
			 * @note Thread-safe : lecture safe de m_Name (immutable après construction)
			 * @note Utile pour logging de debug ou identification dans les erreurs
			 *
			 * @example
			 * @code
			 * // Logging d'erreur avec identification du logger
			 * if (!operationSucceeded) {
			 *     nkentseu::NkLogger::Global().Error(
			 *         "Logger '{0}' reported failure", logger.GetName());
			 * }
			 * @endcode
			 */
			const NkString& GetName() const;

			/**
			 * @brief Vérifie si ce logger est actuellement activé
			 * @return true si le logger traite les messages, false s'il les ignore
			 * @ingroup LoggerUtils
			 *
			 * @note Retourne la valeur de m_Enabled : true par défaut
			 * @note Thread-safe : lecture safe si pas de modification concurrente
			 * @note Un logger désactivé ignore tous les appels Log* (no-op immédiat)
			 *
			 * @example
			 * @code
			 * // Activation conditionnelle pour debugging
			 * if (debugMode && logger.IsEnabled()) {
			 *     logger.Debug("Debug mode active, verbose logging enabled");
			 * }
			 * @endcode
			 */
			bool IsEnabled() const;

			/**
			 * @brief Active ou désactive temporairement ce logger
			 * @param enabled true pour activer, false pour désactiver
			 * @ingroup LoggerUtils
			 *
			 * @note État par défaut : true (activé)
			 * @note Un logger désactivé ignore tous les appels Log* (no-op)
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour : debugging conditionnel, feature flags, tests A/B
			 *
			 * @example
			 * @code
			 * // Désactiver les logs verbeux en production
			 * #ifndef NKENTSEU_DEBUG
			 *     logger.SetEnabled(false);
			 * #endif
			 *
			 * // Activer temporairement pour investigation
			 * void EnableDebugLogging() {
			 *     logger.SetEnabled(true);
			 *     logger.Info("Debug logging enabled for troubleshooting");
			 * }
			 * @endcode
			 */
			void SetEnabled(bool enabled);


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PROTÉGÉS (POUR HÉRITAGE)
			// -----------------------------------------------------------------
		protected:


			// -----------------------------------------------------------------
			// MÉTHODES PROTÉGÉES POUR EXTENSION
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerProtected Méthodes Protégées
			 * @brief Points d'extension pour les classes dérivées
			 */

			/**
			 * @brief Définit le nom du logger (pour classes dérivées)
			 * @param name Nouveau nom à associer
			 * @ingroup LoggerProtected
			 *
			 * @note Permet aux dérivées de modifier le nom après construction
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Usage interne : préférer le constructeur pour initialisation
			 *
			 * @example
			 * @code
			 * class DerivedLogger : public nkentseu::NkLogger {
			 * public:
			 *     DerivedLogger() : nkentseu::NkLogger("base") {
			 *         SetName("derived");  // Override du nom
			 *     }
			 * };
			 * @endcode
			 */
			void SetName(const NkString& name);


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PROTÉGÉES (ÉTAT PARTAGÉ AVEC LES DÉRIVÉES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerProtectedMembers Membres Protégés
			 * @brief État interne accessible aux classes dérivées
			 *
			 * Thread-safety :
			 *  - Ces membres sont protégés par m_Mutex pour accès concurrent
			 *  - Les dérivées doivent acquérir m_Mutex avant modification
			 *  - Lecture sans lock possible si immutable après construction
			 */

			/// @brief Mutex pour synchronisation thread-safe des opérations
			/// @ingroup LoggerProtectedMembers
			/// @note mutable : permet modification dans les méthodes const (GetName, etc.)
			mutable threading::NkMutex m_Mutex;

			/// @brief Liste des sinks attachés à ce logger
			/// @ingroup LoggerProtectedMembers
			/// @note Ownership partagé via NkSharedPtr : sinks réutilisables
			NkVector<memory::NkSharedPtr<NkISink>> m_Sinks;

			/// @brief Formatter principal pour ce logger (optionnel)
			/// @ingroup LoggerProtectedMembers
			/// @note Propagé aux sinks via SetPattern()/SetFormatter()
			memory::NkUniquePtr<NkLoggerFormatter> m_Formatter;


			// -----------------------------------------------------------------
			// SECTION 5 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// MÉTHODES PRIVÉES D'IMPLÉMENTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerPrivate Méthodes Privées
			 * @brief Fonctions internes non exposées dans l'API publique
			 */

			/**
			 * @brief Implémentation centrale de logging avec métadonnées de source
			 * @param level Niveau de log du message
			 * @param message Contenu textuel déjà formaté du message
			 * @param sourceFile Chemin du fichier source (optionnel, peut être nullptr)
			 * @param sourceLine Numéro de ligne source (0 = non spécifié)
			 * @param functionName Nom de la fonction émettrice (optionnel)
			 * @ingroup LoggerPrivate
			 *
			 * @note Point d'entrée unique pour tous les styles de logging
			 * @note Applique la sanitization UTF-8 pour sécurité des outputs
			 * @note Envoie le NkLogMessage à tous les sinks via boucle thread-safe
			 * @note Gère la propagation du formatter aux sinks si nécessaire
			 *
			 * @warning Ne pas appeler directement : passer par les méthodes Log* publiques
			 */
			void LogInternal(NkLogLevel level, const NkString& message, const char* sourceFile = nullptr, uint32 sourceLine = 0, const char* functionName = nullptr);
        
        protected:
			/**
			 * @brief Formatage variadique style printf via NkFormatf
			 * @param format Chaîne de format style printf
			 * @param args va_list des arguments à formater
			 * @return NkString contenant le message formaté
			 * @ingroup LoggerPrivate
			 *
			 * @note Délègue à nkentseu::NkFormatf() pour cohérence avec le reste du projet
			 * @note Gère les erreurs vsnprintf via retour de chaîne vide
			 * @note Thread-safe : aucune mutation d'état partagé
			 *
			 * @example
			 * @code
			 * va_list args;
			 * va_start(args, format);
			 * nkentseu::NkString msg = FormatString(format, args);
			 * va_end(args);
			 * // msg contient le résultat formaté
			 * @endcode
			 */
			NkString FormatString(const char* format, va_list args);

        private:
			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT INTERNE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup LoggerMembers Membres de Données
			 * @brief État interne du logger (non exposé publiquement)
			 */

			/// @brief Nom identifiant ce logger dans les messages
			/// @ingroup LoggerMembers
			/// @note Immutable après construction (sauf via SetName protégé)
			NkString m_Name;

			/// @brief Niveau minimum de log pour filtrage des messages
			/// @ingroup LoggerMembers
			/// @note Défaut : NK_INFO, modifiable via SetLevel()
			NkLogLevel m_Level;

			/// @brief Indicateur d'activation/désactivation du logger
			/// @ingroup LoggerMembers
			/// @note Défaut : true, modifiable via SetEnabled()
			bool m_Enabled;

			/// @brief Métadonnées de source temporaires pour le prochain log
			/// @ingroup LoggerMembers
			/// @note Remplies via Source(), consommées dans LogInternal(), puis réinitialisées
			NkString m_SourceFile;

			/// @brief Numéro de ligne source temporaire
			/// @ingroup LoggerMembers
			NkString m_FunctionName;

			/// @brief Nom de fonction source temporaire
			/// @ingroup LoggerMembers
			uint32 m_SourceLine;


		}; // class NkLogger

        // =============================================================================
        // IMPLÉMENTATIONS INLINE DES MÉTHODES TEMPLATES
        // =============================================================================
        // Ces définitions doivent être dans le header car ce sont des templates.
        // Elles utilisent NkFormatIndexed de NKCore/NkFormat.h pour le formatage positionnel.

        // -------------------------------------------------------------------------
        // MÉTHODE : Log (template principal avec formatage positionnel)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Log(NkLogLevel level, NkStringView format, Args&&... args) {
            // Filtrage précoce : éviter tout formatage si le message sera ignoré
            if (!ShouldLog(level)) {
                return;
            }
        
			NkString formattedMessage;
			if constexpr (sizeof...(Args) == 0) {
				// Pas d'arguments : utiliser directement la chaîne
				formattedMessage = NkString(format);
			} else {
				formattedMessage = NkFormat(format, traits::NkForward<Args>(args)...);
			}

            // Émission via LogInternal avec métadonnées de source courantes
            LogInternal(level, formattedMessage, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Trace (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Trace(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_TRACE, format, traits::NkForward<Args>(args)...);
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Debug (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Debug(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_DEBUG, format, traits::NkForward<Args>(args)...);
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Info (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Info(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_INFO, format, traits::NkForward<Args>(args)...);
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Warn (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Warn(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_WARN, format, traits::NkForward<Args>(args)...);
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Error (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Error(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_ERROR, format, traits::NkForward<Args>(args)...);
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Critical (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Critical(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_CRITICAL, format, traits::NkForward<Args>(args)...);
        }

        // -------------------------------------------------------------------------
        // MÉTHODE : Fatal (template wrapper vers Log)
        // -------------------------------------------------------------------------
        template<typename... Args>
        inline void NkLogger::Fatal(NkStringView format, Args&&... args) {
            Log(NkLogLevel::NK_FATAL, format, traits::NkForward<Args>(args)...);
        }

		// ---------------------------------------------------------------------
		// MACROS DE LOGGING PRATIQUES (OPTIONNELLES)
		// ---------------------------------------------------------------------
		/**
		 * @defgroup LoggerMacros Macros de Logging
		 * @brief Macros de convenance pour logging avec métadonnées de source automatiques
		 *
		 * Ces macros simplifient l'usage de NkLogger en capturant automatiquement
		 * __FILE__, __LINE__ et __FUNCTION__ pour chaque appel.
		 *
		 * Style recommandé : utiliser les méthodes templated Log(level, format, args...)
		 * directement si possible, pour un meilleur debugging et type-safety.
		 *
		 * Les macros sont fournies pour :
		 *  - Compatibilité avec du code existant
		 *  - Préférence personnelle pour le style printf
		 *  - Cas où la verbosité des templates est indésirable
		 */

		/**
		 * @brief Macro pour log trace avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf (format string + args)
		 * @ingroup LoggerMacros
		 *
		 * @note Filtrage précoce via ShouldLog() pour éviter l'overhead de formatage
		 * @note Capture automatique de __FILE__, __LINE__, __FUNCTION__
		 * @note Utilise Logf() style printf : préférer Log() positionnel pour le nouveau code
		 *
		 * @example
		 * @code
		 * NK_LOG_TRACE(&logger, "Function entry with param={0}", paramValue);
		 * // Équivalent à :
		 * if (logger.ShouldLog(nkentseu::NkLogLevel::NK_TRACE))
		 *     logger.Logf(nkentseu::NkLogLevel::NK_TRACE, __FILE__, __LINE__, __func__, "Function entry with param={0}", paramValue);
		 * @endcode
		 */
		#define NK_LOG_TRACE(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_TRACE)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour log debug avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf
		 * @ingroup LoggerMacros
		 *
		 * @note Filtrage précoce via ShouldLog()
		 * @note Capture automatique des métadonnées de source
		 */
		#define NK_LOG_DEBUG(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_DEBUG)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour log info avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf
		 * @ingroup LoggerMacros
		 */
		#define NK_LOG_INFO(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_INFO)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour log warning avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf
		 * @ingroup LoggerMacros
		 */
		#define NK_LOG_WARN(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_WARN)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour log error avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf
		 * @ingroup LoggerMacros
		 */
		#define NK_LOG_ERROR(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_ERROR)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour log critical avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf
		 * @ingroup LoggerMacros
		 */
		#define NK_LOG_CRITICAL(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_CRITICAL)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_CRITICAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour log fatal avec métadonnées de source automatiques
		 * @param logger Pointeur ou référence vers le logger cible
		 * @param ... Arguments pour NkFormatf
		 * @ingroup LoggerMacros
		 */
		#define NK_LOG_FATAL(logger, ...) \
			if ((logger)->ShouldLog(nkentseu::NkLogLevel::NK_FATAL)) \
				(logger)->Logf(nkentseu::NkLogLevel::NK_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

		/**
		 * @brief Macro pour flush explicite des buffers de log
		 * @param logger Pointeur ou référence vers le logger cible
		 * @ingroup LoggerMacros
		 *
		 * @note Utile avant des opérations critiques ou après des logs importants
		 * @note Appelée automatiquement dans le destructeur ~NkLogger()
		 */
		#define NK_LOG_FLUSH(logger) (logger)->Flush()


	} // namespace nkentseu


#endif // NKENTSEU_NKLOGGER_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOGGER.H
// =============================================================================
// Ce fichier fournit la classe principale de logging avec support multi-sink.
// Il combine formatage positionnel, style printf et stream-style pour flexibilité.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration basique d'un logger avec sinks multiples
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>
	#include <NKLogger/NkConsoleSink.h>
	#include <NKLogger/NkFileSink.h>

	void SetupApplicationLogging() {
		// Création d'un logger nommé pour le module principal
		nkentseu::NkLogger appLogger("MyApp.Main");

		// Ajout d'un sink console pour développement (debug et plus)
		auto consoleSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>();
		consoleSink->SetLevel(nkentseu::NkLogLevel::NK_DEBUG);
		consoleSink->SetPattern("[%^%L%$] %v");  // Avec couleurs
		appLogger.AddSink(consoleSink);

		// Ajout d'un sink fichier pour production (info et plus)
		auto fileSink = nkentseu::memory::MakeShared<nkentseu::NkFileSink>("app.log");
		fileSink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
		fileSink->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] %v");
		appLogger.AddSink(fileSink);

		// Niveau global du logger : contrôle ce qui atteint les sinks
		appLogger.SetLevel(nkentseu::NkLogLevel::NK_DEBUG);

		// Usage : les messages sont envoyés aux deux sinks si niveau suffisant
		appLogger.Info("Application started v{0}.{1}", MAJOR_VERSION, MINOR_VERSION);
		appLogger.Debug("Config loaded from {0}", configPath);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Logging avec formatage positionnel (style recommandé)
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>

	void ProcessUserRequest(const nkentseu::NkString& userId, int requestId) {
		static nkentseu::NkLogger logger("RequestHandler");

		// Formatage positionnel : type-safe et extensible
		logger.Info("Processing request {0} for user {1}", requestId, userId);

		// Formatage avancé avec propriétés
		logger.Debug("Request metadata: id={0:hex8}, user={1:upper}, timestamp={2:.3}",
			requestId, userId, currentTime);

		// Échappement des accolades pour JSON ou templates
		logger.Debug("Payload: {{\"id\": {0}, \"action\": \"{1}\"}}", requestId, action);

		// Filtrage précoce pour éviter un calcul coûteux
		if (logger.ShouldLog(nkentseu::NkLogLevel::NK_TRACE)) {
			auto debugInfo = ExpensiveDebugComputation();
			logger.Trace("Debug dump: {0}", debugInfo);
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Logging style printf (compatibilité legacy)
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>

	void LegacyIntegration(const char* legacyBuffer, size_t length) {
		static nkentseu::NkLogger logger("LegacyAdapter");

		// Style printf pour interop avec du code C existant
		logger.Logf(nkentseu::NkLogLevel::NK_INFO,
			"Received %zu bytes from legacy system: %.100s",
			length, legacyBuffer);

		// Avec métadonnées de source manuelles (via macro ou appel direct)
		logger.Logf(nkentseu::NkLogLevel::NK_WARN,
			__FILE__, __LINE__, __func__,
			"Deprecated API called with buffer=%p, length=%zu",
			legacyBuffer, length);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Logging stream-style pour messages simples
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>

	void SimpleStatusUpdates() {
		static nkentseu::NkLogger logger("StatusMonitor");

		// Messages statiques sans formatage : plus performant
		logger.Info("Health check passed");
		logger.Warn("Memory usage above threshold");
		logger.Error("Connection timeout to database");

		// Messages déjà formatés en amont
		nkentseu::NkString preformatted = BuildComplexStatusMessage();
		logger.Info(preformatted);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Pattern fluide avec Source() pour métadonnées enrichies
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>

	// Macro personnalisée pour logging avec métadonnées enrichies
	#define LOG_WITH_CONTEXT(logger, level, fmt, ...) \
		(logger).Source(__FILE__, __LINE__, __func__).Log(level, fmt, ##__VA_ARGS__)

	void ContextualLogging() {
		static nkentseu::NkLogger logger("ContextualModule");

		// Usage de la macro pour capturer automatiquement la source
		LOG_WITH_CONTEXT(logger, nkentseu::NkLogLevel::NK_INFO,
			"Operation {0} completed in {1:.3}ms",
			operationName, elapsedMs);

		// Usage direct du pattern fluide
		logger.Source("custom_file.cpp", 42, "CustomFunction")
			.Debug("Custom source metadata attached to this message");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Gestion dynamique du niveau de log et activation
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>
	#include <NKCore/NkConfig.h>

	class ConfigurableLogger {
	public:
		void Initialize() {
			// Lecture de la configuration depuis fichier/env/CLI
			auto levelStr = nkentseu::core::Config::GetString("logging.level", "info");
			auto level = nkentseu::NkLogLevelFromString(levelStr);

			m_logger.SetLevel(level);
			m_logger.SetEnabled(nkentseu::core::Config::GetBool("logging.enabled", true));

			// Pattern configurable
			auto pattern = nkentseu::core::Config::GetString(
				"logging.pattern", nkentseu::NkLoggerFormatter::NK_DEFAULT_PATTERN);
			m_logger.SetPattern(pattern);
		}

		// Rechargement à chaud de la configuration
		void ReloadConfig() {
			Initialize();  // Re-lit et applique la nouvelle configuration
			m_logger.Info("Logging configuration reloaded");
		}

		void LogEvent(const nkentseu::NkString& event, int code) {
			// Filtrage précoce pour éviter tout overhead si logger désactivé
			if (!m_logger.IsEnabled()) {
				return;
			}

			// Logging conditionnel par niveau
			if (code >= 400) {
				m_logger.Error("Event {0} failed with code {1}", event, code);
			} else if (code >= 300) {
				m_logger.Warn("Event {0} warning: code {1}", event, code);
			} else {
				m_logger.Info("Event {0} succeeded (code {1})", event, code);
			}
		}

	private:
		nkentseu::NkLogger m_logger{"ConfigurableModule"};
	};
*/


// -----------------------------------------------------------------------------
// Exemple 7 : Logger global singleton (pattern optionnel)
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkLogger.h>
	#include <NKMemory/NkSharedPtr.h>

	namespace nkentseu {

		// Déclaration d'un logger global accessible partout
		NkLogger& GlobalLogger() {
			static NkLogger instance("Global");
			static bool initialized = false;

			if (!initialized) {
				// Configuration initiale du logger global
				auto consoleSink = memory::MakeShared<NkConsoleSink>();
				consoleSink->SetLevel(NkLogLevel::NK_INFO);
				instance.AddSink(consoleSink);

				initialized = true;
			}

			return instance;
		}

	} // namespace nkentseu

	// Usage dans n'importe quel fichier du projet :
	// nkentseu::GlobalLogger().Info("Global log message");
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. CHOIX DU STYLE DE LOGGING :
	   - Formatage positionnel (Log(level, "msg {0}", args)) : recommandé pour le nouveau code
	     → Type-safe, extensible via NkToString<T>, meilleure expérience IDE
	   - Style printf (Logf(level, "msg %s", arg)) : pour migration legacy ou interop C
	     → Attention aux mismatches format/args, moins de vérification compile-time
	   - Stream-style (Log(level, "message")) : pour messages statiques ou pré-formatés
	     → Performance maximale, aucun parsing de format

	2. FILTRAGE PRÉCOCE POUR PERFORMANCE :
	   - Toujours utiliser ShouldLog(level) avant un calcul coûteux
	   - Les méthodes Log* font déjà ce filtrage, mais pas pour la construction d'args
	   - Exemple : if (logger.ShouldLog(DEBUG)) { auto debug = Expensive(); logger.Debug(...); }

	3. THREAD-SAFETY ET SYNCHRONISATION :
	   - Toutes les méthodes publiques sont thread-safe via m_Mutex
	   - Les sinks partagés doivent être thread-safe individuellement
	   - Pour performance : envisager un logger asynchrone avec queue de messages

	4. SANITIZATION UTF-8 ET SÉCURITÉ :
	   - LogInternal() applique NkSanitizeLogText() pour éviter les injections
	   - Caractères non-printables → espaces, accents → équivalents ASCII
	   - Pour logs binaires : utiliser un sink dédié avec encodage hex/base64

	5. GESTION DES MÉTADONNÉES DE SOURCE :
	   - Source() est fluide : les métadonnées sont consommées au prochain Log*
	   - Ne pas chaîner plusieurs Source() sans Log* intermédiaire
	   - Pour macros : capturer __FILE__/__LINE__/__func__ au point d'appel

	6. EXTENSIBILITÉ FUTURES :
	   - Ajouter des méthodes virtuelles avec implémentation par défaut pour rétrocompatibilité
	   - Pour breaking changes : créer NkLoggerV2 et migrer progressivement
	   - Documenter clairement le contrat de chaque méthode dans les commentaires Doxygen

	7. TESTING DES LOGGERS :
	   - Utiliser NkMemorySink pour capturer les outputs en tests unitaires
	   - Tester les cas limites : logger désactivé, sinks vides, niveaux filtrés
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================