// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkRegistry.h
// DESCRIPTION: Registre global des loggers (similaire à spdlog).
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLoggerExport.h"
#include "NKLogger/NkLogger.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// CLASSE: NkRegistry
	// DESCRIPTION: Registre global singleton pour la gestion des loggers
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkRegistry {
		public:
			// ---------------------------------------------------------------------
			// MÉTHODES STATIQUES (SINGLETON)
			// ---------------------------------------------------------------------

			/**
			 * @brief Obtient l'instance singleton du registre
			 * @return Référence à l'instance du registre
			 */
			static NkRegistry &Instance();

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
			 * @param logger NkLogger à enregistrer
			 * @return true si enregistré, false si nom déjà existant
			 */
			bool Register(std::shared_ptr<NkLogger> logger);

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
			std::shared_ptr<NkLogger> Get(const std::string &name);

			/**
			 * @brief Obtient un logger par son nom (crée si non existant)
			 * @param name Nom du logger
			 * @return Pointeur vers le logger (existant ou nouvellement créé)
			 */
			std::shared_ptr<NkLogger> GetOrCreate(const std::string &name);

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
			core::usize GetLoggerCount() const;

			// ---------------------------------------------------------------------
			// CONFIGURATION GLOBALE
			// ---------------------------------------------------------------------

			/**
			 * @brief Définit le niveau de log global
			 * @param level Niveau de log global
			 */
			void SetGlobalLevel(NkLogLevel level);

			/**
			 * @brief Obtient le niveau de log global
			 * @return Niveau de log global
			 */
			NkLogLevel GetGlobalLevel() const;

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
			 * @param logger NkLogger par défaut
			 */
			void SetDefaultLogger(std::shared_ptr<NkLogger> logger);

			/**
			 * @brief Obtient le logger par défaut
			 * @return Pointeur vers le logger par défaut
			 */
			std::shared_ptr<NkLogger> GetDefaultLogger();

			/**
			 * @brief Crée un logger par défaut avec console sink
			 * @return Pointeur vers le nouveau logger par défaut
			 */
			std::shared_ptr<NkLogger> CreateDefaultLogger();

		private:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS PRIVÉS (SINGLETON)
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur privé (singleton)
			 */
			NkRegistry();

			/**
			 * @brief Destructeur privé
			 */
			~NkRegistry();

			/**
			 * @brief Constructeur de copie supprimé
			 */
			NkRegistry(const NkRegistry &) = delete;

			/**
			 * @brief Opérateur d'affectation supprimé
			 */
			NkRegistry &operator=(const NkRegistry &) = delete;

			// ---------------------------------------------------------------------
			// VARIABLES MEMBRE PRIVÉES
			// ---------------------------------------------------------------------

			/// Map des loggers par nom
			std::unordered_map<std::string, std::shared_ptr<NkLogger>> m_Loggers;

			/// NkLogger par défaut
			std::shared_ptr<NkLogger> m_DefaultLogger;

			/// Niveau de log global
			NkLogLevel m_GlobalLevel;

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
	NKLOGGER_API std::shared_ptr<NkLogger> GetLogger(const std::string &name);

	/**
	 * @brief Obtient le logger par défaut
	 * @return Pointeur vers le logger par défaut
	 */
	NKLOGGER_API std::shared_ptr<NkLogger> GetDefaultLogger();

	/**
	 * @brief Crée un logger avec un nom spécifique
	 * @param name Nom du logger
	 * @return Nouveau logger
	 */
	NKLOGGER_API std::shared_ptr<NkLogger> CreateLogger(const std::string &name);

	/**
	 * @brief Supprime tous les loggers
	 */
	NKLOGGER_API void DropAll();

	/**
	 * @brief Supprime un logger spécifique
	 * @param name Nom du logger à supprimer
	 */
	NKLOGGER_API void Drop(const std::string &name);

} // namespace nkentseu