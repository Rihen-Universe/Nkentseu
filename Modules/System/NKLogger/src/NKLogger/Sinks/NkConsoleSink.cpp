// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkConsoleSink.cpp
// DESCRIPTION: ImplÃ©mentation du sink console avec support couleur.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/NkLogLevel.h"

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
		
	// -------------------------------------------------------------------------
	// IMPLÃ‰MENTATION DE NkConsoleSink
	// -------------------------------------------------------------------------

	/**
	 * @brief Constructeur par dÃ©faut
	 */
	NkConsoleSink::NkConsoleSink() : m_Stream(NkConsoleStream::NK_STD_OUT), m_UseColors(true), m_UseStderrForErrors(true) {
		m_Formatter = std::make_unique<NkFormatter>(NkFormatter::NK_COLOR_PATTERN);
	}

	/**
	 * @brief Constructeur avec flux spÃ©cifique
	 */
	NkConsoleSink::NkConsoleSink(NkConsoleStream stream, bool useColors)
		: m_Stream(stream), m_UseColors(useColors), m_UseStderrForErrors(true) {
		m_Formatter = std::make_unique<NkFormatter>(useColors ? NkFormatter::NK_COLOR_PATTERN : NkFormatter::NK_DEFAULT_PATTERN);
	}

	/**
	 * @brief Destructeur
	 */
	NkConsoleSink::~NkConsoleSink() {
		Flush();
	}

	/**
	 * @brief Logge un message dans la console
	 */
	void NkConsoleSink::Log(const NkLogMessage &message) {
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
		if (message.level >= NkLogLevel::NK_ERROR) {
			stream.flush();
		}
	}

	/**
	 * @brief Force l'Ã©criture des donnÃ©es en attente
	 */
	void NkConsoleSink::Flush() {
		std::lock_guard<std::mutex> lock(m_Mutex);

		if (m_Stream == NkConsoleStream::NK_STD_OUT || (m_UseStderrForErrors && m_Stream == NkConsoleStream::NK_STD_OUT)) {
			std::cout.flush();
		}

		if (m_Stream == NkConsoleStream::NK_STD_ERR || (m_UseStderrForErrors && m_Stream == NkConsoleStream::NK_STD_OUT)) {
			std::cerr.flush();
		}
	}

	/**
	 * @brief DÃ©finit le formatter pour ce sink
	 */
	void NkConsoleSink::SetFormatter(std::unique_ptr<NkFormatter> formatter) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Formatter = std::move(formatter);
	}

	/**
	 * @brief DÃ©finit le pattern de formatage
	 */
	void NkConsoleSink::SetPattern(const std::string &pattern) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Formatter) {
			m_Formatter->SetPattern(pattern);
		}
	}

	/**
	 * @brief Obtient le formatter courant
	 */
	NkFormatter *NkConsoleSink::GetFormatter() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Formatter.get();
	}

	/**
	 * @brief Obtient le pattern courant
	 */
	std::string NkConsoleSink::GetPattern() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Formatter) {
			return m_Formatter->GetPattern();
		}
		return "";
	}

	/**
	 * @brief Active ou dÃ©sactive les couleurs
	 */
	void NkConsoleSink::SetColorEnabled(bool enable) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_UseColors = enable;
	}

	/**
	 * @brief VÃ©rifie si les couleurs sont activÃ©es
	 */
	bool NkConsoleSink::IsColorEnabled() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_UseColors;
	}

	/**
	 * @brief DÃ©finit le flux de console
	 */
	void NkConsoleSink::SetStream(NkConsoleStream stream) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Stream = stream;
	}

	/**
	 * @brief Obtient le flux de console courant
	 */
	NkConsoleStream NkConsoleSink::GetStream() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Stream;
	}

	/**
	 * @brief DÃ©finit si le sink doit utiliser stderr pour les niveaux d'erreur
	 */
	void NkConsoleSink::SetUseStderrForErrors(bool enable) {
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_UseStderrForErrors = enable;
	}

	/**
	 * @brief VÃ©rifie si le sink utilise stderr pour les erreurs
	 */
	bool NkConsoleSink::IsUsingStderrForErrors() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_UseStderrForErrors;
	}

	/**
	 * @brief Obtient le flux de sortie appropriÃ© pour un niveau de log
	 */
	std::ostream &NkConsoleSink::GetStreamForLevel(NkLogLevel level) {
		if (m_UseStderrForErrors && (level == NkLogLevel::NK_ERROR || level == NkLogLevel::NK_CRITICAL || level == NkLogLevel::NK_FATAL)) {
			return std::cerr;
		}

		return (m_Stream == NkConsoleStream::NK_STD_OUT) ? std::cout : std::cerr;
	}

	/**
	 * @brief VÃ©rifie si la console supporte les couleurs
	 */
	bool NkConsoleSink::SupportsColors() const {
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
	std::string NkConsoleSink::GetColorCode(NkLogLevel level) const {
		return NkLogLevelToANSIColor(level);
	}

	/**
	 * @brief Obtient le code de rÃ©initialisation de couleur
	 */
	std::string NkConsoleSink::GetResetCode() const {
		return "\033[0m";
	}

	/**
	 * @brief Configure la couleur Windows pour un niveau de log
	 */
	void NkConsoleSink::SetWindowsColor(NkLogLevel level) {
	#ifdef _WIN32
		HANDLE hConsole = GetStdHandle((m_Stream == NkConsoleStream::NK_STD_OUT) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);

		if (hConsole != INVALID_HANDLE_VALUE) {
			WORD color = NkLogLevelToWindowsColor(level);
			SetConsoleTextAttribute(hConsole, color);
		}
	#endif
	}

	/**
	 * @brief RÃ©initialise la couleur Windows
	 */
	void NkConsoleSink::ResetWindowsColor() {
	#ifdef _WIN32
		HANDLE hConsole = GetStdHandle((m_Stream == NkConsoleStream::NK_STD_OUT) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);

		if (hConsole != INVALID_HANDLE_VALUE) {
			SetConsoleTextAttribute(hConsole, 0x07); // Gris sur noir
		}
	#endif
	}

} // namespace nkentseu
