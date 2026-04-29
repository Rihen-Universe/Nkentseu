// =============================================================================
// NKLogger/NkLogMessage.h
// Structure contenant toutes les informations d'un message de log.
//
// Design :
//  - Structure POD-like avec constructeurs flexibles pour usage optimisé
//  - Méthodes utilitaires pour conversion temporelle et validation
//  - Intégration avec NkString pour gestion mémoire cohérente
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la structure
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Compatible header-only via NKENTSEU_LOGGER_API_INLINE si nécessaire
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_LOGGER_NKLOGMESSAGE_H
#define NKENTSEU_LOGGER_NKLOGMESSAGE_H


    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions standards pour le temps et les types de base.
    // Dépendances projet pour NkString et NkLogLevel.

    #include <ctime>
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKLogger/NkLogLevel.h"
    #include "NKLogger/NkLoggerApi.h"


    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Encapsulation des types logger dans le namespace dédié.

    namespace nkentseu {

        // ---------------------------------------------------------------------
        // STRUCTURE : NkLogMessage
        // DESCRIPTION : Conteneur complet d'un événement de logging
        // ---------------------------------------------------------------------
        /**
         * @struct NkLogMessage
         * @brief Représentation complète d'un message de log avec métadonnées
         * @ingroup LoggerTypes
         *
         * Cette structure contient toutes les informations nécessaires pour
         * formatter, filtrer et router un message de log vers ses destinations.
         *
         * Organisation mémoire :
         *  - Champs scalaires en premier pour alignement optimal
         *  - NkString à la fin pour flexibilité d'allocation
         *  - Taille totale : ~120-200 bytes selon implémentation NkString
         *
         * Thread-safety :
         *  - Structure copyable et movable par défaut
         *  - Aucune synchronisation interne : à gérer par le caller si partagé
         *
         * @note Les champs sourceFile/sourceLine/functionName sont optionnels.
         *       Ils peuvent être omis en production pour réduire l'overhead.
         *
         * @example
         * @code
         * // Construction minimale
         * NkLogMessage msg(NkLogLevel::NK_INFO, "Application started");
         *
         * // Construction avec métadonnées de debug
         * NkLogMessage debugMsg(
         *     NkLogLevel::NK_DEBUG,
         *     "Buffer allocated",
         *     __FILE__, __LINE__, __FUNCTION__,
         *     "MemoryManager"
         * );
         * @endcode
         */
        struct NKENTSEU_LOGGER_CLASS_EXPORT NkLogMessage {


            // -----------------------------------------------------------------
            // SECTION 3 : DONNÉES TEMPORELLES
            // -----------------------------------------------------------------
            /**
             * @defgroup LogMessageTimestamp Champs Temporels
             * @brief Informations de timestamp pour ordonnancement et affichage
             */

            /// @brief Timestamp en nanosecondes depuis Unix epoch (UTC)
            /// @ingroup LogMessageTimestamp
            uint64 timestamp;


            // -----------------------------------------------------------------
            // SECTION 4 : INFORMATIONS DE THREAD
            // -----------------------------------------------------------------
            /**
             * @defgroup LogMessageThread Champs Thread
             * @brief Identifiants pour corrélation et debugging multi-thread
             */

            /// @brief ID numérique unique du thread émetteur
            /// @ingroup LogMessageThread
            uint32 threadId;

            /// @brief Nom lisible du thread (optionnel, peut être vide)
            /// @ingroup LogMessageThread
            /// @note Alloué dynamiquement via NkString : éviter en chemin critique
            NkString threadName;


            // -----------------------------------------------------------------
            // SECTION 5 : INFORMATIONS DE LOG PRINCIPALES
            // -----------------------------------------------------------------
            /**
             * @defgroup LogMessageCore Champs Principaux de Log
             * @brief Données essentielles pour tout message de log
             */

            /// @brief Niveau de sévérité du message
            /// @ingroup LogMessageCore
            NkLogLevel level;

            /// @brief Contenu textuel du message de log
            /// @ingroup LogMessageCore
            /// @note Supporte UTF-8 via NkString pour internationalisation
            NkString message;

            /// @brief Nom du logger émetteur (hiérarchie pointée optionnelle)
            /// @ingroup LogMessageCore
            /// @example "nkentseu.renderer.texture", "nkentseu.network.http"
            NkString loggerName;


            // -----------------------------------------------------------------
            // SECTION 6 : INFORMATIONS DE SOURCE (DEBUG/DEV)
            // -----------------------------------------------------------------
            /**
             * @defgroup LogMessageSource Champs de Source Code
             * @brief Métadonnées pour tracing et debugging (optionnelles en prod)
             *
             * @note Ces champs peuvent être désactivés via NKLOGGER_DISABLE_SOURCE_INFO
             *       pour réduire la taille des messages en production.
             */

            /// @brief Chemin du fichier source ayant émis le log
            /// @ingroup LogMessageSource
            /// @note Peut être relatif ou absolu selon configuration du build
            NkString sourceFile;

            /// @brief Numéro de ligne dans le fichier source
            /// @ingroup LogMessageSource
            /// @note Valeur 0 indique une information de ligne non disponible
            uint32 sourceLine;

            /// @brief Nom de la fonction/méthode émettrice
            /// @ingroup LogMessageSource
            /// @note Inclut les paramètres templates si compilé avec RTTI
            NkString functionName;


            // -----------------------------------------------------------------
            // SECTION 7 : CONSTRUCTEURS
            // -----------------------------------------------------------------
            /**
             * @defgroup LogMessageCtors Constructeurs de NkLogMessage
             * @brief Constructeurs avec différents niveaux de détail
             */

            /**
             * @brief Constructeur par défaut - initialise avec valeurs neutres
             * @ingroup LogMessageCtors
             *
             * @post timestamp = now, threadId = current, level = NK_INFO
             * @post Tous les NkString sont vides, sourceLine = 0
             *
             * @note Appel NkGetNowNs() : coût ~20-50 cycles selon plateforme
             */
            NkLogMessage();

            /**
             * @brief Constructeur avec niveau et message uniquement
             * @ingroup LogMessageCtors
             *
             * @param lvl Niveau de sévérité du message
             * @param msg Contenu textuel du message (copié via NkString)
             * @param logger Nom optionnel du logger émetteur
             *
             * @post Les autres champs sont initialisés comme le constructeur par défaut
             * @note Constructor delegation vers NkLogMessage() pour cohérence
             *
             * @example
             * @code
             * NkLogMessage info(NkLogLevel::NK_INFO, "Server listening on port 8080");
             * NkLogMessage custom(NkLogLevel::NK_WARN, "High memory usage", "Monitor");
             * @endcode
             */
            NkLogMessage(NkLogLevel lvl, const NkString& msg, const NkString& logger = "");

            /**
             * @brief Constructeur complet avec métadonnées de source
             * @ingroup LogMessageCtors
             *
             * @param lvl Niveau de sévérité du message
             * @param msg Contenu textuel du message
             * @param file Chemin du fichier source (optionnel, peut être vide)
             * @param line Numéro de ligne source (0 = non spécifié)
             * @param func Nom de la fonction émettrice (optionnel)
             * @param logger Nom du logger émetteur (optionnel)
             *
             * @post Délègue au constructeur à 3 paramètres puis complète les champs source
             * @note Utilisation typique via macros : LOG_INFO("msg") → __FILE__, __LINE__, __func__
             *
             * @example
             * @code
             * // Usage manuel
             * NkLogMessage dbg(
             *     NkLogLevel::NK_DEBUG,
             *     "Pointer value",
             *     "debug.cpp",
             *     142,
             *     "InspectValue",
             *     "Debugger"
             * );
             *
             * // Usage via macro (recommandé)
             * #define LOG_DEBUG(msg) NkLogMessage(NkLogLevel::NK_DEBUG, msg, __FILE__, __LINE__, __FUNCTION__)
             * @endcode
             */
            NkLogMessage(
                NkLogLevel lvl,
                const NkString& msg,
                const NkString& file,
                uint32 line,
                const NkString& func,
                const NkString& logger = ""
            );

            /**
             * @brief Destructeur par défaut - libération automatique des NkString
             * @ingroup LogMessageCtors
             *
             * @note = default : génération automatique par le compilateur
             * @note Aucun cleanup manuel nécessaire grâce à RAII de NkString
             */
            ~NkLogMessage() = default;


            // -----------------------------------------------------------------
            // SECTION 8 : MÉTHODES UTILITAIRES
            // -----------------------------------------------------------------
            /**
             * @defgroup LogMessageUtils Méthodes Utilitaires
             * @brief Fonctions d'interrogation et de manipulation des messages
             */

            /**
             * @brief Réinitialise tous les champs aux valeurs par défaut
             * @ingroup LogMessageUtils
             *
             * @post Équivalent à un appel au constructeur par défaut
             * @post timestamp et threadId sont recalculés au moment de l'appel
             * @note Utile pour le pooling de messages et réutilisation d'objets
             *
             * @example
             * @code
             * NkLogMessage msg;
             * for (int i = 0; i < 1000; ++i) {
             *     msg.Reset();  // Réutilise la même instance
             *     msg.level = NkLogLevel::NK_INFO;
             *     msg.message = Format("Iteration %d", i);
             *     Logger::Push(msg);
             * }
             * @endcode
             */
            void Reset();

            /**
             * @brief Vérifie la validité minimale du message pour traitement
             * @return true si message non vide ET timestamp valide, false sinon
             * @ingroup LogMessageUtils
             *
             * @note Critères de validation :
             *   - message.Empty() == false (contenu requis)
             *   - timestamp > 0 (horodatage requis)
             * @note Les autres champs sont optionnels et n'affectent pas la validité
             *
             * @example
             * @code
             * if (msg.IsValid()) {
             *     LoggerBackend::Process(msg);
             * } else {
             *     // Ignorer les messages mal formés ou vides
             * }
             * @endcode
             */
            bool IsValid() const;

            /**
             * @brief Convertit le timestamp en structure tm locale (thread-safe)
             * @return Structure tm avec date/heure locale du système
             * @ingroup LogMessageUtils
             *
             * @note Utilise localtime_r (POSIX) ou localtime_s (Windows) pour thread-safety
             * @note La structure retournée est une copie : safe pour usage prolongé
             * @note Fuseau horaire : celui du système d'exploitation
             *
             * @example
             * @code
             * tm local = msg.GetLocalTime();
             * printf("%04d-%02d-%02d %02d:%02d:%02d\n",
             *        local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
             *        local.tm_hour, local.tm_min, local.tm_sec);
             * @endcode
             */
            tm GetLocalTime() const;

            /**
             * @brief Convertit le timestamp en structure tm UTC (thread-safe)
             * @return Structure tm avec date/heure UTC
             * @ingroup LogMessageUtils
             *
             * @note Utilise gmtime_r (POSIX) ou gmtime_s (Windows) pour thread-safety
             * @note Indépendant du fuseau horaire système : idéal pour logs distribués
             * @note Format ISO 8601 compatible pour sérialisation
             *
             * @example
             * @code
             * tm utc = msg.GetUTCTime();
             * // Format ISO 8601 : 2024-01-15T14:30:45Z
             * @endcode
             */
            tm GetUTCTime() const;

            /**
             * @brief Obtient le timestamp en millisecondes depuis epoch
             * @return uint64 représentant les millisecondes
             * @ingroup LogMessageUtils
             *
             * @note Précision : perte des nanosecondes/microsecondes sub-millisecondes
             * @note Utile pour affichage humain ou compatibilité avec APIs millisecondes
             * @note Overflow : safe jusqu'en ~294 millions d'années après 1970
             *
             * @example
             * @code
             * uint64 ms = msg.GetMillis();  // ex: 1705329045123
             * Logger::LogToMetricSystem("log.timestamp.ms", ms);
             * @endcode
             */
            uint64 GetMillis() const;

            /**
             * @brief Obtient le timestamp en microsecondes depuis epoch
             * @return uint64 représentant les microsecondes
             * @ingroup LogMessageUtils
             *
             * @note Précision : perte des nanosecondes sub-microsecondes uniquement
             * @note Bon compromis précision/performance pour profiling et métriques
             * @note Compatible avec la plupart des systèmes de métriques (Prometheus, etc.)
             *
             * @example
             * @code
             * uint64 us = msg.GetMicros();  // ex: 1705329045123456
             * Profiler::RecordEvent("log_emitted", us);
             * @endcode
             */
            uint64 GetMicros() const;

            /**
             * @brief Obtient le timestamp en secondes depuis epoch (avec fraction)
             * @return double représentant les secondes avec précision nanoseconde
             * @ingroup LogMessageUtils
             *
             * @note Précision : conserve toute la résolution nanoseconde via double
             * @note Attention aux erreurs d'arrondi float pour comparaisons exactes
             * @note Format standard pour Unix time et APIs temporelles haute précision
             *
             * @example
             * @code
             * double seconds = msg.GetSeconds();  // ex: 1705329045.123456789
             * if (seconds > lastLogTime + 1.0) {  // Plus d'1 seconde écoulée
             *     // Action throttling
             * }
             * @endcode
             */
            double GetSeconds() const;


        }; // struct NkLogMessage
            
    } // namespace nkentseu


