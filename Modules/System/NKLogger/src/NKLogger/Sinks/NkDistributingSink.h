// =============================================================================
// NKLogger/Sinks/NkDistributingSink.h
// Sink qui distribue les messages de log à plusieurs sous-sinks.
//
// Design :
//  - Pattern Composite : agrège plusieurs NkISink pour diffusion broadcast
//  - Thread-safe : synchronisation via mutex interne pour modifications concurrentes
//  - Propagation de configuration : SetPattern/SetFormatter appliqués à tous les sous-sinks
//  - Gestion dynamique : ajout/suppression de sinks à runtime sans interruption
//  - Filtrage hérité : ShouldLog/IsEnabled de NkISink appliqués avant distribution
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKDISTRIBUTINGSINK_H
#define NKENTSEU_NKDISTRIBUTINGSINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour les conteneurs et la synchronisation.
	// Dépendances projet pour l'interface de sink, le formatage et les smart pointers.

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKContainers/Sequential/NkVector.h"
	#include "NKMemory/NkSharedPtr.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKThreading/NkMutex.h"
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
		// CLASSE : NkDistributingSink
		// DESCRIPTION : Composite de sinks pour diffusion broadcast des messages
		// ---------------------------------------------------------------------
		/**
		 * @class NkDistributingSink
		 * @brief Sink composite qui distribue les messages à plusieurs sous-sinks
		 * @ingroup LoggerSinks
		 *
		 * NkDistributingSink implémente le pattern Composite pour le logging :
		 *  - Agrège plusieurs NkISink dans une collection interne
		 *  - Log() diffuse le message à tous les sous-sinks valides
		 *  - Flush() propage l'appel à tous les sous-sinks pour persistance garantie
		 *  - SetPattern/SetFormatter appliqués uniformément à tous les sous-sinks
		 *  - Gestion dynamique : ajout/suppression de sinks à runtime thread-safe
		 *
		 * Architecture :
		 *  - Hérite de NkISink : compatible avec l'API de logging existante
		 *  - Contient NkVector<NkSharedPtr<NkISink>> : collection de sous-sinks
		 *  - Mutex interne : protection des modifications de la collection
		 *  - Filtrage hérité : ShouldLog/IsEnabled appliqués avant distribution
		 *
		 * Thread-safety :
		 *  - Toutes les méthodes publiques sont thread-safe via m_Mutex
		 *  - Log() acquiert le mutex pour itération safe sur la collection
		 *  - Les sous-sinks partagés doivent être thread-safe individuellement
		 *
		 * Cas d'usage principaux :
		 *  @b Logging multi-destination :
		 *  Envoyer simultanément vers console + fichier + réseau sans duplication de code.
		 *
		 *  @b Configuration hiérarchique :
		 *  Appliquer un pattern global via SetPattern() qui se propage à tous les sinks.
		 *
		 *  @b Gestion dynamique :
		 *  Ajouter/retirer des destinations de log à runtime sans redémarrage.
		 *
		 * @note Le filtrage par niveau (ShouldLog) est appliqué au niveau du distributing sink
		 *       avant distribution. Les sous-sinks peuvent avoir leurs propres filtres additionnels.
		 *
		 * @example Configuration basique multi-destination
		 * @code
		 * // Création du sink distributeur
		 * auto distributor = nkentseu::memory::MakeShared<nkentseu::NkDistributingSink>();
		 *
		 * // Ajout de destinations multiples
		 * distributor->AddSink(nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>());
		 * distributor->AddSink(nkentseu::memory::MakeShared<nkentseu::NkFileSink>("app.log"));
		 * distributor->AddSink(nkentseu::memory::MakeShared<nkentseu::NkRotatingFileSink>(
		 *     "archive.log", 100*1024*1024, 5));
		 *
		 * // Configuration globale propagée à tous les sous-sinks
		 * distributor->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
		 * distributor->SetLevel(nkentseu::NkLogLevel::NK_INFO);
		 *
		 * // Ajout au logger global
		 * nkentseu::NkLog::Instance().AddSink(distributor);
		 * @endcode
		 *
		 * @example Gestion dynamique à runtime
		 * @code
		 * // Ajout d'un sink de debug temporairement
		 * auto debugSink = nkentseu::memory::MakeShared<nkentseu::NkFileSink>("debug.log");
		 * debugSink->SetLevel(nkentseu::NkLogLevel::NK_TRACE);
		 * distributor->AddSink(debugSink);
		 *
		 * // ... période de debugging ...
		 *
		 * // Retrait du sink de debug après investigation
		 * distributor->RemoveSink(debugSink);
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkDistributingSink : public NkISink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup DistributingSinkCtors Constructeurs de NkDistributingSink
			 * @brief Initialisation avec ou sans liste de sous-sinks
			 */

			/**
			 * @brief Constructeur par défaut : collection vide de sous-sinks
			 * @ingroup DistributingSinkCtors
			 *
			 * @post m_Sinks vide, prêt pour ajout dynamique via AddSink()
			 * @post Mutex interne initialisé, thread-safe dès la construction
			 * @note noexcept implicite : aucune allocation, aucune exception possible
			 *
			 * @example
			 * @code
			 * // Construction vide puis ajout progressif
			 * auto distributor = nkentseu::memory::MakeShared<nkentseu::NkDistributingSink>();
			 * distributor->AddSink(consoleSink);
			 * distributor->AddSink(fileSink);
			 * @endcode
			 */
			NkDistributingSink();

			/**
			 * @brief Constructeur avec liste initiale de sous-sinks
			 * @ingroup DistributingSinkCtors
			 *
			 * @param sinks Vecteur de shared_ptr vers NkISink à ajouter initialement
			 * @post m_Sinks initialisé avec une copie des shared_ptr fournis
			 * @post Ownership partagé : les sinks peuvent être réutilisés ailleurs
			 * @note Thread-safe : construction sans verrouillage requis (premier accès)
			 *
			 * @example
			 * @code
			 * // Construction avec configuration initiale
			 * nkentseu::NkVector<nkentseu::memory::NkSharedPtr<nkentseu::NkISink>> initialSinks;
			 * initialSinks.PushBack(consoleSink);
			 * initialSinks.PushBack(fileSink);
			 *
			 * auto distributor = nkentseu::memory::MakeShared<nkentseu::NkDistributingSink>(initialSinks);
			 * @endcode
			 */
			explicit NkDistributingSink(const NkVector<memory::NkSharedPtr<NkISink>>& sinks);

			/**
			 * @brief Destructeur : libération des références partagées
			 * @ingroup DistributingSinkCtors
			 *
			 * @post Décrémentation des refcount des NkSharedPtr dans m_Sinks
			 * @post Les sinks partagés ne sont détruits que si plus aucun propriétaire
			 * @note noexcept implicite : destruction safe dans tous les contextes
			 */
			~NkDistributingSink() override;


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'INTERFACE NKISINK
			// -----------------------------------------------------------------
			/**
			 * @defgroup DistributingSinkInterface Implémentation de NkISink
			 * @brief Méthodes requises avec logique de distribution broadcast
			 */

			/**
			 * @brief Distribue un message de log à tous les sous-sinks valides
			 * @param message Message de log contenant les données et métadonnées
			 * @ingroup DistributingSinkInterface
			 *
			 * @note Filtrage précoce : retour immédiat si !IsEnabled() ou !ShouldLog()
			 * @note Itération thread-safe sur m_Sinks via acquisition de m_Mutex
			 * @note Appel de Log() sur chaque sous-sink non-null individuellement
			 * @note Les erreurs dans un sous-sink n'affectent pas les autres (isolation)
			 *
			 * @par Comportement en cas d'erreur sous-sink :
			 *  - Exception dans un sink : propagée vers l'appelant (pas de catch interne)
			 *  - Sink null dans la collection : ignoré silencieusement via check if (sink)
			 *  - Pour robustesse : envisager try/catch par sink si isolation requise
			 *
			 * @example
			 * @code
			 * nkentseu::NkLogMessage msg(
			 *     nkentseu::NkLogLevel::NK_ERROR,
			 *     "Connection failed to database"
			 * );
			 * distributor.Log(msg);  // Diffusé à console + fichier + réseau simultanément
			 * @endcode
			 */
			void Log(const NkLogMessage& message) override;

			/**
			 * @brief Force le flush de tous les sous-sinks pour persistance garantie
			 * @ingroup DistributingSinkInterface
			 *
			 * @note Itération thread-safe sur m_Sinks via acquisition de m_Mutex
			 * @note Appel de Flush() sur chaque sous-sink non-null individuellement
			 * @note Critique pour : crash recovery, debugging temps-réel, audits
			 * @note Les erreurs dans un sous-sink n'affectent pas les autres (isolation)
			 *
			 * @example
			 * @code
			 * // Flush après message critique pour persistance sur toutes les destinations
			 * logger.Error("Critical system failure");
			 * distributor.Flush();  // Force écriture console + fichier + réseau
			 * @endcode
			 */
			void Flush() override;

			/**
			 * @brief Définit le formatter pour tous les sous-sinks via clonage
			 * @param formatter Pointeur unique vers le formatter source à cloner
			 * @ingroup DistributingSinkInterface
			 *
			 * @note Clonage : chaque sous-sink reçoit une copie indépendante du formatter
			 * @note Pattern-based : le clone est créé via formatter->GetPattern()
			 * @note Thread-safe : synchronisé via m_Mutex pour modification de la collection
			 * @note Ownership : le formatter source est consommé (move semantics)
			 *
			 * @par Pourquoi cloner plutôt que partager ?
			 *  - Indépendance : chaque sink peut ensuite modifier son formatter localement
			 *  - Sécurité : pas de race condition sur un formatter partagé
			 *  - Flexibilité : permet des overrides locaux après configuration globale
			 *
			 * @example
			 * @code
			 * // Configuration globale propagée à tous les sinks
			 * auto globalFormatter = nkentseu::memory::MakeUnique<nkentseu::NkFormatter>(
			 *     "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] %v"
			 * );
			 * distributor.SetFormatter(std::move(globalFormatter));
			 *
			 * // Override local possible ensuite sur un sink spécifique
			 * if (auto* consoleSink = GetConsoleSink(distributor)) {
			 *     consoleSink->SetPattern("%L: %v");  // Pattern différent pour console
			 * }
			 * @endcode
			 */
			void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

			/**
			 * @brief Définit le pattern de formatage pour tous les sous-sinks
			 * @param pattern Chaîne de pattern style spdlog à propager
			 * @ingroup DistributingSinkInterface
			 *
			 * @note Propagation directe : SetPattern(pattern) appelé sur chaque sous-sink
			 * @note Thread-safe : synchronisé via m_Mutex pour itération sur la collection
			 * @note Efficace : pas de clonage requis, le pattern est une chaîne immutable
			 * @note Les sous-sinks peuvent ensuite override localement si nécessaire
			 *
			 * @example
			 * @code
			 * // Pattern global appliqué à toutes les destinations
			 * distributor.SetPattern("[%L] %v");
			 *
			 * // Pattern détaillé pour debugging
			 * distributor.SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%s:%#] %v");
			 * @endcode
			 */
			void SetPattern(const NkString& pattern) override;

			/**
			 * @brief Obtient le formatter du premier sous-sink non-null (si disponible)
			 * @return Pointeur brut vers le formatter, ou nullptr si aucun sink valide
			 * @ingroup DistributingSinkInterface
			 *
			 * @note Retourne le formatter du premier sink : comportement de convenance
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Pointeur non-possédé : ne pas delete, durée de vie liée au sous-sink
			 * @note Pour accès à tous les formatters : itérer via GetSinks()
			 *
			 * @example
			 * @code
			 * // Accès au formatter courant pour preview
			 * if (auto* fmt = distributor.GetFormatter()) {
			 *     nkentseu::NkString preview = fmt->Format(sampleMessage);
			 *     printf("Preview: %s\n", preview.CStr());
			 * }
			 * @endcode
			 */
			NkFormatter* GetFormatter() const override;

			/**
			 * @brief Obtient le pattern du premier sous-sink non-null (si disponible)
			 * @return Chaîne NkString contenant le pattern, ou vide si aucun sink valide
			 * @ingroup DistributingSinkInterface
			 *
			 * @note Retourne le pattern du premier sink : comportement de convenance
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Retour par valeur : copie safe pour usage prolongé
			 * @note Pour accès à tous les patterns : itérer via GetSinks()
			 *
			 * @example
			 * @code
			 * // Sauvegarde de la configuration courante
			 * nkentseu::NkString currentPattern = distributor.GetPattern();
			 * Config::SetString("logging.distributor.pattern", currentPattern.CStr());
			 * @endcode
			 */
			NkString GetPattern() const override;


			// -----------------------------------------------------------------
			// GESTION DYNAMIQUE DES SOUS-SINKS
			// -----------------------------------------------------------------
			/**
			 * @defgroup DistributingSinkManagement Gestion des Sous-Sinks
			 * @brief Méthodes pour modification dynamique de la collection de sinks
			 */

			/**
			 * @brief Ajoute un sous-sink à la collection de distribution
			 * @param sink Pointeur partagé vers le sink à ajouter
			 * @ingroup DistributingSinkManagement
			 *
			 * @post Le sink est ajouté en fin de m_Sinks via PushBack()
			 * @post Ownership partagé : le sink peut être utilisé ailleurs simultanément
			 * @note Thread-safe : synchronisé via m_Mutex pour modification de la collection
			 * @note Ignoré silencieusement si sink est nullptr (robustesse)
			 * @note Les futurs appels à Log()/Flush() incluront ce nouveau sink
			 *
			 * @example
			 * @code
			 * // Ajout dynamique d'un sink de debug pour investigation temporaire
			 * auto debugSink = nkentseu::memory::MakeShared<nkentseu::NkFileSink>("debug.log");
			 * debugSink->SetLevel(nkentseu::NkLogLevel::NK_TRACE);
			 * distributor.AddSink(debugSink);
			 *
			 * // Les logs suivants seront également écrits dans debug.log
			 * logger.Debug("Debug info now captured in debug.log");
			 * @endcode
			 */
			void AddSink(memory::NkSharedPtr<NkISink> sink);

			/**
			 * @brief Supprime un sous-sink spécifique de la collection de distribution
			 * @param sink Pointeur partagé vers le sink à retirer
			 * @ingroup DistributingSinkManagement
			 *
			 * @post Le premier sink correspondant (par adresse pointeur) est retiré de m_Sinks
			 * @post La référence partagée est décrémentée : destruction si dernier propriétaire
			 * @note Thread-safe : synchronisé via m_Mutex pour modification de la collection
			 * @note Comparaison par adresse (Get()) : deux shared_ptr vers même objet = égal
			 * @note Ignoré silencieusement si sink nullptr ou non trouvé dans la collection
			 *
			 * @example
			 * @code
			 * // Retrait d'un sink temporaire après investigation
			 * distributor.RemoveSink(debugSink);
			 *
			 * // Les logs suivants ne seront plus écrits dans debug.log
			 * logger.Debug("This debug message won't go to debug.log anymore");
			 * @endcode
			 */
			void RemoveSink(memory::NkSharedPtr<NkISink> sink);

			/**
			 * @brief Supprime tous les sous-sinks de la collection
			 * @ingroup DistributingSinkManagement
			 *
			 * @post m_Sinks vidé via Clear() : toutes les références partagées décrémentées
			 * @post Les sinks partagés ne sont détruits que si plus aucun propriétaire
			 * @note Thread-safe : synchronisé via m_Mutex pour modification de la collection
			 * @note Utile pour réinitialisation complète ou changement de configuration majeur
			 * @note Après Clear(), Log()/Flush() deviennent no-op jusqu'à nouvel ajout
			 *
			 * @example
			 * @code
			 * // Réinitialisation complète avant reconfiguration
			 * distributor.ClearSinks();
			 *
			 * // Ajout de nouvelle configuration
			 * distributor.AddSink(newConsoleSink);
			 * distributor.AddSink(newFileSink);
			 * @endcode
			 */
			void ClearSinks();

			/**
			 * @brief Obtient une copie de la collection courante de sous-sinks
			 * @return Vecteur contenant des copies des shared_ptr vers les sous-sinks
			 * @ingroup DistributingSinkManagement
			 *
			 * @note Retour par valeur : copie safe pour itération hors lock
			 * @note Thread-safe : lecture protégée via m_Mutex pendant la copie
			 * @note Ownership partagé : les sinks retournés peuvent être utilisés ailleurs
			 * @note La copie est figée au moment de l'appel : modifications ultérieures non reflétées
			 *
			 * @example
			 * @code
			 * // Inspection de la configuration courante
			 * auto sinks = distributor.GetSinks();
			 * for (const auto& sink : sinks) {
			 *     if (sink) {
			 *         printf("Sink: %s, Level: %s\n",
			 *             sink->GetName().CStr(),
			 *             nkentseu::NkLogLevelToString(sink->GetLevel()));
			 *     }
			 * }
			 * @endcode
			 */
			NkVector<memory::NkSharedPtr<NkISink>> GetSinks() const;

			/**
			 * @brief Obtient le nombre courant de sous-sinks dans la collection
			 * @return Nombre d'éléments dans m_Sinks
			 * @ingroup DistributingSinkManagement
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour affichage de statut, validation de configuration, debugging
			 * @note Retourne la taille au moment de l'appel : peut changer après retour
			 *
			 * @example
			 * @code
			 * // Logging de statut de configuration
			 * usize count = distributor.GetSinkCount();
			 * logger.Info("DistributingSink configured with {0} destinations", count);
			 * @endcode
			 */
			usize GetSinkCount() const;

			/**
			 * @brief Vérifie si un sink spécifique est présent dans la collection
			 * @param sink Pointeur partagé vers le sink à rechercher
			 * @return true si un sink avec même adresse pointeur est trouvé, false sinon
			 * @ingroup DistributingSinkManagement
			 *
			 * @note Comparaison par adresse (Get()) : deux shared_ptr vers même objet = égal
			 * @note Thread-safe : lecture protégée via m_Mutex pendant l'itération
			 * @note Linear search O(n) : acceptable pour petites collections, envisager hash pour grandes
			 * @note Ignoré silencieusement si sink est nullptr : retourne false
			 *
			 * @example
			 * @code
			 * // Éviter les doublons avant ajout
			 * if (!distributor.ContainsSink(consoleSink)) {
			 *     distributor.AddSink(consoleSink);
			 * } else {
			 *     logger.Debug("Console sink already registered, skipping duplicate");
			 * }
			 * @endcode
			 */
			bool ContainsSink(memory::NkSharedPtr<NkISink> sink) const;


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT INTERNE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup DistributingSinkMembers Membres de Données
			 * @brief État interne du distributeur (non exposé publiquement)
			 */

			/// @brief Collection des sous-sinks vers lesquels distribuer les messages
			/// @ingroup DistributingSinkMembers
			/// @note Ownership partagé via NkSharedPtr : sinks réutilisables ailleurs
			/// @note Modifiable via AddSink/RemoveSink/ClearSinks (thread-safe)
			NkVector<memory::NkSharedPtr<NkISink>> m_Sinks;

			/// @brief Mutex pour synchronisation thread-safe des opérations sur m_Sinks
			/// @ingroup DistributingSinkMembers
			/// @note mutable : permet modification dans les méthodes const (GetSinks, etc.)
			/// @note Protège l'itération et la modification de la collection
			mutable threading::NkMutex m_Mutex;


		}; // class NkDistributingSink


	} // namespace nkentseu


