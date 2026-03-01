// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/RotatingFileSink.cpp
// DESCRIPTION: Implémentation du sink avec rotation basée sur la taille.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Sinks/RotatingFileSink.h"
#include <filesystem>
#include <sstream>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

/**
 * @brief Constructeur avec configuration de rotation
 */
RotatingFileSink::RotatingFileSink(const std::string &filename, size_t maxSize, size_t maxFiles)
	: FileSink(filename, false), m_MaxSize(maxSize), m_MaxFiles(maxFiles), m_CurrentSize(0) {
}

/**
 * @brief Destructeur
 */
RotatingFileSink::~RotatingFileSink() {
}

/**
 * @brief Logge un message avec vérification de rotation
 */
void RotatingFileSink::Log(const LogMessage &message) {
	FileSink::Log(message);
	m_CurrentSize = GetFileSize();
}

/**
 * @brief Définit la taille maximum des fichiers
 */
void RotatingFileSink::SetMaxSize(size_t maxSize) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_MaxSize = maxSize;
}

/**
 * @brief Obtient la taille maximum des fichiers
 */
size_t RotatingFileSink::GetMaxSize() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_MaxSize;
}

/**
 * @brief Définit le nombre maximum de fichiers
 */
void RotatingFileSink::SetMaxFiles(size_t maxFiles) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_MaxFiles = maxFiles;
}

/**
 * @brief Obtient le nombre maximum de fichiers
 */
size_t RotatingFileSink::GetMaxFiles() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_MaxFiles;
}

/**
 * @brief Force la rotation du fichier
 */
bool RotatingFileSink::Rotate() {
	PerformRotation();
	return true;
}

/**
 * @brief Vérifie et effectue la rotation si nécessaire
 */
void RotatingFileSink::CheckRotation() {
	if (m_CurrentSize >= m_MaxSize) {
		PerformRotation();
	}
}

/**
 * @brief Effectue la rotation des fichiers
 */
void RotatingFileSink::PerformRotation() {
	// Fermer le fichier courant
	Close();

	// Rotation des fichiers: .log.N -> .log.N+1
	for (size_t i = m_MaxFiles - 1; i > 0; --i) {
		std::string oldFile = GetFilenameForIndex(i - 1);
		std::string newFile = GetFilenameForIndex(i);

		if (std::filesystem::exists(oldFile)) {
			std::filesystem::rename(oldFile, newFile);
		}
	}

	// Renommer le fichier courant en .log.0
	std::string currentFile = GetFilename();
	std::string rotatedFile = GetFilenameForIndex(0);

	if (std::filesystem::exists(currentFile)) {
		std::filesystem::rename(currentFile, rotatedFile);
	}

	// Rouvrir le fichier
	Open();
	m_CurrentSize = 0;
}

/**
 * @brief Génère le nom de fichier pour un index donné
 */
std::string RotatingFileSink::GetFilenameForIndex(size_t index) const {
	std::ostringstream oss;
	oss << GetFilename() << "." << index;
	return oss.str();
}

} // namespace logger
} // namespace nkentseu
