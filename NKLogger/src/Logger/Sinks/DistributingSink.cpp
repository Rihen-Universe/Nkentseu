// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/DistributingSink.cpp
// DESCRIPTION: Implémentation du sink distribuant les messages à plusieurs sous-sinks.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Sinks/DistributingSink.h"
#include <algorithm>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

/**
 * @brief Constructeur par défaut
 */
DistributingSink::DistributingSink() {
}

/**
 * @brief Constructeur avec liste initiale de sinks
 */
DistributingSink::DistributingSink(const std::vector<std::shared_ptr<ISink>> &sinks) : m_Sinks(sinks) {
}

/**
 * @brief Destructeur
 */
DistributingSink::~DistributingSink() {
}

/**
 * @brief Distribue un message à tous les sous-sinks
 */
void DistributingSink::Log(const LogMessage &message) {
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
void DistributingSink::Flush() {
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
void DistributingSink::SetFormatter(std::unique_ptr<Formatter> formatter) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	// Clone le formatter pour chaque sink
	for (auto &sink : m_Sinks) {
		if (sink && formatter) {
			auto clonedFormatter = std::make_unique<Formatter>(formatter->GetPattern());
			sink->SetFormatter(std::move(clonedFormatter));
		}
	}
}

/**
 * @brief Définit le pattern de formatage pour tous les sous-sinks
 */
void DistributingSink::SetPattern(const std::string &pattern) {
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
Formatter *DistributingSink::GetFormatter() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (!m_Sinks.empty() && m_Sinks[0]) {
		return m_Sinks[0]->GetFormatter();
	}
	return nullptr;
}

/**
 * @brief Obtient le pattern (du premier sink)
 */
std::string DistributingSink::GetPattern() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (!m_Sinks.empty() && m_Sinks[0]) {
		return m_Sinks[0]->GetPattern();
	}
	return "";
}

/**
 * @brief Ajoute un sous-sink
 */
void DistributingSink::AddSink(std::shared_ptr<ISink> sink) {
	if (!sink)
		return;

	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Sinks.push_back(sink);
}

/**
 * @brief Supprime un sous-sink
 */
void DistributingSink::RemoveSink(std::shared_ptr<ISink> sink) {
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
void DistributingSink::ClearSinks() {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Sinks.clear();
}

/**
 * @brief Obtient la liste des sous-sinks
 */
std::vector<std::shared_ptr<ISink>> DistributingSink::GetSinks() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Sinks;
}

/**
 * @brief Obtient le nombre de sous-sinks
 */
size_t DistributingSink::GetSinkCount() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Sinks.size();
}

/**
 * @brief Vérifie si un sink spécifique est présent
 */
bool DistributingSink::ContainsSink(std::shared_ptr<ISink> sink) const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return std::find(m_Sinks.begin(), m_Sinks.end(), sink) != m_Sinks.end();
}

} // namespace logger
} // namespace nkentseu
