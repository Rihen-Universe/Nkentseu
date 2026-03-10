// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkRotatingFileSink.cpp
// DESCRIPTION: Implémentation du sink avec rotation basée sur la taille.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkRotatingFileSink.h"
#include <cstdio>
#include <sys/stat.h>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	namespace {
		static bool NkFileExists(const NkString& path) {
			struct stat info {};
			return ::stat(path.CStr(), &info) == 0;
		}
	}

	/**
	 * @brief Constructeur avec configuration de rotation
	 */
	NkRotatingFileSink::NkRotatingFileSink(const NkString &filename, usize maxSize, usize maxFiles)
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
	void NkRotatingFileSink::SetMaxSize(usize maxSize) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_MaxSize = maxSize;
	}

	/**
	 * @brief Obtient la taille maximum des fichiers
	 */
	usize NkRotatingFileSink::GetMaxSize() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_MaxSize;
	}

	/**
	 * @brief Définit le nombre maximum de fichiers
	 */
	void NkRotatingFileSink::SetMaxFiles(usize maxFiles) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_MaxFiles = maxFiles;
	}

	/**
	 * @brief Obtient le nombre maximum de fichiers
	 */
	usize NkRotatingFileSink::GetMaxFiles() const {
		logger_sync::NkScopedLock lock(m_Mutex);
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
		if (m_MaxFiles == 0) {
			return;
		}

		// Fermer le fichier courant
		Close();

		// Rotation des fichiers: .log.N -> .log.N+1
		for (usize i = m_MaxFiles - 1; i > 0; --i) {
			const NkString oldFile = GetFilenameForIndex(i - 1);
			const NkString newFile = GetFilenameForIndex(i);
			if (NkFileExists(oldFile)) {
				(void)::rename(oldFile.CStr(), newFile.CStr());
			}
		}

		// Renommer le fichier courant en .log.0
		const NkString currentFile = GetFilename();
		const NkString rotatedFile = GetFilenameForIndex(0);
		if (NkFileExists(currentFile)) {
			(void)::rename(currentFile.CStr(), rotatedFile.CStr());
		}

		// Rouvrir le fichier
		Open();
		m_CurrentSize = 0;
	}

	/**
	 * @brief Génère le nom de fichier pour un index donné
	 */
	NkString NkRotatingFileSink::GetFilenameForIndex(usize index) const {
		char suffix[32];
		const int written = ::snprintf(suffix, sizeof(suffix), ".%zu", static_cast<size_t>(index));
		NkString name = GetFilename();
		if (written > 0) {
			name.Append(suffix, static_cast<usize>(written));
		}
		return name;
	}

} // namespace nkentseu
