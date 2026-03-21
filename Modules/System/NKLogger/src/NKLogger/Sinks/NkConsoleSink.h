// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkConsoleSink.h
// DESCRIPTION: Sink pour la sortie console (stdout/stderr) avec support couleur.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkSink.h"
#include "NKLogger/NkSync.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstdio>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
	// -------------------------------------------------------------------------
	// ÉNUMÉRATION: NkConsoleStream
	// DESCRIPTION: Flux de console disponibles
	// -------------------------------------------------------------------------
	enum class NkConsoleStream {
		NK_STD_OUT, // Sortie standard (stdout)
		NK_STD_ERR	// Erreur standard (stderr)
	};

	// -------------------------------------------------------------------------
	// CLASSE: NkConsoleSink
	// DESCRIPTION: Sink pour la sortie console avec couleurs
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkConsoleSink : public NkISink {
		public:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur par défaut (stdout, avec couleurs)
			 */
			NkConsoleSink();

			/**
			 * @brief Constructeur avec flux spécifique
			 * @param stream Flux à utiliser
			 * @param useColors Utiliser les couleurs (si supporté)
			 */
			explicit NkConsoleSink(NkConsoleStream stream, bool useColors = true);

			/**
			 * @brief Destructeur
			 */
			~NkConsoleSink() override;

			// ---------------------------------------------------------------------
			// IMPLÉMENTATION DE NkISink
			// ---------------------------------------------------------------------

			/**
			 * @brief Logge un message dans la console
			 */
			void Log(const NkLogMessage &message) override;

			/**
			 * @brief Force l'écriture des données en attente
			 */
			void Flush() override;

			/**
			 * @brief Définit le formatter pour ce sink
			 */
			void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

			/**
			 * @brief Définit le pattern de formatage
			 */
			void SetPattern(const NkString &pattern) override;

			/**
			 * @brief Obtient le formatter courant
			 */
			NkFormatter *GetFormatter() const override;

			/**
			 * @brief Obtient le pattern courant
			 */
			NkString GetPattern() const override;

			// ---------------------------------------------------------------------
			// CONFIGURATION SPÉCIFIQUE À LA CONSOLE
			// ---------------------------------------------------------------------

			/**
			 * @brief Active ou désactive les couleurs
			 * @param enable true pour activer les couleurs
			 */
			void SetColorEnabled(bool enable);

			/**
			 * @brief Vérifie si les couleurs sont activées
			 * @return true si activées, false sinon
			 */
			bool IsColorEnabled() const;

			/**
			 * @brief Définit le flux de console
			 * @param stream Flux à utiliser
			 */
			void SetStream(NkConsoleStream stream);

			/**
			 * @brief Obtient le flux de console courant
			 * @return Flux courant
			 */
			NkConsoleStream GetStream() const;

			/**
			 * @brief Définit si le sink doit utiliser stderr pour les niveaux d'erreur
			 * @param enable true pour utiliser stderr pour Error/Critical/Fatal
			 */
			void SetUseStderrForErrors(bool enable);

			/**
			 * @brief Vérifie si le sink utilise stderr pour les erreurs
			 * @return true si activé, false sinon
			 */
			bool IsUsingStderrForErrors() const;

		private:
			// ---------------------------------------------------------------------
			// MÉTHODES PRIVÉES
			// ---------------------------------------------------------------------

			/**
			 * @brief Obtient le flux de sortie approprié pour un niveau de log
			 * @param level Niveau de log
			 * @return Flux C approprié
			 */
			FILE *GetStreamForLevel(NkLogLevel level);

			/**
			 * @brief Vérifie si la console supporte les couleurs
			 * @return true si supporté, false sinon
			 */
			bool SupportsColors() const;

			/**
			 * @brief Obtient le code couleur pour un niveau de log
			 * @param level Niveau de log
			 * @return Code couleur ANSI
			 */
			NkString GetColorCode(NkLogLevel level) const;

			/**
			 * @brief Obtient le code de réinitialisation de couleur
			 * @return Code de réinitialisation ANSI
			 */
			NkString GetResetCode() const;

			/**
			 * @brief Configure la couleur Windows pour un niveau de log
			 * @param level Niveau de log
			 */
			void SetWindowsColor(NkLogLevel level);

			/**
			 * @brief Réinitialise la couleur Windows
			 */
			void ResetWindowsColor();

			// ---------------------------------------------------------------------
			// VARIABLES MEMBRE PRIVÉES
			// ---------------------------------------------------------------------

			/// NkFormatter pour ce sink
			memory::NkUniquePtr<NkFormatter> m_Formatter;

			/// Flux de console principal
			NkConsoleStream m_Stream;

			/// Utiliser les couleurs
			bool m_UseColors;

			/// Utiliser stderr pour les niveaux d'erreur
			bool m_UseStderrForErrors;

			/// Mutex pour la synchronisation thread-safe
			mutable threading::NkMutex m_Mutex;
	};

} // namespace nkentseu
