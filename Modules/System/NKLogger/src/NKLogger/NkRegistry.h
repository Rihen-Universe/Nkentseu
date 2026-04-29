// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkRegistry.h
// DESCRIPTION: Registre global des loggers (similaire à spdlog).
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLoggerApi.h"
#include "NKLogger/NkLogger.h"
#include "NKThreading/NkMutex.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKMemory/NkSharedPtr.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // CLASSE: NkRegistry
    // DESCRIPTION: Registre global singleton pour la gestion des loggers
    // -------------------------------------------------------------------------
    class NKENTSEU_LOGGER_API NkRegistry {
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
            bool Register(memory::NkSharedPtr<NkLogger> logger);

            /**
             * @brief Désenregistre un logger du registre
             * @param name Nom du logger à désenregistrer
             * @return true si désenregistré, false si non trouvé
             */
            bool Unregister(const NkString &name);

            /**
             * @brief Obtient un logger par son nom
             * @param name Nom du logger
             * @return Pointeur vers le logger, nullptr si non trouvé
             */
            memory::NkSharedPtr<NkLogger> Get(const NkString &name);

            /**
             * @brief Obtient un logger par son nom (crée si non existant)
             * @param name Nom du logger
             * @return Pointeur vers le logger (existant ou nouvellement créé)
             */
            memory::NkSharedPtr<NkLogger> GetOrCreate(const NkString &name);

            /**
             * @brief Vérifie si un logger existe
             * @param name Nom du logger
             * @return true si existe, false sinon
             */
            bool Exists(const NkString &name) const;

            /**
             * @brief Supprime tous les loggers du registre
             */
            void Clear();

            /**
             * @brief Obtient la liste de tous les noms de loggers
             * @return Vecteur des noms de loggers
             */
            NkVector<NkString> GetLoggerNames() const;

            /**
             * @brief Obtient le nombre de loggers enregistrés
             * @return Nombre de loggers
             */
            usize GetLoggerCount() const;

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
            void SetGlobalPattern(const NkString &pattern);

            /**
             * @brief Obtient le pattern global
             * @return Pattern de formatage global
             */
            NkString GetGlobalPattern() const;

            /**
             * @brief Force le flush de tous les loggers
             */
            void FlushAll();

            /**
             * @brief Définit le logger par défaut
             * @param logger NkLogger par défaut
             */
            void SetDefaultLogger(memory::NkSharedPtr<NkLogger> logger);

            /**
             * @brief Obtient le logger par défaut
             * @return Pointeur vers le logger par défaut
             */
            memory::NkSharedPtr<NkLogger> GetDefaultLogger();

            /**
             * @brief Crée un logger par défaut avec console sink
             * @return Pointeur vers le nouveau logger par défaut
             */
            memory::NkSharedPtr<NkLogger> CreateDefaultLogger();

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
            NkVector<NkPair<NkString, memory::NkSharedPtr<NkLogger>>> m_Loggers;

            /// NkLogger par défaut
            memory::NkSharedPtr<NkLogger> m_DefaultLogger;

            /// Niveau de log global
            NkLogLevel m_GlobalLevel;

            /// Pattern global
            NkString m_GlobalPattern;

            /// Mutex pour la synchronisation thread-safe
            mutable threading::NkMutex m_Mutex;

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
    NKENTSEU_LOGGER_API memory::NkSharedPtr<NkLogger> GetLogger(const NkString &name);

    /**
     * @brief Obtient le logger par défaut
     * @return Pointeur vers le logger par défaut
     */
    NKENTSEU_LOGGER_API memory::NkSharedPtr<NkLogger> GetDefaultLogger();

    /**
     * @brief Crée un logger avec un nom spécifique
     * @param name Nom du logger
     * @return Nouveau logger
     */
    NKENTSEU_LOGGER_API memory::NkSharedPtr<NkLogger> CreateLogger(const NkString &name);

    /**
     * @brief Supprime tous les loggers
     */
    NKENTSEU_LOGGER_API void DropAll();

    /**
     * @brief Supprime un logger spécifique
     * @param name Nom du logger à supprimer
     */
    NKENTSEU_LOGGER_API void Drop(const NkString &name);

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION - NkRegistry
// =============================================================================
//
//   NkRegistry::Initialize();
//   memory::NkSharedPtr<NkLogger> core = NkRegistry::CreateLogger("core");
//   if (core) {
//       core->Info("Registry OK");
//   }
//   NkRegistry::Shutdown();
//
// =============================================================================
