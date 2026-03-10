// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkRotatingFileSink.h
// DESCRIPTION: Sink pour fichiers avec rotation basée sur la taille.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/Sinks/NkFileSink.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
		
	// -------------------------------------------------------------------------
	// CLASSE: NkRotatingFileSink
	// DESCRIPTION: Sink avec rotation de fichier basée sur la taille
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkRotatingFileSink : public NkFileSink {
		public:
			// ---------------------------------------------------------------------
			// CONSTRUCTEURS
			// ---------------------------------------------------------------------

			/**
			 * @brief Constructeur avec configuration de rotation
			 * @param filename Chemin du fichier
			 * @param maxSize Taille maximum par fichier (octets)
			 * @param maxFiles Nombre maximum de fichiers conservés
			 */
			NkRotatingFileSink(const NkString &filename, usize maxSize, usize maxFiles);

			/**
			 * @brief Destructeur
			 */
			~NkRotatingFileSink() override;

			// ---------------------------------------------------------------------
			// IMPLÉMENTATION DE NkFileSink
			// ---------------------------------------------------------------------

			/**
			 * @brief Logge un message avec vérification de rotation
			 */
			void Log(const NkLogMessage &message) override;

			// ---------------------------------------------------------------------
			// CONFIGURATION DE LA ROTATION
			// ---------------------------------------------------------------------

			/**
			 * @brief Définit la taille maximum des fichiers
			 * @param maxSize Taille en octets
			 */
			void SetMaxSize(usize maxSize);

			/**
			 * @brief Obtient la taille maximum des fichiers
			 * @return Taille en octets
			 */
			usize GetMaxSize() const;

			/**
			 * @brief Définit le nombre maximum de fichiers
			 * @param maxFiles Nombre maximum
			 */
			void SetMaxFiles(usize maxFiles);

			/**
			 * @brief Obtient le nombre maximum de fichiers
			 * @return Nombre maximum
			 */
			usize GetMaxFiles() const;

			/**
			 * @brief Force la rotation du fichier
			 * @return true si rotation réussie, false sinon
			 */
			bool Rotate();

		private:
			// ---------------------------------------------------------------------
			// MÉTHODES PRIVÉES
			// ---------------------------------------------------------------------

			/**
			 * @brief Vérifie et effectue la rotation si nécessaire
			 */
			void CheckRotation() override;

			/**
			 * @brief Effectue la rotation des fichiers
			 */
			void PerformRotation();

			/**
			 * @brief Génère le nom de fichier pour un index donné
			 * @param index Index du fichier
			 * @return Nom de fichier
			 */
			NkString GetFilenameForIndex(usize index) const;

			// ---------------------------------------------------------------------
			// VARIABLES MEMBRE PRIVÉES
			// ---------------------------------------------------------------------

			/// Taille maximum par fichier (octets)
			usize m_MaxSize;

			/// Nombre maximum de fichiers conservés
			usize m_MaxFiles;

			/// Taille courante du fichier
			usize m_CurrentSize;
	};

} // namespace nkentseu
