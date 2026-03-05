/**
* @File LoggerTarget.cpp
* @Description Implémentation des composants de base du logging
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#include "pch.h"
#include "LoggerTarget.h"

namespace nkentseu {

    /// @Region: Implémentation LogMessage ----------------------------------------

    /**
    * @Constructor LogMessage
    * @Description Initialise toutes les métadonnées d'un message de log
    * @Note Les paramètres optionnels utilisent des valeurs par défaut pertinentes
    */
    LogMessage::LogMessage(
        const std::string& logger,
        LogSeverity::Level severityLevel,
        const std::string& msg,
        const Date& date,
        const Time& time,
        const std::string& file,
        int32 line,
        const std::string& func
    ) : 
        date(date),
        time(time),
        severity(severityLevel),
        loggerName(logger),
        message(msg),
        sourceFile(file),
        sourceLine(line),
        functionName(func) 
    {}

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.