/**
* @File Logger.h
* @Description Système centralisé de journalisation applicative
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/
#pragma once

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Types.h"

#include "LogSeverity.h"
#include "LoggerTarget.h"
#include "FormatterSystem.h"
#include "Nkentseu/Memory/SharedPtr.h"

#include <source_location>
#include <vector>
#include <string>


namespace nkentseu {

    /**
    * - Logger : Gestionnaire principal de journalisation
    *
    * @Description :
    * Classe centrale pour l'émission et la gestion des logs applicatifs.
    * Supporte :
    * - Multiple cibles de sortie (console, fichier, réseau, etc.)
    * - Formatage avancé des messages
    * - Contextualisation temporelle et spatiale
    * - Niveaux de sévérité personnalisables
    */
    class NKENTSEU_API Logger {
    public:
        /**
        * @Function Logger
        * @brief Constructeur initialisant le logger avec un nom d'application
        * @param appName Identifiant de l'application/logger
        */
        explicit Logger(const std::string& appName);
        
        // Gestion du nom
        /**
        * @Function SetName
        * @brief Modifie le nom associé au logger
        * @param appName Nouveau nom de l'application
        */
        void SetName(const std::string& appName);
        
        /**
        * @Function GetName
        * @brief Récupère le nom actuel du logger
        * @return Référence constante vers le nom
        */
        const std::string& GetName() const noexcept;
        
        // Configuration du contexte
        /**
        * @Function Source
        * @brief Définit le contexte source pour les prochains logs
        * @param loc Localisation du code source
        * @param date Date associée
        * @param time Heure associée
        * @return Référence au logger pour chaînage
        */
        Logger& Source(const std::source_location& loc = std::source_location::current(), const Date& date = Date::GetCurrent(), const Time& time = Time::GetCurrent());
        Logger& Source(const std::string& file, uint32 line, const std::string& function, const Date& date = Date::GetCurrent(), const Time& time = Time::GetCurrent());
        
        // Méthodes de log bas niveau
        /**
        * @Function Log
        * @brief Émet un message de log brut
        * @param severity Niveau de sévérité
        * @param message Contenu du message
        * @return Référence au logger pour chaînage
        */
        Logger& Log(LogSeverity::Level severity, const std::string& message);
        
        // Méthode de log formatée
        /**
        * @Function Log
        * @brief Émet un message formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param severity Niveau de sévérité
        * @param format Chaîne de format avec placeholders
        * @param args Arguments à injecter
        * @return Référence au logger pour chaînage
        */
        template<typename... Args>
        Logger& Log(LogSeverity::Level severity, const std::string& format, const Args&... args) {
            std::string formatted = Formatter.Format(m_style, m_anyStyle, format, args...);
            return Log(severity, formatted);
        }
        
        // Méthodes de base (non formatées)
        /**
        * @Function Trace
        * @brief Journalise un message de niveau Trace (diagnostic le plus détaillé)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @note Niveau recommandé pour le tracing fin des opérations
        */
        Logger& Trace(const std::string& message = "");

        /**
        * @Function Debug
        * @brief Journalise un message de niveau Debug (diagnostic de développement)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @note Visible seulement en mode debug généralement
        */
        Logger& Debug(const std::string& message = "");

        /**
        * @Function Info
        * @brief Journalise un message informatif (suivi d'activité normal)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @example Info("Service démarré");
        */
        Logger& Info(const std::string& message = "");

        /**
        * @Function Warning
        * @brief Journalise un avertissement (situation anormale non critique)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @note Devrait déclencher une investigation préventive
        */
        Logger& Warning(const std::string& message = "");

        /**
        * @Function Errors
        * @brief Journalise une erreur (échec d'opération importante)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @warning Nécessite une action corrective immédiate
        */
        Logger& Errors(const std::string& message = "");

        /**
        * @Function Critical
        * @brief Journalise une erreur critique (impact majeur)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @alert Peut entraîner l'interruption de fonctionnalités
        */
        Logger& Critical(const std::string& message = "");

        /**
        * @Function Fatal
        * @brief Journalise une erreur fatale (crash imminent)
        * @param message Message à logger (vide par défaut pour usage fluent)
        * @return Référence au logger pour chaînage
        * @critical Arrêt immédiat de l'application après journalisation
        */
        Logger& Fatal(const std::string& message = "");

        // Méthodes formatées (avec arguments)
        /**
        * @Function Trace
        * @brief Journalise un message Trace formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @details Utilise le style de formatage configuré (m_style/m_anyStyle)
        * @example Trace("User {} connecté (ID: {})", username, user_id);
        */
        template<typename... Args>
        Logger& Trace(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Trace, formatted);
        }

        /**
        * @Function Debug
        * @brief Journalise un message Debug formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @note Les messages sont filtrés en production selon le niveau
        */
        template<typename... Args>
        Logger& Debug(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Debug, formatted);
        }

        /**
        * @Function Info
        * @brief Journalise un message Info formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @example Info("Nouvelle connexion de {}", ip_address);
        */
        template<typename... Args>
        Logger& Info(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Info, formatted);
        }

        /**
        * @Function Warning
        * @brief Journalise un message Warning formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @alert Surveiller les occurrences répétées de ces messages
        */
        template<typename... Args>
        Logger& Warning(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Warning, formatted);
        }

        /**
        * @Function Errors
        * @brief Journalise un message Error formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @warning Génère une entrée dans les rapports d'erreurs
        * @note Attention : Nom de méthode au pluriel (Errors vs Error)
        */
        template<typename... Args>
        Logger& Errors(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Error, formatted);
        }

        /**
        * @Function Critical
        * @brief Journalise un message Critical formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @critical Déclenche potentiellement des alertes systèmes
        */
        template<typename... Args>
        Logger& Critical(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Critical, formatted);
        }

        /**
        * @Function Fatal
        * @brief Journalise un message Fatal formaté
        * @Template (typename... Args) : Types des arguments à formater
        * @param format Chaîne de format avec placeholders
        * @param args Valeurs à injecter dans le format
        * @return Référence au logger pour chaînage
        * @critical Arrête l'application après journalisation
        * @example Fatal("Erreur irrécupérable dans {}: code {}", module, errno);
        */
        template<typename... Args>
        Logger& Fatal(const std::string& format, const Args&... args) {
            std::string formatted = FormatterSystem::Instance().Format(m_style, m_anyStyle, format, args...);
            return Log(LogSeverity::Level::Fatal, formatted);
        }

        // [Documentation similaire pour les autres méthodes template]

        // Gestion des cibles
        /**
        * @Function AddTarget
        * @brief Ajoute une cible de sortie pour les logs
        * @param target Pointeur partagé vers la cible
        */
        void AddTarget(SharedPtr<LoggerTarget> target);
        
        /**
        * @Function RemoveTarget
        * @brief Supprime une cible par son adresse
        * @param target Pointeur brut vers la cible à supprimer
        */
        void RemoveTarget(LoggerTarget* target);
        
        /**
        * @Function HasTarget
        * @brief Vérifie la présence d'une cible
        * @param target Pointeur vers la cible à vérifier
        * @return true si la cible existe, false sinon
        */
        bool HasTarget(LoggerTarget* target) const;
        
        /**
        * @Function FlushAllTargets
        * @brief Force l'écriture de tous les buffers des cibles
        */
        void FlushAllTargets();

        // Gestion du style
        /**
        * @Function SetStyle
        * @brief Définit le style de formatage des messages
        * @param style Style à utiliser (Curly par défaut)
        */
        void SetStyle(FormatterSystem::Style style);
        
        /**
        * @Function GetStyle
        * @brief Récupère le style de formatage actuel
        * @return Style courant
        */
        FormatterSystem::Style GetStyle() const;
        
        /**
        * @Function SetAnyStyle
        * @brief Active/désactive le mélange de styles
        * @param anyStyle true pour autoriser les styles multiples
        */
        void SetAnyStyle(bool anyStyle);
        
        /**
        * @Function GetAnyStyle
        * @brief Indique si le mélange de styles est actif
        * @return État actuel du mode anyStyle
        */
        bool GetAnyStyle() const;
        
        /**
        * @Function SetStyleAndAnyStyle
        * @brief Configure simultanément style et anyStyle
        * @param style Nouveau style à appliquer
        * @param anyStyle Nouvel état pour anyStyle (false par défaut)
        */
        void SetStyleAndAnyStyle(FormatterSystem::Style style, bool anyStyle = false);

        /**
        * @Function ClearTargetsAndFlush
        * @brief Supprime toutes les cibles après vidage des buffers
        */
        void ClearTargetsAndFlush();
        
        /**
        * @Function Shutdown
        * @brief Arrêt propre du logger (vide les buffers et libère les ressources)
        */
        void Shutdown();

        /**
        * @Function SystemLogger
        * @brief Accès au logger système global
        * @return Référence au logger singleton système
        */
        static Logger& SystemLogger();

        /*
        * @Function GetMinLevel
        * @brief Récupère le niveau minimum de log
        * @return Niveau minimum de log
        */
        LogSeverity::Level GetMinLevel() const { return m_minLevel; } ///< Récupère le niveau minimum de log

        /*
        * @Function GetMaxLevel
        * @brief Récupère le niveau maximum de log
        * @return Niveau maximum de log
        */
        LogSeverity::Level GetMaxLevel() const { return m_maxLevel; } ///< Récupère le niveau maximum de log

        /*
        * @Function SetMinLevel
        * @brief Définit le niveau minimum de log
        * @param level Niveau minimum à appliquer
        * @return void
        */
        void SetMinLevel(LogSeverity::Level level) { m_minLevel = level; } ///< Définit le niveau minimum de log

        /*
        * @Function SetMaxLevel
        * @brief Définit le niveau maximum de log
        * @param level Niveau maximum à appliquer
        * @return void
        */
        void SetMaxLevel(LogSeverity::Level level) { m_maxLevel = level; } ///< Définit le niveau maximum de log

        /*
        * @Function SetMinMaxLevel
        * @brief Définit les niveaux minimum et maximum de log
        * @param minLevel Niveau minimum à appliquer
        * @param maxLevel Niveau maximum à appliquer
        * @return void
        */
        void SetMinMaxLevel(LogSeverity::Level minLevel, LogSeverity::Level maxLevel) { m_minLevel = minLevel; m_maxLevel = maxLevel; } ///< Définit les niveaux minimum et maximum de log

    private:
        std::string m_appName; ///< Identifiant de l'application
        std::vector<SharedPtr<LoggerTarget>> m_targets; ///< Liste des cibles configurées
        LoggerTarget* m_consolTarget = nullptr; ///< Cible console optionnelle
        LoggerTarget* m_fileTarget = nullptr; ///< Cible fichier optionnelle
        std::string m_currentFile;
        uint32 m_currentLine;
        std::string m_currentFunction;
        Date m_currentDate; ///< Date du dernier log
        Time m_currentTime; ///< Heure du dernier log

        bool m_anyStyle = false; ///< Autorise le mix de styles de formatage
        FormatterSystem::Style m_style = FormatterSystem::Style::Curly; ///< Style de placeholder actif
        LogSeverity::Level m_minLevel = LogSeverity::Level::Trace; /////< Niveau de sévérité minimum pour journaliser
        LogSeverity::Level m_maxLevel = LogSeverity::Level::Fatal; /////< Niveau de sévérité maximum pour journaliser

        /**
        * @Function CreateLogMessage
        * @brief Construit un objet de message de log
        * @param severity Niveau de sévérité
        * @param message Contenu du message
        * @param loc Localisation du code
        * @return Message de log structuré
        */
        LogMessage CreateLogMessage(LogSeverity::Level severity, const std::string& message, const std::source_location& loc) const;
    };

} // namespace nkentseu

/**
* @Macro logger
* @Description Raccourci pour accéder au logger système avec contexte
* @Example logger.Info("Démarrage de l'application");
*/
#define logger ::nkentseu::Logger::SystemLogger().Source(std::source_location::current(), ::nkentseu::Date::GetCurrent(), ::nkentseu::Time::GetCurrent())

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.