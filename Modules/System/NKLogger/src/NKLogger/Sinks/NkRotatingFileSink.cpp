// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkRotatingFileSink.cpp
// DESCRIPTION: Implémentation du sink avec rotation basée sur la taille.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkRotatingFileSink.h"
#include <filesystem>
#include <sstream>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	/**
	 * @brief Constructeur avec configuration de rotation
	 */
	NkRotatingFileSink::NkRotatingFileSink(const std::string &filename, core::usize maxSize, core::usize maxFiles)
		: NkFileSink(filename, false), m_MaxSize(maxSize), m_MaxFiles(maxFiles), m_CurrentSize(0) {
	}

	/**
	 * @brief Destructeur
	 */
	NkRotatingFileSink::~NkRotatingFileSink() {
	}

	/**
	 * @brief Logge un message avec vérification de rotation
	 */
	void NkRotatingFileSink::Log(const NkLogMessage &message) {
		NkFileSink::Log(message);
		m_CurrentSize = GetFileSize();
	}

	/**
	 * @brief Définit la taille maximum des fichiers
	 */
	void NkRotatingFileSink::SetMaxSize(core::usize maxSize) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_MaxSize = maxSize;
	}

	/**
	 * @brief Obtient la taille maximum des fichiers
	 */
	core::usize NkRotatingFileSink::GetMaxSize() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_MaxSize;
	}

	/**
	 * @brief Définit le nombre maximum de fichiers
	 */
	void NkRotatingFileSink::SetMaxFiles(core::usize maxFiles) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_MaxFiles = maxFiles;
	}

	/**
	 * @brief Obtient le nombre maximum de fichiers
	 */
	core::usize NkRotatingFileSink::GetMaxFiles() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_MaxFiles;
	}

	/**
	 * @brief Force la rotation du fichier
	 */
	bool NkRotatingFileSink::Rotate() {
		PerformRotation();
		return true;
	}

	/**
	 * @brief Vérifie et effectue la rotation si nécessaire
	 */
	void NkRotatingFileSink::CheckRotation() {
		if (m_CurrentSize >= m_MaxSize) {
			PerformRotation();
		}
	}

	/**
	 * @brief Effectue la rotation des fichiers
	 */
	void NkRotatingFileSink::PerformRotation() {
		// Fermer le fichier courant
		Close();

		// Rotation des fichiers: .log.N -> .log.N+1
		for (core::usize i = m_MaxFiles - 1; i > 0; --i) {
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
	std::string NkRotatingFileSink::GetFilenameForIndex(core::usize index) const {
		std::ostringstream oss;
		oss << GetFilename() << "." << index;
		return oss.str();
	}

} // namespace nkentseu
