#ifndef __LOGGER_LOG_H__
#define __LOGGER_LOG_H__

#include "Logger.h"
#include <iostream>
#include <fstream>
#include <exception>

#include "Nkentseu/Platform/PlatformDetection.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Inline.h"

/**
 * @Class ConsoleLogger
 * @Description Logger vers la console avec coloration syntaxique.
 */
class NKENTSEU_API ConsoleLogger : public LoggerTarget {
public:
    void Log(LogLevel level, const std::string& appName, const std::string& message,
             const char* file, int line, const char* func,
             const std::string& timestamp) override;
};

/**
 * @Class FileLogger
 * @Description Logger vers un fichier en mode append.
 */
class NKENTSEU_API FileLogger : public LoggerTarget {
public:
    FileLogger(const std::string& filename);
    void Log(LogLevel level, const std::string& appName, const std::string& message,
             const char* file, int line, const char* func,
             const std::string& timestamp) override;
private:
    std::unique_ptr<std::ofstream> OutFile;
};

/**
 * @Class GuiLogger
 * @Description Simule un logger vers une interface graphique.
 */
class NKENTSEU_API GuiLogger : public LoggerTarget {
public:
    void Log(LogLevel level, const std::string& appName, const std::string& message,
             const char* file, int line, const char* func,
             const std::string& timestamp) override;
};

#endif // __LOGGER_LOG_H__