#endif // NKENTSEU_NKDISTRIBUTINGSINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKDISTRIBUTINGSINK.H
// =============================================================================
// Ce fichier fournit l'implémentation de sink composite pour diffusion broadcast.
// Il combine flexibilité de configuration et thread-safety pour usage production.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration multi-destination classique
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDistributingSink.h>
	#include <NKLogger/Sinks/NkConsoleSink.h>
	#include <NKLogger/Sinks/NkFileSink.h>
	#include <NKLogger/Sinks/NkRotatingFileSink.h>

	void SetupProductionLogging() {
		using namespace nkentseu;

		// Création du distributeur principal
		auto distributor = memory::MakeShared<NkDistributingSink>();

		// Destination 1 : Console pour visibilité immédiate en dev/ops
		auto consoleSink = memory::MakeShared<NkConsoleSink>();
		consoleSink->SetLevel(NkLogLevel::NK_WARN);  // Seulement warnings+ en console
		consoleSink->SetColorEnabled(true);
		distributor->AddSink(consoleSink);

		// Destination 2 : Fichier courant pour persistance
		auto fileSink = memory::MakeShared<NkFileSink>("logs/app.log");
		fileSink->SetLevel(NkLogLevel::NK_INFO);  // Info+ en fichier
		fileSink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
		distributor->AddSink(fileSink);

		// Destination 3 : Fichier tournant pour archivage long terme
		auto archiveSink = memory::MakeShared<NkRotatingFileSink>(
			"logs/archive.log",
			100 * 1024 * 1024,  // 100 MB max par fichier
			10                   // Conserver 10 backups
		);
		archiveSink->SetLevel(NkLogLevel::NK_DEBUG);  // Debug+ pour archive
		distributor->AddSink(archiveSink);

		// Configuration globale propagée à tous les sinks
		distributor->SetLevel(NkLogLevel::NK_DEBUG);  // Niveau global
		distributor->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] %v");

		// Ajout au logger global
		NkLog::Instance().AddSink(distributor);

		// Logging : diffusé simultanément vers les 3 destinations
		logger.Info("Production logging configured with 3 destinations");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Gestion dynamique à runtime pour debugging temporaire
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDistributingSink.h>
	#include <NKLogger/Sinks/NkFileSink.h>

	class DebugSession {
	public:
		static void Start(nkentseu::NkDistributingSink& distributor, const nkentseu::NkString& debugFile) {
			using namespace nkentseu;

			// Création d'un sink de debug dédié
			auto debugSink = memory::MakeShared<NkFileSink>(debugFile, true);  // truncate
			debugSink->SetLevel(NkLogLevel::NK_TRACE);  // Verbose maximal
			debugSink->SetPattern("[%H:%M:%S.%e] [%f:%#] %v");  // Pattern compact avec source

			// Ajout temporaire au distributeur
			distributor.AddSink(debugSink);
			m_DebugSink = debugSink;  // Garder référence pour retrait ultérieur

			logger.Info("Debug session started: verbose logging to {0}", debugFile);
		}

		static void Stop(nkentseu::NkDistributingSink& distributor) {
			if (m_DebugSink) {
				distributor.RemoveSink(m_DebugSink);
				m_DebugSink.Reset();  // Libération de la référence partagée
				logger.Info("Debug session ended: verbose logging disabled");
			}
		}

	private:
		static nkentseu::memory::NkSharedPtr<nkentseu::NkISink> m_DebugSink;
	};

	// Variable statique pour référence au sink de debug
	nkentseu::memory::NkSharedPtr<nkentseu::NkISink> DebugSession::m_DebugSink;

	// Usage : activer/désactiver via commande admin ou signal
	// DebugSession::Start(distributor, "logs/debug_session_20240115.log");
	// ... période d'investigation ...
	// DebugSession::Stop(distributor);
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Configuration hiérarchique avec overrides locaux
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDistributingSink.h>

	void SetupHierarchicalLogging() {
		using namespace nkentseu;

		auto distributor = memory::MakeShared<NkDistributingSink>();

		// Configuration globale appliquée à tous les sinks
		distributor->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");

		// Sink console : pattern court pour lisibilité terminal
		auto consoleSink = memory::MakeShared<NkConsoleSink>();
		consoleSink->SetPattern("[%L] %v");  // Override local
		consoleSink->SetColorEnabled(true);
		distributor->AddSink(consoleSink);

		// Sink fichier : pattern détaillé pour analyse post-mortem
		auto fileSink = memory::MakeShared<NkFileSink>("logs/app.log");
		fileSink->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%s:%# in %f] %v");  // Override local
		distributor->AddSink(fileSink);

		// Les autres sinks héritent du pattern global
		// ... ajout d'autres sinks ...

		logger.Info("Hierarchical logging: global pattern with local overrides");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Inspection et monitoring de la configuration
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDistributingSink.h>

	void LogDistributorStatus(const nkentseu::NkDistributingSink& distributor) {
		using namespace nkentseu;

		// Informations de base
		usize count = distributor.GetSinkCount();
		NkString pattern = distributor.GetPattern();

		logger.Info("Distributor status: {0} sinks, pattern: '{1}'", count, pattern);

		// Inspection détaillée de chaque sink
		auto sinks = distributor.GetSinks();
		for (usize i = 0; i < sinks.Size(); ++i) {
			const auto& sink = sinks[i];
			if (sink) {
				logger.Debug("Sink[{0}]: name='{1}', level={2}, open={3}",
					i,
					sink->GetName(),
					NkLogLevelToString(sink->GetLevel()),
					sink->IsEnabled() ? "yes" : "no");
			}
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Testing avec vérification de distribution
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDistributingSink.h>
	#include <NKLogger/Sinks/NkNullSink.h>
	#include <NKLogger/NkLogMessage.h>
	#include <gtest/gtest.h>

	class CountingSink : public nkentseu::NkISink {
	public:
		void Log(const nkentseu::NkLogMessage&) override { ++m_Count; }
		void Flush() override {}
		void SetFormatter(nkentseu::memory::NkUniquePtr<nkentseu::NkFormatter>) override {}
		void SetPattern(const nkentseu::NkString&) override {}
		nkentseu::NkFormatter* GetFormatter() const override { return nullptr; }
		nkentseu::NkString GetPattern() const override { return {}; }
		usize GetCount() const { return m_Count; }
		void Reset() { m_Count = 0; }
	private:
		usize m_Count = 0;
	};

	TEST(NkDistributingSinkTest, BroadcastToAllSinks) {
		using namespace nkentseu;

		// Setup : distributeur avec 3 sinks de comptage
		NkDistributingSink distributor;
		auto sink1 = memory::MakeShared<CountingSink>();
		auto sink2 = memory::MakeShared<CountingSink>();
		auto sink3 = memory::MakeShared<CountingSink>();

		distributor.AddSink(sink1);
		distributor.AddSink(sink2);
		distributor.AddSink(sink3);

		// Execution : un seul message loggé
		NkLogMessage msg(NkLogLevel::NK_INFO, "Test broadcast");
		distributor.Log(msg);

		// Verification : chaque sink a reçu exactement un appel
		EXPECT_EQ(sink1->GetCount(), 1u);
		EXPECT_EQ(sink2->GetCount(), 1u);
		EXPECT_EQ(sink3->GetCount(), 1u);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Configuration via fichier avec fallback robuste
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDistributingSink.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkSinkPtr CreateDistributingSinkFromConfig(const nkentseu::NkString& section) {
		using namespace nkentseu;

		// Création du distributeur principal
		auto distributor = memory::MakeShared<NkDistributingSink>();

		// Lecture de la liste des sous-sinks depuis configuration
		auto sinkConfigs = core::Config::GetArray(section + ".sinks");

		for (const auto& sinkConfig : sinkConfigs) {
			NkString type = sinkConfig.GetString("type", "null");
			NkString filepath = sinkConfig.GetString("filepath", "");
			usize maxSize = sinkConfig.GetUint64("maxSize", 0);
			usize maxFiles = sinkConfig.GetUint64("maxFiles", 0);
			NkLogLevel level = NkLogLevelFromString(sinkConfig.GetString("level", "info"));

			memory::NkSharedPtr<NkISink> sink;

			if (type == "console") {
				sink = memory::MakeShared<NkConsoleSink>();
			} else if (type == "file" && !filepath.Empty()) {
				sink = memory::MakeShared<NkFileSink>(filepath);
			} else if (type == "rotating" && !filepath.Empty() && maxSize > 0) {
				sink = memory::MakeShared<NkRotatingFileSink>(filepath, maxSize, maxFiles);
			} else {
				// Fallback robuste : NkNullSink pour configuration invalide
				sink = memory::MakeShared<NkNullSink>();
			}

			// Configuration individuelle du sink
			sink->SetLevel(level);
			distributor->AddSink(sink);
		}

		// Configuration globale du distributeur
		distributor->SetPattern(core::Config::GetString(section + ".pattern", NkFormatter::NK_DEFAULT_PATTERN));
		distributor->SetLevel(NkLogLevelFromString(core::Config::GetString(section + ".level", "info")));

		return distributor;
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. THREAD-SAFETY ET ITÉRATION SUR COLLECTION :
	   - Toutes les méthodes acquièrent m_Mutex avant d'itérer sur m_Sinks
	   - Log()/Flush() : itération avec check if (sink) pour robustesse null
	   - GetSinks() : retourne une copie pour éviter l'itération hors lock
	   - Les sous-sinks partagés doivent être thread-safe individuellement

	2. CLONAGE VS PARTAGE DE FORMATTER :
	   - SetFormatter() clone via GetPattern() : indépendance des sinks
	   - Avantage : chaque sink peut ensuite modifier son formatter localement
	   - Inconvénient : overhead de clonage si nombreux sinks + patterns complexes
	   - Alternative : partager le formatter si immutabilité garantie (à implémenter)

	3. GESTION DES ERREURS DANS LES SOUS-SINKS :
	   - Exception dans un sink Log()/Flush() : propagée vers l'appelant
	   - Pour isolation stricte : envelopper chaque appel dans try/catch
	   - Trade-off : robustesse vs visibilité des erreurs de sous-sinks
	   - Recommandation : logging d'erreur interne + propagation optionnelle

	4. PERFORMANCE DE LA DISTRIBUTION :
	   - Log() appelle Log() sur chaque sous-sink : overhead linéaire O(n)
	   - Pour haute fréquence : envisager buffering + flush batché
	   - Filtrage précoce via ShouldLog() évite distribution si message ignoré
	   - Pour optimisation : trier les sinks par probabilité de filtrage

	5. GESTION MÉMOIRE VIA SHARED_PTR :
	   - m_Sinks contient des NkSharedPtr : refcount géré automatiquement
	   - AddSink() incrémente le refcount, RemoveSink() le décrémente
	   - Destruction du distributeur : décrémentation sans destruction forcée
	   - Safe pour partage : un sink peut appartenir à plusieurs distributeurs

	6. COMPARAISON DE SINKS PAR ADRESSE :
	   - ContainsSink/RemoveSink comparent via Get() (adresse pointeur)
	   - Deux shared_ptr vers même objet = considérés égaux
	   - Attention : deux sinks différents avec même configuration ≠ égaux
	   - Pour comparaison sémantique : implémenter operator== dans NkISink si requis

	7. EXTENSIBILITÉ FUTURES :
	   - Filtrage par nom/type : ajouter ShouldForward(sink, message) pour routing conditionnel
	   - Priorité de distribution : ordonner m_Sinks par priorité pour traitement séquentiel
	   - Async distribution : queue de messages + thread worker pour non-blocking Log()
	   - Métriques : compter les messages distribués/ignorés par sous-sink pour monitoring

	8. TESTING ET VALIDATION :
	   - Utiliser CountingSink ou MockSink pour vérifier la distribution
	   - Tester les cas limites : collection vide, sinks null, exceptions dans sous-sinks
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
	   - Benchmarking : mesurer l'overhead de distribution vs sink unique
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================