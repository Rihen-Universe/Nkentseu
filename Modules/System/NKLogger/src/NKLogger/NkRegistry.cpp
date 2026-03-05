// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkRegistry.cpp
// DESCRIPTION: Implémentation du registre global des loggers.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkRegistry.h"
#include "NKLogger/Sinks/NkConsoleSink.h"
#include <algorithm>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// IMPLÉMENTATION DE NkRegistry
	// -------------------------------------------------------------------------

	/**
	 * @brief Constructeur privé
	 */
	NkRegistry::NkRegistry()
		: m_GlobalLevel(NkLogLevel::NK_INFO), m_GlobalPattern(NkFormatter::NK_DEFAULT_PATTERN), m_Initialized(false) {
	}

	/**
	 * @brief Destructeur privé
	 */
	NkRegistry::~NkRegistry() {
		Clear();
	}

	/**
	 * @brief Obtient l'instance singleton du registre
	 */
	NkRegistry &NkRegistry::Instance() {
		static NkRegistry instance;
		return instance;
	}

	/**
	 * @brief Initialise le registre avec des paramètres par défaut
	 */
	void NkRegistry::Initialize() {
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
	void NkRegistry::Shutdown() {
		auto &instance = Instance();
		instance.Clear();
		instance.m_Initialized = false;
	}

	/**
	 * @brief Enregistre un logger dans le registre
	 */
	bool NkRegistry::Register(std::shared_ptr<NkLogger> logger) {
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
	bool NkRegistry::Unregister(const std::string &name) {
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
	std::shared_ptr<NkLogger> NkRegistry::Get(const std::string &name) {
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
	std::shared_ptr<NkLogger> NkRegistry::GetOrCreate(const std::string &name) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_Loggers.find(name);

		if (it != m_Loggers.end()) {
			return it->second;
		}

		// Création d'un nouveau logger
		auto logger = std::make_shared<NkLogger>(name);
		logger->SetLevel(m_GlobalLevel);
		logger->SetPattern(m_GlobalPattern);

		m_Loggers[name] = logger;
		return logger;
	}

	/**
	 * @brief Vérifie si un logger existe
	 */
	bool NkRegistry::Exists(const std::string &name) const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Loggers.find(name) != m_Loggers.end();
	}

	/**
	 * @brief Supprime tous les loggers du registre
	 */
	void NkRegistry::Clear() {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Loggers.clear();
		m_DefaultLogger.reset();
	}

	/**
	 * @brief Obtient la liste de tous les noms de loggers
	 */
	std::vector<std::string> NkRegistry::GetLoggerNames() const {
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
	core::usize NkRegistry::GetLoggerCount() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Loggers.size();
	}

	/**
	 * @brief Définit le niveau de log global
	 */
	void NkRegistry::SetGlobalLevel(NkLogLevel level) {
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
	NkLogLevel NkRegistry::GetGlobalLevel() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_GlobalLevel;
	}

	/**
	 * @brief Définit le pattern global
	 */
	void NkRegistry::SetGlobalPattern(const std::string &pattern) {
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
	std::string NkRegistry::GetGlobalPattern() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_GlobalPattern;
	}

	/**
	 * @brief Force le flush de tous les loggers
	 */
	void NkRegistry::FlushAll() {
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
	void NkRegistry::SetDefaultLogger(std::shared_ptr<NkLogger> logger) {
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
	std::shared_ptr<NkLogger> NkRegistry::GetDefaultLogger() {
		std::lock_guard<std::mutex> lock(m_Mutex);

		if (!m_DefaultLogger) {
			m_DefaultLogger = CreateDefaultLogger();
		}

		return m_DefaultLogger;
	}

	/**
	 * @brief Crée un logger par défaut avec console sink
	 */
	std::shared_ptr<NkLogger> NkRegistry::CreateDefaultLogger() {
		auto logger = std::make_shared<NkLogger>("default");

		logger->SetLevel(m_GlobalLevel);
		logger->SetPattern(m_GlobalPattern);

		// Ajouter un sink console par défaut
		auto consoleSink = std::make_shared<NkConsoleSink>();
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
	std::shared_ptr<NkLogger> GetLogger(const std::string &name) {
		return NkRegistry::Instance().Get(name);
	}

	/**
	 * @brief Obtient le logger par défaut
	 */
	std::shared_ptr<NkLogger> GetDefaultLogger() {
		return NkRegistry::Instance().GetDefaultLogger();
	}

	/**
	 * @brief Crée un logger avec un nom spécifique
	 */
	std::shared_ptr<NkLogger> CreateLogger(const std::string &name) {
		auto &registry = NkRegistry::Instance();
		return registry.GetOrCreate(name);
	}

	/**
	 * @brief Supprime tous les loggers
	 */
	void DropAll() {
		NkRegistry::Instance().Clear();
	}

	/**
	 * @brief Supprime un logger spécifique
	 */
	void Drop(const std::string &name) {
		NkRegistry::Instance().Unregister(name);
	}
} // namespace nkentseu