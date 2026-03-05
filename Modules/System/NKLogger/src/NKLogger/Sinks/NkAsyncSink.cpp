// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/AsyncSink.cpp
// DESCRIPTION: Implémentation du logger asynchrone avec file d'attente.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkAsyncSink.h"
#include <thread>
#include <cstdarg>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
		
	/**
	 * @brief Constructeur avec configuration
	 */
	NkAsyncLogger::NkAsyncLogger(const std::string &name, core::usize queueSize, uint32 flushInterval)
		: NkLogger(name), m_MaxQueueSize(queueSize), m_FlushInterval(flushInterval), m_Running(false),
		m_StopRequested(false) {
	}

	/**
	 * @brief Destructeur
	 */
	NkAsyncLogger::~NkAsyncLogger() {
		Stop();
	}

	/**
	 * @brief Log asynchrone
	 */
	void NkAsyncLogger::Logf(NkLogLevel level, const char *format, ...) {
		if (!ShouldLog(level))
			return;

		va_list args;
		va_start(args, format);
		std::string message = FormatString(format, args);
		va_end(args);

		Log(level, message);
	}

	/**
	 * @brief Log asynchrone avec message pré-formaté
	 */
	void NkAsyncLogger::Log(NkLogLevel level, const std::string &message) {
		if (!ShouldLog(level))
			return;

		NkLogMessage msg;
		msg.timestamp =
			std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
				.count();
		msg.threadId = static_cast<uint32>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
		msg.level = level;
		msg.message = message;
		msg.loggerName = GetName();

		Enqueue(msg);
	}

	/**
	 * @brief Force le flush des messages en attente
	 */
	void NkAsyncLogger::Flush() {
		FlushQueue();
		NkLogger::Flush();
	}

	/**
	 * @brief Démarre le thread de traitement
	 */
	void NkAsyncLogger::Start() {
		if (m_Running)
			return;

		m_Running = true;
		m_StopRequested = false;
		m_WorkerThread = std::thread(&NkAsyncLogger::WorkerThread, this);
	}

	/**
	 * @brief Arrête le thread de traitement
	 */
	void NkAsyncLogger::Stop() {
		if (!m_Running)
			return;

		m_StopRequested = true;
		m_Condition.notify_one();

		if (m_WorkerThread.joinable()) {
			m_WorkerThread.join();
		}

		m_Running = false;
		FlushQueue();
	}

	/**
	 * @brief Vérifie si le logger est en cours d'exécution
	 */
	bool NkAsyncLogger::IsRunning() const {
		return m_Running;
	}

	/**
	 * @brief Obtient la taille actuelle de la file
	 */
	core::usize NkAsyncLogger::GetQueueSize() const {
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		return m_MessageQueue.size();
	}

	/**
	 * @brief Définit la taille maximum de la file
	 */
	void NkAsyncLogger::SetMaxQueueSize(core::usize size) {
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		m_MaxQueueSize = size;
	}

	/**
	 * @brief Obtient la taille maximum de la file
	 */
	core::usize NkAsyncLogger::GetMaxQueueSize() const {
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		return m_MaxQueueSize;
	}

	/**
	 * @brief Définit l'intervalle de flush
	 */
	void NkAsyncLogger::SetFlushInterval(uint32 ms) {
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		m_FlushInterval = ms;
	}

	/**
	 * @brief Obtient l'intervalle de flush
	 */
	uint32 NkAsyncLogger::GetFlushInterval() const {
		std::lock_guard<std::mutex> lock(m_QueueMutex);
		return m_FlushInterval;
	}

	/**
	 * @brief Fonction du thread de traitement
	 */
	void NkAsyncLogger::WorkerThread() {
		while (!m_StopRequested) {
			std::unique_lock<std::mutex> lock(m_QueueMutex);

			m_Condition.wait_for(lock, std::chrono::milliseconds(m_FlushInterval),
								[this] { return !m_MessageQueue.empty() || m_StopRequested; });

			while (!m_MessageQueue.empty()) {
				NkLogMessage msg = m_MessageQueue.front();
				m_MessageQueue.pop();
				lock.unlock();

				ProcessMessage(msg);

				lock.lock();
			}
		}
	}

	/**
	 * @brief Ajoute un message à la file
	 */
	bool NkAsyncLogger::Enqueue(const NkLogMessage &message) {
		std::unique_lock<std::mutex> lock(m_QueueMutex);

		if (m_MessageQueue.size() >= m_MaxQueueSize) {
			return false; // File pleine
		}

		m_MessageQueue.push(message);
		lock.unlock();

		m_Condition.notify_one();
		return true;
	}

	/**
	 * @brief Traite un message de la file
	 */
	void NkAsyncLogger::ProcessMessage(const NkLogMessage &message) {
		// Utiliser le logger parent pour traiter
		std::lock_guard<std::mutex> lock(m_Mutex);

		std::string formatted;
		if (m_Formatter) {
			formatted = m_Formatter->Format(message);
		} else {
			formatted = message.message;
		}

		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->Log(message);
			}
		}
	}

	/**
	 * @brief Vide toute la file d'attente
	 */
	void NkAsyncLogger::FlushQueue() {
		std::unique_lock<std::mutex> lock(m_QueueMutex);

		while (!m_MessageQueue.empty()) {
			NkLogMessage msg = m_MessageQueue.front();
			m_MessageQueue.pop();
			lock.unlock();

			ProcessMessage(msg);

			lock.lock();
		}
	}

} // namespace nkentseu
