// =============================================================================
// NKLogger/NkLogLevel.h
// Définition des niveaux de log et fonctions utilitaires de conversion.
//
// Design :
//  - Énumération fortement typée pour la sécurité des niveaux de log
//  - Fonctions de conversion bidirectionnelles (enum ↔ string)
//  - Support multiplateforme : couleurs ANSI et Windows Console
//  - Intégration avec les macros d'export NKENTSEU_LOGGER_API
//  - Zéro dépendance externe hors NKCore/NkTypes.h
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_LOGGER_NKLOGLEVEL_H
#define NKENTSEU_LOGGER_NKLOGLEVEL_H


    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des types de base du projet pour uint8 et compatibilité.
    // NkLogLevel est un header fondamental : dépendances minimales requises.

    #include "NKCore/NkTypes.h"
    #include "NKLogger/NkLoggerApi.h"


    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module logger sont encapsulés dans ce namespace
    // pour éviter les collisions et organiser l'API publiquement.

    namespace nkentseu {


        // ---------------------------------------------------------------------
        // ÉNUMÉRATION : NkLogLevel
        // DESCRIPTION : Niveaux de log disponibles, ordonnés par sévérité
        // ---------------------------------------------------------------------
        /**
         * @enum NkLogLevel
         * @brief Niveaux de log hiérarchisés pour le filtrage et l'affichage
         * @ingroup LoggerTypes
         *
         * Les niveaux sont ordonnés par sévérité croissante (0 = plus verbeux).
         * Cette hiérarchie permet un filtrage efficace : un niveau N affiche
         * tous les messages de niveau >= N.
         *
         * Compatibilité : Inspiré de spdlog/log4j pour familiarité développeur.
         *
         * @note NK_OFF (7) est une valeur sentinelle pour désactiver tout logging.
         *
         * @example
         * @code
         * // Filtrage par niveau : afficher uniquement warnings et plus graves
         * if (currentLevel >= NkLogLevel::NK_WARN) {
         *     Logger::Log(NkLogLevel::NK_WARN, "Attention requise");
         * }
         * @endcode
         */
        enum class NKENTSEU_LOGGER_CLASS_EXPORT NkLogLevel : uint8 {
            /// @brief Niveau trace - messages de trace très détaillés (développement)
            NK_TRACE = 0,

            /// @brief Niveau debug - messages de débogage technique
            NK_DEBUG = 1,

            /// @brief Niveau info - messages informatifs normaux (production)
            NK_INFO = 2,

            /// @brief Niveau warn - messages d'avertissement non bloquants
            NK_WARN = 3,

            /// @brief Niveau error - erreurs récupérables nécessitant attention
            NK_ERROR = 4,

            /// @brief Niveau critical - erreurs critiques impactant une fonctionnalité
            NK_CRITICAL = 5,

            /// @brief Niveau fatal - erreurs fatales entraînant l'arrêt de l'application
            NK_FATAL = 6,

            /// @brief Désactivation complète du système de logging
            NK_OFF = 7
        };


        // ---------------------------------------------------------------------
        // SECTION 3 : FONCTIONS UTILITAIRES DE CONVERSION (API PUBLIQUE)
        // ---------------------------------------------------------------------
        /**
         * @defgroup LogLevelUtils Fonctions Utilitaires NkLogLevel
         * @brief Fonctions de conversion et d'interrogation des niveaux de log
         * @ingroup LoggerApi
         *
         * Ces fonctions sont thread-safe et sans état interne.
         * Elles peuvent être appelées depuis n'importe quel contexte.
         */

        /**
         * @brief Convertit un NkLogLevel en chaîne de caractères lisible
         * @param level Niveau de log à convertir
         * @return Chaîne statique représentant le niveau (ex: "info", "error")
         * @ingroup LogLevelUtils
         *
         * @note La chaîne retournée est en mémoire statique : ne pas libérer.
         * @note Retourne "unknown" pour les valeurs hors énumération.
         *
         * @example
         * @code
         * auto level = NkLogLevel::NK_WARN;
         * const char* text = NkLogLevelToString(level);  // "warning"
         * @endcode
         */
        NKENTSEU_LOGGER_API const char* NkLogLevelToString(NkLogLevel level);

        /**
         * @brief Convertit un NkLogLevel en chaîne courte (3 caractères majuscules)
         * @param level Niveau de log à convertir
         * @return Chaîne statique courte (ex: "INF", "ERR", "WRN")
         * @ingroup LogLevelUtils
         *
         * @note Format optimisé pour l'affichage dans des logs compacts ou tables.
         * @note La chaîne retournée est en mémoire statique : ne pas libérer.
         *
         * @example
         * @code
         * // Affichage compact : [INF] Application démarrée
         * printf("[%s] %s\n", NkLogLevelToShortString(level), message);
         * @endcode
         */
        NKENTSEU_LOGGER_API const char* NkLogLevelToShortString(NkLogLevel level);

        /**
         * @brief Convertit une chaîne de caractères en NkLogLevel (case-insensitive)
         * @param str Chaîne à convertir (ex: "info", "WARNING", "Err")
         * @return NkLogLevel correspondant, ou NkLogLevel::NK_INFO par défaut
         * @ingroup LogLevelUtils
         *
         * @note La comparaison est insensible à la casse : "INFO" == "info" == "Info"
         * @note Accepte les alias : "warn" et "warning" → NK_WARN
         * @note Retourne NK_INFO pour toute chaîne non reconnue (fail-safe)
         *
         * @example
         * @code
         * // Parsing depuis configuration utilisateur
         * const char* configLevel = GetUserConfig("log_level");  // "error"
         * auto level = NkStringToLogLevel(configLevel);  // NkLogLevel::NK_ERROR
         * @endcode
         */
        NKENTSEU_LOGGER_API NkLogLevel NkStringToLogLevel(const char* str);

        /**
         * @brief Convertit une chaîne courte (3 lettres) en NkLogLevel
         * @param str Chaîne courte à convertir (ex: "INF", "ERR", "WRN")
         * @return NkLogLevel correspondant, ou NkLogLevel::NK_INFO par défaut
         * @ingroup LogLevelUtils
         *
         * @note La comparaison est sensible à la casse : utiliser majuscules uniquement
         * @note Plus rapide que NkStringToLogLevel pour les formats compacts
         * @note Retourne NK_INFO pour toute chaîne non reconnue (fail-safe)
         *
         * @example
         * @code
         * // Parsing depuis format binaire ou protocole réseau
         * char shortCode[4] = "CRT";
         * auto level = NkShortStringToLogLevel(shortCode);  // NkLogLevel::NK_CRITICAL
         * @endcode
         */
        NKENTSEU_LOGGER_API NkLogLevel NkShortStringToLogLevel(const char* str);

        /**
         * @brief Obtient le code couleur ANSI pour l'affichage console Unix/Linux
         * @param level Niveau de log pour lequel obtenir la couleur
         * @return Séquence d'échappement ANSI (ex: "\033[32m" pour vert)
         * @ingroup LogLevelUtils
         *
         * @note Retourne "\033[0m" (reset) pour les niveaux non colorés ou inconnus
         * @note À combiner avec la sortie console : printf("%s%s\033[0m\n", color, msg)
         * @note Compatible avec la plupart des terminaux modernes (xterm, gnome-terminal, etc.)
         *
         * @example
         * @code
         * // Affichage coloré en console Linux/macOS
         * const char* color = NkLogLevelToANSIColor(NkLogLevel::NK_ERROR);
         * printf("%s[ERROR] %s\033[0m\n", color, errorMessage);
         * @endcode
         */
        NKENTSEU_LOGGER_API const char* NkLogLevelToANSIColor(NkLogLevel level);

        /**
         * @brief Obtient le code couleur Windows Console pour l'affichage natif
         * @param level Niveau de log pour lequel obtenir la couleur
         * @return Attribut de couleur Windows (WORD, 16 bits)
         * @ingroup LogLevelUtils
         *
         * @note Format compatible avec SetConsoleTextAttribute() de l'API Win32
         * @note Les bits hauts (4-7) définissent le fond, les bits bas (0-3) le texte
         * @note Retourne 0x07 (gris sur noir) pour les niveaux non colorés ou inconnus
         *
         * @example
         * @code
         * // Affichage coloré en console Windows
         * #ifdef NKENTSEU_PLATFORM_WINDOWS
         *     HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
         *     WORD color = NkLogLevelToWindowsColor(NkLogLevel::NK_WARN);
         *     SetConsoleTextAttribute(hConsole, color);
         *     printf("[WARNING] %s\n", message);
         *     SetConsoleTextAttribute(hConsole, 0x07);  // Reset
         * #endif
         * @endcode
         */
        NKENTSEU_LOGGER_API uint16 NkLogLevelToWindowsColor(NkLogLevel level);

        /**
         * @brief Vérifie si un niveau de log est activé pour un seuil donné
         * @param level Niveau de log à tester
         * @param threshold Seuil d'activation minimal (ex: NK_INFO)
         * @return true si level >= threshold, false sinon
         * @ingroup LogLevelUtils
         *
         * @note Fonction inline optimisée pour les checks fréquents dans le chemin critique
         * @note Permet d'éviter la construction de messages de log inutiles
         *
         * @example
         * @code
         * // Éviter un calcul coûteux si le niveau debug n'est pas activé
         * if (NkLogLevelIsEnabled(NkLogLevel::NK_DEBUG, currentThreshold)) {
         *     Logger::Log(NkLogLevel::NK_DEBUG, ComputeExpensiveDebugInfo());
         * }
         * @endcode
         */
        NKENTSEU_LOGGER_API_INLINE bool NkLogLevelIsEnabled(NkLogLevel level, NkLogLevel threshold);


    } // namespace nkentseu


    // -------------------------------------------------------------------------
    // SECTION 4 : IMPLÉMENTATIONS INLINE (HEADER-ONLY COMPATIBLE)
    // -------------------------------------------------------------------------
    // Ces fonctions sont définies inline pour permettre l'optimisation
    // tout en restant compatibles avec le mode header-only.

    namespace nkentseu {

        NKENTSEU_LOGGER_API_INLINE bool NkLogLevelIsEnabled(NkLogLevel level, NkLogLevel threshold) {
            return static_cast<uint8>(level) >= static_cast<uint8>(threshold);
        }
        
    } // namespace nkentseu


