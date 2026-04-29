// =============================================================================
// NKLogger/NkAsyncSink.h
// Logger asynchrone avec file d'attente pour performances élevées.
//
// Design :
//  - Héritage de NkLogger : réutilisation complète de l'infrastructure de logging
//  - File d'attente lock-free ou mutex-protected pour découplage producteur/consommateur
//  - Thread worker dédié : traitement des logs en arrière-plan sans bloquer l'appelant
//  - Flush périodique ou sur demande : garantie de persistance des messages critiques
//  - Gestion de backpressure : comportement configurable quand la file est pleine
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Propriétaire - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKASYNCLOGGER_H
#define NKENTSEU_NKASYNCLOGGER_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour le threading et le formatage.
	// Dépendances projet pour le logger de base, les files d'attente et la synchronisation.

	#include <cstdarg>

	#include "NKCore/NkTypes.h"
	#include "NKCore/NkTraits.h"
	#include "NKCore/NkAtomic.h"
	#include "NKContainers/String/NkFormat.h"
	#include "NKContainers/String/NkString.h"
	#include "NKContainers/String/NkStringView.h"
	#include "NKContainers/Adapters/NkQueue.h"
	#include "NKThreading/NkMutex.h"
	#include "NKThreading/NkConditionVariable.h"
	#include "NKThreading/NkThread.h"
	#include "NKThreading/NkScopedLock.h"
	#include "NKLogger/NkLogger.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// ÉNUMÉRATION : NkAsyncOverflowPolicy
		// DESCRIPTION : Stratégies de gestion quand la file d'attente est pleine
		// ---------------------------------------------------------------------
		/**
		 * @enum NkAsyncOverflowPolicy
		 * @brief Politiques de gestion de débordement de la file d'attente asynchrone
		 * @ingroup LoggerTypes
		 *
		 * Définit le comportement du logger asynchrone lorsque la file d'attente
		 * atteint sa capacité maximale (m_MaxQueueSize).
		 *
		 * @note Le choix de la politique impacte la fiabilité vs la performance :
		 *   - DropOldest : perte de logs anciens, garantie de traitement des récents
		 *   - DropNewest : perte de logs récents, préservation du contexte historique
		 *   - Block : garantie de traitement complet, mais risque de blocage de l'appelant
		 *
		 * @example
		 * @code
		 * // Pour monitoring haute fréquence : privilégier les logs récents
		 * asyncLogger.SetOverflowPolicy(nkentseu::NkAsyncOverflowPolicy::NK_DROP_OLDEST);
		 *
		 * // Pour audit critique : bloquer plutôt que perdre des logs
		 * asyncLogger.SetOverflowPolicy(nkentseu::NkAsyncOverflowPolicy::NK_BLOCK);
		 * @endcode
		 */
		enum class NKENTSEU_LOGGER_CLASS_EXPORT NkAsyncOverflowPolicy : uint8 {
			/// @brief Supprimer le message le plus ancien de la file pour faire de la place
			NK_DROP_OLDEST = 0,

			/// @brief Supprimer le nouveau message (celui en cours d'ajout) et continuer
			NK_DROP_NEWEST = 1,

			/// @brief Bloquer l'appelant jusqu'à ce qu'une place se libère dans la file
			NK_BLOCK = 2
		};


		// ---------------------------------------------------------------------
		// CLASSE : NkAsyncLogger
		// DESCRIPTION : Logger asynchrone avec traitement en arrière-plan
		// ---------------------------------------------------------------------
		/**
		 * @class NkAsyncLogger
		 * @brief Logger asynchrone avec file d'attente et thread worker dédié
		 * @ingroup LoggerComponents
		 *
		 * NkAsyncLogger fournit un logging haute performance via découplage :
		 *  - Thread appelant : enqueue rapide du message dans une file lock-free ou mutex-protected
		 *  - Thread worker : traitement asynchrone des messages vers les sinks configurés
		 *  - Synchronisation minimale : condition variable pour wake-up efficace du worker
		 *
		 * Architecture :
		 *  - Hérite de NkLogger : réutilisation complète de l'API de logging (Log, Logf, etc.)
		 *  - File d'attente circulaire ou dynamique : NkQueue<NkLogMessage> avec capacité configurable
		 *  - Thread worker : boucle de traitement avec sleep/wait pour économie CPU
		 *  - Flush périodique : garantie de persistance même sans nouveaux messages
		 *
		 * Thread-safety :
		 *  - Enqueue() : thread-safe pour appels concurrents depuis multiples threads producteurs
		 *  - WorkerThread() : exécution séquentielle dans le thread dédié, pas de concurrence interne
		 *  - Sinks : doivent être thread-safe individuellement car appelés depuis le worker thread
		 *
		 * Gestion de la mémoire :
		 *  - NkLogMessage copié dans la file : isolation entre producteur et consommateur
		 *  - Pas d'allocation dans le chemin critique d'enqueue (sauf si file dynamique et croissance)
		 *  - Flush() force le vidage de la file : garantie de persistance avant shutdown
		 *
		 * @note Le logger asynchrone est conçu pour les applications haute fréquence où
		 *       le logging ne doit pas impacter la latence du thread principal.
		 *
		 * @example Usage basique
		 * @code
		 * // Création avec file de 8192 messages et flush toutes les 1000ms
		 * auto asyncLogger = nkentseu::memory::MakeShared<nkentseu::NkAsyncLogger>(
		 *     "AsyncApp",      // Nom du logger
		 *     8192,            // Taille max de la file
		 *     1000             // Intervalle de flush en ms
		 * );
		 *
		 * // Ajout de sinks comme pour un logger normal
		 * asyncLogger->AddSink(nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>());
		 * asyncLogger->AddSink(nkentseu::memory::MakeShared<nkentseu::NkFileSink>("app.log"));
		 *
		 * // Démarrage du thread worker
		 * asyncLogger->Start();
		 *
		 * // Logging : retour immédiat, traitement en arrière-plan
		 * asyncLogger->Info("High-frequency event {0}", eventId);
		 *
		 * // Arrêt propre : flush et vidage de la file avant destruction
		 * asyncLogger->Stop();
		 * @endcode
		 *
		 * @example Gestion de débordement pour monitoring haute fréquence
		 * @code
		 * // Pour un système de métriques où les logs récents sont plus importants
		 * asyncLogger->SetOverflowPolicy(nkentseu::NkAsyncOverflowPolicy::NK_DROP_OLDEST);
		 * asyncLogger->SetMaxQueueSize(16384);  // File plus grande pour absorber les pics
		 *
		 * // En cas de pic de trafic : les anciens logs sont supprimés, les récents préservés
		 * for (int i = 0; i < 100000; ++i) {
		 *     asyncLogger->Debug("Metric sample {0}", i);  // Enqueue rapide, pas de blocage
		 * }
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkAsyncLogger : public NkLogger {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup AsyncLoggerCtors Constructeurs de NkAsyncLogger
			 * @brief Initialisation avec configuration de la file d'attente et du worker
			 */

			/**
			 * @brief Constructeur avec nom et paramètres de configuration asynchrone
			 * @ingroup AsyncLoggerCtors
			 *
			 * @param name Nom identifiant le logger (utilisé dans les messages et pour debugging)
			 * @param queueSize Taille maximum de la file d'attente (nombre de messages)
			 * @param flushInterval Intervalle de flush périodique en millisecondes (0 = désactivé)
			 * @post File d'attente initialisée avec capacité queueSize
			 * @post m_FlushInterval configuré pour wake-up périodique du worker si > 0
			 * @post Worker thread non démarré : appeler Start() explicitement pour activation
			 * @note Thread-safe : construction sans verrouillage requis (premier accès)
			 *
			 * @par Choix des paramètres :
			 *  - queueSize : équilibre entre mémoire utilisée et capacité d'absorption des pics
			 *  - flushInterval : 0 = flush uniquement sur demande, >0 = flush périodique automatique
			 *
			 * @example
			 * @code
			 * // Configuration pour application interactive : file moyenne, flush fréquent
			 * nkentseu::NkAsyncLogger logger("InteractiveApp", 4096, 500);  // 500ms de flush
			 *
			 * // Configuration pour serveur haute fréquence : file grande, flush moins fréquent
			 * nkentseu::NkAsyncLogger serverLogger("HighFreqServer", 32768, 2000);  // 2s de flush
			 * @endcode
			 */
			NkAsyncLogger(const NkString& name, usize queueSize = 8192, uint32 flushInterval = 1000);

			/**
			 * @brief Destructeur : arrêt propre du worker et vidage de la file
			 * @ingroup AsyncLoggerCtors
			 *
			 * @post Stop() appelé automatiquement : flush et vidage de la file avant destruction
			 * @post Thread worker joint : garantie de terminaison avant retour du destructeur
			 * @note Thread-safe : safe pour destruction depuis n'importe quel thread
			 * @note Garantit qu'aucun message en file n'est perdu à la destruction
			 */
			~NkAsyncLogger();


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'API NKLOGGER (ASYNC-OVERRIDE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup AsyncLoggerOverrides Overrides de NkLogger
			 * @brief Méthodes de logging réimplémentées pour comportement asynchrone
			 */

			/**
			 * @brief Log asynchrone avec formatage printf-style via NkFormatf
			 * @param level Niveau de log pour ce message
			 * @param format Chaîne de format style printf (%s, %d, %.2f, etc.)
			 * @param ... Arguments variadiques correspondant aux spécificateurs
			 * @ingroup AsyncLoggerOverrides
			 *
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Formatage via NkFormatf() de NKCore/NkFormat.h pour cohérence projet
			 * @note Enqueue du message formaté dans la file d'attente : retour immédiat
			 * @note Thread-safe : enqueue protégé par mutex pour accès concurrent à la file
			 *
			 * @par Performance :
			 *  - Overhead minimal : formatage + enqueue typiquement < 10 µs
			 *  - Pas d'I/O bloquant dans le thread appelant : traitement déporté au worker
			 *  - Scalabilité : multiple threads producteurs sans contention sur les sinks
			 *
			 * @example
			 * @code
			 * // Logging haute fréquence sans impact sur la latence
			 * asyncLogger->Logf(nkentseu::NkLogLevel::NK_DEBUG,
			 *     "Request processed: user=%s, duration=%.3fms, status=%d",
			 *     userId.CStr(), durationMs, statusCode);
			 * // Retour immédiat, traitement asynchrone vers console/fichier/réseau
			 * @endcode
			 */
			void Logf(NkLogLevel level, const char* format, ...) override;

			/**
			 * @brief Log asynchrone avec formatage positionnel via NkFormat
			 * @tparam Args Types des arguments à formater (déduits automatiquement)
			 * @param level Niveau de log pour ce message
			 * @param format Chaîne de format avec placeholders {index:props}
			 * @param args Pack d'arguments à insérer aux positions indiquées
			 * @ingroup AsyncLoggerOverrides
			 *
			 * @note SFINAE : désactivé si sizeof...(Args) == 0 pour éviter l'ambiguïté
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Formatage via NkFormatIndexed() → NkFormat() unifié de NKCore/NkFormat.h
			 * @note Enqueue du message formaté dans la file : retour immédiat
			 * @note Thread-safe : enqueue protégé par mutex pour accès concurrent à la file
			 *
			 * @example
			 * @code
			 * // Formatage positionnel type-safe avec arguments variadiques
			 * asyncLogger->Log(nkentseu::NkLogLevel::NK_INFO,
			 *     "User {0} performed action {1} with result {2:hex}",
			 *     username, actionName, resultCode);
			 * @endcode
			 */
			template<typename... Args,
				typename = traits::NkEnableIf_t<(sizeof...(Args) > 0)>>
			void Log(NkLogLevel level, NkStringView format, Args&&... args);

			/**
			 * @brief Log asynchrone avec message pré-formaté (stream-style)
			 * @param level Niveau de log pour ce message
			 * @param message Chaîne NkString contenant le message déjà formaté
			 * @ingroup AsyncLoggerOverrides
			 *
			 * @note Filtrage précoce : retour immédiat si !ShouldLog(level)
			 * @note Aucun formatage supplémentaire : le message est utilisé tel quel
			 * @note Enqueue du message dans la file : retour immédiat
			 * @note Thread-safe : enqueue protégé par mutex pour accès concurrent à la file
			 *
			 * @example
			 * @code
			 * // Logging de messages pré-formatés ou littéraux
			 * asyncLogger->Log(nkentseu::NkLogLevel::NK_INFO, "Simple status message");
			 *
			 * // Ou avec message formaté en amont
			 * nkentseu::NkString preformatted = BuildComplexMessage();
			 * asyncLogger->Log(nkentseu::NkLogLevel::NK_DEBUG, preformatted);
			 * @endcode
			 */
			void Log(NkLogLevel level, const NkString& message);

			/**
			 * @brief Force le flush des messages en attente et des sinks
			 * @ingroup AsyncLoggerOverrides
			 *
			 * @post FlushQueue() : vidage immédiat de tous les messages en file
			 * @post NkLogger::Flush() : flush des buffers des sinks sous-jacents
			 * @note Thread-safe : synchronisé pour éviter les race conditions avec le worker
			 * @note Critique pour : crash recovery, debugging temps-réel, audits
			 * @note Appelée automatiquement dans Stop() et ~NkAsyncLogger()
			 *
			 * @example
			 * @code
			 * // Flush avant une opération critique pour garantir la persistance
			 * asyncLogger->Flush();
			 * PerformCriticalOperation();
			 *
			 * // Flush après un message fatal avant terminaison
			 * asyncLogger->Fatal("Unrecoverable error");
			 * asyncLogger->Flush();  // Optionnel : déjà fait dans Stop()
			 * @endcode
			 */
			void Flush() override;


			// -----------------------------------------------------------------
			// GESTION DU CYCLE DE VIE ASYNCHRONE
			// -----------------------------------------------------------------
			/**
			 * @defgroup AsyncLoggerLifecycle Gestion du Worker Thread
			 * @brief Méthodes pour contrôle du thread de traitement asynchrone
			 */

			/**
			 * @brief Démarre le thread worker de traitement asynchrone
			 * @ingroup AsyncLoggerLifecycle
			 *
			 * @post m_Running = true, m_StopRequested = false
			 * @post Thread worker créé et démarré via NkThread::Start()
			 * @post Worker entre dans sa boucle de traitement : wait/notify sur m_Condition
			 * @note Thread-safe : vérification atomique de m_Running pour éviter double démarrage
			 * @note Idempotent : appel multiple sans effet secondaire si déjà en cours
			 *
			 * @example
			 * @code
			 * // Démarrage après configuration complète des sinks
			 * asyncLogger->AddSink(consoleSink);
			 * asyncLogger->AddSink(fileSink);
			 * asyncLogger->Start();  // Activation du traitement asynchrone
			 *
			 * // Les appels Log() suivants seront traités en arrière-plan
			 * asyncLogger->Info("Logging started asynchronously");
			 * @endcode
			 */
			void Start();

			/**
			 * @brief Arrête proprement le thread worker et vide la file
			 * @ingroup AsyncLoggerLifecycle
			 *
			 * @post m_StopRequested = true, notification du worker via m_Condition.NotifyOne()
			 * @post Join() du thread worker : attente de terminaison avant retour
			 * @post FlushQueue() : vidage des messages restants avant arrêt complet
			 * @post m_Running = false : logger marqué comme inactif
			 * @note Thread-safe : synchronisation pour arrêt propre depuis n'importe quel thread
			 * @note Garantit qu'aucun message en file n'est perdu à l'arrêt
			 *
			 * @example
			 * @code
			 * // Arrêt propre avant terminaison de l'application
			 * asyncLogger->Stop();  // Flush + vidage + arrêt du worker
			 *
			 * // Après Stop(), les appels Log() sont ignorés (ShouldLog() peut encore passer,
			 * // mais Enqueue() échouera car m_Running == false)
			 * asyncLogger->Info("This message won't be processed");  // Ignoré
			 * @endcode
			 */
			void Stop();

			/**
			 * @brief Vérifie si le thread worker est actuellement en cours d'exécution
			 * @return true si m_Running == true, false sinon
			 * @ingroup AsyncLoggerLifecycle
			 *
			 * @note Lecture atomique de m_Running : thread-safe sans mutex
			 * @note Utile pour monitoring, debugging, ou logique conditionnelle
			 * @note Ne garantit pas que le worker est actif à l'instant T (race condition possible)
			 *
			 * @example
			 * @code
			 * // Logging de statut pour monitoring
			 * if (asyncLogger->IsRunning()) {
			 *     logger.Debug("Async logger active, queue size: {0}", asyncLogger->GetQueueSize());
			 * } else {
			 *     logger.Warn("Async logger not running, logs may be dropped");
			 * }
			 * @endcode
			 */
			bool IsRunning() const;


			// -----------------------------------------------------------------
			// CONFIGURATION DE LA FILE D'ATTENTE
			// -----------------------------------------------------------------
			/**
			 * @defgroup AsyncLoggerQueueConfig Configuration de la File d'Attente
			 * @brief Méthodes pour ajuster le comportement de la file asynchrone
			 */

			/**
			 * @brief Obtient le nombre courant de messages en attente dans la file
			 * @return Taille actuelle de m_MessageQueue
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : lecture protégée via m_QueueMutex
			 * @note Utile pour monitoring, alerting, ou ajustement dynamique de queueSize
			 * @note Retourne la taille au moment de l'appel : peut changer immédiatement après
			 *
			 * @example
			 * @code
			 * // Monitoring de la charge de logging
			 * usize queueSize = asyncLogger->GetQueueSize();
			 * if (queueSize > asyncLogger->GetMaxQueueSize() * 0.8) {
			 *     logger.Warn("Async queue at {0}% capacity", (queueSize * 100) / asyncLogger->GetMaxQueueSize());
			 * }
			 * @endcode
			 */
			usize GetQueueSize() const;

			/**
			 * @brief Définit la taille maximum de la file d'attente
			 * @param size Nouvelle capacité maximale (nombre de messages)
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : synchronisé via m_QueueMutex
			 * @note Modification effective immédiatement pour les prochains Enqueue()
			 * @note Réduction de size ne supprime pas les messages existants : seul l'ajout futur est affecté
			 * @note Augmentation de size peut nécessiter réallocation si file dynamique
			 *
			 * @example
			 * @code
			 * // Ajustement dynamique selon la charge système
			 * if (SystemLoadIsHigh()) {
			 *     asyncLogger->SetMaxQueueSize(4096);  // Réduire pour économiser la mémoire
			 * } else {
			 *     asyncLogger->SetMaxQueueSize(16384);  // Augmenter pour absorber les pics
			 * }
			 * @endcode
			 */
			void SetMaxQueueSize(usize size);

			/**
			 * @brief Obtient la taille maximum configurée de la file d'attente
			 * @return Capacité maximale de m_MessageQueue
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : lecture protégée via m_QueueMutex
			 * @note Utile pour affichage de configuration ou calculs de capacité
			 *
			 * @example
			 * @code
			 * // Calcul de l'utilisation relative de la file
			 * usize current = asyncLogger->GetQueueSize();
			 * usize max = asyncLogger->GetMaxQueueSize();
			 * double usagePercent = (max > 0) ? (100.0 * current / max) : 0.0;
			 * printf("Queue usage: %.1f%% (%zu/%zu)\n", usagePercent, current, max);
			 * @endcode
			 */
			usize GetMaxQueueSize() const;

			/**
			 * @brief Définit l'intervalle de flush périodique du worker
			 * @param ms Intervalle en millisecondes (0 = désactiver le flush périodique)
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : synchronisé via m_QueueMutex
			 * @note Modification effective au prochain cycle de wait du worker
			 * @note Utile pour équilibrer persistance (flush fréquent) vs performance (flush rare)
			 *
			 * @example
			 * @code
			 * // Flush plus fréquent en mode debug pour visibilité immédiate
			 * #ifdef NKENTSEU_DEBUG
			 *     asyncLogger->SetFlushInterval(100);  // 100ms
			 * #else
			 *     asyncLogger->SetFlushInterval(2000);  // 2s en production
			 * #endif
			 * @endcode
			 */
			void SetFlushInterval(uint32 ms);

			/**
			 * @brief Obtient l'intervalle de flush périodique configuré
			 * @return Intervalle en millisecondes (0 = désactivé)
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : lecture protégée via m_QueueMutex
			 * @note Utile pour affichage de configuration ou validation de setup
			 *
			 * @example
			 * @code
			 * // Logging de configuration au startup
			 * uint32 flushMs = asyncLogger->GetFlushInterval();
			 * logger.Info("Async logger flush interval: {0}ms", flushMs > 0 ? flushMs : "on-demand");
			 * @endcode
			 */
			uint32 GetFlushInterval() const;

			/**
			 * @brief Définit la politique de gestion de débordement de la file
			 * @param policy Nouvelle politique à appliquer (NK_DROP_OLDEST, NK_DROP_NEWEST, NK_BLOCK)
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : synchronisé via m_QueueMutex
			 * @note Modification effective immédiatement pour les prochains Enqueue() quand file pleine
			 * @note Choix critique : impacte la fiabilité vs la performance en situation de pic
			 *
			 * @example
			 * @code
			 * // Pour système de métriques : privilégier les logs récents
			 * asyncLogger->SetOverflowPolicy(nkentseu::NkAsyncOverflowPolicy::NK_DROP_OLDEST);
			 *
			 * // Pour audit financier : bloquer plutôt que perdre des logs
			 * asyncLogger->SetOverflowPolicy(nkentseu::NkAsyncOverflowPolicy::NK_BLOCK);
			 * @endcode
			 */
			void SetOverflowPolicy(NkAsyncOverflowPolicy policy);

			/**
			 * @brief Obtient la politique de débordement configurée
			 * @return NkAsyncOverflowPolicy courant
			 * @ingroup AsyncLoggerQueueConfig
			 *
			 * @note Thread-safe : lecture protégée via m_QueueMutex
			 * @note Utile pour affichage de configuration ou validation de comportement attendu
			 *
			 * @example
			 * @code
			 * // Vérification que la politique correspond aux exigences métier
			 * auto policy = asyncLogger->GetOverflowPolicy();
			 * if (policy == nkentseu::NkAsyncOverflowPolicy::NK_BLOCK && IsLatencySensitive()) {
			 *     logger.Warn("Blocking overflow policy may impact latency in sensitive paths");
			 * }
			 * @endcode
			 */
			NkAsyncOverflowPolicy GetOverflowPolicy() const;


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// MÉTHODES PRIVÉES D'IMPLÉMENTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup AsyncLoggerPrivate Méthodes Privées
			 * @brief Fonctions internes pour gestion de la file et du worker thread
			 */

			/**
			 * @brief Boucle principale du thread worker de traitement asynchrone
			 * @ingroup AsyncLoggerPrivate
			 *
			 * @note Exécutée dans le thread dédié m_WorkerThread
			 * @note Boucle infinie jusqu'à m_StopRequested == true
			 * @note Wait/Notify sur m_Condition pour économie CPU : sleep quand file vide
			 * @note Traitement batch : vidage de plusieurs messages par cycle si disponible
			 * @note Flush périodique : wake-up toutes les m_FlushInterval ms même si file vide
			 *
			 * @par Algorithme :
			 * @code
			 * while (!m_StopRequested) {
			 *     lock(m_QueueMutex);
			 *     while (queue.empty() && !stop) {
			 *         if (flushInterval > 0)
			 *             condition.wait_for(lock, flushInterval);  // Wake-up périodique
			 *         else
			 *             condition.wait(lock);  // Wake-up uniquement sur notification
			 *     }
			 *     if (!queue.empty()) {
			 *         msg = queue.pop_front();
			 *     }
			 *     unlock(m_QueueMutex);
			 *     if (hasMessage) process(msg);  // Traitement hors lock pour performance
			 * }
			 * @endcode
			 *
			 * @warning Ne pas appeler directement : exécutée uniquement par NkThread
			 */
			void WorkerThread();

			/**
			 * @brief Point d'entrée C-compatible pour NkThread::Start()
			 * @param userData Pointeur vers l'instance NkAsyncLogger (passé via Start())
			 * @ingroup AsyncLoggerPrivate
			 *
			 * @note Fonction statique requise par l'API NkThread::Start(void(*)(void*))
			 * @note Cast de userData vers NkAsyncLogger* et délégation à WorkerThread()
			 * @note Gestion de nullptr : retour silencieux si userData invalide
			 *
			 * @warning Ne pas appeler directement : utilisé uniquement par NkThread
			 */
			static void WorkerThreadEntry(void* userData);

			/**
			 * @brief Ajoute un message à la file d'attente avec gestion de débordement
			 * @param message Message de log à enqueue
			 * @return true si message ajouté avec succès, false si échec (file pleine + policy NK_DROP_NEWEST)
			 * @ingroup AsyncLoggerPrivate
			 *
			 * @note Thread-safe : synchronisé via m_QueueMutex pour accès concurrent à la file
			 * @note Gestion de débordement selon m_OverflowPolicy :
			 *   - NK_DROP_OLDEST : pop_front() avant push_back() si file pleine
			 *   - NK_DROP_NEWEST : retour false sans modification si file pleine
			 *   - NK_BLOCK : wait() jusqu'à ce qu'une place se libère (peut bloquer indéfiniment)
			 * @note Notification du worker via m_Condition.NotifyOne() après ajout réussi
			 *
			 * @par Performance :
			 *  - Cas normal (file non pleine) : O(1) avec mutex uniquement
			 *  - Cas NK_BLOCK : potentiellement bloquant si worker lent ou file saturée
			 *  - Cas NK_DROP_* : O(1) avec décision immédiate
			 *
			 * @warning En situation de pic prolongé avec NK_DROP_OLDEST : perte potentielle de logs anciens
			 */
			bool Enqueue(const NkLogMessage& message);

			/**
			 * @brief Traite un message en le distribuant à tous les sinks configurés
			 * @param message Message de log à traiter
			 * @ingroup AsyncLoggerPrivate
			 *
			 * @note Acquisition du mutex de base NkLogger (m_Mutex) pour itération thread-safe sur m_Sinks
			 * @note Appel de Log() sur chaque sink non-null : filtrage local appliqué si configuré
			 * @note Les erreurs dans un sink n'affectent pas les autres : isolation par try/catch optionnel
			 * @note Exécuté hors du lock de la file : permet traitement parallèle enqueue/process
			 *
			 * @par Gestion d'erreurs :
			 *  - Exception dans un sink : propagée vers le worker thread (peut terminer le worker)
			 *  - Pour robustesse : envisager try/catch interne avec logging d'erreur
			 *  - Alternative : wrapper des sinks avec gestion d'erreurs centralisée
			 *
			 * @warning Ne pas appeler directement avec lock de file acquis : risque de deadlock
			 */
			void ProcessMessage(const NkLogMessage& message);

			/**
			 * @brief Vide complètement la file d'attente en traitant tous les messages restants
			 * @ingroup AsyncLoggerPrivate
			 *
			 * @note Boucle de vidage : pop + process jusqu'à file vide
			 * @note Thread-safe : synchronisé via m_QueueMutex pour accès exclusif à la file
			 * @note Utilisé dans Flush() et Stop() pour garantie de persistance
			 * @note Peut être appelé depuis n'importe quel thread : safe avec synchronisation
			 *
			 * @par Performance :
			 *  - O(n) avec n = nombre de messages en file au moment de l'appel
			 *  - Traitement séquentiel : chaque message traité individuellement
			 *  - Pour gros volumes : envisager traitement batch si sinks supportent
			 *
			 * @warning En situation de shutdown urgent : FlushQueue() peut prendre du temps si file pleine
			 */
			void FlushQueue();


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT ASYNCHRONE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup AsyncLoggerMembers Membres de Données
			 * @brief État interne du logger asynchrone (non exposé publiquement)
			 */

			/// @brief File d'attente des messages en attente de traitement
			/// @ingroup AsyncLoggerMembers
			/// @note NkQueue<NkLogMessage> : copie des messages pour isolation producteur/consommateur
			/// @note Modifiable via Enqueue/FlushQueue (thread-safe via m_QueueMutex)
			NkQueue<NkLogMessage> m_MessageQueue;

			/// @brief Taille maximum de la file d'attente (nombre de messages)
			/// @ingroup AsyncLoggerMembers
			/// @note Défaut : 8192, modifiable via SetMaxQueueSize()
			/// @note Impacte la mémoire maximale allouée et la capacité d'absorption des pics
			usize m_MaxQueueSize;

			/// @brief Intervalle de flush périodique en millisecondes
			/// @ingroup AsyncLoggerMembers
			/// @note 0 = flush uniquement sur demande (Flush() ou Stop()), >0 = wake-up périodique du worker
			/// @note Défaut : 1000ms, modifiable via SetFlushInterval()
			uint32 m_FlushInterval;

			/// @brief Politique de gestion de débordement de la file
			/// @ingroup AsyncLoggerMembers
			/// @note Défaut : NK_DROP_OLDEST, modifiable via SetOverflowPolicy()
			/// @note Impacte le comportement quand m_MessageQueue.Size() >= m_MaxQueueSize
			NkAsyncOverflowPolicy m_OverflowPolicy;

			/// @brief Thread worker dédié au traitement asynchrone des messages
			/// @ingroup AsyncLoggerMembers
			/// @note Démarré via Start(), arrêté via Stop()
			/// @note Exécute WorkerThread() en boucle jusqu'à m_StopRequested
			threading::NkThread m_WorkerThread;

			/// @brief Mutex pour synchronisation des accès à m_MessageQueue
			/// @ingroup AsyncLoggerMembers
			/// @note mutable : permet modification dans les méthodes const (GetQueueSize, etc.)
			/// @note Protège l'enqueue/dequeue concurrent depuis multiples threads producteurs
			mutable threading::NkMutex m_QueueMutex;

			/// @brief Condition variable pour synchronisation worker/producteurs
			/// @ingroup AsyncLoggerMembers
			/// @note Utilisé pour wait/notify : worker sleep quand file vide, wake-up sur nouveau message
			/// @note Supporte wait avec timeout pour flush périodique via m_FlushInterval
			threading::NkConditionVariable m_Condition;

			/// @brief Indicateur atomique d'exécution du worker thread
			/// @ingroup AsyncLoggerMembers
			/// @note true si Start() appelé et worker actif, false sinon
			/// @note Lecture/écriture atomique : thread-safe sans mutex supplémentaire
			NkAtomicBool m_Running;

			/// @brief Indicateur atomique d'arrêt demandé au worker thread
			/// @ingroup AsyncLoggerMembers
			/// @note true si Stop() appelé, false sinon
			/// @note Utilisé par WorkerThread() pour condition de sortie de boucle
			/// @note Lecture/écriture atomique : thread-safe sans mutex supplémentaire
			NkAtomicBool m_StopRequested;


		}; // class NkAsyncLogger


	} // namespace nkentseu


