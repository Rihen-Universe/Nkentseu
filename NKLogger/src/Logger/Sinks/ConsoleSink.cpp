// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Sinks/ConsoleSink.cpp
// DESCRIPTION: ImplÃ©mentation du sink console avec support couleur.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Sinks/ConsoleSink.h"
#include "Logger/LogLevel.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h> // Pour isatty(), fileno()
#include <cstring>	// Pour strstr()
#endif

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// IMPLÃ‰MENTATION DE ConsoleSink
// -------------------------------------------------------------------------

/**
 * @brief Constructeur par dÃ©faut
 */
ConsoleSink::ConsoleSink() : m_Stream(ConsoleStream::StdOut), m_UseColors(true), m_UseStderrForErrors(true) {
	m_Formatter = std::make_unique<Formatter>(Formatter::COLOR_PATTERN);
}

/**
 * @brief Constructeur avec flux spÃ©cifique
 */
ConsoleSink::ConsoleSink(ConsoleStream stream, bool useColors)
	: m_Stream(stream), m_UseColors(useColors), m_UseStderrForErrors(true) {
	m_Formatter = std::make_unique<Formatter>(useColors ? Formatter::COLOR_PATTERN : Formatter::DEFAULT_PATTERN);
}

/**
 * @brief Destructeur
 */
ConsoleSink::~ConsoleSink() {
	Flush();
}

/**
 * @brief Logge un message dans la console
 */
void ConsoleSink::Log(const LogMessage &message) {
	if (!IsEnabled() || !ShouldLog(message.level)) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_Mutex);

	// Formater le message
	std::string formatted = m_Formatter->Format(message, m_UseColors && SupportsColors());

	// Obtenir le flux appropriÃ©
	std::ostream &stream = GetStreamForLevel(message.level);

	// Ã‰crire le message
	stream << formatted << std::endl;

	// Flush pour les niveaux critiques
	if (message.level >= LogLevel::Error) {
		stream.flush();
	}
}

/**
 * @brief Force l'Ã©criture des donnÃ©es en attente
 */
void ConsoleSink::Flush() {
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_Stream == ConsoleStream::StdOut || (m_UseStderrForErrors && m_Stream == ConsoleStream::StdOut)) {
		std::cout.flush();
	}

	if (m_Stream == ConsoleStream::StdErr || (m_UseStderrForErrors && m_Stream == ConsoleStream::StdOut)) {
		std::cerr.flush();
	}
}

/**
 * @brief DÃ©finit le formatter pour ce sink
 */
void ConsoleSink::SetFormatter(std::unique_ptr<Formatter> formatter) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Formatter = std::move(formatter);
}

/**
 * @brief DÃ©finit le pattern de formatage
 */
void ConsoleSink::SetPattern(const std::string &pattern) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_Formatter) {
		m_Formatter->SetPattern(pattern);
	}
}

/**
 * @brief Obtient le formatter courant
 */
Formatter *ConsoleSink::GetFormatter() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Formatter.get();
}

/**
 * @brief Obtient le pattern courant
 */
std::string ConsoleSink::GetPattern() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_Formatter) {
		return m_Formatter->GetPattern();
	}
	return "";
}

/**
 * @brief Active ou dÃ©sactive les couleurs
 */
void ConsoleSink::SetColorEnabled(bool enable) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_UseColors = enable;
}

/**
 * @brief VÃ©rifie si les couleurs sont activÃ©es
 */
bool ConsoleSink::IsColorEnabled() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_UseColors;
}

/**
 * @brief DÃ©finit le flux de console
 */
void ConsoleSink::SetStream(ConsoleStream stream) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Stream = stream;
}

/**
 * @brief Obtient le flux de console courant
 */
ConsoleStream ConsoleSink::GetStream() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Stream;
}

/**
 * @brief DÃ©finit si le sink doit utiliser stderr pour les niveaux d'erreur
 */
void ConsoleSink::SetUseStderrForErrors(bool enable) {
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_UseStderrForErrors = enable;
}

/**
 * @brief VÃ©rifie si le sink utilise stderr pour les erreurs
 */
bool ConsoleSink::IsUsingStderrForErrors() const {
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_UseStderrForErrors;
}

/**
 * @brief Obtient le flux de sortie appropriÃ© pour un niveau de log
 */
std::ostream &ConsoleSink::GetStreamForLevel(LogLevel level) {
	if (m_UseStderrForErrors && (level == LogLevel::Error || level == LogLevel::Critical || level == LogLevel::Fatal)) {
		return std::cerr;
	}

	return (m_Stream == ConsoleStream::StdOut) ? std::cout : std::cerr;
}

/**
 * @brief VÃ©rifie si la console supporte les couleurs
 */
bool ConsoleSink::SupportsColors() const {
#ifdef _WIN32
	// Windows: vÃ©rifier si la console supporte les couleurs
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD mode;
	if (!GetConsoleMode(hConsole, &mode)) {
		return false;
	}

	return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
	// Unix: vÃ©rifier si c'est un terminal et supporte les couleurs
	static bool checked = false;
	static bool supports = false;

	if (!checked) {
		// VÃ©rifier si stdout est un terminal
		int fd = fileno(stdout);
		if (fd < 0) {
			supports = false;
		} else {
			// VÃ©rifier si c'est un TTY
			if (isatty(fd)) {
				// VÃ©rifier la variable TERM
				const char *term = getenv("TERM");
				if (term != nullptr) {
					// Chercher des terminaux qui supportent les couleurs
					supports = (strstr(term, "xterm") != nullptr || strstr(term, "color") != nullptr ||
								strstr(term, "ansi") != nullptr);
				}
			}
		}
		checked = true;
	}

	return supports;
#endif
}

/**
 * @brief Obtient le code couleur pour un niveau de log
 */
std::string ConsoleSink::GetColorCode(LogLevel level) const {
	return LogLevelToANSIColor(level);
}

/**
 * @brief Obtient le code de rÃ©initialisation de couleur
 */
std::string ConsoleSink::GetResetCode() const {
	return "\033[0m";
}

/**
 * @brief Configure la couleur Windows pour un niveau de log
 */
void ConsoleSink::SetWindowsColor(LogLevel level) {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle((m_Stream == ConsoleStream::StdOut) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);

	if (hConsole != INVALID_HANDLE_VALUE) {
		WORD color = LogLevelToWindowsColor(level);
		SetConsoleTextAttribute(hConsole, color);
	}
#endif
}

/**
 * @brief RÃ©initialise la couleur Windows
 */
void ConsoleSink::ResetWindowsColor() {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle((m_Stream == ConsoleStream::StdOut) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);

	if (hConsole != INVALID_HANDLE_VALUE) {
		SetConsoleTextAttribute(hConsole, 0x07); // Gris sur noir
	}
#endif
}

} // namespace logger
} // namespace nkentseu
