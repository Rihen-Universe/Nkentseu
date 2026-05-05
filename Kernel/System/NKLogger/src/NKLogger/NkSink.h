// =============================================================================
// NKLogger/NkSink.h
// Interface de base pour tous les sinks (destinations) de logging.
//
// Design :
//  - Interface abstraite NkISink pour polymorphisme des destinations de log
//  - Méthodes pures virtuelles pour l'implémentation obligatoire (Log, Flush)
//  - Méthodes virtuelles avec implémentation par défaut pour configuration
//  - Gestion de formatter via NkUniquePtr pour ownership clair
//  - Filtrage par niveau de log intégré au niveau du sink
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKSINK_H
#define NKENTSEU_NKSINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour la gestion mémoire et les chaînes.
	// Dépendances projet pour les types de log, le formatage et les smart pointers.

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKMemory/NkSharedPtr.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLoggerFormatter.h"
	#include "NKLogger/NkLoggerApi.h"
    #include "NKThreading/NkMutex.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkISink
		// DESCRIPTION : Interface abstraite pour toutes les destinations de log
		// ---------------------------------------------------------------------
		/**
		 * @class NkISink
		 * @brief Interface de base pour tous les sinks (destinations) de logging
		 * @ingroup LoggerComponents
		 *
		 * Un "sink" représente une destination de sortie pour les messages de log :
		 *  - Console (stdout/stderr avec couleurs)
		 *  - Fichier texte ou binaire
		 *  - Socket réseau (TCP/UDP, syslog)
		 *  - Base de données ou système de métriques
		 *  - Buffer mémoire pour tests ou agrégation
		 *
		 * Architecture :
		 *  - Pattern Strategy : chaque implémentation définit son comportement d'écriture
		 *  - Filtrage intégré : chaque sink peut avoir son propre niveau minimum
		 *  - Formatage configurable : formatter indépendant par sink
		 *  - Thread-safety : à garantir par chaque implémentation concrète
		 *
		 * Cycle de vie :
		 *  - Construction : configuration initiale (nom, niveau, pattern)
		 *  - Log() : appelé pour chaque message passant le filtre
		 *  - Flush() : appelé pour forcer l'écriture des buffers (optionnel)
		 *  - Destruction : cleanup des ressources (fichiers, sockets, etc.)
		 *
		 * @note Cette classe est conçue pour être héritée, pas instanciée directement.
		 * @note Les implémentations concrètes doivent être thread-safe si utilisées
		 *       depuis multiples threads (cas typique des loggers asynchrones).
		 *
		 * @example Pattern d'implémentation minimal
		 * @code
		 * class ConsoleSink : public nkentseu::NkISink {
		 * public:
		 *     void Log(const nkentseu::NkLogMessage& message) override {
		 *         if (!ShouldLog(message.level)) return;
		 *         auto formatted = GetFormatter()->Format(message);
		 *         fprintf(stderr, "%s\n", formatted.CStr());
		 *     }
		 *     void Flush() override { fflush(stderr); }
		 *     // ... autres overrides requis ...
		 * };
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkISink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// DESTRUCTEUR VIRTUEL
			// -----------------------------------------------------------------
			/**
			 * @defgroup SinkDtor Destructeur
			 * @brief Nettoyage polymorphique des ressources du sink
			 */

			/**
			 * @brief Destructeur virtuel par défaut
			 * @ingroup SinkDtor
			 *
			 * @note = default : génération automatique par le compilateur
			 * @note Virtuel : permet la destruction correcte via pointeur de base
			 * @note Les classes dérivées doivent libérer leurs ressources dans leur propre destructeur
			 *
			 * @example
			 * @code
			 * NkISink* sink = new ConsoleSink();
			 * delete sink;  // Appelle ~ConsoleSink() puis ~NkISink() grâce au virtuel
			 * @endcode
			 */
			virtual ~NkISink();


			// -----------------------------------------------------------------
			// MÉTHODES VIRTUELLES PURES (OBLIGATOIRES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup SinkPureMethods Méthodes Pures Virtuelles
			 * @brief Interface minimale à implémenter pour tout sink concret
			 *
			 * Ces méthodes définissent le contrat de base pour toute destination de log.
			 * Toute classe dérivée DOIT les implémenter sous peine d'erreur de compilation.
			 */

			/**
			 * @brief Écrit un message de log vers la destination du sink
			 * @param message Message de log contenant toutes les métadonnées
			 * @ingroup SinkPureMethods
			 *
			 * @note Cette méthode est le point d'entrée principal pour l'écriture des logs
			 * @note Doit respecter le niveau de log configuré via ShouldLog()
			 * @note Doit appliquer le formatter courant si configuré
			 * @note Thread-safety : à garantir par l'implémentation concrète
			 *
			 * @par Comportement attendu :
			 *  1. Vérifier ShouldLog(message.level) pour filtrage
			 *  2. Formater le message via GetFormatter()->Format() si disponible
			 *  3. Écrire le résultat vers la destination (console, fichier, réseau, etc.)
			 *  4. Gérer les erreurs d'E/O de manière robuste (ne pas crasher l'app)
			 *
			 * @example
			 * @code
			 * void FileSink::Log(const NkLogMessage& message) {
			 *     if (!ShouldLog(message.level)) return;
			 *
			 *     NkString output;
			 *     if (auto* fmt = GetFormatter()) {
			 *         output = fmt->Format(message);
			 *     } else {
			 *         output = message.message;  // Fallback minimal
			 *     }
			 *
			 *     // Écriture avec gestion d'erreur
			 *     if (fwrite(output.CStr(), 1, output.Length(), mFile) != output.Length()) {
			 *         // Gérer l'erreur d'écriture (logging vers stderr en fallback ?)
			 *     }
			 * }
			 * @endcode
			 */
			virtual void Log(const NkLogMessage& message) = 0;

			/**
			 * @brief Force l'écriture immédiate des données en buffer
			 * @ingroup SinkPureMethods
			 *
			 * @note Cette méthode est appelée pour garantir la persistance des logs
			 * @note Critique pour : crash recovery, debugging temps-réel, audits
			 * @note Peut être un no-op pour les sinks sans buffering (ex: console directe)
			 * @note Thread-safety : doit être safe pour appel concurrent avec Log()
			 *
			 * @par Cas d'usage typiques :
			 *  - Avant un shutdown propre de l'application
			 *  - Après un message de niveau NK_CRITICAL ou NK_FATAL
			 *  - Périodiquement via un timer pour éviter la perte de logs en buffer
			 *
			 * @example
			 * @code
			 * void FileSink::Flush() {
			 *     if (mFile != nullptr) {
			 *         fflush(mFile);  // Force l'écriture du buffer C stdio
			 *         #ifdef NKENTSEU_PLATFORM_WINDOWS
			 *             _commit(_fileno(mFile));  // Force flush OS sur Windows
			 *         #else
			 *             fsync(fileno(mFile));     // Force flush OS sur POSIX
			 *         #endif
			 *     }
			 * }
			 * @endcode
			 */
			virtual void Flush() = 0;

			/**
			 * @brief Définit le formatter utilisé par ce sink
			 * @param formatter Pointeur unique vers le nouveau formatter à adopter
			 * @ingroup SinkPureMethods
			 *
			 * @note Transfert de propriété : le sink devient propriétaire du formatter
			 * @note Le formatter précédent est automatiquement détruit (RAII via NkUniquePtr)
			 * @note Thread-safety : synchronisation requise si appelé pendant Log()
			 *
			 * @example
			 * @code
			 * // Création d'un formatter avec pattern personnalisé
			 * auto formatter = memory::MakeUnique<NkLoggerFormatter>("[%L] %v");
			 *
			 * // Transfert au sink : le sink prend possession
			 * sink->SetFormatter(std::move(formatter));
			 *
			 * // formatter est maintenant nullptr, le sink gère la durée de vie
			 * @endcode
			 */
			virtual void SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> formatter) = 0;

			/**
			 * @brief Définit le pattern de formatage via création interne de formatter
			 * @param pattern Chaîne de pattern style spdlog à parser
			 * @ingroup SinkPureMethods
			 *
			 * @note Méthode de convenance : crée un NkLoggerFormatter interne avec le pattern
			 * @note Équivalent à SetFormatter(MakeUnique<NkLoggerFormatter>(pattern))
			 * @note Thread-safety : synchronisation requise si appelé pendant Log()
			 *
			 * @example
			 * @code
			 * // Pattern simple : niveau + message
			 * sink->SetPattern("%L: %v");
			 *
			 * // Pattern détaillé avec timestamp et thread
			 * sink->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%L] [%t] %v");
			 *
			 * // Pattern avec couleurs console
			 * sink->SetPattern("[%^%L%$] %v");
			 * @endcode
			 */
			virtual void SetPattern(const NkString& pattern) = 0;

			/**
			 * @brief Obtient le formatter courant utilisé par ce sink
			 * @return Pointeur brut vers le formatter, ou nullptr si non configuré
			 * @ingroup SinkPureMethods
			 *
			 * @note Retourne un pointeur brut non-possédé : ne pas delete
			 * @note Le pointeur peut devenir invalide après SetFormatter() ou destruction du sink
			 * @note Thread-safety : lecture safe si pas de modification concurrente
			 *
			 * @example
			 * @code
			 * if (auto* fmt = sink->GetFormatter()) {
			 *     // Utilisation temporaire du formatter
			 *     NkString preview = fmt->Format(sampleMessage);
			 *     Logger::Log(NkLogLevel::NK_DEBUG, "Preview: %s", preview.CStr());
			 * }
			 * @endcode
			 */
			virtual NkLoggerFormatter* GetFormatter() const = 0;

			/**
			 * @brief Obtient le pattern de formatage courant
			 * @return Chaîne NkString contenant le pattern, ou vide si non configuré
			 * @ingroup SinkPureMethods
			 *
			 * @note Retour par valeur : copie du pattern interne (safe pour usage prolongé)
			 * @note Utile pour sauvegarde de configuration ou affichage de debug
			 * @note Thread-safety : lecture safe si pas de modification concurrente
			 *
			 * @example
			 * @code
			 * // Sauvegarde de la configuration courante
			 * NkString currentPattern = sink->GetPattern();
			 * Config::SetString("logging.sink.pattern", currentPattern.CStr());
			 * @endcode
			 */
			virtual NkString GetPattern() const = 0;


			// -----------------------------------------------------------------
			// MÉTHODES VIRTUELLES AVEC IMPLÉMENTATION PAR DÉFAUT (OPTIONNELLES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup SinkOptionalMethods Méthodes Optionnelles
			 * @brief Méthodes de configuration avec comportement par défaut
			 *
			 * Ces méthodes ont une implémentation par défaut dans NkISink.
			 * Les classes dérivées peuvent les surcharger pour un comportement personnalisé,
			 * ou les utiliser telles quelles si le comportement par défaut convient.
			 */

			/**
			 * @brief Définit le niveau minimum de log pour ce sink
			 * @param level Niveau minimum : seuls les messages >= level seront loggés
			 * @ingroup SinkOptionalMethods
			 *
			 * @note Niveau par défaut : NkLogLevel::NK_TRACE (le plus verbeux)
			 * @note Filtrage appliqué dans ShouldLog() : level >= m_Level
			 * @note Thread-safety : synchronisation requise si appelé pendant Log()
			 *
			 * @example
			 * @code
			 * // Sink console : uniquement warnings et plus graves
			 * consoleSink->SetLevel(NkLogLevel::NK_WARN);
			 *
			 * // Sink fichier debug : tous les niveaux pour investigation
			 * debugFileSink->SetLevel(NkLogLevel::NK_TRACE);
			 *
			 * // Sink production : uniquement erreurs critiques
			 * prodSink->SetLevel(NkLogLevel::NK_ERROR);
			 * @endcode
			 */
			virtual void SetLevel(NkLogLevel level);

			/**
			 * @brief Obtient le niveau minimum de log configuré pour ce sink
			 * @return NkLogLevel représentant le seuil de filtrage actuel
			 * @ingroup SinkOptionalMethods
			 *
			 * @note Retourne la valeur de m_Level : NK_TRACE par défaut
			 * @note Utile pour affichage de configuration ou validation de setup
			 * @note Thread-safety : lecture safe si pas de modification concurrente
			 *
			 * @example
			 * @code
			 * // Vérification de configuration au startup
			 * auto level = sink->GetLevel();
			 * if (level > NkLogLevel::NK_INFO) {
			 *     Logger::Log(NkLogLevel::NK_WARN,
			 *         "Sink '%s' configured with high threshold: %s",
			 *         sink->GetName().CStr(),
			 *         NkLogLevelToString(level));
			 * }
			 * @endcode
			 */
			virtual NkLogLevel GetLevel() const;

			/**
			 * @brief Vérifie si un niveau de log donné devrait être traité par ce sink
			 * @param level Niveau de log à évaluer
			 * @return true si level >= m_Level, false sinon (message filtré)
			 * @ingroup SinkOptionalMethods
			 *
			 * @note Méthode clé pour le filtrage précoce : éviter le formatage inutile
			 * @note Utilisée en début de Log() pour short-circuit les messages non pertinents
			 * @note Thread-safety : lecture safe de m_Level (atomic si nécessaire en dérivée)
			 *
			 * @example
			 * @code
			 * void MySink::Log(const NkLogMessage& message) {
			 *     // Filtrage précoce : éviter tout travail si message ignoré
			 *     if (!ShouldLog(message.level)) {
			 *         return;  // Message filtré, aucun traitement supplémentaire
			 *     }
			 *
			 *     // ... traitement coûteux uniquement pour messages pertinents ...
			 *     auto formatted = GetFormatter()->Format(message);
			 *     WriteToDestination(formatted);
			 * }
			 * @endcode
			 */
			virtual bool ShouldLog(NkLogLevel level) const;

			/**
			 * @brief Active ou désactive temporairement ce sink sans le supprimer
			 * @param enabled true pour activer, false pour désactiver
			 * @ingroup SinkOptionalMethods
			 *
			 * @note État par défaut : true (activé)
			 * @note Un sink désactivé ignore tous les appels à Log() (no-op)
			 * @note Utile pour : debugging conditionnel, feature flags, tests A/B
			 * @note Thread-safety : synchronisation requise si appelé pendant Log()
			 *
			 * @example
			 * @code
			 * // Désactiver les logs verbeux en production
			 * #ifndef NKENTSEU_DEBUG
			 *     debugSink->SetEnabled(false);
			 * #endif
			 *
			 * // Activer temporairement pour investigation d'un bug
			 * void EnableDebugLogging() {
			 *     debugSink->SetEnabled(true);
			 *     Logger::Log(NkLogLevel::NK_INFO, "Debug logging enabled for troubleshooting");
			 * }
			 * @endcode
			 */
			virtual void SetEnabled(bool enabled);

			/**
			 * @brief Vérifie si ce sink est actuellement activé
			 * @return true si le sink traite les messages, false s'il les ignore
			 * @ingroup SinkOptionalMethods
			 *
			 * @note Retourne la valeur de m_Enabled : true par défaut
			 * @note Utile pour affichage de statut ou logique conditionnelle
			 * @note Thread-safety : lecture safe si pas de modification concurrente
			 *
			 * @example
			 * @code
			 * // Affichage de l'état des sinks dans une commande de debug
			 * for (auto& sink : Logger::GetSinks()) {
			 *     printf("Sink '%s': %s (level: %s)\n",
			 *         sink->GetName().CStr(),
			 *         sink->IsEnabled() ? "active" : "disabled",
			 *         NkLogLevelToString(sink->GetLevel()));
			 * }
			 * @endcode
			 */
			virtual bool IsEnabled() const;

			/**
			 * @brief Obtient le nom identifiant ce sink
			 * @return NkString contenant le nom, ou vide si non configuré
			 * @ingroup SinkOptionalMethods
			 *
			 * @note Nom par défaut : chaîne vide
			 * @note Utile pour : logging de debug, configuration, identification dans les erreurs
			 * @note Thread-safety : lecture safe si pas de modification concurrente
			 *
			 * @example
			 * @code
			 * // Logging d'erreur avec identification du sink fautif
			 * if (!sink->Log(message)) {
			 *     Logger::Log(NkLogLevel::NK_ERROR,
			 *         "Failed to write to sink '%s'",
			 *         sink->GetName().CStr());
			 * }
			 * @endcode
			 */
			virtual NkString GetName() const;

			/**
			 * @brief Définit le nom identifiant ce sink
			 * @param name Nouvelle chaîne de nom à associer au sink
			 * @ingroup SinkOptionalMethods
			 *
			 * @note Nom recommandé : court, unique, descriptif (ex: "console", "file_debug")
			 * @note Thread-safety : synchronisation requise si appelé pendant Log()
			 *
			 * @example
			 * @code
			 * // Configuration explicite des noms pour identification
			 * consoleSink->SetName("console");
			 * fileSink->SetName("app.log");
			 * syslogSink->SetName("syslog");
			 *
			 * // Utilisation dans les messages de diagnostic
			 * Logger::Log(NkLogLevel::NK_INFO,
			 *     "Initialized %zu sinks: console, file, syslog",
			 *     Logger::GetSinkCount());
			 * @endcode
			 */
			virtual void SetName(const NkString& name);


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PROTÉGÉS (POUR HÉRITAGE)
			// -----------------------------------------------------------------
		protected:


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PROTÉGÉES (ÉTAT PARTAGÉ AVEC LES DÉRIVÉES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup SinkProtectedMembers Membres Protégés
			 * @brief État interne accessible aux classes dérivées pour implémentation
			 *
			 * Ces membres sont protégés pour permettre aux classes dérivées
			 * d'accéder et de modifier l'état de configuration du sink.
			 *
			 * Thread-safety :
			 *  - Ces membres ne sont PAS thread-safe par défaut
			 *  - Les dérivées doivent synchroniser l'accès si modifiés depuis multiples threads
			 *  - Alternative : utiliser des atomiques ou mutex internes dans les dérivées
			 */

			/// @brief Niveau minimum de log pour filtrage des messages
			/// @ingroup SinkProtectedMembers
			/// @note Défaut : NK_TRACE (accepte tous les niveaux)
			/// @note Comparaison : message.level >= m_Level pour autoriser le log
			NkLogLevel m_Level = NkLogLevel::NK_TRACE;

			/// @brief Indicateur d'activation/désactivation du sink
			/// @ingroup SinkProtectedMembers
			/// @note Défaut : true (sink actif)
			/// @note Si false : Log() doit être un no-op immédiat
			bool m_Enabled = true;

			/// @brief Nom identifiant ce sink pour logging et configuration
			/// @ingroup SinkProtectedMembers
			/// @note Défaut : chaîne vide
			/// @note Recommandé : définir un nom explicite dans le constructeur dérivé
			NkString m_Name = NkString();


		}; // class NkISink


		// ---------------------------------------------------------------------
		// TYPE ALIAS POUR GESTION DE MÉMOIRE DES SINKS
		// ---------------------------------------------------------------------
		/**
		 * @typedef NkSinkPtr
		 * @brief Pointeur partagé vers NkISink pour ownership multiple
		 * @ingroup LoggerTypes
		 *
		 * Alias vers memory::NkSharedPtr<NkISink> pour :
		 *  - Partage de sink entre plusieurs loggers ou composants
		 *  - Durée de vie gérée par comptage de références
		 *  - Compatibilité avec le système d'allocateur du projet
		 *
		 * @note Thread-safety : NkSharedPtr doit être thread-safe pour inc/dec refcount
		 * @note Usage typique : stockage dans un vector de sinks du logger principal
		 *
		 * @example
		 * @code
		 * // Création d'un sink partagé entre plusieurs loggers
		 * auto sharedFileSink = memory::MakeShared<NkFileSink>("app.log");
		 *
		 * // Ajout à plusieurs loggers (module A et module B)
		 * loggerA.AddSink(sharedFileSink);
		 * loggerB.AddSink(sharedFileSink);
		 *
		 * // Le sink reste valide tant qu'au moins un logger l'utilise
		 * @endcode
		 */
		using NkSinkPtr = memory::NkSharedPtr<NkISink>;

		/**
		 * @typedef NkSinkUniquePtr
		 * @brief Pointeur unique vers NkISink pour ownership exclusif
		 * @ingroup LoggerTypes
		 *
		 * Alias vers memory::NkUniquePtr<NkISink> pour :
		 *  - Ownership exclusif d'un sink par un seul logger
		 *  - Transfert de propriété via move semantics
		 *  - Libération automatique à la destruction du propriétaire
		 *
		 * @note Thread-safety : le pointeur lui-même n'est pas thread-safe,
		 *       mais l'objet pointé peut être utilisé depuis multiples threads
		 *       si l'implémentation du sink est thread-safe.
		 *
		 * @example
		 * @code
		 * // Création d'un sink avec ownership exclusif
		 * NkSinkUniquePtr consoleSink = memory::MakeUnique<NkConsoleSink>();
		 *
		 * // Transfert au logger : le logger prend possession
		 * logger.AddSink(std::move(consoleSink));
		 *
		 * // consoleSink est maintenant nullptr, le logger gère la durée de vie
		 * @endcode
		 */
		using NkSinkUniquePtr = memory::NkUniquePtr<NkISink>;


	} // namespace nkentseu


