// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/AsyncSink.cpp
// DESCRIPTION: Implémentation du logger asynchrone avec file d'attente.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkAsyncSink.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstdarg>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
		
	/**
	 * @brief Constructeur avec configuration
	 */
	NkAsyncLogger::NkAsyncLogger(const NkString &name, usize queueSize, uint32 flushInterval)
		: NkLogger(name), m_MaxQueueSize(queueSize), m_FlushInterval(flushInterval), m_Running(false), m_StopRequested(false) {
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
		NkString message = FormatString(format, args);
		va_end(args);

		Log(level, message);
	}

	/**
	 * @brief Log asynchrone avec message pré-formaté
	 */
	void NkAsyncLogger::Log(NkLogLevel level, const NkString &message) {
		if (!ShouldLog(level))
			return;

		NkLogMessage msg;
		msg.threadId = static_cast<uint32>(logger_sync::GetCurrentThreadId());
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
		if (m_Running.Load())
			return;

		m_Running.Store(true);
		m_StopRequested.Store(false);
		if (!m_WorkerThread.Start(&NkAsyncLogger::WorkerThreadEntry, this)) {
			m_Running.Store(false);
			m_StopRequested.Store(true);
		}
	}

	/**
	 * @brief Arrête le thread de traitement
	 */
	void NkAsyncLogger::Stop() {
		if (!m_Running.Load())
			return;

		m_StopRequested.Store(true);
		m_Condition.NotifyOne();

		if (m_WorkerThread.Joinable()) {
			m_WorkerThread.Join();
		}

		m_Running.Store(false);
		FlushQueue();
	}

	/**
	 * @brief Vérifie si le logger est en cours d'exécution
	 */
	bool NkAsyncLogger::IsRunning() const {
		return m_Running.Load();
	}

	/**
	 * @brief Obtient la taille actuelle de la file
	 */
	usize NkAsyncLogger::GetQueueSize() const {
		logger_sync::NkScopedLock lock(m_QueueMutex);
		return m_MessageQueue.Size();
	}

	/**
	 * @brief Définit la taille maximum de la file
	 */
	void NkAsyncLogger::SetMaxQueueSize(usize size) {
		logger_sync::NkScopedLock lock(m_QueueMutex);
		m_MaxQueueSize = size;
	}

	/**
	 * @brief Obtient la taille maximum de la file
	 */
	usize NkAsyncLogger::GetMaxQueueSize() const {
		logger_sync::NkScopedLock lock(m_QueueMutex);
		return m_MaxQueueSize;
	}

	/**
	 * @brief Définit l'intervalle de flush
	 */
	void NkAsyncLogger::SetFlushInterval(uint32 ms) {
		logger_sync::NkScopedLock lock(m_QueueMutex);
		m_FlushInterval = ms;
	}

	/**
	 * @brief Obtient l'intervalle de flush
	 */
	uint32 NkAsyncLogger::GetFlushInterval() const {
		logger_sync::NkScopedLock lock(m_QueueMutex);
		return m_FlushInterval;
	}

	/**
	 * @brief Fonction du thread de traitement
	 */
	void NkAsyncLogger::WorkerThread() {
		while (!m_StopRequested.Load()) {
			NkLogMessage msg;
			bool hasMessage = false;

			{
				logger_sync::NkScopedLock lock(m_QueueMutex);

				while (m_MessageQueue.Empty() && !m_StopRequested.Load()) {
					if (m_FlushInterval == 0) {
						m_Condition.Wait(lock);
					} else {
						(void)m_Condition.WaitFor(lock, m_FlushInterval);
					}

					if (m_MessageQueue.Empty() && m_StopRequested.Load()) {
						break;
					}
					if (m_MessageQueue.Empty()) {
						// Timeout périodique pour permettre un flush régulier.
						break;
					}
				}

				if (!m_MessageQueue.Empty()) {
					msg = m_MessageQueue.Front();
					m_MessageQueue.Pop();
					hasMessage = true;
				}
			}

			if (hasMessage) {
				ProcessMessage(msg);
			}
		}
	}

	void NkAsyncLogger::WorkerThreadEntry(void* userData) {
		NkAsyncLogger* self = static_cast<NkAsyncLogger*>(userData);
		if (self != nullptr) {
			self->WorkerThread();
		}
	}

	/**
	 * @brief Ajoute un message à la file
	 */
	bool NkAsyncLogger::Enqueue(const NkLogMessage &message) {
		logger_sync::NkScopedLock lock(m_QueueMutex);

		if (m_MessageQueue.Size() >= m_MaxQueueSize) {
			return false; // File pleine
		}

		m_MessageQueue.Push(message);
		m_Condition.NotifyOne();
		return true;
	}

	/**
	 * @brief Traite un message de la file
	 */
	void NkAsyncLogger::ProcessMessage(const NkLogMessage &message) {
		logger_sync::NkScopedLock lock(m_Mutex);

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
		for (;;) {
			NkLogMessage msg;
			bool hasMessage = false;

			{
				logger_sync::NkScopedLock lock(m_QueueMutex);
				if (!m_MessageQueue.Empty()) {
					msg = m_MessageQueue.Front();
					m_MessageQueue.Pop();
					hasMessage = true;
				}
			}

			if (!hasMessage) {
				break;
			}

			ProcessMessage(msg);
		}
	}

} // namespace nkentseu
