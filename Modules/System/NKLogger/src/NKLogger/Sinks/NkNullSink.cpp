// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkNullSink.cpp
// DESCRIPTION: Implémentation du sink null (no-op).
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkNullSink.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
		
	/**
	 * @brief Constructeur par défaut
	 */
	NkNullSink::NkNullSink() {
	}

	/**
	 * @brief Destructeur
	 */
	NkNullSink::~NkNullSink() {
	}

	/**
	 * @brief Ignore le message (no-op)
	 */
	void NkNullSink::Log(const NkLogMessage &message) {
		// No-op
	}

	/**
	 * @brief No-op
	 */
	void NkNullSink::Flush() {
		// No-op
	}

	/**
	 * @brief No-op
	 */
	void NkNullSink::SetFormatter(std::unique_ptr<NkFormatter> formatter) {
		// No-op
	}

	/**
	 * @brief No-op
	 */
	void NkNullSink::SetPattern(const std::string &pattern) {
		// No-op
	}

	/**
	 * @brief Retourne nullptr
	 */
	NkFormatter *NkNullSink::GetFormatter() const {
		return nullptr;
	}

	/**
	 * @brief Retourne une chaîne vide
	 */
	std::string NkNullSink::GetPattern() const {
		return "";
	}

} // namespace nkentseu
