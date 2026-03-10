// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogMessage.h
// DESCRIPTION: Structure contenant toutes les informations d'un message de log.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include <ctime>
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLogLevel.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// STRUCTURE: NkLogMessage
	// DESCRIPTION: Contient toutes les informations d'un message de log
	// -------------------------------------------------------------------------
	struct NkLogMessage {
		// ---------------------------------------------------------------------
		// DONNÉES TEMPORELLES
		// ---------------------------------------------------------------------

		/// Timestamp en nanosecondes depuis l'epoch
		uint64 timestamp;

		// ---------------------------------------------------------------------
		// INFORMATIONS DE THREAD
		// ---------------------------------------------------------------------

		/// ID du thread émetteur
		uint32 threadId;

		/// Nom du thread (optionnel)
		NkString threadName;

		// ---------------------------------------------------------------------
		// INFORMATIONS DE LOG
		// ---------------------------------------------------------------------

		/// Niveau de log
		NkLogLevel level;

		/// Message de log
		NkString message;

		/// Nom du logger
		NkString loggerName;

		// ---------------------------------------------------------------------
		// INFORMATIONS DE SOURCE (optionnelles)
		// ---------------------------------------------------------------------

		/// Fichier source
		NkString sourceFile;

		/// Ligne source
		uint32 sourceLine;

		/// Nom de la fonction
		NkString functionName;

		// ---------------------------------------------------------------------
		// CONSTRUCTEURS
		// ---------------------------------------------------------------------

		/**
		 * @brief Constructeur par défaut
		 */
		NkLogMessage();

		/**
		 * @brief Constructeur avec niveau et message
		 * @param lvl Niveau de log
		 * @param msg Message de log
		 * @param logger Nom du logger (optionnel)
		 */
		NkLogMessage(NkLogLevel lvl, const NkString &msg, const NkString &logger = "");

		/**
		 * @brief Constructeur avec informations complètes
		 * @param lvl Niveau de log
		 * @param msg Message de log
		 * @param file Fichier source (optionnel)
		 * @param line Ligne source (optionnel)
		 * @param func Fonction source (optionnel)
		 * @param logger Nom du logger (optionnel)
		 */
		NkLogMessage(NkLogLevel lvl, const NkString &msg, const NkString &file, uint32 line, const NkString &func,
				const NkString &logger = "");

		/**
		 * @brief Destructeur
		 */
		~NkLogMessage() = default;

		// ---------------------------------------------------------------------
		// MÉTHODES UTILITAIRES
		// ---------------------------------------------------------------------

		/**
		 * @brief Réinitialise le message
		 */
		void Reset();

		/**
		 * @brief Vérifie si le message est valide
		 * @return true si valide, false sinon
		 */
		bool IsValid() const;

		/**
		 * @brief Obtient l'heure sous forme de structure tm
		 * @return Structure tm avec l'heure locale
		 */
		tm GetLocalTime() const;

		/**
		 * @brief Obtient l'heure sous forme de structure tm (UTC)
		 * @return Structure tm avec l'heure UTC
		 */
		tm GetUTCTime() const;

		/**
		 * @brief Obtient le timestamp en millisecondes
		 * @return Timestamp en millisecondes
		 */
		uint64 GetMillis() const;

		/**
		 * @brief Obtient le timestamp en microsecondes
		 * @return Timestamp en microsecondes
		 */
		uint64 GetMicros() const;

		/**
		 * @brief Obtient le timestamp en secondes
		 * @return Timestamp en secondes (double)
		 */
		double GetSeconds() const;
	};

} // namespace nkentseu