#endif // NKENTSEU_LOGGER_NKLOGLEVEL_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOGLEVEL.H
// =============================================================================
// Ce fichier définit les niveaux de log et utilitaires de conversion.
// Il est conçu pour être utilisé dans tout le codebase avec une API cohérente.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Utilisation basique des niveaux de log
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogLevel.h>

    void ProcessData() {
        using namespace nkentseu::logger;

        // Logging à différents niveaux selon la sévérité
        NkLogLevelToString(NkLogLevel::NK_DEBUG);    // "debug"
        NkLogLevelToShortString(NkLogLevel::NK_INFO); // "INF"

        // Vérification rapide avant logging coûteux
        if (NkLogLevelIsEnabled(NkLogLevel::NK_TRACE, GetCurrentLogLevel())) {
            LogTrace("Entry point with params: %p", this);
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Parsing de configuration utilisateur
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogLevel.h>
    #include <NKLogger/LoggerConfig.h>

    bool LoadLoggingConfig(const char* configPath) {
        using namespace nkentseu::logger;

        // Lecture depuis fichier JSON/XML/INI
        const char* levelStr = ConfigGetString(configPath, "logging.level", "info");

        // Conversion robuste avec fallback sécurisé
        NkLogLevel level = NkStringToLogLevel(levelStr);  // "warn" → NK_WARN

        // Validation et application
        if (level != NkLogLevel::NK_INFO) {  // Vérification valeur par défaut
            Logger::SetGlobalLevel(level);
            Logger::Log(NkLogLevel::NK_INFO, "Log level set to: %s",
                       NkLogLevelToString(level));
        }

        return true;
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Affichage console coloré multiplateforme
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogLevel.h>
    #include <NKPlatform/NkPlatformDetect.h>

    #ifdef NKENTSEU_PLATFORM_WINDOWS
        #include <windows.h>
    #endif

    void ConsoleLog(nkentseu::logger::NkLogLevel level, const char* message) {
        using namespace nkentseu::logger;

        #ifdef NKENTSEU_PLATFORM_WINDOWS
            // Windows Console : utilisation des attributs de couleur natifs
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            WORD color = NkLogLevelToWindowsColor(level);
            SetConsoleTextAttribute(hConsole, color);
            printf("[%s] %s\n", NkLogLevelToShortString(level), message);
            SetConsoleTextAttribute(hConsole, 0x07);  // Reset to default
        #else
            // Unix/Linux/macOS : utilisation des séquences ANSI
            const char* color = NkLogLevelToANSIColor(level);
            printf("%s[%s] %s\033[0m\n", color,
                   NkLogLevelToShortString(level), message);
        #endif
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Filtrage dynamique dans un backend de logging
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogLevel.h>
    #include <NKLogger/NkLogEntry.h>

    class LogFilter {
    public:
        // Constructeur avec seuil configurable
        LogFilter(nkentseu::logger::NkLogLevel minLevel)
            : m_minLevel(minLevel) {}

        // Méthode de filtrage optimisée
        NKENTSEU_LOGGER_API_INLINE bool ShouldLog(nkentseu::logger::NkLogLevel entryLevel) const {
            return nkentseu::logger::NkLogLevelIsEnabled(entryLevel, m_minLevel);
        }

        // Mise à jour dynamique du seuil (thread-safe à gérer en externe)
        void SetMinLevel(nkentseu::logger::NkLogLevel level) {
            m_minLevel = level;
        }

        nkentseu::logger::NkLogLevel GetMinLevel() const {
            return m_minLevel;
        }

    private:
        nkentseu::logger::NkLogLevel m_minLevel;
    };

    // Utilisation
    void ProcessLogBatch(const NkLogEntry* entries, size_t count, LogFilter& filter) {
        for (size_t i = 0; i < count; ++i) {
            if (filter.ShouldLog(entries[i].level)) {
                ForwardToBackend(entries[i]);  // Seulement si niveau suffisant
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Tests unitaires des conversions
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogLevel.h>
    #include <cassert>
    #include <cstring>

    void TestNkLogLevelConversions() {
        using namespace nkentseu::logger;

        // Test round-trip : enum → string → enum
        assert(NkStringToLogLevel(NkLogLevelToString(NkLogLevel::NK_ERROR))
               == NkLogLevel::NK_ERROR);

        // Test insensibilité à la casse
        assert(NkStringToLogLevel("WARNING") == NkLogLevel::NK_WARN);
        assert(NkStringToLogLevel("warning") == NkLogLevel::NK_WARN);
        assert(NkStringToLogLevel("WaRnInG") == NkLogLevel::NK_WARN);

        // Test des alias
        assert(NkStringToLogLevel("warn") == NkLogLevel::NK_WARN);
        assert(NkStringToLogLevel("warning") == NkLogLevel::NK_WARN);

        // Test format court
        assert(std::strcmp(NkLogLevelToShortString(NkLogLevel::NK_CRITICAL), "CRT") == 0);
        assert(NkShortStringToLogLevel("FTL") == NkLogLevel::NK_FATAL);

        // Test valeur par défaut (fail-safe)
        assert(NkStringToLogLevel(nullptr) == NkLogLevel::NK_INFO);
        assert(NkStringToLogLevel("invalid_level") == NkLogLevel::NK_INFO);
        assert(NkShortStringToLogLevel("XYZ") == NkLogLevel::NK_INFO);

        // Test fonction inline de filtrage
        assert(NkLogLevelIsEnabled(NkLogLevel::NK_ERROR, NkLogLevel::NK_WARN) == true);
        assert(NkLogLevelIsEnabled(NkLogLevel::NK_DEBUG, NkLogLevel::NK_INFO) == false);
        assert(NkLogLevelIsEnabled(NkLogLevel::NK_INFO, NkLogLevel::NK_INFO) == true);
    }
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Intégration avec système de configuration hiérarchique
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogLevel.h>
    #include <NKCore/NkConfig.h>

    class LoggerConfig {
    public:
        // Chargement avec priorités : CLI > Env > File > Default
        static nkentseu::logger::NkLogLevel ResolveLogLevel(const char* moduleName) {
            using namespace nkentseu::logger;

            // 1. Override en ligne de commande (plus haute priorité)
            const char* cliLevel = nkentseu::core::Config::GetCommandLineArg("--log-level");
            if (cliLevel) {
                return NkStringToLogLevel(cliLevel);
            }

            // 2. Variable d'environnement module-spécifique
            char envVar[64];
            snprintf(envVar, sizeof(envVar), "NK_LOG_%s", moduleName);
            const char* envLevel = std::getenv(envVar);
            if (envLevel) {
                return NkStringToLogLevel(envLevel);
            }

            // 3. Variable d'environnement globale
            const char* globalEnv = std::getenv("NK_LOG_LEVEL");
            if (globalEnv) {
                return NkStringToLogLevel(globalEnv);
            }

            // 4. Fichier de configuration
            const char* fileLevel = nkentseu::core::Config::GetString(
                "logging.modules." + std::string(moduleName) + ".level", "info");
            if (fileLevel) {
                return NkStringToLogLevel(fileLevel);
            }

            // 5. Valeur par défaut du système
            return NkLogLevel::NK_INFO;
        }
    };
*/


// =============================================================================
// NOTES DE MAINTENANCE ET EXTENSIBILITÉ
// =============================================================================
/*
    1. AJOUT D'UN NOUVEAU NIVEAU DE LOG :
       - Ajouter l'entrée dans l'enum NkLogLevel avec valeur numérique appropriée
       - Mettre à jour TOUS les switch dans NkLogLevel.cpp (5 fonctions)
       - Ajouter la documentation Doxygen pour le nouveau niveau
       - Mettre à jour les exemples et tests unitaires
       - Vérifier la rétrocompatibilité ABI si bibliothèque partagée

    2. PERSONNALISATION DES COULEURS :
       - Les fonctions NkLogLevelToANSIColor/WindowsColor peuvent être surchargées
         via des callbacks si nécessaire pour des thèmes personnalisés
       - Conserver les valeurs par défaut pour la cohérence cross-projet

    3. OPTIMISATIONS FUTURES :
       - Envisager une table de lookup constexpr pour NkLogLevelToString en C++17+
       - Ajouter NkLogLevelToANSIColorBold() pour emphasis visuel optionnel
       - Support des couleurs 24-bit (truecolor) pour terminaux compatibles

    4. INTERNATIONALISATION :
       - Les chaînes retournées sont en anglais pour cohérence technique
       - Pour une UI localisée, utiliser une couche de traduction séparée
       - Ne pas modifier les retours de ces fonctions pour éviter de casser le parsing

    5. THREAD-SAFETY :
       - Toutes les fonctions sont stateless et thread-safe par conception
       - Les chaînes retournées pointent vers du stockage statique en lecture seule
       - Aucune synchronisation nécessaire pour l'appel concurrent
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================