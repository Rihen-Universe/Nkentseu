// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/NullSink.h
// DESCRIPTION: Sink nul qui ignore tous les messages (pour désactiver le logging).
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Sink.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: NullSink
// DESCRIPTION: Sink qui ignore tous les messages (no-op)
// -------------------------------------------------------------------------
class LOGGER_API NullSink : public ISink {
public:
	// ---------------------------------------------------------------------
	// CONSTRUCTEURS
	// ---------------------------------------------------------------------

	/**
	 * @brief Constructeur par défaut
	 */
	NullSink();

	/**
	 * @brief Destructeur
	 */
	~NullSink() override;

	// ---------------------------------------------------------------------
	// IMPLÉMENTATION DE ISink
	// ---------------------------------------------------------------------

	/**
	 * @brief Ignore le message (no-op)
	 */
	void Log(const LogMessage &message) override;

	/**
	 * @brief No-op
	 */
	void Flush() override;

	/**
	 * @brief No-op
	 */
	void SetFormatter(std::unique_ptr<Formatter> formatter) override;

	/**
	 * @brief No-op
	 */
	void SetPattern(const std::string &pattern) override;

	/**
	 * @brief Retourne nullptr
	 */
	Formatter *GetFormatter() const override;

	/**
	 * @brief Retourne une chaîne vide
	 */
	std::string GetPattern() const override;
};

} // namespace logger
} // namespace nkentseu