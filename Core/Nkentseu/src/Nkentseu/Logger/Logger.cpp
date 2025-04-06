#include "Logger.h"
#include "Log.h"
#include "Utilities.h"
#include <iostream>
#include <fstream>


Logger Logger::logger__("Base");

// --- Logger Implementation ---
Logger::Logger(const std::string& appName) : ApplicationName(appName) {}

void Logger::SetApplicationName(const std::string& appName) {
    ApplicationName = appName;
}

Logger& Logger::Source(const std::source_location& location) {
    mLocation = location;
    return *this;
}

void Logger::Log(LogLevel level, const std::string& message,
                 const std::source_location& defaultLocation) {
    std::source_location location = mLocation.has_value() ? mLocation.value() : defaultLocation;
    mLocation.reset();

    std::string timestamp = GetCurrentTimestamp();
    std::string fullMessage = "[" + ApplicationName + "] " + message;

    if (Targets.empty()) {
        std::cerr << "[Fallback Logger] [" << timestamp << "] "
                  << "[" << LogLevelToString(level) << "] "
                  << fullMessage << std::endl;
        return;
    }

    for (auto& target : Targets) {
        try {
            target->Log(level, ApplicationName, message,
                        location.file_name(),
                        location.line(),
                        location.function_name(),
                        timestamp);
        } catch (const std::exception& ex) {
            std::cerr << "[Logging Error] Exception dans la cible : " << ex.what() << std::endl;
        }
    }
}

void Logger::LogAssert(const std::string& message, const std::source_location& defaultLocation){
    std::source_location location = mLocation.has_value() ? mLocation.value() : defaultLocation;
    mLocation.reset();

    std::string timestamp = GetCurrentTimestamp();
    std::string fullMessage = "[" + ApplicationName + "] " + message;

    if (Targets.empty()) {
        std::cerr << "[Fallback Logger][" << timestamp << "] "
                  << "[" << LogLevelToString(LogLevel::ASSERT) << "] "
                  << fullMessage << std::endl;
        return;
    }

    for (auto& target : Targets) {
        try {
            target->Log(LogLevel::ASSERT, ApplicationName, fullMessage,
                        location.file_name(),
                        location.line(),
                        location.function_name(),
                        timestamp);
        } catch (const std::exception& ex) {
            std::cerr << "[Logging Error] Exception dans la cible : " << ex.what() << std::endl;
        }
    }
}

void Logger::AddTarget(std::unique_ptr<LoggerTarget> target) {
    Targets.push_back(std::move(target));
}
