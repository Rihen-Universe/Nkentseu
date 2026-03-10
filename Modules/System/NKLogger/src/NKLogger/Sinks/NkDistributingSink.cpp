// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkDistributingSink.cpp
// DESCRIPTION: Implémentation du sink distribuant les messages à plusieurs sous-sinks.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkDistributingSink.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	/**
	 * @brief Constructeur par défaut
	 */
	NkDistributingSink::NkDistributingSink() {
	}

	/**
	 * @brief Constructeur avec liste initiale de sinks
	 */
	NkDistributingSink::NkDistributingSink(const NkVector<memory::NkSharedPtr<NkISink>> &sinks) : m_Sinks(sinks) {
	}

	/**
	 * @brief Destructeur
	 */
	NkDistributingSink::~NkDistributingSink() {
	}

	/**
	 * @brief Distribue un message à tous les sous-sinks
	 */
	void NkDistributingSink::Log(const NkLogMessage &message) {
		if (!IsEnabled() || !ShouldLog(message.level)) {
			return;
		}

		logger_sync::NkScopedLock lock(m_Mutex);
		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->Log(message);
			}
		}
	}

	/**
	 * @brief Force le flush de tous les sous-sinks
	 */
	void NkDistributingSink::Flush() {
		logger_sync::NkScopedLock lock(m_Mutex);
		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->Flush();
			}
		}
	}

	/**
	 * @brief Définit le formatter pour tous les sous-sinks
	 */
	void NkDistributingSink::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
		logger_sync::NkScopedLock lock(m_Mutex);
		// Clone le formatter pour chaque sink
		for (auto &sink : m_Sinks) {
			if (sink && formatter) {
				auto clonedFormatter = memory::NkMakeUnique<NkFormatter>(formatter->GetPattern());
				sink->SetFormatter(traits::NkMove(clonedFormatter));
			}
		}
	}

	/**
	 * @brief Définit le pattern de formatage pour tous les sous-sinks
	 */
	void NkDistributingSink::SetPattern(const NkString &pattern) {
		logger_sync::NkScopedLock lock(m_Mutex);
		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->SetPattern(pattern);
			}
		}
	}

	/**
	 * @brief Obtient le formatter (du premier sink)
	 */
	NkFormatter *NkDistributingSink::GetFormatter() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (!m_Sinks.Empty() && m_Sinks[0]) {
			return m_Sinks[0]->GetFormatter();
		}
		return nullptr;
	}

	/**
	 * @brief Obtient le pattern (du premier sink)
	 */
	NkString NkDistributingSink::GetPattern() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (!m_Sinks.Empty() && m_Sinks[0]) {
			return m_Sinks[0]->GetPattern();
		}
		return "";
	}

	/**
	 * @brief Ajoute un sous-sink
	 */
	void NkDistributingSink::AddSink(memory::NkSharedPtr<NkISink> sink) {
		if (!sink)
			return;

		logger_sync::NkScopedLock lock(m_Mutex);
		m_Sinks.PushBack(sink);
	}

	/**
	 * @brief Supprime un sous-sink
	 */
	void NkDistributingSink::RemoveSink(memory::NkSharedPtr<NkISink> sink) {
		if (!sink)
			return;

		logger_sync::NkScopedLock lock(m_Mutex);
		for (usize i = 0; i < m_Sinks.Size(); ++i) {
			if (m_Sinks[i].Get() == sink.Get()) {
				m_Sinks.Erase(m_Sinks.begin() + i);
				return;
			}
		}
	}

	/**
	 * @brief Supprime tous les sous-sinks
	 */
	void NkDistributingSink::ClearSinks() {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_Sinks.Clear();
	}

	/**
	 * @brief Obtient la liste des sous-sinks
	 */
	NkVector<memory::NkSharedPtr<NkISink>> NkDistributingSink::GetSinks() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Sinks;
	}

	/**
	 * @brief Obtient le nombre de sous-sinks
	 */
	usize NkDistributingSink::GetSinkCount() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Sinks.Size();
	}

	/**
	 * @brief Vérifie si un sink spécifique est présent
	 */
	bool NkDistributingSink::ContainsSink(memory::NkSharedPtr<NkISink> sink) const {
		logger_sync::NkScopedLock lock(m_Mutex);
		for (const auto& current : m_Sinks) {
			if (current.Get() == sink.Get()) {
				return true;
			}
		}
		return false;
	}

} // namespace nkentseu
