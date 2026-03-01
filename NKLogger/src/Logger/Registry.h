// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Registry.h
// DESCRIPTION: Registre global des loggers (similaire à spdlog).
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Export.h"
#include "Logger/Logger.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: Registry
// DESCRIPTION: Registre global singleton pour la gestion des loggers
// -------------------------------------------------------------------------
class LOGGER_API Registry {
public:
	// ---------------------------------------------------------------------
	// MÉTHODES STATIQUES (SINGLETON)
	// ---------------------------------------------------------------------

	/**
	 * @brief Obtient l'instance singleton du registre
	 * @return Référence à l'instance du registre
	 */
	static Registry &Instance();

	/**
	 * @brief Initialise le registre avec des paramètres par défaut
	 */
	static void Initialize();

	/**
	 * @brief Nettoie le registre (détruit tous les loggers)
	 */
	static void Shutdown();

	// ---------------------------------------------------------------------
	// GESTION DES LOGGERS
	// ---------------------------------------------------------------------

	/**
	 * @brief Enregistre un logger dans le registre
	 * @param logger Logger à enregistrer
	 * @return true si enregistré, false si nom déjà existant
	 */
	bool Register(std::shared_ptr<Logger> logger);

	/**
	 * @brief Désenregistre un logger du registre
	 * @param name Nom du logger à désenregistrer
	 * @return true si désenregistré, false si non trouvé
	 */
	bool Unregister(const std::string &name);

	/**
	 * @brief Obtient un logger par son nom
	 * @param name Nom du logger
	 * @return Pointeur vers le logger, nullptr si non trouvé
	 */
	std::shared_ptr<Logger> Get(const std::string &name);

	/**
	 * @brief Obtient un logger par son nom (crée si non existant)
	 * @param name Nom du logger
	 * @return Pointeur vers le logger (existant ou nouvellement créé)
	 */
	std::shared_ptr<Logger> GetOrCreate(const std::string &name);

	/**
	 * @brief Vérifie si un logger existe
	 * @param name Nom du logger
	 * @return true si existe, false sinon
	 */
	bool Exists(const std::string &name) const;

	/**
	 * @brief Supprime tous les loggers du registre
	 */
	void Clear();

	/**
	 * @brief Obtient la liste de tous les noms de loggers
	 * @return Vecteur des noms de loggers
	 */
	std::vector<std::string> GetLoggerNames() const;

	/**
	 * @brief Obtient le nombre de loggers enregistrés
	 * @return Nombre de loggers
	 */
	size_t GetLoggerCount() const;

	// ---------------------------------------------------------------------
	// CONFIGURATION GLOBALE
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit le niveau de log global
	 * @param level Niveau de log global
	 */
	void SetGlobalLevel(LogLevel level);

	/**
	 * @brief Obtient le niveau de log global
	 * @return Niveau de log global
	 */
	LogLevel GetGlobalLevel() const;

	/**
	 * @brief Définit le pattern global
	 * @param pattern Pattern de formatage global
	 */
	void SetGlobalPattern(const std::string &pattern);

	/**
	 * @brief Obtient le pattern global
	 * @return Pattern de formatage global
	 */
	std::string GetGlobalPattern() const;

	/**
	 * @brief Force le flush de tous les loggers
	 */
	void FlushAll();

	/**
	 * @brief Définit le logger par défaut
	 * @param logger Logger par défaut
	 */
	void SetDefaultLogger(std::shared_ptr<Logger> logger);

	/**
	 * @brief Obtient le logger par défaut
	 * @return Pointeur vers le logger par défaut
	 */
	std::shared_ptr<Logger> GetDefaultLogger();

	/**
	 * @brief Crée un logger par défaut avec console sink
	 * @return Pointeur vers le nouveau logger par défaut
	 */
	std::shared_ptr<Logger> CreateDefaultLogger();

private:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS PRIVÉS (SINGLETON)
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur privé (singleton)
	 */
	Registry();

	/**
	 * @brief Destructeur privé
	 */
	~Registry();

	/**
	 * @brief Constructeur de copie supprimé
	 */
	Registry(const Registry &) = delete;

	/**
	 * @brief Opérateur d'affectation supprimé
	 */
	Registry &operator=(const Registry &) = delete;

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Map des loggers par nom
	std::unordered_map<std::string, std::shared_ptr<Logger>> m_Loggers;

	/// Logger par défaut
	std::shared_ptr<Logger> m_DefaultLogger;

	/// Niveau de log global
	LogLevel m_GlobalLevel;

	/// Pattern global
	std::string m_GlobalPattern;

	/// Mutex pour la synchronisation thread-safe
	mutable std::mutex m_Mutex;

	/// Indicateur d'initialisation
	bool m_Initialized;
};

// -------------------------------------------------------------------------
// FONCTIONS UTILITAIRES GLOBALES
// -------------------------------------------------------------------------

/**
 * @brief Obtient un logger par son nom
 * @param name Nom du logger
 * @return Pointeur vers le logger
 */
LOGGER_API std::shared_ptr<Logger> GetLogger(const std::string &name);

/**
 * @brief Obtient le logger par défaut
 * @return Pointeur vers le logger par défaut
 */
LOGGER_API std::shared_ptr<Logger> GetDefaultLogger();

/**
 * @brief Crée un logger avec un nom spécifique
 * @param name Nom du logger
 * @return Nouveau logger
 */
LOGGER_API std::shared_ptr<Logger> CreateLogger(const std::string &name);

/**
 * @brief Supprime tous les loggers
 */
LOGGER_API void DropAll();

/**
 * @brief Supprime un logger spécifique
 * @param name Nom du logger à supprimer
 */
LOGGER_API void Drop(const std::string &name);

} // namespace logger
} // namespace nkentseu