// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sink.h
// DESCRIPTION: Interface de base pour tous les sinks de logging.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkLoggerExport.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkFormatter.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKMemory/NkSharedPtr.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// CLASSE: NkISink
	// DESCRIPTION: Interface de base pour tous les sinks de logging
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkISink {
		public:
			/**
			 * @brief Destructeur virtuel
			 */
			virtual ~NkISink() = default;

			// ---------------------------------------------------------------------
			// MÉTHODES VIRTUELLES PURES
			// ---------------------------------------------------------------------

			/**
			 * @brief Logge un message
			 * @param message Message à logger
			 */
			virtual void Log(const NkLogMessage &message) = 0;

			/**
			 * @brief Force l'écriture des données en attente
			 */
			virtual void Flush() = 0;

			/**
			 * @brief Définit le formatter pour ce sink
			 * @param formatter NkFormatter à utiliser
			 */
			virtual void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) = 0;

			/**
			 * @brief Définit le pattern de formatage
			 * @param pattern Pattern à utiliser
			 */
			virtual void SetPattern(const NkString &pattern) = 0;

			/**
			 * @brief Obtient le formatter courant
			 * @return Pointeur vers le formatter
			 */
			virtual NkFormatter *GetFormatter() const = 0;

			/**
			 * @brief Obtient le pattern courant
			 * @return Pattern de formatage
			 */
			virtual NkString GetPattern() const = 0;

			// ---------------------------------------------------------------------
			// MÉTHODES VIRTUELLES (OPTIONNELLES)
			// ---------------------------------------------------------------------

			/**
			 * @brief Définit le niveau minimum de log pour ce sink
			 * @param level Niveau minimum
			 */
			virtual void SetLevel(NkLogLevel level) {
				m_Level = level;
			}

			/**
			 * @brief Obtient le niveau minimum de log
			 * @return Niveau minimum
			 */
			virtual NkLogLevel GetLevel() const {
				return m_Level;
			}

			/**
			 * @brief Vérifie si un niveau devrait être loggé
			 * @param level Niveau à vérifier
			 * @return true si le niveau est >= niveau minimum, false sinon
			 */
			virtual bool ShouldLog(NkLogLevel level) const {
				return level >= m_Level;
			}

			/**
			 * @brief Active ou désactive le sink
			 * @param enabled État d'activation
			 */
			virtual void SetEnabled(bool enabled) {
				m_Enabled = enabled;
			}

			/**
			 * @brief Vérifie si le sink est activé
			 * @return true si activé, false sinon
			 */
			virtual bool IsEnabled() const {
				return m_Enabled;
			}

			/**
			 * @brief Obtient le nom du sink
			 * @return Nom du sink
			 */
			virtual NkString GetName() const {
				return m_Name;
			}

			/**
			 * @brief Définit le nom du sink
			 * @param name Nom du sink
			 */
			virtual void SetName(const NkString &name) {
				m_Name = name;
			}

		protected:
			/// Niveau minimum de log
			NkLogLevel m_Level = NkLogLevel::NK_TRACE;

			/// Indicateur d'activation
			bool m_Enabled = true;

			/// Nom du sink
			NkString m_Name;
	};

	// -------------------------------------------------------------------------
	// TYPE ALIAS POUR LES SINKS
	// -------------------------------------------------------------------------

	/// Type pour les pointeurs partagés de sinks
	using NkSinkPtr = memory::NkSharedPtr<NkISink>;

	/// Type pour les pointeurs uniques de sinks
	using NkSinkUniquePtr = memory::NkUniquePtr<NkISink>;

} // namespace nkentseu
