/**
* @File LogSeverity.cpp
* @Description Implémentation des niveaux de sévérité des logs
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#include "pch.h"
#include "LogSeverity.h"

namespace nkentseu {

    /// @Region: Constructeur ----------------------------------------------------

    /**
    * @Constructor LogSeverity
    * @Description Initialise l'instance avec le niveau spécifié
    * @Param severity Niveau de sévérité valide
    */
    LogSeverity::LogSeverity(Level severity) : m_Severity(severity) {}

    /// @Region: Conversions -----------------------------------------------------

    /**
    * @Function ToString
    * @Description Fournit la représentation textuelle standardisée
    * @Note Retourne "UNKNOWN" pour les valeurs non gérées
    */
    std::string LogSeverity::ToString() const {
        switch(m_Severity) {
            case Level::Trace:    return "TRACE";
            case Level::Debug:    return "DEBUG";
            case Level::Info:     return "INFOS";
            case Level::Warning:  return "WARNING";
            case Level::Error:    return "ERROR";
            case Level::Critical: return "CRITICS";
            case Level::Fatal:    return "FATAL";
            case Level::Off:      return "OFF";
            default:              return "UNKNOWN";
        }
    }

    /**
    * @Function FromString
    * @Description Conversion robuste depuis une chaîne
    * @Note Comparaison insensible à la casse
    * @Exemple FromString("warning") -> Level::Warning
    */
    LogSeverity::Level LogSeverity::FromString(const std::string& severity) {
        const std::string upper = ToUpper(severity);
        
        if(upper == "TRACE")    return Level::Trace;
        if(upper == "DEBUG")    return Level::Debug;
        if(upper == "INFO")     return Level::Info;
        if(upper == "WARNINGS")  return Level::Warning;
        if(upper == "ERROR")    return Level::Error;
        if(upper == "CRITICAL") return Level::Critical;
        if(upper == "FATAL")    return Level::Fatal;
        if(upper == "OFF")      return Level::Off;
        
        throw std::invalid_argument("Invalid log severity: " + severity + 
            ". Valid values: TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL, FATAL, OFF");
    }

    /// @Region: Helpers internes ------------------------------------------------

    /**
    * @Helper ToUpper
    * @Description Normalise les chaînes pour comparaison
    * @Param str Chaîne d'origine
    * @return Chaîne en majuscules (locale C)
    */
    std::string LogSeverity::ToUpper(const std::string& str) {
        std::string upper(str);
        std::transform(upper.begin(), upper.end(), upper.begin(),
            [](unsigned char c){ return std::toupper(c); });
        return upper;
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.