#endif // NKENTSEU_NKSINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKSINK.H
// =============================================================================
// Ce fichier définit l'interface de base pour les destinations de logging.
// Il permet l'extensibilité via héritage pour supporter de nouveaux sinks.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Implémentation minimaliste d'un sink console
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkSink.h>
	#include <NKLogger/NkLoggerFormatter.h>
	#include <cstdio>

	class ConsoleSink : public nkentseu::NkISink {
	public:
		ConsoleSink() {
			SetName("console");
			SetLevel(nkentseu::NkLogLevel::NK_INFO);  // Défaut : info et plus grave
		}

		void Log(const nkentseu::NkLogMessage& message) override {
			// Filtrage par niveau
			if (!ShouldLog(message.level)) {
				return;
			}

			// Formatage si formatter configuré
			nkentseu::NkString output;
			if (auto* formatter = GetFormatter()) {
				output = formatter->Format(message, true);  // Avec couleurs
			} else {
				output = message.message;  // Fallback minimal
			}

			// Écriture vers stderr avec flush immédiat pour visibilité
			fprintf(stderr, "%s\n", output.CStr());
			fflush(stderr);
		}

		void Flush() override {
			// Console : flush immédiat dans Log(), rien de plus à faire
			fflush(stderr);
		}

		void SetFormatter(nkentseu::memory::NkUniquePtr<nkentseu::NkLoggerFormatter> formatter) override {
			m_Formatter = std::move(formatter);
		}

		void SetPattern(const nkentseu::NkString& pattern) override {
			m_Formatter = nkentseu::memory::MakeUnique<nkentseu::NkLoggerFormatter>(pattern);
		}

		nkentseu::NkLoggerFormatter* GetFormatter() const override {
			return m_Formatter.get();
		}

		nkentseu::NkString GetPattern() const override {
			return m_Formatter ? m_Formatter->GetPattern() : nkentseu::NkString{};
		}

	private:
		nkentseu::memory::NkUniquePtr<nkentseu::NkLoggerFormatter> m_Formatter;
	};

	// Usage :
	// auto sink = nkentseu::memory::MakeShared<ConsoleSink>();
	// sink->SetPattern("[%^%L%$] %v");  // Pattern avec couleurs
	// logger.AddSink(sink);
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Implémentation d'un sink fichier avec buffering
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkSink.h>
	#include <NKLogger/NkLoggerFormatter.h>
	#include <NKThreading/NkMutex.h>
	#include <cstdio>

	class FileSink : public nkentseu::NkISink {
	public:
		explicit FileSink(const nkentseu::NkString& filepath)
			: mFile(nullptr) {

			SetName(filepath);  // Utiliser le chemin comme nom identifiant
			SetLevel(nkentseu::NkLogLevel::NK_TRACE);  // Tout logger par défaut

			// Ouverture du fichier en mode append
			mFile = fopen(filepath.CStr(), "a");
			if (!mFile) {
				// Gérer l'erreur : fallback vers stderr ou désactivation
				fprintf(stderr, "Failed to open log file: %s\n", filepath.CStr());
				SetEnabled(false);
			}
		}

		~FileSink() override {
			if (mFile) {
				fclose(mFile);
				mFile = nullptr;
			}
		}

		void Log(const nkentseu::NkLogMessage& message) override {
			if (!IsEnabled() || !ShouldLog(message.level) || !mFile) {
				return;
			}

			// Formatage thread-safe via mutex
			nkentseu::threading::NkScopedLock lock(mMutex);

			nkentseu::NkString output;
			if (auto* formatter = GetFormatter()) {
				output = formatter->Format(message, false);  // Pas de couleurs en fichier
			} else {
				output = message.message;
			}

			// Écriture avec vérification d'erreur
			if (fwrite(output.CStr(), 1, output.Length(), mFile) != output.Length()) {
				// Gérer l'erreur d'écriture (désactiver le sink pour éviter spam)
				SetEnabled(false);
			}
		}

		void Flush() override {
			if (!mFile) return;

			nkentseu::threading::NkScopedLock lock(mMutex);
			fflush(mFile);

			// Force flush OS pour persistance garantie
			#ifdef NKENTSEU_PLATFORM_WINDOWS
				_commit(_fileno(mFile));
			#else
				fsync(fileno(mFile));
			#endif
		}

		// ... autres overrides requis (SetFormatter, SetPattern, etc.) ...

	private:
		FILE* mFile;
		mutable nkentseu::threading::NkMutex mMutex;  // Protection écriture concurrente
	};
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Sink mémoire pour tests unitaires
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkSink.h>
	#include <NKContainers/Sequential/NkVector.h>

	class MemorySink : public nkentseu::NkISink {
	public:
		void Log(const nkentseu::NkLogMessage& message) override {
			if (!ShouldLog(message.level)) {
				return;
			}

			// Stockage brut du message pour inspection ultérieure
			m_Messages.PushBack(message);
		}

		void Flush() override {
			// Mémoire : pas de buffer à flush, no-op
		}

		// Accès aux messages pour assertions dans les tests
		const nkentseu::NkVector<nkentseu::NkLogMessage>& GetMessages() const {
			return m_Messages;
		}

		void Clear() {
			m_Messages.Clear();
		}

		// ... autres overrides requis ...

	private:
		nkentseu::NkVector<nkentseu::NkLogMessage> m_Messages;
	};

	// Usage dans un test unitaire :
	// TEST(LoggerTest, InfoLevelLogs) {
	//     auto sink = nkentseu::memory::MakeShared<MemorySink>();
	//     logger.AddSink(sink);
	//
	//     Logger::Log(nkentseu::NkLogLevel::NK_INFO, "Test message");
	//
	//     const auto& messages = sink->GetMessages();
	//     ASSERT_EQ(messages.Size(), 1);
	//     EXPECT_EQ(messages[0].message, "Test message");
	//     EXPECT_EQ(messages[0].level, nkentseu::NkLogLevel::NK_INFO);
	// }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Configuration dynamique des sinks via fichier de config
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkSink.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkSinkPtr CreateSinkFromConfig(const nkentseu::NkString& configSection) {
		using namespace nkentseu;

		// Lecture du type de sink depuis configuration
		NkString type = core::Config::GetString(configSection + ".type", "console");

		if (type == "console") {
			auto sink = memory::MakeShared<ConsoleSink>();
			sink->SetLevel(NkLogLevelFromString(
				core::Config::GetString(configSection + ".level", "info")));
			sink->SetPattern(core::Config::GetString(
				configSection + ".pattern", NkLoggerFormatter::NK_DEFAULT_PATTERN));
			return sink;

		} else if (type == "file") {
			NkString filepath = core::Config::GetString(
				configSection + ".filepath", "app.log");
			auto sink = memory::MakeShared<FileSink>(filepath);
			sink->SetLevel(NkLogLevelFromString(
				core::Config::GetString(configSection + ".level", "trace")));
			sink->SetPattern(core::Config::GetString(
				configSection + ".pattern", NkLoggerFormatter::NK_DETAILED_PATTERN));
			return sink;

		} else if (type == "null") {
			// Sink nul pour désactiver le logging sans retirer le sink
			return memory::MakeShared<NullSink>();
		}

		// Fallback : sink console par défaut
		return memory::MakeShared<ConsoleSink>();
	}

	// Usage au startup de l'application :
	// auto sink1 = CreateSinkFromConfig("logging.sinks.console");
	// auto sink2 = CreateSinkFromConfig("logging.sinks.file");
	// Logger::AddSink(sink1);
	// Logger::AddSink(sink2);
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Filtrage avancé avec ShouldLog personnalisé
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkSink.h>

	class SmartFilteredSink : public nkentseu::NkISink {
	public:
		// Filtrage basé sur le nom du logger en plus du niveau
		bool ShouldLog(nkentseu::NkLogLevel level, const nkentseu::NkString& loggerName) const {
			// Toujours ignorer si sink désactivé ou niveau trop bas
			if (!IsEnabled() || level < m_Level) {
				return false;
			}

			// Filtrage par nom de logger (wildcard simple)
			if (!m_LoggerFilter.Empty() && !loggerName.StartsWith(m_LoggerFilter)) {
				return false;
			}

			// Filtrage par mot-clé dans le message (optionnel, coûteux)
			if (!m_KeywordFilter.Empty()) {
				// Note : ce filtrage nécessite d'accéder au message,
				// donc à faire dans Log() plutôt que ShouldLog()
				// Cette méthode reste un pré-filtre rapide
			}

			return true;
		}

		void Log(const nkentseu::NkLogMessage& message) override {
			// Pré-filtrage rapide
			if (!ShouldLog(message.level, message.loggerName)) {
				return;
			}

			// Filtrage avancé sur le contenu du message
			if (!m_KeywordFilter.Empty() && !message.message.Contains(m_KeywordFilter)) {
				return;
			}

			// ... écriture normale ...
		}

		void SetLoggerFilter(const nkentseu::NkString& prefix) {
			m_LoggerFilter = prefix;
		}

		void SetKeywordFilter(const nkentseu::NkString& keyword) {
			m_KeywordFilter = keyword;
		}

	private:
		nkentseu::NkString m_LoggerFilter;   // Ex: "nkentseu.network.*"
		nkentseu::NkString m_KeywordFilter;  // Ex: "ERROR", "timeout"
	};
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Composition de sinks avec décorateur
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkSink.h>

	// Décorateur qui ajoute un préfixe à tous les messages
	class PrefixedSink : public nkentseu::NkISink {
	public:
		PrefixedSink(nkentseu::NkSinkPtr inner, const nkentseu::NkString& prefix)
			: mInner(std::move(inner))
			, mPrefix(prefix) {
		}

		void Log(const nkentseu::NkLogMessage& message) override {
			if (!mInner || !mInner->ShouldLog(message.level)) {
				return;
			}

			// Création d'un message dérivé avec préfixe
			nkentseu::NkLogMessage prefixed = message;
			prefixed.message = mPrefix + prefixed.message;

			// Délégation au sink interne
			mInner->Log(prefixed);
		}

		void Flush() override {
			if (mInner) mInner->Flush();
		}

		// Délégation des autres méthodes au sink interne
		void SetFormatter(nkentseu::memory::NkUniquePtr<nkentseu::NkLoggerFormatter> f) override {
			if (mInner) mInner->SetFormatter(std::move(f));
		}
		// ... autres délégations ...

	private:
		nkentseu::NkSinkPtr mInner;
		nkentseu::NkString mPrefix;
	};

	// Usage :
	// auto baseSink = nkentseu::memory::MakeShared<ConsoleSink>();
	// auto prefixed = nkentseu::memory::MakeShared<PrefixedSink>(baseSink, "[MYAPP] ");
	// logger.AddSink(prefixed);
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. THREAD-SAFETY DES IMPLÉMENTATIONS :
	   - NkISink n'impose pas de thread-safety : chaque dérivée doit gérer
	   - Pour sinks partagés : utiliser nkentseu::threading::NkMutex interne
	   - Alternative : logger asynchrone qui sérialise les appels vers les sinks

	2. GESTION DES ERREURS D'ÉCRITURE :
	   - Ne jamais propager d'exceptions depuis Log() (noexcept implicite)
	   - En cas d'erreur I/O : désactiver le sink (SetEnabled(false)) pour éviter spam
	   - Logger les erreurs de sink vers un fallback (stderr) si possible

	3. PERFORMANCE DU FILTRAGE :
	   - ShouldLog() doit être ultra-rapide : éviter les allocations, regex, etc.
	   - Filtrage par niveau : comparaison d'entiers, O(1)
	   - Filtrage par nom/mot-clé : à faire dans Log() si nécessaire, après ShouldLog()

	4. GESTION MÉMOIRE DES FORMATTERS :
	   - SetFormatter() prend ownership via NkUniquePtr : pas de copie
	   - GetFormatter() retourne pointeur brut : ne pas delete, durée de vie liée au sink
	   - Pour changer de pattern : SetPattern() recrée le formatter interne

	5. EXTENSIBILITÉ FUTURES :
	   - Ajouter des méthodes virtuelles avec implémentation par défaut pour rétrocompatibilité
	   - Pour breaking changes : créer NkISinkV2 et migrer progressivement
	   - Documenter clairement le contrat de chaque méthode dans les commentaires Doxygen

	6. TESTING DES SINKS :
	   - Utiliser MemorySink pour capturer les outputs en tests unitaires
	   - Tester les cas limites : fichier plein, socket fermée, permissions refusées
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================