// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/RotatingFileSink.h
// DESCRIPTION: Sink pour fichiers avec rotation basée sur la taille.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "Logger/Sinks/FileSink.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// CLASSE: RotatingFileSink
// DESCRIPTION: Sink avec rotation de fichier basée sur la taille
// -------------------------------------------------------------------------
class LOGGER_API RotatingFileSink : public FileSink {
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
	RotatingFileSink(const std::string &filename, size_t maxSize, size_t maxFiles);

	/**
	 * @brief Destructeur
	 */
	~RotatingFileSink() override;

	// ---------------------------------------------------------------------
	// IMPLÉMENTATION DE FileSink
	// ---------------------------------------------------------------------

	/**
	 * @brief Logge un message avec vérification de rotation
	 */
	void Log(const LogMessage &message) override;

	// ---------------------------------------------------------------------
	// CONFIGURATION DE LA ROTATION
	// ---------------------------------------------------------------------

	/**
	 * @brief Définit la taille maximum des fichiers
	 * @param maxSize Taille en octets
	 */
	void SetMaxSize(size_t maxSize);

	/**
	 * @brief Obtient la taille maximum des fichiers
	 * @return Taille en octets
	 */
	size_t GetMaxSize() const;

	/**
	 * @brief Définit le nombre maximum de fichiers
	 * @param maxFiles Nombre maximum
	 */
	void SetMaxFiles(size_t maxFiles);

	/**
	 * @brief Obtient le nombre maximum de fichiers
	 * @return Nombre maximum
	 */
	size_t GetMaxFiles() const;

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
	std::string GetFilenameForIndex(size_t index) const;

	// ---------------------------------------------------------------------
	// VARIABLES MEMBRE PRIVÉES
	// ---------------------------------------------------------------------

	/// Taille maximum par fichier (octets)
	size_t m_MaxSize;

	/// Nombre maximum de fichiers conservés
	size_t m_MaxFiles;

	/// Taille courante du fichier
	size_t m_CurrentSize;
};

} // namespace logger
} // namespace nkentseu