#endif // NKENTSEU_NKASYNCLOGGER_H


// =============================================================================
// IMPLÉMENTATIONS INLINE DES MÉTHODES TEMPLATES (HÉRITÉES DE NKLOGGER)
// =============================================================================
// Ces implémentations doivent être dans le header car ce sont des templates.
// Elles délèguent à NkAsyncLogger::Log(level, message) via NkFormatIndexed.

namespace nkentseu {

	// -------------------------------------------------------------------------
	// MÉTHODE : Log (template principal avec formatage positionnel)
	// -------------------------------------------------------------------------
	template<typename... Args, typename>
	inline void NkAsyncLogger::Log(NkLogLevel level, NkStringView format, Args&&... args) {
		// Filtrage précoce : éviter tout formatage si le message sera ignoré
		if (!ShouldLog(level)) {
			return;
		}

		// Formatage via NkFormatIndexed : style positionnel {0}, {1:hex}, etc.
		NkString formattedMessage = NkFormatIndexed(format, traits::NkForward<Args>(args)...);

		// Enqueue asynchrone du message formaté
		Log(level, formattedMessage);
	}

} // namespace nkentseu


// =============================================================================
// EXEMPLES D'UTILISATION DE NKASYNCLOGGER.H
// =============================================================================
// Ce fichier fournit le logger asynchrone pour haute performance.
// Il combine découplage producteur/consommateur et garanties de persistance.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration basique pour application interactive
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkAsyncLogger.h>
	#include <NKLogger/Sinks/NkConsoleSink.h>
	#include <NKLogger/Sinks/NkFileSink.h>

	void SetupAsyncLogging() {
		using namespace nkentseu;

		// Logger asynchrone avec file de 4096 messages et flush toutes les 500ms
		auto asyncLogger = memory::MakeShared<NkAsyncLogger>(
			"InteractiveApp",  // Nom du logger
			4096,              // Taille de file modérée
			500                // Flush toutes les 500ms pour réactivité
		);

		// Ajout de sinks comme pour un logger normal
		asyncLogger->AddSink(memory::MakeShared<NkConsoleSink>());
		asyncLogger->AddSink(memory::MakeShared<NkFileSink>("logs/app.log"));

		// Configuration optionnelle
		asyncLogger->SetLevel(NkLogLevel::NK_DEBUG);
		asyncLogger->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] %v");

		// Démarrage du worker thread
		asyncLogger->Start();

		// Logging : retour immédiat, traitement en arrière-plan
		asyncLogger->Info("Application started with async logging");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Gestion de débordement pour système de métriques haute fréquence
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkAsyncLogger.h>

	void SetupMetricsLogging() {
		using namespace nkentseu;

		// Logger asynchrone optimisé pour métriques : file grande, drop oldest
		auto metricsLogger = memory::MakeShared<NkAsyncLogger>(
			"MetricsCollector",
			65536,  // File très grande pour absorber les pics de métriques
			5000    // Flush toutes les 5s : compromis persistance/performance
		);

		// Politique : privilégier les métriques récentes en cas de saturation
		metricsLogger->SetOverflowPolicy(NkAsyncOverflowPolicy::NK_DROP_OLDEST);

		// Sink vers système de métriques externe (ex: Prometheus, StatsD)
		metricsLogger->AddSink(memory::MakeShared<NkMetricsSink>("udp://metrics-server:8125"));

		metricsLogger->Start();

		// Logging haute fréquence : enqueue rapide sans blocage
		for (int i = 0; i < 10000; ++i) {
			metricsLogger->Debug("metric.sample value={0} ts={1}", GetValue(i), GetTimestamp());
		}
		// Retour immédiat, traitement asynchrone vers le serveur de métriques
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Arrêt propre avec garantie de persistance
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkAsyncLogger.h>

	class Application {
	public:
		void Shutdown() {
			// Arrêt propre du logger asynchrone avant terminaison
			if (m_Logger && m_Logger->IsRunning()) {
				// Flush explicite pour persistance garantie des messages critiques
				m_Logger->Flush();

				// Arrêt du worker : vidage de la file + terminaison thread
				m_Logger->Stop();
			}

			// Continuation du shutdown de l'application...
			CleanupResources();
		}

	private:
		nkentseu::memory::NkSharedPtr<nkentseu::NkAsyncLogger> m_Logger;
	};
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Monitoring de la file d'attente pour alerting
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkAsyncLogger.h>

	void MonitorAsyncLogger(nkentseu::NkAsyncLogger& logger) {
		using namespace nkentseu;

		// Vérification périodique de l'état du logger asynchrone
		if (!logger.IsRunning()) {
			logger.Error("Async logger worker not running! Logs may be dropped");
			return;
		}

		usize queueSize = logger.GetQueueSize();
		usize maxSize = logger.GetMaxQueueSize();
		double usagePercent = (maxSize > 0) ? (100.0 * queueSize / maxSize) : 0.0;

		// Alerting si utilisation > 80%
		if (usagePercent > 80.0) {
			logger.Warn("Async queue at {0:.1f}% capacity ({1}/{2})",
				usagePercent, queueSize, maxSize);

			// Optionnel : ajustement dynamique de la politique de débordement
			if (usagePercent > 95.0 && logger.GetOverflowPolicy() != NkAsyncOverflowPolicy::NK_DROP_OLDEST) {
				logger.Warn("Switching to DROP_OLDEST policy to prevent blocking");
				logger.SetOverflowPolicy(NkAsyncOverflowPolicy::NK_DROP_OLDEST);
			}
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Testing avec vérification de traitement asynchrone
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkAsyncLogger.h>
	#include <NKLogger/Sinks/NkMemorySink.h>  // Sink fictif pour tests
	#include <gtest/gtest.h>

	TEST(NkAsyncLoggerTest, AsyncProcessing) {
		using namespace nkentseu;

		// Setup : logger asynchrone avec sink mémoire pour capture
		auto asyncLogger = memory::MakeShared<NkAsyncLogger>("TestLogger", 1024, 100);
		auto memorySink = memory::MakeShared<NkMemorySink>();
		asyncLogger->AddSink(memorySink);
		asyncLogger->Start();

		// Execution : logging asynchrone
		asyncLogger->Info("Async message {0}", 42);

		// Attente courte pour laisser le worker traiter
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		// Verification : le message a été traité par le sink
		const auto& captured = memorySink->GetMessages();
		ASSERT_GE(captured.Size(), 1u);
		EXPECT_TRUE(captured[0].message.Contains("Async message 42"));

		// Cleanup : arrêt propre
		asyncLogger->Stop();
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Configuration dynamique via fichier de config
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/NkAsyncLogger.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkLoggerPtr CreateAsyncLoggerFromConfig(const nkentseu::NkString& section) {
		using namespace nkentseu;

		// Lecture des paramètres asynchrones depuis configuration
		NkString name = core::Config::GetString(section + ".name", "AsyncDefault");
		usize queueSize = core::Config::GetUint64(section + ".queueSize", 8192);
		uint32 flushInterval = static_cast<uint32>(core::Config::GetInt64(section + ".flushInterval", 1000));
		NkString overflowStr = core::Config::GetString(section + ".overflowPolicy", "drop_oldest");

		// Mapping de la politique de débordement
		NkAsyncOverflowPolicy overflowPolicy = NkAsyncOverflowPolicy::NK_DROP_OLDEST;
		if (overflowStr == "drop_newest") overflowPolicy = NkAsyncOverflowPolicy::NK_DROP_NEWEST;
		else if (overflowStr == "block") overflowPolicy = NkAsyncOverflowPolicy::NK_BLOCK;

		// Création du logger asynchrone
		auto logger = memory::MakeShared<NkAsyncLogger>(name, queueSize, flushInterval);
		logger->SetOverflowPolicy(overflowPolicy);

		// Ajout des sinks configurés (délégation à fonction helper)
		AddSinksFromConfig(*logger, section + ".sinks");

		// Démarrage automatique si configuré
		if (core::Config::GetBool(section + ".autostart", true)) {
			logger->Start();
		}

		return logger;
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. PERFORMANCE ET DÉCOUPLAGE :
	   - Enqueue() : typiquement < 10 µs (formatage + mutex + notification)
	   - Traitement worker : dépend des sinks configurés (I/O fichier/réseau potentiellement lent)
	   - Avantage : thread appelant non bloqué par les I/O lents des sinks
	   - Trade-off : latence ajoutée entre Log() et écriture réelle (flushInterval + temps de traitement)

	2. GESTION DE DÉBORDEMENT :
	   - NK_DROP_OLDEST : perte de logs anciens, garantie de traitement des récents → idéal pour monitoring
	   - NK_DROP_NEWEST : perte de logs récents, préservation du contexte historique → idéal pour debugging
	   - NK_BLOCK : garantie de traitement complet, mais risque de blocage de l'appelant → idéal pour audit critique
	   - Choisir selon les exigences métier : fiabilité vs performance vs latence

	3. THREAD-SAFETY ET SYNCHRONISATION :
	   - m_QueueMutex protège l'accès à m_MessageQueue : enqueue/dequeue concurrents safe
	   - m_Condition permet wait/notify efficace : worker sleep quand file vide, wake-up sur nouveau message
	   - m_Running/m_StopRequested atomiques : lecture/écriture thread-safe sans mutex supplémentaire
	   - Les sinks doivent être thread-safe : appelés depuis le worker thread, potentiellement en concurrence avec Flush()

	4. FLUSH ET PERSISTANCE :
	   - Flush() force le vidage immédiat de la file + flush des sinks : garantie de persistance
	   - flushInterval > 0 : wake-up périodique du worker pour flush même sans nouveaux messages
	   - Stop() : FlushQueue() + Join() : garantie de traitement complet avant retour
	   - Pour applications critiques : appeler Flush() avant opérations irréversibles

	5. GESTION DE LA MÉMOIRE :
	   - NkLogMessage copié dans la file : isolation entre producteur et consommateur
	   - Taille mémoire max estimée : queueSize * sizeof(NkLogMessage) (~200-500 bytes/msg selon contenu)
	   - Exemple : 8192 messages * 300 bytes = ~2.4 MB maximum alloué pour la file
	   - Pour usage mémoire critique : ajuster queueSize ou utiliser NkLogMessage léger (sans source info)

	6. DEBUGGING ET MONITORING :
	   - GetQueueSize() : monitoring de la charge de logging en temps réel
	   - IsRunning() : vérification que le worker est actif pour alerting
	   - Pour debugging : flushInterval court (100-500ms) pour visibilité immédiate des logs
	   - Pour production : flushInterval plus long (1000-5000ms) pour performance, avec Flush() explicite aux points critiques

	7. EXTENSIBILITÉ FUTURES :
	   - Priorité de messages : ajouter un champ priority à NkLogMessage et file prioritaire
	   - Compression de messages : compresser les messages en file pour réduire l'empreinte mémoire
	   - Traitement batch : regrouper plusieurs messages pour écriture batch vers les sinks (si supporté)
	   - Metrics intégrées : compter messages enqueue/dequeue/dropped pour monitoring interne

	8. TESTING ET VALIDATION :
	   - Tester les cas limites : file pleine avec chaque politique de débordement
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
	   - Benchmarking : mesurer l'overhead de Enqueue() vs logging synchrone pour différents volumes
	   - Pour tests de performance : désactiver flushInterval et mesurer le throughput max
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================