#endif // NKENTSEU_LOGGER_NKLOGMESSAGE_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOGMESSAGE.H
// =============================================================================
// Ce fichier définit la structure NkLogMessage pour encapsuler les messages
// de log avec leurs métadonnées complètes.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Construction et usage basique
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogMessage.h>

    void LogApplicationStart() {
        using namespace nkentseu::logger;

        // Construction minimale : niveau + message
        NkLogMessage startupMsg(NkLogLevel::NK_INFO, "Application v2.1.0 started");

        // Vérification de validité avant traitement
        if (startupMsg.IsValid()) {
            Logger::Submit(startupMsg);
        }

        // Accès aux métadonnées auto-générées
        uint64 timestamp = startupMsg.GetMillis();
        uint32 threadId = startupMsg.threadId;
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Construction avec métadonnées de debug (macros)
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogMessage.h>

    // Macro utilitaire pour simplifier la construction avec source info
    #define NK_LOG_MSG(level, msg) \
        nkentseu::logger::NkLogMessage( \
            level, \
            msg, \
            __FILE__, \
            __LINE__, \
            __FUNCTION__ \
        )

    void ProcessNetworkPacket(const uint8* data, size_t size) {
        using namespace nkentseu::logger;

        // Log de debug avec localisation source automatique
        NkLogMessage dbgMsg = NK_LOG_MSG(
            NkLogLevel::NK_DEBUG,
            nkentseu::core::NkString::Format("Packet received: %zu bytes", size)
        );

        // Ajout manuel de métadonnées contextuelles
        dbgMsg.loggerName = "nkentseu.network";
        dbgMsg.threadName = "NetworkIO";

        Logger::Submit(dbgMsg);
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Conversion temporelle pour affichage formaté
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogMessage.h>
    #include <cstdio>

    void FormatLogForConsole(const nkentseu::logger::NkLogMessage& msg) {
        using namespace nkentseu::logger;

        // Récupération de l'heure locale pour affichage utilisateur
        tm local = msg.GetLocalTime();

        // Formatage style journal : [2024-01-15 14:30:45] [INF] Message
        printf("[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
            local.tm_year + 1900,
            local.tm_mon + 1,
            local.tm_mday,
            local.tm_hour,
            local.tm_min,
            local.tm_sec,
            NkLogLevelToShortString(msg.level),
            msg.message.CStr()  // Conversion NkString → const char*
        );
    }

    void FormatLogForJSON(const nkentseu::logger::NkLogMessage& msg) {
        // Format JSON pour ingestion dans système de métriques
        printf(
            "{\"timestamp\":%.3f,\"level\":\"%s\",\"thread\":%u,\"message\":\"%s\"}\n",
            msg.GetSeconds(),
            NkLogLevelToString(msg.level),
            msg.threadId,
            msg.message.CStr()
        );
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Pooling de messages pour haute performance
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogMessage.h>
    #include <NKContainers/Vector/NkVector.h>

    class LogMessagePool {
    public:
        // Acquisition d'un message depuis le pool (ou création si vide)
        nkentseu::logger::NkLogMessage Acquire() {
            if (m_pool.IsEmpty()) {
                return nkentseu::logger::NkLogMessage();  // Construction neuve
            }
            nkentseu::logger::NkLogMessage msg = m_pool.Back();
            m_pool.PopBack();
            msg.Reset();  // Réinitialisation pour réutilisation
            return msg;
        }

        // Retour d'un message au pool après traitement
        void Release(nkentseu::logger::NkLogMessage&& msg) {
            msg.Reset();  // Nettoyage avant stockage
            m_pool.PushBack(std::move(msg));
        }

        // Pré-allocation pour éviter les allocations dynamiques en runtime
        void Reserve(size_t capacity) {
            m_pool.Reserve(capacity);
            for (size_t i = 0; i < capacity; ++i) {
                m_pool.PushBack(nkentseu::logger::NkLogMessage());
            }
        }

    private:
        nkentseu::core::NkVector<nkentseu::logger::NkLogMessage> m_pool;
    };

    // Usage dans un logger asynchrone haute performance
    void AsyncLogger::WorkerThread() {
        LogMessagePool pool;
        pool.Reserve(256);  // Pré-allouer 256 messages

        while (m_running) {
            auto msg = pool.Acquire();  // Zero-allocation si pool non vide

            // ... traitement du message ...

            pool.Release(std::move(msg));  // Retour au pool pour réutilisation
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Filtrage et validation avant traitement
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogMessage.h>

    class LogFilter {
    public:
        // Filtre par niveau minimal
        bool PassesLevelFilter(const nkentseu::logger::NkLogMessage& msg) const {
            return static_cast<uint8>(msg.level) >= static_cast<uint8>(m_minLevel);
        }

        // Filtre par nom de logger (wildcard simple)
        bool PassesNameFilter(const nkentseu::logger::NkLogMessage& msg) const {
            if (m_loggerFilter.Empty()) {
                return true;  // Aucun filtre : tout passe
            }
            return msg.loggerName.StartsWith(m_loggerFilter);
        }

        // Filtre combiné avec validation de base
        bool ShouldProcess(const nkentseu::logger::NkLogMessage& msg) const {
            return msg.IsValid()
                && PassesLevelFilter(msg)
                && PassesNameFilter(msg);
        }

        void SetMinLevel(nkentseu::logger::NkLogLevel level) {
            m_minLevel = level;
        }

        void SetLoggerFilter(const nkentseu::core::NkString& prefix) {
            m_loggerFilter = prefix;
        }

    private:
        nkentseu::logger::NkLogLevel m_minLevel = nkentseu::logger::NkLogLevel::NK_INFO;
        nkentseu::core::NkString m_loggerFilter;
    };

    // Usage dans un backend de logging
    void FileBackend::Write(const nkentseu::logger::NkLogMessage& msg) {
        static LogFilter filter;  // Filtre global configurable

        if (!filter.ShouldProcess(msg)) {
            return;  // Message filtré : aucun I/O coûteux
        }

        // ... écriture sur disque uniquement si message validé ...
    }
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Sérialisation pour transport réseau ou stockage
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLogMessage.h>
    #include <NKSerialization/NkBinaryWriter.h>

    // Sérialisation binaire compacte pour transmission réseau
    void SerializeLogMessage(
        const nkentseu::logger::NkLogMessage& msg,
        nkentseu::serialization::NkBinaryWriter& writer
    ) {
        using namespace nkentseu::logger;

        // En-tête fixe : champs scalaires en premier pour alignement
        writer.WriteUint64(msg.timestamp);
        writer.WriteUint32(msg.threadId);
        writer.WriteUint8(static_cast<uint8>(msg.level));
        writer.WriteUint32(msg.sourceLine);

        // Champs variables : NkString avec préfixe longueur
        writer.WriteString(msg.message);
        writer.WriteString(msg.loggerName);
        writer.WriteString(msg.threadName);
        writer.WriteString(msg.sourceFile);
        writer.WriteString(msg.functionName);
    }

    // Désérialisation avec validation
    bool DeserializeLogMessage(
        nkentseu::logger::NkLogMessage& msg,
        nkentseu::serialization::NkBinaryReader& reader
    ) {
        using namespace nkentseu::logger;

        // Lecture des champs scalaires
        if (!reader.ReadUint64(msg.timestamp)) return false;
        if (!reader.ReadUint32(msg.threadId)) return false;

        uint8 levelRaw = 0;
        if (!reader.ReadUint8(levelRaw)) return false;
        msg.level = static_cast<NkLogLevel>(levelRaw);

        if (!reader.ReadUint32(msg.sourceLine)) return false;

        // Lecture des champs string avec validation
        if (!reader.ReadString(msg.message)) return false;
        if (!reader.ReadString(msg.loggerName)) return false;
        if (!reader.ReadString(msg.threadName)) return false;
        if (!reader.ReadString(msg.sourceFile)) return false;
        if (!reader.ReadString(msg.functionName)) return false;

        // Validation post-désérialisation
        return msg.IsValid();
    }
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. AJOUT DE NOUVEAUX CHAMPS :
       - Placer les champs scalaires avant les NkString pour alignement mémoire
       - Mettre à jour tous les constructeurs pour initialiser le nouveau champ
       - Documenter le champ avec Doxygen et l'ajouter au bon @defgroup
       - Vérifier l'impact sur la taille de la structure (cache locality)

    2. OPTIMISATIONS MÉMOIRE :
       - Utiliser NKLOGGER_DISABLE_SOURCE_INFO en production pour exclure
         sourceFile/sourceLine/functionName si non nécessaires
       - Envisager NkStringView pour les champs en lecture seule si lifetime garantie
       - Pooling recommandé pour les loggers haute fréquence (>10k msg/s)

    3. THREAD-SAFETY :
       - NkLogMessage est copyable/movable mais pas thread-safe par défaut
       - Si partagé entre threads : utiliser copie ou synchronisation externe
       - Les méthodes const sont safe pour lecture concurrente

    4. COMPATIBILITÉ ABI :
       - Ne jamais réordonner les champs publics d'une structure exportée
       - Ajouter les nouveaux champs à la fin pour préserver l'offset des existants
       - Utiliser static_assert(sizeof(NkLogMessage) == expected) pour détection de break

    5. INTERNATIONALISATION :
       - Le champ message supporte UTF-8 via NkString
       - Les métadonnées (fichier, fonction) restent en ASCII pour compatibilité tooling
       - Pour UI localisée : traduire au niveau du formatter, pas dans NkLogMessage
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================