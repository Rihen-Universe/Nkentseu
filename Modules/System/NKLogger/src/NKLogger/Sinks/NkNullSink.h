// =============================================================================
// NKLogger/Sinks/NkNullSink.h
// Sink nul qui ignore tous les messages de log (no-op).
//
// Design :
//  - Implémentation minimale de NkISink : toutes les méthodes sont des no-op
//  - Zéro allocation, zéro I/O, zéro overhead : optimal pour désactivation
//  - Thread-safe par conception : aucun état mutable, aucune synchronisation requise
//  - Utilité : désactiver le logging en production, testing sans effets de bord
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKNULLSINK_H
#define NKENTSEU_NKNULLSINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards minimales : ce sink n'a aucune dépendance externe.
	// Dépendances projet uniquement pour l'interface de base NkISink.

	#include "NKCore/NkTypes.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKLogger/NkSink.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLoggerFormatter.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkNullSink
		// DESCRIPTION : Implémentation no-op de NkISink pour désactivation/logging test
		// ---------------------------------------------------------------------
		/**
		 * @class NkNullSink
		 * @brief Sink nul qui ignore silencieusement tous les messages de log
		 * @ingroup LoggerSinks
		 *
		 * NkNullSink fournit une implémentation "null object" de l'interface NkISink :
		 *  - Log() : ignore le message sans traitement ni allocation
		 *  - Flush() : no-op immédiat
		 *  - SetFormatter/SetPattern : ignore les paramètres sans effet
		 *  - GetFormatter/GetPattern : retourne nullptr/chaîne vide
		 *
		 * Cas d'usage principaux :
		 *  @b Désactivation du logging en production :
		 *  Remplacer un sink coûteux (fichier, réseau) par NkNullSink pour désactiver
		 *  la sortie sans modifier le code appelant ni les configurations.
		 *
		 *  @b Testing sans effets de bord :
		 *  Utiliser NkNullSink dans les tests unitaires pour éviter l'écriture
		 *  dans des fichiers ou la pollution de la console pendant l'exécution des tests.
		 *
		 *  @b Benchmarking et profiling :
		 *  Éliminer l'overhead du logging pour mesurer les performances brutes
		 *  du code métier sans interférence des appels Log().
		 *
		 * Architecture :
		 *  - Hérite de NkISink : compatibilité totale avec l'API de logging
		 *  - Aucun membre mutable : état constant, thread-safe par conception
		 *  - Méthodes inline potentielles : compilateur peut optimiser les appels
		 *
		 * Thread-safety :
		 *  - Toutes les méthodes sont thread-safe sans synchronisation
		 *  - Aucun état partagé : safe pour appel concurrent depuis multiples threads
		 *  - Aucun mutex requis : overhead minimal garanti
		 *
		 * @note Cette classe est conçue pour être instanciée directement ou via shared_ptr
		 *       et ajoutée à un logger comme n'importe quel autre sink.
		 *
		 * @example Désactivation conditionnelle du logging
		 * @code
		 * #ifdef NKENTSEU_ENABLE_LOGGING
		 *     auto sink = nkentseu::memory::MakeShared<nkentseu::NkFileSink>("app.log");
		 * #else
		 *     auto sink = nkentseu::memory::MakeShared<nkentseu::NkNullSink>();
		 * #endif
		 *     logger.AddSink(sink);  // Même API, comportement différent
		 * @endcode
		 *
		 * @example Testing sans effets de bord
		 * @code
		 * TEST(MyComponentTest, DoesNotCrash) {
		 *     // Utiliser NkNullSink pour éviter l'écriture dans les logs pendant les tests
		 *     auto nullSink = nkentseu::memory::MakeShared<nkentseu::NkNullSink>();
		 *     testLogger.AddSink(nullSink);
		 *
		 *     // Exécuter le code qui logge : aucun effet observable
		 *     componentUnderTest.Process();
		 *
		 *     // Le test passe sans créer de fichiers de log
		 * }
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkNullSink : public NkISink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup NullSinkCtors Constructeurs de NkNullSink
			 * @brief Initialisation sans état, sans allocation
			 */

			/**
			 * @brief Constructeur par défaut : aucune initialisation requise
			 * @ingroup NullSinkCtors
			 *
			 * @post Aucun état interne initialisé : objet prêt à l'emploi immédiat
			 * @note noexcept implicite : aucune allocation, aucune exception possible
			 * @note Thread-safe : construction safe depuis n'importe quel thread
			 *
			 * @example
			 * @code
			 * // Construction directe sur la stack
			 * nkentseu::NkNullSink nullSink;
			 *
			 * // Ou via shared_ptr pour ajout à un logger
			 * auto sink = nkentseu::memory::MakeShared<nkentseu::NkNullSink>();
			 * logger.AddSink(sink);
			 * @endcode
			 */
			NkNullSink();

			/**
			 * @brief Destructeur par défaut : aucun cleanup requis
			 * @ingroup NullSinkCtors
			 *
			 * @note = default : génération automatique par le compilateur
			 * @note Aucune ressource à libérer : fichier, mutex, buffer, etc.
			 * @note noexcept implicite : destruction safe dans tous les contextes
			 */
			~NkNullSink() override;


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'INTERFACE NKISINK (NO-OP)
			// -----------------------------------------------------------------
			/**
			 * @defgroup NullSinkInterface Implémentation de NkISink
			 * @brief Toutes les méthodes sont des no-op pour overhead minimal
			 *
			 * Ces méthodes satisfont le contrat de NkISink sans effectuer
			 * aucun traitement réel. Idéal pour désactivation ou testing.
			 */

			/**
			 * @brief Ignore silencieusement le message de log (no-op)
			 * @param message Message de log ignoré (non utilisé)
			 * @ingroup NullSinkInterface
			 *
			 * @note Aucun traitement : le message est ignoré sans allocation ni I/O
			 * @note Thread-safe : safe pour appel concurrent depuis multiples threads
			 * @note Performance : overhead minimal, potentiellement optimisé par le compilateur
			 *
			 * @example
			 * @code
			 * nkentseu::NkNullSink nullSink;
			 * nkentseu::NkLogMessage msg(nkentseu::NkLogLevel::NK_ERROR, "Ignored");
			 * nullSink.Log(msg);  // Aucun effet, retour immédiat
			 * @endcode
			 */
			void Log(const NkLogMessage& message) override;

			/**
			 * @brief No-op : aucun buffer à flush pour ce sink
			 * @ingroup NullSinkInterface
			 *
			 * @note Retour immédiat sans opération : aucun buffer interne à vider
			 * @note Thread-safe : safe pour appel concurrent
			 * @note Utile pour compatibilité avec l'API Flush() commune à tous les sinks
			 *
			 * @example
			 * @code
			 * // Flush appelé sur un NkNullSink : no-op silencieux
			 * nullSink.Flush();  // Aucun effet, aucun overhead
			 * @endcode
			 */
			void Flush() override;

			/**
			 * @brief Ignore le formatter fourni (no-op)
			 * @param formatter Pointeur unique ignoré (détruit automatiquement)
			 * @ingroup NullSinkInterface
			 *
			 * @note Le formatter passé est détruit via RAII de NkUniquePtr
			 * @note Aucun état interne modifié : ce sink n'utilise pas de formatter
			 * @note Thread-safe : safe pour appel concurrent
			 *
			 * @example
			 * @code
			 * auto formatter = nkentseu::memory::MakeUnique<nkentseu::NkFormatter>("%L: %v");
			 * nullSink.SetFormatter(std::move(formatter));  // Formatter détruit, aucun effet
			 * @endcode
			 */
			void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

			/**
			 * @brief Ignore le pattern fourni (no-op)
			 * @param pattern Chaîne de pattern ignorée (non utilisée)
			 * @ingroup NullSinkInterface
			 *
			 * @note Aucun état interne modifié : ce sink n'utilise pas de pattern
			 * @note Thread-safe : safe pour appel concurrent
			 * @note Utile pour compatibilité avec l'API SetPattern() commune à tous les sinks
			 *
			 * @example
			 * @code
			 * // SetPattern appelé sur un NkNullSink : ignoré silencieusement
			 * nullSink.SetPattern("[%Y-%m-%d] %v");  // Aucun effet
			 * @endcode
			 */
			void SetPattern(const NkString& pattern) override;

			/**
			 * @brief Retourne nullptr : aucun formatter associé à ce sink
			 * @return nullptr toujours
			 * @ingroup NullSinkInterface
			 *
			 * @note Retour constant : ce sink ne gère pas de formatter interne
			 * @note Thread-safe : lecture safe sans synchronisation
			 * @note Utile pour code générique qui interroge le formatter d'un sink
			 *
			 * @example
			 * @code
			 * if (auto* fmt = nullSink.GetFormatter()) {
			 *     // Cette branche ne sera jamais exécutée pour NkNullSink
			 * } else {
			 *     // Confirmé : NkNullSink n'a pas de formatter
			 * }
			 * @endcode
			 */
			NkFormatter* GetFormatter() const override;

			/**
			 * @brief Retourne une chaîne vide : aucun pattern associé à ce sink
			 * @return NkString vide toujours
			 * @ingroup NullSinkInterface
			 *
			 * @note Retour constant : ce sink ne gère pas de pattern interne
			 * @note Thread-safe : lecture safe sans synchronisation
			 * @note Utile pour code générique qui interroge le pattern d'un sink
			 *
			 * @example
			 * @code
			 * nkentseu::NkString pattern = nullSink.GetPattern();
			 * if (pattern.Empty()) {
			 *     // Confirmé : NkNullSink n'a pas de pattern configuré
			 * }
			 * @endcode
			 */
			NkString GetPattern() const override;


		}; // class NkNullSink


	} // namespace nkentseu


