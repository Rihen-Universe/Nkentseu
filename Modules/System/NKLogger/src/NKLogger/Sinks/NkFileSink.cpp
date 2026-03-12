// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkFileSink.cpp
// DESCRIPTION: Implémentation du sink fichier.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkFileSink.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

#if defined(_WIN32)
	#include <direct.h>
#endif

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
	namespace {
		static bool NkPathExists(const NkString& path) {
			if (path.Empty()) {
				return false;
			}
			struct stat info {};
			return ::stat(path.CStr(), &info) == 0;
		}

		static usize NkGetPathFileSize(const NkString& path) {
			struct stat info {};
			if (::stat(path.CStr(), &info) != 0) {
				return 0;
			}
			return static_cast<usize>(info.st_size);
		}

		static bool NkCreateDirectory(const char* path) {
#if defined(_WIN32)
			const int rc = ::_mkdir(path);
#else
			const int rc = ::mkdir(path, 0755);
#endif
			return (rc == 0 || errno == EEXIST);
		}

		static bool NkCreateDirectories(const NkString& directory) {
			if (directory.Empty()) {
				return true;
			}
			if (NkPathExists(directory)) {
				return true;
			}

			NkString current;
			current.Reserve(directory.Size());
			for (usize i = 0; i < directory.Size(); ++i) {
				const char ch = directory[i];
				current.PushBack(ch);
				const bool separator = (ch == '/' || ch == '\\');
				if (!separator) {
					continue;
				}
				if (current.Size() <= 1) {
					continue;
				}
				(void)NkCreateDirectory(current.CStr());
			}
			return NkCreateDirectory(directory.CStr());
		}

		static void NkEnsureParentDirectory(const NkString& filename) {
			const usize slashPos = filename.FindLastOf("/\\");
			if (slashPos == NkString::npos) {
				return;
			}
			NkString directory = filename.SubStr(0, slashPos);
			(void)NkCreateDirectories(directory);
		}
	}

	// -------------------------------------------------------------------------
	// IMPLÉMENTATION DE NkFileSink
	// -------------------------------------------------------------------------

	/**
	 * @brief Constructeur avec chemin de fichier
	 */
	NkFileSink::NkFileSink(const NkString &filename, bool truncate)
		: m_Formatter(memory::NkMakeUnique<NkFormatter>(NkFormatter::NK_DEFAULT_PATTERN)),
		  m_FileStream(nullptr),
		  m_Filename(filename),
		  m_Truncate(truncate) {
		NkEnsureParentDirectory(filename);

		Open();
	}

	/**
	 * @brief Destructeur
	 */
	NkFileSink::~NkFileSink() {
		Close();
	}

	/**
	 * @brief Logge un message dans le fichier
	 */
	void NkFileSink::Log(const NkLogMessage &message) {
		if (!IsEnabled() || !ShouldLog(message.level)) {
			return;
		}

		logger_sync::NkScopedLock lock(m_Mutex);

		// Vérifier si le fichier est ouvert
		if (m_FileStream == nullptr) {
			if (!OpenFile()) {
				return; // Impossible d'ouvrir le fichier
			}
		}

		// Formater le message
		NkString formatted = m_Formatter->Format(message, false);

		// Écrire dans le fichier
		if (formatted.Size() > 0) {
			(void)::fwrite(formatted.CStr(), sizeof(char), static_cast<size_t>(formatted.Size()), m_FileStream);
		}
		(void)::fputc('\n', m_FileStream);

		// Vérifier la rotation si nécessaire
		CheckRotation();
	}

	/**
	 * @brief Force l'écriture des données en attente
	 */
	void NkFileSink::Flush() {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (m_FileStream != nullptr) {
			(void)::fflush(m_FileStream);
		}
	}

	/**
	 * @brief Définit le formatter pour ce sink
	 */
	void NkFileSink::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_Formatter = traits::NkMove(formatter);
	}

	/**
	 * @brief Définit le pattern de formatage
	 */
	void NkFileSink::SetPattern(const NkString &pattern) {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (m_Formatter) {
			m_Formatter->SetPattern(pattern);
		}
	}

	/**
	 * @brief Obtient le formatter courant
	 */
	NkFormatter *NkFileSink::GetFormatter() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Formatter.Get();
	}

	/**
	 * @brief Obtient le pattern courant
	 */
	NkString NkFileSink::GetPattern() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (m_Formatter) {
			return m_Formatter->GetPattern();
		}
		return NkString();
	}

	/**
	 * @brief Ouvre le fichier
	 */
	bool NkFileSink::Open() {
		logger_sync::NkScopedLock lock(m_Mutex);
		return OpenUnlocked();
	}

	/**
	 * @brief Ferme le fichier
	 */
	void NkFileSink::Close() {
		logger_sync::NkScopedLock lock(m_Mutex);
		CloseUnlocked();
	}

	/**
	 * @brief Vérifie si le fichier est ouvert
	 */
	bool NkFileSink::IsOpen() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_FileStream != nullptr;
	}

	/**
	 * @brief Obtient le nom du fichier
	 */
	NkString NkFileSink::GetFilename() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Filename;
	}

	/**
	 * @brief Définit un nouveau nom de fichier
	 */
	void NkFileSink::SetFilename(const NkString &filename) {
		logger_sync::NkScopedLock lock(m_Mutex);

		if (m_Filename != filename) {
			// Fermer l'ancien fichier
			CloseUnlocked();

			// Mettre à jour le nom
			m_Filename = filename;

			// Créer le répertoire parent si nécessaire
			NkEnsureParentDirectory(filename);

			// Ouvrir le nouveau fichier
			OpenUnlocked();
		}
	}

	/**
	 * @brief Obtient la taille actuelle du fichier
	 */
	usize NkFileSink::GetFileSize() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return NkGetPathFileSize(m_Filename);
	}

	/**
	 * @brief Définit le mode d'ouverture
	 */
	void NkFileSink::SetTruncate(bool truncate) {
		logger_sync::NkScopedLock lock(m_Mutex);

		if (m_Truncate != truncate) {
			m_Truncate = truncate;

			// Re-ouvrir le fichier avec le nouveau mode
			if (m_FileStream != nullptr) {
				CloseUnlocked();
				OpenUnlocked();
			}
		}
	}

	/**
	 * @brief Obtient le mode d'ouverture
	 */
	bool NkFileSink::GetTruncate() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Truncate;
	}

	/**
	 * @brief Ouvre le fichier avec le mode approprié
	 */
	bool NkFileSink::OpenFile() {
		if (m_Filename.Empty()) {
			return false;
		}

		NkEnsureParentDirectory(m_Filename);

		const char* mode = m_Truncate ? "wb" : "ab";
		m_FileStream = ::fopen(m_Filename.CStr(), mode);
		if (m_FileStream == nullptr) {
			NkEnsureParentDirectory(m_Filename);
			m_FileStream = ::fopen(m_Filename.CStr(), mode);
		}
		if (m_FileStream == nullptr) {
			return false;
		}

		(void)::setvbuf(m_FileStream, nullptr, _IONBF, 0); // Pas de buffering
		return true;
	}

	bool NkFileSink::OpenUnlocked() {
		return OpenFile();
	}

	void NkFileSink::CloseUnlocked() {
		if (m_FileStream != nullptr) {
			(void)::fclose(m_FileStream);
			m_FileStream = nullptr;
		}
	}

	NkString NkFileSink::GetFilenameUnlocked() const {
		return m_Filename;
	}

	/**
	 * @brief Vérifie et gère la rotation de fichier si nécessaire
	 */
	void NkFileSink::CheckRotation() {
		// Par défaut, pas de rotation
		// Les sous-classes peuvent override cette méthode
	}

} // namespace nkentseu
