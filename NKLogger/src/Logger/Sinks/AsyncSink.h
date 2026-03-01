// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/AsyncSink.h
// DESCRIPTION: Logger asynchrone pour les performances.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Logger.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: AsyncLogger
// DESCRIPTION: Logger asynchrone avec file d'attente
// -------------------------------------------------------------------------
class LOGGER_API AsyncLogger : public Logger {
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
	AsyncLogger(const std::string &name, size_t queueSize = 8192, uint32 flushInterval = 1000);

	/**
	 * @brief Destructeur
	 */
	~AsyncLogger();

	// ---------------------------------------------------------------------
	// IMPLÉMENTATION DE Logger
	// ---------------------------------------------------------------------

	/**
	 * @brief Log asynchrone
	 */
	virtual void Log(LogLevel level, const char *format, ...) override;
	void Log(LogLevel level, const std::string &message);
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
	size_t GetQueueSize() const;

	/**
	 * @brief Définit la taille maximum de la file
	 * @param size Taille maximum
	 */
	void SetMaxQueueSize(size_t size);

	/**
	 * @brief Obtient la taille maximum de la file
	 * @return Taille maximum
	 */
	size_t GetMaxQueueSize() const;

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
	 * @brief Ajoute un message à la file d'attente
	 * @param message Message à ajouter
	 * @return true si ajouté, false si file pleine
	 */
	bool Enqueue(const LogMessage &message);

	/**
	 * @brief Traite un message de la file
	 * @param message Message à traiter
	 */
	void ProcessMessage(const LogMessage &message);

	/**
	 * @brief Vide toute la file d'attente
	 */
	void FlushQueue();

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// File d'attente des messages
	std::queue<LogMessage> m_MessageQueue;

	/// Taille maximum de la file
	size_t m_MaxQueueSize;

	/// Intervalle de flush en ms
	uint32 m_FlushInterval;

	/// Thread de traitement
	std::thread m_WorkerThread;

	/// Mutex pour la file d'attente
	mutable std::mutex m_QueueMutex;

	/// Condition variable pour la synchronisation
	std::condition_variable m_Condition;

	/// Indicateur d'exécution
	std::atomic<bool> m_Running;

	/// Indicateur d'arrêt demandé
	std::atomic<bool> m_StopRequested;
};

} // namespace logger
} // namespace nkentseu