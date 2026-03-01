// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/AsyncSink.cpp
// DESCRIPTION: Implémentation du logger asynchrone avec file d'attente.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Sinks/AsyncSink.h"
#include <thread>
#include <cstdarg>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

/**
 * @brief Constructeur avec configuration
 */
AsyncLogger::AsyncLogger(const std::string &name, size_t queueSize, uint32 flushInterval)
	: Logger(name), m_MaxQueueSize(queueSize), m_FlushInterval(flushInterval), m_Running(false),
	  m_StopRequested(false) {
}

/**
 * @brief Destructeur
 */
AsyncLogger::~AsyncLogger() {
	Stop();
}

/**
 * @brief Log asynchrone
 */
void AsyncLogger::Log(LogLevel level, const char *format, ...) {
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
void AsyncLogger::Log(LogLevel level, const std::string &message) {
	if (!ShouldLog(level))
		return;

	LogMessage msg;
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
void AsyncLogger::Flush() {
	FlushQueue();
	Logger::Flush();
}

/**
 * @brief Démarre le thread de traitement
 */
void AsyncLogger::Start() {
	if (m_Running)
		return;

	m_Running = true;
	m_StopRequested = false;
	m_WorkerThread = std::thread(&AsyncLogger::WorkerThread, this);
}

/**
 * @brief Arrête le thread de traitement
 */
void AsyncLogger::Stop() {
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
bool AsyncLogger::IsRunning() const {
	return m_Running;
}

/**
 * @brief Obtient la taille actuelle de la file
 */
size_t AsyncLogger::GetQueueSize() const {
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	return m_MessageQueue.size();
}

/**
 * @brief Définit la taille maximum de la file
 */
void AsyncLogger::SetMaxQueueSize(size_t size) {
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	m_MaxQueueSize = size;
}

/**
 * @brief Obtient la taille maximum de la file
 */
size_t AsyncLogger::GetMaxQueueSize() const {
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	return m_MaxQueueSize;
}

/**
 * @brief Définit l'intervalle de flush
 */
void AsyncLogger::SetFlushInterval(uint32 ms) {
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	m_FlushInterval = ms;
}

/**
 * @brief Obtient l'intervalle de flush
 */
uint32 AsyncLogger::GetFlushInterval() const {
	std::lock_guard<std::mutex> lock(m_QueueMutex);
	return m_FlushInterval;
}

/**
 * @brief Fonction du thread de traitement
 */
void AsyncLogger::WorkerThread() {
	while (!m_StopRequested) {
		std::unique_lock<std::mutex> lock(m_QueueMutex);

		m_Condition.wait_for(lock, std::chrono::milliseconds(m_FlushInterval),
							 [this] { return !m_MessageQueue.empty() || m_StopRequested; });

		while (!m_MessageQueue.empty()) {
			LogMessage msg = m_MessageQueue.front();
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
bool AsyncLogger::Enqueue(const LogMessage &message) {
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
void AsyncLogger::ProcessMessage(const LogMessage &message) {
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
void AsyncLogger::FlushQueue() {
	std::unique_lock<std::mutex> lock(m_QueueMutex);

	while (!m_MessageQueue.empty()) {
		LogMessage msg = m_MessageQueue.front();
		m_MessageQueue.pop();
		lock.unlock();

		ProcessMessage(msg);

		lock.lock();
	}
}

} // namespace logger
} // namespace nkentseu
