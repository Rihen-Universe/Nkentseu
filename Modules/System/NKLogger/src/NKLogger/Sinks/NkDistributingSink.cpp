// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkDistributingSink.cpp
// DESCRIPTION: Implémentation du sink distribuant les messages à plusieurs sous-sinks.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkDistributingSink.h"
#include <algorithm>

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
	NkDistributingSink::NkDistributingSink(const std::vector<std::shared_ptr<NkISink>> &sinks) : m_Sinks(sinks) {
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

		std::lock_guard<std::mutex> lock(m_Mutex);
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
		std::lock_guard<std::mutex> lock(m_Mutex);
		for (auto &sink : m_Sinks) {
			if (sink) {
				sink->Flush();
			}
		}
	}

	/**
	 * @brief Définit le formatter pour tous les sous-sinks
	 */
	void NkDistributingSink::SetFormatter(std::unique_ptr<NkFormatter> formatter) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		// Clone le formatter pour chaque sink
		for (auto &sink : m_Sinks) {
			if (sink && formatter) {
				auto clonedFormatter = std::make_unique<NkFormatter>(formatter->GetPattern());
				sink->SetFormatter(std::move(clonedFormatter));
			}
		}
	}

	/**
	 * @brief Définit le pattern de formatage pour tous les sous-sinks
	 */
	void NkDistributingSink::SetPattern(const std::string &pattern) {
		std::lock_guard<std::mutex> lock(m_Mutex);
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
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (!m_Sinks.empty() && m_Sinks[0]) {
			return m_Sinks[0]->GetFormatter();
		}
		return nullptr;
	}

	/**
	 * @brief Obtient le pattern (du premier sink)
	 */
	std::string NkDistributingSink::GetPattern() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (!m_Sinks.empty() && m_Sinks[0]) {
			return m_Sinks[0]->GetPattern();
		}
		return "";
	}

	/**
	 * @brief Ajoute un sous-sink
	 */
	void NkDistributingSink::AddSink(std::shared_ptr<NkISink> sink) {
		if (!sink)
			return;

		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Sinks.push_back(sink);
	}

	/**
	 * @brief Supprime un sous-sink
	 */
	void NkDistributingSink::RemoveSink(std::shared_ptr<NkISink> sink) {
		if (!sink)
			return;

		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = std::find(m_Sinks.begin(), m_Sinks.end(), sink);
		if (it != m_Sinks.end()) {
			m_Sinks.erase(it);
		}
	}

	/**
	 * @brief Supprime tous les sous-sinks
	 */
	void NkDistributingSink::ClearSinks() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Sinks.clear();
	}

	/**
	 * @brief Obtient la liste des sous-sinks
	 */
	std::vector<std::shared_ptr<NkISink>> NkDistributingSink::GetSinks() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Sinks;
	}

	/**
	 * @brief Obtient le nombre de sous-sinks
	 */
	core::usize NkDistributingSink::GetSinkCount() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Sinks.size();
	}

	/**
	 * @brief Vérifie si un sink spécifique est présent
	 */
	bool NkDistributingSink::ContainsSink(std::shared_ptr<NkISink> sink) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return std::find(m_Sinks.begin(), m_Sinks.end(), sink) != m_Sinks.end();
	}

} // namespace nkentseu
