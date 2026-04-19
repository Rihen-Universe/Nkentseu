// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkConsoleSink.cpp
// DESCRIPTION: Implémentation du sink console avec support couleur.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/NkLogLevel.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <cstdio>
#include <cstdlib>

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include <android/log.h>
#endif

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

namespace {
#if defined(NKENTSEU_PLATFORM_ANDROID)
    int NkToAndroidPriority(NkLogLevel level) {
        switch (level) {
            case NkLogLevel::NK_TRACE:
                return ANDROID_LOG_VERBOSE;
            case NkLogLevel::NK_DEBUG:
                return ANDROID_LOG_DEBUG;
            case NkLogLevel::NK_INFO:
                return ANDROID_LOG_INFO;
            case NkLogLevel::NK_WARN:
                return ANDROID_LOG_WARN;
            case NkLogLevel::NK_ERROR:
                return ANDROID_LOG_ERROR;
            case NkLogLevel::NK_CRITICAL:
            case NkLogLevel::NK_FATAL:
                return ANDROID_LOG_FATAL;
            default:
                return ANDROID_LOG_INFO;
        }
    }

    NkString NkMakeAndroidTag(const NkString& loggerName) {
        if (loggerName.Empty()) {
            return "NkConsole";
        }
        if (loggerName.Length() > 23) {
            return loggerName.SubStr(0, 23);
        }
        return loggerName;
    }
#endif
} // namespace
        
    // -------------------------------------------------------------------------
    // IMPLÉMENTATION DE NkConsoleSink
    // -------------------------------------------------------------------------

    /**
     * @brief Constructeur par défaut
     */
    NkConsoleSink::NkConsoleSink() : m_Stream(NkConsoleStream::NK_STD_OUT), m_UseColors(true), m_UseStderrForErrors(true) {
        m_Formatter = memory::NkMakeUnique<NkFormatter>(NkFormatter::NK_COLOR_PATTERN);
    }

    /**
     * @brief Constructeur avec flux spécifique
     */
    NkConsoleSink::NkConsoleSink(NkConsoleStream stream, bool useColors)
        : m_Stream(stream), m_UseColors(useColors), m_UseStderrForErrors(true) {
        m_Formatter = memory::NkMakeUnique<NkFormatter>(useColors ? NkFormatter::NK_COLOR_PATTERN : NkFormatter::NK_DEFAULT_PATTERN);
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

        loggersync::NkScopedLock lock(m_Mutex);

        // Formater le message
        NkString formatted = m_Formatter->Format(message, m_UseColors && SupportsColors());

#if defined(NKENTSEU_PLATFORM_ANDROID)
        // Android: route la sortie console vers logcat.
        const NkString tag = NkMakeAndroidTag(message.loggerName);
        __android_log_print(NkToAndroidPriority(message.level), tag.CStr(), "%s", formatted.CStr());
        return;
#endif

        // Obtenir le flux approprié
        FILE *stream = GetStreamForLevel(message.level);
        if (stream == nullptr) {
            return;
        }

        // Écrire le message
        if (formatted.Size() > 0) {
            (void)::fwrite(formatted.CStr(), sizeof(char), static_cast<size_t>(formatted.Size()), stream);
        }
        (void)::fputc('\n', stream);

        // Flush pour les niveaux critiques
        if (message.level >= NkLogLevel::NK_ERROR) {
            (void)::fflush(stream);
        }
    }

    /**
     * @brief Force l'écriture des données en attente
     */
    void NkConsoleSink::Flush() {
        loggersync::NkScopedLock lock(m_Mutex);

#if defined(NKENTSEU_PLATFORM_ANDROID)
        // Logcat gère son propre buffering.
        return;
#endif

        if (m_Stream == NkConsoleStream::NK_STD_OUT || (m_UseStderrForErrors && m_Stream == NkConsoleStream::NK_STD_OUT)) {
            (void)::fflush(stdout);
        }

        if (m_Stream == NkConsoleStream::NK_STD_ERR || (m_UseStderrForErrors && m_Stream == NkConsoleStream::NK_STD_OUT)) {
            (void)::fflush(stderr);
        }
    }

    /**
     * @brief Définit le formatter pour ce sink
     */
    void NkConsoleSink::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Formatter = traits::NkMove(formatter);
    }

    /**
     * @brief Définit le pattern de formatage
     */
    void NkConsoleSink::SetPattern(const NkString &pattern) {
        loggersync::NkScopedLock lock(m_Mutex);
        if (m_Formatter) {
            m_Formatter->SetPattern(pattern);
        }
    }

    /**
     * @brief Obtient le formatter courant
     */
    NkFormatter *NkConsoleSink::GetFormatter() const {
        loggersync::NkScopedLock lock(m_Mutex);
        return m_Formatter.Get();
    }

    /**
     * @brief Obtient le pattern courant
     */
    NkString NkConsoleSink::GetPattern() const {
        loggersync::NkScopedLock lock(m_Mutex);
        if (m_Formatter) {
            return m_Formatter->GetPattern();
        }
        return NkString();
    }

    /**
     * @brief Active ou désactive les couleurs
     */
    void NkConsoleSink::SetColorEnabled(bool enable) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_UseColors = enable;
    }

    /**
     * @brief Vérifie si les couleurs sont activées
     */
    bool NkConsoleSink::IsColorEnabled() const {
        loggersync::NkScopedLock lock(m_Mutex);
        return m_UseColors;
    }

    /**
     * @brief Définit le flux de console
     */
    void NkConsoleSink::SetStream(NkConsoleStream stream) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Stream = stream;
    }

    /**
     * @brief Obtient le flux de console courant
     */
    NkConsoleStream NkConsoleSink::GetStream() const {
        loggersync::NkScopedLock lock(m_Mutex);
        return m_Stream;
    }

    /**
     * @brief Définit si le sink doit utiliser stderr pour les niveaux d'erreur
     */
    void NkConsoleSink::SetUseStderrForErrors(bool enable) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_UseStderrForErrors = enable;
    }

    /**
     * @brief Vérifie si le sink utilise stderr pour les erreurs
     */
    bool NkConsoleSink::IsUsingStderrForErrors() const {
        loggersync::NkScopedLock lock(m_Mutex);
        return m_UseStderrForErrors;
    }

    /**
     * @brief Obtient le flux de sortie approprié pour un niveau de log
     */
    FILE *NkConsoleSink::GetStreamForLevel(NkLogLevel level) {
        if (m_UseStderrForErrors && (level == NkLogLevel::NK_ERROR || level == NkLogLevel::NK_CRITICAL || level == NkLogLevel::NK_FATAL)) {
            return stderr;
        }

        return (m_Stream == NkConsoleStream::NK_STD_OUT) ? stdout : stderr;
    }

    /**
     * @brief Vérifie si la console supporte les couleurs
     */
    bool NkConsoleSink::SupportsColors() const {
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
        // Les codes ANSI n'ont pas d'utilité dans logcat.
        return false;
#elif defined(_WIN32)
        // Windows: vérifier si la console supporte les couleurs
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
        // Unix: vérifier si c'est un terminal et supporte les couleurs
        static bool checked = false;
        static bool supports = false;

        if (!checked) {
            // Vérifier si stdout est un terminal
            int fd = fileno(stdout);
            if (fd < 0) {
                supports = false;
            } else {
                // Vérifier si c'est un TTY
                if (isatty(fd)) {
                    // Vérifier la variable TERM
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
    NkString NkConsoleSink::GetColorCode(NkLogLevel level) const {
        return NkLogLevelToANSIColor(level);
    }

    /**
     * @brief Obtient le code de réinitialisation de couleur
     */
    NkString NkConsoleSink::GetResetCode() const {
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
     * @brief Réinitialise la couleur Windows
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
