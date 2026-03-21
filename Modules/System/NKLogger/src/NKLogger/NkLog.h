// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLog.h
// DESCRIPTION: NkLogger par défaut avec API fluide pour un usage simplifié.
//              Fournit une instance singleton et des méthodes chaînables.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLogger.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// CLASSE: NkLog
	// DESCRIPTION: NkLogger par défaut singleton avec API fluide
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkLog : public NkLogger {
		public:
			// ---------------------------------------------------------------------
			// MÉTHODES STATIQUES (SINGLETON)
			// ---------------------------------------------------------------------

			/**
			 * @brief Obtient l'instance singleton du logger par défaut
			 * @return Référence à l'instance du logger par défaut
			 */
			static NkLog &Instance();

			/**
			 * @brief Initialise le logger par défaut
			 * @param name Nom du logger (optionnel, "default" par défaut)
			 * @param pattern Pattern de formatage (optionnel)
			 * @param level Niveau de log (optionnel, Info par défaut)
			 */
			static void Initialize(const NkString &name = "default", const NkString &pattern = NkFormatter::NK_DETAILED_PATTERN,
								NkLogLevel level = NkLogLevel::NK_INFO);

			/**
			 * @brief Nettoie et détruit le logger par défaut
			 */
			static void Shutdown();

			// ---------------------------------------------------------------------
			// API FLUIDE
			// ---------------------------------------------------------------------

			/**
			 * @brief Configure les informations de source pour le prochain log
			 * @param file Fichier source
			 * @param line Ligne source
			 * @param function Fonction source
			 * @return Objet SourceContext pour chaînage
			 */
			// SourceContext Source(const char* file, int line, const char* function) const;

			/**
			 * @brief Configure le nom du logger (retourne *this pour chaînage)
			 */
			NkLog &Named(const NkString &name);

			/**
			 * @brief Configure le niveau de log (retourne *this pour chaînage)
			 */
			NkLog &Level(NkLogLevel level);

			/**
			 * @brief Configure le pattern (retourne *this pour chaînage)
			 */
			NkLog &Pattern(const NkString &pattern);

			virtual NkLog &Source(const char *sourceFile = nullptr, uint32 sourceLine = 0,
										const char *functionName = nullptr) override;

		private:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS PRIVÉS (SINGLETON)
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur privé
			 */
			explicit NkLog(const NkString &name = "default");

			/**
			 * @brief Destructeur privé
			 */
			~NkLog();

			/**
			 * @brief Constructeur de copie supprimé
			 */
			NkLog(const NkLog &) = delete;

			/**
			 * @brief Opérateur d'affectation supprimé
			 */
			NkLog &operator=(const NkLog &) = delete;

			// ---------------------------------------------------------------------
			// VARIABLES MEMBRE
			// ---------------------------------------------------------------------

			/// Indicateur d'initialisation
			static bool s_Initialized;
	};

} // namespace nkentseu

// -----------------------------------------------------------------------------
// MACRO PRINCIPALE POUR UN USAGE SIMPLIFIÉ
// -----------------------------------------------------------------------------

/// Logger par défaut (sans mutation de source context à chaque appel).
#define logger nkentseu::NkLog::Instance().Source(__FILE__, __LINE__, __FUNCTION__)

/// Variante explicite avec source context quand nécessaire.
#define logger_src logger
