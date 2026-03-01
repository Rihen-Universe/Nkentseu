// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/NullSink.cpp
// DESCRIPTION: Implémentation du sink null (no-op).
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Sinks/NullSink.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

/**
 * @brief Constructeur par défaut
 */
NullSink::NullSink() {
}

/**
 * @brief Destructeur
 */
NullSink::~NullSink() {
}

/**
 * @brief Ignore le message (no-op)
 */
void NullSink::Log(const LogMessage &message) {
	// No-op
}

/**
 * @brief No-op
 */
void NullSink::Flush() {
	// No-op
}

/**
 * @brief No-op
 */
void NullSink::SetFormatter(std::unique_ptr<Formatter> formatter) {
	// No-op
}

/**
 * @brief No-op
 */
void NullSink::SetPattern(const std::string &pattern) {
	// No-op
}

/**
 * @brief Retourne nullptr
 */
Formatter *NullSink::GetFormatter() const {
	return nullptr;
}

/**
 * @brief Retourne une chaîne vide
 */
std::string NullSink::GetPattern() const {
	return "";
}

} // namespace logger
} // namespace nkentseu
