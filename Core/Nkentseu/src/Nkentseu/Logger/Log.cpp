#include "Log.h"

// --- ConsoleLogger Implementation ---
void ConsoleLogger::Log(LogLevel level, const std::string& appName, const std::string& message, const char* file, int line, const char* func, const std::string& timestamp) {
    std::string color = GetColorCode(level);
    std::cout << "[" << appName << "] " << color << "[" << LogLevelToString(level) << "]" << "\033[0m" << " [" << timestamp << "] " << "[" << file << " (" << line << ") " << func << "] >> " << message << std::endl;
}

// --- FileLogger Implementation ---
FileLogger::FileLogger(const std::string& filename) : OutFile(std::make_unique<std::ofstream>(filename, std::ios::app)) {
    if (!OutFile->is_open()) {
        std::cerr << "Erreur lors de l'ouverture du fichier : " << filename << std::endl;
    }
}

void FileLogger::Log(LogLevel level, const std::string& appName, const std::string& message, const char* file, int line, const char* func, const std::string& timestamp) {
    if (OutFile && OutFile->is_open()) {
        (*OutFile) << "[" << appName << "] " << "[" << LogLevelToString(level) << "]" << " [" << timestamp << "] " << "[" << file << " (" << line << ") " << func << "] >> " << message << std::endl;
    }
}

// --- GuiLogger Implementation ---
void GuiLogger::Log(LogLevel level, const std::string& appName, const std::string& message, const char* file, int line, const char* func, const std::string& timestamp) {
    std::cout << "[" << appName << "] " << "[" << LogLevelToString(level) << "]" << " [" << timestamp << "] " << "[" << file << " (" << line << ") " << func << "] >> " << message << std::endl;
}