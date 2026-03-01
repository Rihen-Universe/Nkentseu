// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/NkentseuLogger.cpp
// DESCRIPTION: Implémentation du logger par défaut avec API fluide.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Log.h"
#include "Logger/Sinks/ConsoleSink.h"
#include "Logger/Sinks/FileSink.h"
#include <cstdarg>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// INITIALISATION DES VARIABLES STATIQUES
// -------------------------------------------------------------------------

bool NkentseuLogger::s_Initialized = false;

// -------------------------------------------------------------------------
// IMPLÉMENTATION DE NkentseuLogger
// -------------------------------------------------------------------------

/**
 * @brief Constructeur privé
 */
NkentseuLogger::NkentseuLogger(const std::string &name) : Logger(name) {

	// Ajouter un sink console par défaut avec couleurs
	auto consoleSink = std::make_shared<ConsoleSink>();
	consoleSink->SetColorEnabled(true);
	AddSink(consoleSink);

	// Ajouter un sink fichier par défaut
	auto fileSink = std::make_shared<FileSink>("logs/app.log");
	AddSink(fileSink);

	// Configuration par défaut
	SetLevel(LogLevel::Info);
	SetPattern(Formatter::DEFAULT_PATTERN);
}

/**
 * @brief Destructeur
 */
NkentseuLogger::~NkentseuLogger() {
	Flush();
}

/**
 * @brief Obtient l'instance singleton
 */
NkentseuLogger &NkentseuLogger::Instance() {
	static NkentseuLogger instance;
	s_Initialized = true;
	return instance;
}

/**
 * @brief Initialise le logger par défaut
 */
void NkentseuLogger::Initialize(const std::string &name, const std::string &pattern, LogLevel level) {
	auto &instance = Instance();

	if (instance.GetName() != name && !name.empty()) {
		instance.SetName(name);
	}

	instance.SetPattern(pattern);
	instance.SetLevel(level);
}

/**
 * @brief Nettoie le logger par défaut
 */
void NkentseuLogger::Shutdown() {
	auto &instance = Instance();
	instance.Flush();
	instance.ClearSinks();
}

/**
 * @brief Configure les informations de source
 */
// SourceContext NkentseuLogger::Source(const char* file, int line, const char* function) const {
//     return SourceContext(file, line, function);
// }

/**
 * @brief Configure le nom du logger
 */
NkentseuLogger &NkentseuLogger::Named(const std::string &name) {
	SetName(name);
	return *this;
}

/**
 * @brief Configure le niveau de log
 */
NkentseuLogger &NkentseuLogger::Level(LogLevel level) {
	SetLevel(level);
	return *this;
}

/**
 * @brief Configure le pattern
 */
NkentseuLogger &NkentseuLogger::Pattern(const std::string &pattern) {
	SetPattern(pattern);
	return *this;
}

NkentseuLogger &NkentseuLogger::Source(const char *sourceFile, uint32 sourceLine, const char *functionName) {
	Logger::Source(sourceFile, sourceLine, functionName);
	return *this;
}

} // namespace logger
} // namespace nkentseu