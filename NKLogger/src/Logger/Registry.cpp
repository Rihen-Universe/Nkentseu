// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Registry.cpp
// DESCRIPTION: Implémentation du registre global des loggers.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Registry.h"
#include "Logger/Sinks/ConsoleSink.h"
#include <algorithm>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// IMPLÉMENTATION DE Registry
// -------------------------------------------------------------------------

/**
 * @brief Constructeur privé
 */
Registry::Registry()
	: m_GlobalLevel(LogLevel::Info), m_GlobalPattern(Formatter::DEFAULT_PATTERN), m_Initialized(false) {
}

/**
 * @brief Destructeur privé
 */
Registry::~Registry() {
	Clear();
}

/**
 * @brief Obtient l'instance singleton du registre
 */
Registry &Registry::Instance() {
	static Registry instance;
	return instance;
}

/**
 * @brief Initialise le registre avec des paramètres par défaut
 */
void Registry::Initialize() {
	auto &instance = Instance();

	std::lock_guard<std::mutex> lock(instance.m_Mutex);
	if (!instance.m_Initialized) {
		instance.CreateDefaultLogger();
		instance.m_Initialized = true;
	}
}

/**
 * @brief Nettoie le registre (détruit tous les loggers)
 */
void Registry::Shutdown() {
	auto &instance = Instance();
	instance.Clear();
	instance.m_Initialized = false;
}

/**
 * @brief Enregistre un logger dans le registre
 */
bool Registry::Register(std::shared_ptr<Logger> logger) {
	if (!logger)
		return false;

	std::string name = logger->GetName();

	if (m_Loggers.find(name) != m_Loggers.end()) {
		return false; // Nom déjà existant
	}

	m_Loggers[name] = logger;
	return true;
}

/**
 * @brief Désenregistre un logger du registre
 */
bool Registry::Unregister(const std::string &name) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_Loggers.find(name);

	if (it != m_Loggers.end()) {
		m_Loggers.erase(it);
		return true;
	}

	return false;
}

/**
 * @brief Obtient un logger par son nom
 */
std::shared_ptr<Logger> Registry::Get(const std::string &name) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_Loggers.find(name);

	if (it != m_Loggers.end()) {
		return it->second;
	}

	return nullptr;
}

/**
 * @brief Obtient un logger par son nom (crée si non existant)
 */
std::shared_ptr<Logger> Registry::GetOrCreate(const std::string &name) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_Loggers.find(name);

	if (it != m_Loggers.end()) {
		return it->second;
	}

	// Création d'un nouveau logger
	auto logger = std::make_shared<Logger>(name);
	logger->SetLevel(m_GlobalLevel);
	logger->SetPattern(m_GlobalPattern);

	m_Loggers[name] = logger;
	return logger;
}

/**
 * @brief Vérifie si un logger existe
 */
bool Registry::Exists(const std::string &name) const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Loggers.find(name) != m_Loggers.end();
}

/**
 * @brief Supprime tous les loggers du registre
 */
void Registry::Clear() {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Loggers.clear();
	m_DefaultLogger.reset();
}

/**
 * @brief Obtient la liste de tous les noms de loggers
 */
std::vector<std::string> Registry::GetLoggerNames() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	std::vector<std::string> names;
	names.reserve(m_Loggers.size());

	for (const auto &pair : m_Loggers) {
		names.push_back(pair.first);
	}

	return names;
}

/**
 * @brief Obtient le nombre de loggers enregistrés
 */
size_t Registry::GetLoggerCount() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Loggers.size();
}

/**
 * @brief Définit le niveau de log global
 */
void Registry::SetGlobalLevel(LogLevel level) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_GlobalLevel = level;

	// Appliquer à tous les loggers existants
	for (auto &pair : m_Loggers) {
		if (pair.second) {
			pair.second->SetLevel(level);
		}
	}
}

/**
 * @brief Obtient le niveau de log global
 */
LogLevel Registry::GetGlobalLevel() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_GlobalLevel;
}

/**
 * @brief Définit le pattern global
 */
void Registry::SetGlobalPattern(const std::string &pattern) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_GlobalPattern = pattern;

	// Appliquer à tous les loggers existants
	for (auto &pair : m_Loggers) {
		if (pair.second) {
			pair.second->SetPattern(pattern);
		}
	}
}

/**
 * @brief Obtient le pattern global
 */
std::string Registry::GetGlobalPattern() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_GlobalPattern;
}

/**
 * @brief Force le flush de tous les loggers
 */
void Registry::FlushAll() {
	std::lock_guard<std::mutex> lock(m_Mutex);

	for (auto &pair : m_Loggers) {
		if (pair.second) {
			pair.second->Flush();
		}
	}
}

/**
 * @brief Définit le logger par défaut
 */
void Registry::SetDefaultLogger(std::shared_ptr<Logger> logger) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_DefaultLogger = logger;

	// S'assurer qu'il est aussi dans le registre
	if (logger && !Exists(logger->GetName())) {
		Register(logger);
	}
}

/**
 * @brief Obtient le logger par défaut
 */
std::shared_ptr<Logger> Registry::GetDefaultLogger() {
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (!m_DefaultLogger) {
		m_DefaultLogger = CreateDefaultLogger();
	}

	return m_DefaultLogger;
}

/**
 * @brief Crée un logger par défaut avec console sink
 */
std::shared_ptr<Logger> Registry::CreateDefaultLogger() {
	auto logger = std::make_shared<Logger>("default");

	logger->SetLevel(m_GlobalLevel);
	logger->SetPattern(m_GlobalPattern);

	// Ajouter un sink console par défaut
	auto consoleSink = std::make_shared<ConsoleSink>();
	logger->AddSink(consoleSink);

	// Enregistrer le logger
	Register(logger);
	m_DefaultLogger = logger;

	return logger;
}

// -------------------------------------------------------------------------
// FONCTIONS UTILITAIRES GLOBALES
// -------------------------------------------------------------------------

/**
 * @brief Obtient un logger par son nom
 */
std::shared_ptr<Logger> GetLogger(const std::string &name) {
	return Registry::Instance().Get(name);
}

/**
 * @brief Obtient le logger par défaut
 */
std::shared_ptr<Logger> GetDefaultLogger() {
	return Registry::Instance().GetDefaultLogger();
}

/**
 * @brief Crée un logger avec un nom spécifique
 */
std::shared_ptr<Logger> CreateLogger(const std::string &name) {
	auto &registry = Registry::Instance();
	return registry.GetOrCreate(name);
}

/**
 * @brief Supprime tous les loggers
 */
void DropAll() {
	Registry::Instance().Clear();
}

/**
 * @brief Supprime un logger spécifique
 */
void Drop(const std::string &name) {
	Registry::Instance().Unregister(name);
}

} // namespace logger
} // namespace nkentseu