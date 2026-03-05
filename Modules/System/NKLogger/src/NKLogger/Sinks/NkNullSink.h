// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkNullSink.h
// DESCRIPTION: Sink nul qui ignore tous les messages (pour désactiver le logging).
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkSink.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
	// -------------------------------------------------------------------------
	// CLASSE: NkNullSink
	// DESCRIPTION: Sink qui ignore tous les messages (no-op)
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkNullSink : public NkISink {
		public:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur par défaut
			 */
			NkNullSink();

			/**
			 * @brief Destructeur
			 */
			~NkNullSink() override;

			// ---------------------------------------------------------------------
			// IMPLÉMENTATION DE NkISink
			// ---------------------------------------------------------------------

			/**
			 * @brief Ignore le message (no-op)
			 */
			void Log(const NkLogMessage &message) override;

			/**
			 * @brief No-op
			 */
			void Flush() override;

			/**
			 * @brief No-op
			 */
			void SetFormatter(std::unique_ptr<NkFormatter> formatter) override;

			/**
			 * @brief No-op
			 */
			void SetPattern(const std::string &pattern) override;

			/**
			 * @brief Retourne nullptr
			 */
			NkFormatter *GetFormatter() const override;

			/**
			 * @brief Retourne une chaîne vide
			 */
			std::string GetPattern() const override;
	};

} // namespace nkentseu