// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/AsyncSink.h
// DESCRIPTION: NkLogger asynchrone pour les performances.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLogger.h"
#include "NKLogger/NkSync.h"
#include "NKContainers/Adapters/NkQueue.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
		
	// -------------------------------------------------------------------------
	// CLASSE: NkAsyncLogger
	// DESCRIPTION: NkLogger asynchrone avec file d'attente
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkAsyncLogger : public NkLogger {
		public:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur avec nom et configuration
			 * @param name Nom du logger
			 * @param queueSize Taille maximum de la file d'attente
			 * @param flushInterval Intervalle de flush en ms
			 */
			NkAsyncLogger(const NkString &name, usize queueSize = 8192, uint32 flushInterval = 1000);

			/**
			 * @brief Destructeur
			 */
			~NkAsyncLogger();

			// ---------------------------------------------------------------------
			// IMPLÉMENTATION DE NkLogger
			// ---------------------------------------------------------------------

			/**
			 * @brief Log asynchrone
			 */
			virtual void Logf(NkLogLevel level, const char *format, ...) override;
			void Log(NkLogLevel level, const NkString &message);
			/**
			 * @brief Force le flush des messages en attente
			 */
			void Flush() override;

			// ---------------------------------------------------------------------
			// CONFIGURATION ASYNCHRONE
			// ---------------------------------------------------------------------

			/**
			 * @brief Démarre le thread de traitement
			 */
			void Start();

			/**
			 * @brief Arrête le thread de traitement
			 */
			void Stop();

			/**
			 * @brief Vérifie si le logger est en cours d'exécution
			 * @return true si en cours d'exécution, false sinon
			 */
			bool IsRunning() const;

			/**
			 * @brief Obtient la taille actuelle de la file d'attente
			 * @return Taille de la file
			 */
			usize GetQueueSize() const;

			/**
			 * @brief Définit la taille maximum de la file
			 * @param size Taille maximum
			 */
			void SetMaxQueueSize(usize size);

			/**
			 * @brief Obtient la taille maximum de la file
			 * @return Taille maximum
			 */
			usize GetMaxQueueSize() const;

			/**
			 * @brief Définit l'intervalle de flush
			 * @param ms Intervalle en millisecondes
			 */
			void SetFlushInterval(uint32 ms);

			/**
			 * @brief Obtient l'intervalle de flush
			 * @return Intervalle en millisecondes
			 */
			uint32 GetFlushInterval() const;

		private:
			// ---------------------------------------------------------------------
			// MÉTHODES PRIVÉES
			// ---------------------------------------------------------------------

			/**
			 * @brief Fonction du thread de traitement
			 */
			void WorkerThread();

			/**
			 * @brief Entrée C-compatible du thread worker.
			 */
			static void WorkerThreadEntry(void* userData);

			/**
			 * @brief Ajoute un message à la file d'attente
			 * @param message Message à ajouter
			 * @return true si ajouté, false si file pleine
			 */
			bool Enqueue(const NkLogMessage &message);

			/**
			 * @brief Traite un message de la file
			 * @param message Message à traiter
			 */
			void ProcessMessage(const NkLogMessage &message);

			/**
			 * @brief Vide toute la file d'attente
			 */
			void FlushQueue();

			// ---------------------------------------------------------------------
			// VARIABLES MEMBRE PRIVÉES
			// ---------------------------------------------------------------------

			/// File d'attente des messages
			NkQueue<NkLogMessage> m_MessageQueue;

			/// Taille maximum de la file
			usize m_MaxQueueSize;

			/// Intervalle de flush en ms
			uint32 m_FlushInterval;

			/// Thread de traitement
			loggersync::NkThread m_WorkerThread;

			/// Mutex pour la file d'attente
			mutable threading::NkMutex m_QueueMutex;

			/// Condition variable pour la synchronisation
			loggersync::NkConditionVariable m_Condition;

			/// Indicateur d'exécution
			loggersync::NkAtomicBool m_Running;

			/// Indicateur d'arrêt demandé
			loggersync::NkAtomicBool m_StopRequested;
	};

} // namespace nkentseu