#endif // NKENTSEU_NKNULLSINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKNULLSINK.H
// =============================================================================
// Ce fichier fournit l'implémentation de sink nul pour désactivation ou testing.
// Il combine compatibilité API complète et overhead minimal garanti.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Désactivation conditionnelle du logging en production
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <NKLogger/Sinks/NkFileSink.h>

	void SetupLogging(bool enableFileLogging) {
		using namespace nkentseu;

		memory::NkSharedPtr<NkISink> sink;

		if (enableFileLogging) {
			// Production : logging vers fichier avec rotation
			sink = memory::MakeShared<NkRotatingFileSink>(
				"logs/app.log",
				50 * 1024 * 1024,  // 50 MB
				5                   // 5 backups
			);
		} else {
			// Développement/testing : désactiver le logging fichier
			sink = memory::MakeShared<NkNullSink>();
		}

		NkLog::Instance().AddSink(sink);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Testing unitaire sans effets de bord
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <gtest/gtest.h>

	class ComponentUnderTest {
	public:
		void Process() {
			// Code qui appelle logger.Info(), logger.Debug(), etc.
			logger.Info("Processing item {0}", itemId);
			// ...
		}
	private:
		nkentseu::NkLogger& logger;
	};

	TEST(ComponentTest, ProcessDoesNotCrash) {
		// Setup : logger avec NkNullSink pour éviter l'écriture dans les logs
		nkentseu::NkLogger testLogger("TestLogger");
		auto nullSink = nkentseu::memory::MakeShared<nkentseu::NkNullSink>();
		testLogger.AddSink(nullSink);

		// Execution : le code loggue mais aucun effet observable
		ComponentUnderTest component(testLogger);
		component.Process();  // Aucun fichier créé, aucune console polluée

		// Verification : le test passe sans dépendre des outputs de logging
		EXPECT_TRUE(true);  // Placeholder : vérifier la logique métier réelle
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Benchmarking sans overhead de logging
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <NKCore/NkBenchmark.h>

	void BenchmarkCriticalPath() {
		using namespace nkentseu;

		// Désactiver le logging pour mesurer les performances brutes
		auto nullSink = memory::MakeShared<NkNullSink>();
		NkLog::Instance().ClearSinks();  // Retirer les sinks existants
		NkLog::Instance().AddSink(nullSink);

		// Benchmark : mesurer le code sans interférence du logging
		NkBenchmark bench;
		bench.Start();

		for (int i = 0; i < 1000000; ++i) {
			CriticalFunction(i);  // Code qui appelle logger.Debug() potentiellement
		}

		bench.Stop();
		printf("CriticalFunction: %.3f ms/op\n", bench.GetElapsedMs() / 1000000.0);

		// Restoration : réactiver le logging normal après benchmark
		SetupProductionLogging();
	}
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Substitution de sink à runtime pour debugging
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <NKLogger/Sinks/NkConsoleSink.h>

	class DebugToggle {
	public:
		static void EnableLogging() {
			auto& logger = nkentseu::NkLog::Instance();
			logger.ClearSinks();
			logger.AddSink(nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>());
		}

		static void DisableLogging() {
			auto& logger = nkentseu::NkLog::Instance();
			logger.ClearSinks();
			logger.AddSink(nkentseu::memory::MakeShared<nkentseu::NkNullSink>());
		}
	};

	// Usage : activer/désactiver le logging via commande console ou signal
	// DebugToggle::DisableLogging();  // Mode silencieux
	// DebugToggle::EnableLogging();   // Mode verbose pour debugging
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Configuration via fichier avec fallback null
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkSinkPtr CreateSinkFromConfig(const nkentseu::NkString& section) {
		using namespace nkentseu;

		// Lecture du type de sink depuis configuration
		NkString type = core::Config::GetString(section + ".type", "null");

		if (type == "file") {
			NkString filepath = core::Config::GetString(section + ".filepath", "app.log");
			return memory::MakeShared<NkFileSink>(filepath);

		} else if (type == "console") {
			return memory::MakeShared<NkConsoleSink>();

		} else if (type == "rotating") {
			usize maxSize = core::Config::GetUint64(section + ".maxSize", 50 * 1024 * 1024);
			usize maxFiles = core::Config::GetUint64(section + ".maxFiles", 5);
			NkString filepath = core::Config::GetString(section + ".filepath", "app.log");
			return memory::MakeShared<NkRotatingFileSink>(filepath, maxSize, maxFiles);

		} else {
			// Fallback : NkNullSink pour désactivation implicite
			// Utile si la configuration est manquante ou invalide
			return memory::MakeShared<NkNullSink>();
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Vérification que NkNullSink est bien un no-op (test interne)
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <NKLogger/NkLogMessage.h>
	#include <chrono>
	#include <cassert>

	void VerifyNullSinkPerformance() {
		using namespace nkentseu;

		NkNullSink nullSink;
		NkLogMessage msg(NkLogLevel::NK_INFO, "Test message");

		// Mesurer le temps de 1 million d'appels Log() sur NkNullSink
		auto start = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < 1000000; ++i) {
			nullSink.Log(msg);  // Doit être un no-op optimisé
		}

		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

		// Vérifier que l'overhead est minimal (< 10 ns par appel en moyenne)
		double avgNs = static_cast<double>(duration.count()) / 1000000.0;
		assert(avgNs < 10.0 && "NkNullSink Log() should be near-zero overhead");

		printf("NkNullSink::Log() average: %.2f ns/call\n", avgNs);
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. OVERHEAD MINIMAL GARANTI :
	   - Toutes les méthodes sont des no-op : aucun branchement complexe, aucune allocation
	   - Le compilateur peut inline et optimiser les appels vers du code nul
	   - Pour benchmarking : mesurer l'overhead réel avec des tests de performance

	2. THREAD-SAFETY PAR CONCEPTION :
	   - Aucun état mutable : pas de race conditions possibles
	   - Aucune synchronisation requise : pas de mutex, pas d'atomics
	   - Safe pour appel concurrent depuis n'importe quel nombre de threads

	3. UTILISATION POUR DÉSACTIVATION :
	   - Préférer NkNullSink à la suppression complète des appels Log()
	   - Avantage : le code reste compilé et testé, juste les outputs sont ignorés
	   - Pour désactivation compile-time : utiliser des macros #ifdef autour des appels Log()

	4. TESTING ET MOCKING :
	   - NkNullSink est idéal pour les tests unitaires sans effets de bord
	   - Alternative : créer un MockSink avec Google Mock pour vérifier les appels Log()
	   - Pour tests d'intégration : utiliser NkMemorySink pour capturer et inspecter les logs

	5. COMPATIBILITÉ API COMPLÈTE :
	   - Toutes les méthodes de NkISink sont implémentées : pas d'exceptions à runtime
	   - Code générique qui manipule NkISink* fonctionne avec NkNullSink sans adaptation
	   - Retour de GetFormatter()/GetPattern() cohérent : nullptr/chaîne vide documentés

	6. EXTENSIBILITÉ FUTURES :
	   - Pour logging conditionnel : dériver NkNullSink avec override de Log() pour filtrage
	   - Pour métriques : ajouter un compteur d'appels Log() ignorés pour monitoring
	   - Pour debugging : optionnel, ajouter un flag pour logger les messages ignorés en mode verbose

	7. PERFORMANCE VS FLEXIBILITÉ :
	   - NkNullSink : overhead minimal, aucune flexibilité (tous les messages ignorés)
	   - NkFileSink/NkConsoleSink : overhead modéré, flexibilité maximale (filtrage, formatage)
	   - Choisir selon le cas d'usage : production silencieuse vs debugging riche
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================