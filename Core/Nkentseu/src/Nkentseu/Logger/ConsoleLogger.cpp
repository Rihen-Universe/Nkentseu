/**
* @File ConsoleLogger.cpp
* @Description Implémentation du logger console avec gestion des couleurs ANSI.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#include "pch.h"
#include "ConsoleLogger.h"
#include "FormatterSystem.h"
#include "Nkentseu/Epoch/Date.h"
#include "Nkentseu/Epoch/Time.h"


#ifdef NKENTSEU_PLATFORM_WINDOWS
    #include <windows.h>
#endif

namespace nkentseu {

    /**
    * @Function Write
    * @Description Implémentation de l'écriture formatée dans la console
    * @param (const LogMessage&) msg : Message de log à formatter
    */
    void ConsoleLogger::Write(const LogMessage& msg) {
        #ifdef NKENTSEU_DEBUG_SYMBOLS
            #ifdef NKENTSEU_PLATFORM_WINDOWS
            static bool ansiEnabled = [](){ 
                ConsoleLogger::EnableWindowsANSI(); 
                return true; 
            }();
            ansiEnabled = !(!ansiEnabled);
            #endif

            // Formatage des éléments du message
            std::string severity = LogSeverity(msg.severity).ToString();

            // Centrage du niveau de sévérité dans un champ de 9 caractères
            int spaceLeft = (9 - severity.size()) / 2;
            int spaceRight = 9 - spaceLeft - severity.size();

            // Construction de la ligne de log
            std::cout << colors.app << "[" << msg.loggerName << "]" << colors.reset
                      << GetColor(msg.severity, colors) 
                      << std::string(spaceLeft, '[') << severity << std::string(spaceRight, ']')
                      << colors.reset << "[" << msg.date.ToString() << " " << msg.time.ToString() << "] "
                      << "[" << msg.sourceFile << " -> (" << msg.sourceLine << ") -> " 
                      << msg.functionName << "]" << GetColor(msg.severity, colors) << " >> " 
                      << colors.reset << msg.message << std::endl;
        #endif
    }

    /**
    * @Function Flush
    * @Description Force l'écriture immédiate dans le flux de sortie
    */
    void ConsoleLogger::Flush() {
        std::cout.flush();
    }

    /**
    * @Function GetColor
    * @Description Sélection dynamique des codes couleur ANSI
    */
    const char* ConsoleLogger::GetColor(LogSeverity::Level level, const ColorCode& colors) {
        switch(level) {
            case LogSeverity::Level::Trace:    return colors.trace;
            case LogSeverity::Level::Debug:    return colors.debug;
            case LogSeverity::Level::Info:     return colors.info;
            case LogSeverity::Level::Warning:  return colors.warning;
            case LogSeverity::Level::Error:    return colors.error;
            case LogSeverity::Level::Critical: return colors.critical;
            case LogSeverity::Level::Fatal:    return colors.fatal;
            default:                          return colors.reset;
        }
    }

    /**
    * @Function EnableWindowsANSI
    * @Description Active le support des séquences ANSI sur Windows 10+
    */
    void ConsoleLogger::EnableWindowsANSI() {
        #ifdef NKENTSEU_PLATFORM_WINDOWS
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        #endif
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.