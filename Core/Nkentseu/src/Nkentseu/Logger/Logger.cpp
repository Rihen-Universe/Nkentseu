/**
* @File Logger.cpp
* @Description Implémentation du système central de journalisation
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/
#include "pch.h"
#include "Logger.h"

#include "ConsoleLogger.h"
#include "FileLogger.h"
#include "FormatterSystem.h"
#include "LogSeverity.h"
#include "Nkentseu/Memory/Memory.h"


namespace nkentseu {

    /**
    * @Function SystemLogger
    * @brief Fournit l'instance singleton du logger système
    * @return Référence au logger système global
    * 
    * @Details :
    * - Initialise les cibles par défaut (console + fichiers)
    * - Thread-safe via initialisation statique
    */
    Logger& Logger::SystemLogger() {
        static Logger logger__("Nkentseu");
        static bool initialized = false;
        if (!initialized) {
            logger__.m_consolTarget = new ConsoleLogger();
            logger__.m_fileTarget = new FileLogger("./logs", "nkentseu");
            initialized = true;
        }
        return logger__;
    }

    /**
    * @Function Logger
    * @brief Constructeur initialisant le contexte de base
    * @param appName Nom de l'application/logger
    */
    Logger::Logger(const std::string& appName) 
        : m_appName(appName), 
          m_currentDate(Date::GetCurrent()), 
          m_currentTime(Time::GetCurrent()) {}

    /**
    * @Function SetName
    * @brief Modifie l'identifiant du logger
    * @param appName Nouveau nom à utiliser
    */
    void Logger::SetName(const std::string& appName) {
        m_appName = appName;
    }

    /**
    * @Function GetName
    * @brief Récupère le nom actuel du logger
    * @return Nom de l'application/logger (non modifiable)
    */
    const std::string& Logger::GetName() const noexcept {
        return m_appName;
    }

    /**
    * @Function Source
    * @brief Définit le contexte source pour les logs suivants
    * @param loc Localisation dans le code source
    * @param date Date associée
    * @param time Heure associée
    * @return Référence au logger pour chaînage
    */
    Logger& Logger::Source(const std::source_location& loc, const Date& date, const Time& time) {
        m_currentFile = loc.file_name();
        m_currentLine = loc.line();
        m_currentFunction = loc.function_name();
        m_currentDate = date;
        m_currentTime = time;
        return *this;
    }

    
    Logger& Logger::Source(const std::string& file, uint32 line, const std::string& function, const Date& date, const Time& time) {
        m_currentFile = file;
        m_currentLine = line;
        m_currentFunction = function;
        m_currentDate = date;
        m_currentTime = time;
        return *this;

    }

    /**
    * @Function Log
    * @brief Émet un message de log vers toutes les cibles
    * @param severity Niveau de sévérité
    * @param message Contenu brut du message
    * @return Référence au logger pour chaînage
    */
    Logger& Logger::Log(LogSeverity::Level severity, const std::string& message) {
        if (severity < m_minLevel || severity > m_maxLevel) {
            return *this; // Ignore les messages hors des limites
        }
        
        // Construction du message structuré
        LogMessage msg(
            m_appName, 
            severity, 
            message, 
            m_currentDate, 
            m_currentTime, 
            m_currentFile, 
            m_currentLine, 
            m_currentFunction
        );

        // Envoi aux cibles personnalisées
        if (!m_targets.empty()) {
            for (auto& target : m_targets) {
                target->Log(msg);
            }
        }

        // Fallback aux cibles par défaut
        if (m_consolTarget) {
            m_consolTarget->Log(msg);
        }

        if (m_fileTarget) {
            m_fileTarget->Log(msg);
        }
        return *this;
    }

    // Méthodes spécialisées par niveau de sévérité
    // (Documentation similaire pour chaque niveau)

    /**
    * @Function Trace
    * @brief Journalise un message de niveau Trace (diagnostic le plus détaillé)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @note Niveau recommandé pour le tracing fin des opérations
    */
    Logger& Logger::Trace(const std::string& message) {
        return Log(LogSeverity::Level::Trace, message);
    }

    /**
    * @Function Debug
    * @brief Journalise un message de niveau Debug (diagnostic de développement)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @note Visible seulement en mode debug généralement
    */
    Logger& Logger::Debug(const std::string& message) {
        return Log(LogSeverity::Level::Debug, message);
    }

    /**
    * @Function Info
    * @brief Journalise un message informatif (suivi d'activité normal)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @example Info("Service démarré");
    */
    Logger& Logger::Info(const std::string& message) {
        return Log(LogSeverity::Level::Info, message);
    }

    /**
    * @Function Warning
    * @brief Journalise un avertissement (situation anormale non critique)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @note Devrait déclencher une investigation préventive
    */
    Logger& Logger::Warning(const std::string& message) {
        return Log(LogSeverity::Level::Warning, message);
    }

    /**
    * @Function Errors
    * @brief Journalise une erreur (échec d'opération importante)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @warning Nécessite une action corrective immédiate
    */
    Logger& Logger::Errors(const std::string& message) {
        return Log(LogSeverity::Level::Error, message);
    }

    /**
    * @Function Critical
    * @brief Journalise une erreur critique (impact majeur)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @alert Peut entraîner l'interruption de fonctionnalités
    */
    Logger& Logger::Critical(const std::string& message) {
        return Log(LogSeverity::Level::Critical, message);
    }

    /**
    * @Function Fatal
    * @brief Journalise une erreur fatale (crash imminent)
    * @param message Message à logger (vide par défaut pour usage fluent)
    * @return Référence au logger pour chaînage
    * @critical Arrêt immédiat de l'application après journalisation
    */
    Logger& Logger::Fatal(const std::string& message) {
        return Log(LogSeverity::Level::Fatal, message);
    }

    /**
    * @Function AddTarget
    * @brief Ajoute une nouvelle cible de journalisation
    * @param target Pointeur partagé vers la cible
    * @warning Le logger prend la propriété de la cible
    */
    void Logger::AddTarget(SharedPtr<LoggerTarget> target) {
        m_targets.push_back(std::move(target));
    }

    /**
    * @Function RemoveTarget
    * @brief Supprime une cible par son adresse mémoire
    * @param target Pointeur brut vers la cible à supprimer
    */
    void Logger::RemoveTarget(LoggerTarget* target){
        auto it = std::remove_if(m_targets.begin(), m_targets.end(), 
            [target](const SharedPtr<LoggerTarget>& t) { 
                return t.Get() == target; 
            });
        if (it != m_targets.end()) {
            m_targets.erase(it, m_targets.end());
        }
    }

    /**
    * @Function HasTarget
    * @brief Vérifie la présence d'une cible spécifique
    * @param target Cible à rechercher
    * @return true si la cible existe, false sinon
    */
    bool Logger::HasTarget(LoggerTarget* target) const {
        for (const auto& t : m_targets) {
            if (t.Get() == target) return true;
        }
        return false;
    }

    /**
    * @Function FlushAllTargets
    * @brief Force le vidage des buffers de toutes les cibles
    */
    void Logger::FlushAllTargets() {
        for (auto& target : m_targets) {
            target->ForceFlush();
        }
    }

    // Gestion du style de formatage
    /**
    * @Function SetStyle
    * @brief Modifie le style des placeholders
    * @param style Nouveau style à utiliser
    */
    void Logger::SetStyle(FormatterSystem::Style style) { 
        m_style = style; 
    }

    FormatterSystem::Style Logger::GetStyle() const { 
        return m_style; 
    }

    /**
    * @Function SetAnyStyle
    * @brief Active/désactive le mix de styles
    * @param anyStyle true pour permettre différents styles
    */
    void Logger::SetAnyStyle(bool anyStyle) { 
        m_anyStyle = anyStyle; 
    }

    bool Logger::GetAnyStyle() const { 
        return m_anyStyle; 
    }

    /**
    * @Function SetStyleAndAnyStyle
    * @brief Configure simultanément style et mix de styles
    * @param style Style principal
    * @param anyStyle Autorise les styles multiples
    */
    void Logger::SetStyleAndAnyStyle(FormatterSystem::Style style, bool anyStyle) {
        m_style = style;
        m_anyStyle = anyStyle;
    }

    /**
    * @Function ClearTargetsAndFlush
    * @brief Supprime toutes les cibles après vidage
    * @warning Libère toutes les ressources des cibles
    */
    void Logger::ClearTargetsAndFlush() {
        for (auto& target : m_targets) {
            target->ForceFlush();
            target.Reset();
            target = nullptr;
        }
        m_targets.clear();
    }

    /**
    * @Function Shutdown
    * @brief Arrêt propre du système de logging
    * @details :
    * - Vide les buffers
    * - Libère les cibles par défaut
    * - Réinitialise les pointeurs
    */
    void Logger::Shutdown() {
        if (m_consolTarget) {
            m_consolTarget->ForceFlush();
            delete m_consolTarget;
            m_consolTarget = nullptr;
        }

        if (m_fileTarget) {
            m_fileTarget->ForceFlush();
            delete m_fileTarget;
            m_fileTarget = nullptr;
        }
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.