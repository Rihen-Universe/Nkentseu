// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/FileSink.cpp
// DESCRIPTION: Implémentation du sink fichier.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Sinks/FileSink.h"
#include <filesystem>
#include <chrono>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// IMPLÉMENTATION DE FileSink
// -------------------------------------------------------------------------

/**
 * @brief Constructeur avec chemin de fichier
 */
FileSink::FileSink(const std::string &filename, bool truncate) : m_Filename(filename), m_Truncate(truncate) {
	m_Formatter = std::make_unique<Formatter>(Formatter::DEFAULT_PATTERN);

	// Créer le répertoire parent si nécessaire
	std::filesystem::path path(filename);
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}

	Open();
}

/**
 * @brief Destructeur
 */
FileSink::~FileSink() {
	Close();
}

/**
 * @brief Logge un message dans le fichier
 */
void FileSink::Log(const LogMessage &message) {
	if (!IsEnabled() || !ShouldLog(message.level)) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_Mutex);

	// Vérifier si le fichier est ouvert
	if (!m_FileStream.is_open()) {
		if (!OpenFile()) {
			return; // Impossible d'ouvrir le fichier
		}
	}

	// Formater le message
	std::string formatted = m_Formatter->Format(message, false);

	// Écrire dans le fichier
	m_FileStream << formatted << std::endl;

	// Vérifier la rotation si nécessaire
	CheckRotation();
}

/**
 * @brief Force l'écriture des données en attente
 */
void FileSink::Flush() {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_FileStream.is_open()) {
		m_FileStream.flush();
	}
}

/**
 * @brief Définit le formatter pour ce sink
 */
void FileSink::SetFormatter(std::unique_ptr<Formatter> formatter) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Formatter = std::move(formatter);
}

/**
 * @brief Définit le pattern de formatage
 */
void FileSink::SetPattern(const std::string &pattern) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_Formatter) {
		m_Formatter->SetPattern(pattern);
	}
}

/**
 * @brief Obtient le formatter courant
 */
Formatter *FileSink::GetFormatter() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Formatter.get();
}

/**
 * @brief Obtient le pattern courant
 */
std::string FileSink::GetPattern() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_Formatter) {
		return m_Formatter->GetPattern();
	}
	return "";
}

/**
 * @brief Ouvre le fichier
 */
bool FileSink::Open() {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return OpenFile();
}

/**
 * @brief Ferme le fichier
 */
void FileSink::Close() {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_FileStream.is_open()) {
		m_FileStream.close();
	}
}

/**
 * @brief Vérifie si le fichier est ouvert
 */
bool FileSink::IsOpen() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_FileStream.is_open();
}

/**
 * @brief Obtient le nom du fichier
 */
std::string FileSink::GetFilename() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Filename;
}

/**
 * @brief Définit un nouveau nom de fichier
 */
void FileSink::SetFilename(const std::string &filename) {
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_Filename != filename) {
		// Fermer l'ancien fichier
		if (m_FileStream.is_open()) {
			m_FileStream.close();
		}

		// Mettre à jour le nom
		m_Filename = filename;

		// Créer le répertoire parent si nécessaire
		std::filesystem::path path(filename);
		if (path.has_parent_path()) {
			std::filesystem::create_directories(path.parent_path());
		}

		// Ouvrir le nouveau fichier
		OpenFile();
	}
}

/**
 * @brief Obtient la taille actuelle du fichier
 */
size_t FileSink::GetFileSize() const {
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (!std::filesystem::exists(m_Filename)) {
		return 0;
	}

	try {
		return std::filesystem::file_size(m_Filename);
	} catch (...) {
		return 0;
	}
}

/**
 * @brief Définit le mode d'ouverture
 */
void FileSink::SetTruncate(bool truncate) {
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_Truncate != truncate) {
		m_Truncate = truncate;

		// Re-ouvrir le fichier avec le nouveau mode
		if (m_FileStream.is_open()) {
			m_FileStream.close();
			OpenFile();
		}
	}
}

/**
 * @brief Obtient le mode d'ouverture
 */
bool FileSink::GetTruncate() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Truncate;
}

/**
 * @brief Ouvre le fichier avec le mode approprié
 */
bool FileSink::OpenFile() {
	if (m_Filename.empty()) {
		return false;
	}

	// Déterminer le mode d'ouverture
	std::ios_base::openmode mode = std::ios_base::out;
	if (!m_Truncate) {
		mode |= std::ios_base::app;
	}

	// Ouvrir le fichier
	m_FileStream.open(m_Filename, mode);

	if (!m_FileStream.is_open()) {
		// Essayer de créer le répertoire parent
		std::filesystem::path path(m_Filename);
		if (path.has_parent_path()) {
			std::filesystem::create_directories(path.parent_path());
			m_FileStream.open(m_Filename, mode);
		}
	}

	// Vérifier l'ouverture
	if (!m_FileStream.is_open()) {
		return false;
	}

	// Configurer le flux
	m_FileStream << std::unitbuf; // Pas de buffering
	return true;
}

/**
 * @brief Vérifie et gère la rotation de fichier si nécessaire
 */
void FileSink::CheckRotation() {
	// Par défaut, pas de rotation
	// Les sous-classes peuvent override cette méthode
}

} // namespace logger
} // namespace nkentseu