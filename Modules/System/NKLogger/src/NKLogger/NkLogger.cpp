// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogger.cpp
// DESCRIPTION: Implémentation de la classe NkLogger principale.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkLogger.h"
#include "NKLogger/NkLogMessage.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstdarg>
#include <cstddef>
#include <ctime>
#include <cstdio>
#include <cstdint>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

namespace {

    static char NkMapLatinAccentC3(uint8_t c2) {
        switch (c2) {
            case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: return 'A';
            case 0x87: return 'C';
            case 0x88: case 0x89: case 0x8A: case 0x8B: return 'E';
            case 0x8C: case 0x8D: case 0x8E: case 0x8F: return 'I';
            case 0x91: return 'N';
            case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: return 'O';
            case 0x99: case 0x9A: case 0x9B: case 0x9C: return 'U';
            case 0x9D: return 'Y';
            case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: return 'a';
            case 0xA6: return 'a';
            case 0xA7: return 'c';
            case 0xA8: case 0xA9: case 0xAA: case 0xAB: return 'e';
            case 0xAC: case 0xAD: case 0xAE: case 0xAF: return 'i';
            case 0xB1: return 'n';
            case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: return 'o';
            case 0xB9: case 0xBA: case 0xBB: case 0xBC: return 'u';
            case 0xBD: case 0xBF: return 'y';
            default: return '\0';
        }
    }

    static NkString NkSanitizeLogText(const NkString& input) {
        NkString out;
        if (input.Empty()) {
            return out;
        }

        out.Reserve(input.Size());
        const usize n = input.Size();
        usize i = 0;
        while (i < n) {
            const uint8_t c = static_cast<uint8_t>(input[i]);

            if (c < 0x80) {
                if ((c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\t') {
                    out.PushBack(static_cast<char>(c));
                } else {
                    out.PushBack(' ');
                }
                ++i;
                continue;
            }

            if (c == 0xC2 && (i + 1) < n) {
                const uint8_t c2 = static_cast<uint8_t>(input[i + 1]);
                if (c2 == 0xA0) {
                    out.PushBack(' ');
                } else {
                    out.PushBack('?');
                }
                i += 2;
                continue;
            }

            if (c == 0xC3 && (i + 1) < n) {
                const uint8_t c2 = static_cast<uint8_t>(input[i + 1]);
                const char mapped = NkMapLatinAccentC3(c2);
                out.PushBack(mapped ? mapped : '?');
                i += 2;
                continue;
            }

            if (c == 0xC5 && (i + 1) < n) {
                const uint8_t c2 = static_cast<uint8_t>(input[i + 1]);
                if (c2 == 0x92) {
                    out.Append("OE");
                } else if (c2 == 0x93) {
                    out.Append("oe");
                } else {
                    out.PushBack('?');
                }
                i += 2;
                continue;
            }

            if (c == 0xE2 && (i + 2) < n) {
                const uint8_t c2 = static_cast<uint8_t>(input[i + 1]);
                const uint8_t c3 = static_cast<uint8_t>(input[i + 2]);
                if (c2 == 0x80) {
                    switch (c3) {
                        case 0x93: // en dash
                        case 0x94: // em dash
                        case 0x95: // bullet
                            out.PushBack('-');
                            break;
                        case 0x98: // left single quote
                        case 0x99: // right single quote
                            out.PushBack('\'');
                            break;
                        case 0x9C: // left double quote
                        case 0x9D: // right double quote
                            out.PushBack('"');
                            break;
                        case 0xA6: // ellipsis
                            out.Append("...");
                            break;
                        default:
                            out.PushBack('?');
                            break;
                    }
                } else {
                    out.PushBack('?');
                }
                i += 3;
                continue;
            }

            // Fallback for other UTF-8 sequences.
            if ((c & 0xE0) == 0xC0 && (i + 1) < n) {
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && (i + 2) < n) {
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && (i + 3) < n) {
                i += 4;
            } else {
                ++i;
            }
            out.PushBack('?');
        }

        return out;
    }

    static NkString NkSanitizeLogText(const char* input) {
        return input ? NkSanitizeLogText(NkString(input)) : NkString();
    }

} // namespace


    // -------------------------------------------------------------------------
    // IMPLÉMENTATION DE NkLogger
    // -------------------------------------------------------------------------

    /**
     * @brief Constructeur de logger avec nom
     * @param name Nom du logger
     */
    NkLogger::NkLogger(const NkString &name)
        : m_Name(name), m_Level(NkLogLevel::NK_INFO), m_Enabled(true), m_Formatter(memory::NkMakeUnique<NkFormatter>()) {
    }

    /**
     * @brief Destructeur du logger
     */
    NkLogger::~NkLogger() {
        Flush();
        ClearSinks();
    }

    /**
     * @brief Ajoute un sink au logger
     * @param sink Sink à ajouter (partagé)
     */
    void NkLogger::AddSink(memory::NkSharedPtr<NkISink> sink) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Sinks.PushBack(sink);
    }

    /**
     * @brief Supprime tous les sinks du logger
     */
    void NkLogger::ClearSinks() {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Sinks.Clear();
    }

    /**
     * @brief Obtient le nombre de sinks attachés
     * @return Nombre de sinks
     */
    usize NkLogger::GetSinkCount() const {
        loggersync::NkScopedLock lock(m_Mutex);
        return m_Sinks.Size();
    }

    /**
     * @brief Définit le formatter pour tous les sinks
     * @param formatter NkFormatter à utiliser
     */
    void NkLogger::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Formatter = traits::NkMove(formatter);
    }

    /**
     * @brief Définit le pattern de formatage
     * @param pattern Pattern à utiliser
     */
    void NkLogger::SetPattern(const NkString &pattern) {
        loggersync::NkScopedLock lock(m_Mutex);
        if (m_Formatter) {
            m_Formatter->SetPattern(pattern);
        }
    }

    /**
     * @brief Définit le niveau de log minimum
     * @param level Niveau minimum à logger
     */
    void NkLogger::SetLevel(NkLogLevel level) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Level = level;
    }

    /**
     * @brief Obtient le niveau de log courant
     * @return Niveau de log
     */
    NkLogLevel NkLogger::GetLevel() const {
        loggersync::NkScopedLock lock(m_Mutex);
        return m_Level;
    }

    /**
     * @brief Vérifie si un niveau devrait être loggé
     * @param level Niveau à vérifier
     * @return true si le niveau est >= niveau minimum, false sinon
     */
    bool NkLogger::ShouldLog(NkLogLevel level) const {
        if (!m_Enabled)
            return false;
        return level >= m_Level;
    }

    /**
     * @brief Log interne avec informations de source
     */
    void NkLogger::LogInternal(NkLogLevel level, const NkString &message, const char *sourceFile, uint32 sourceLine,
                            const char *functionName) {
        if (!m_Enabled || level < m_Level)
            return;

        // Création du message de log avec nom du logger
        NkLogMessage msg;
        msg.threadId = static_cast<uint32>(loggersync::GetCurrentThreadId());
        msg.level = level;
        msg.message = NkSanitizeLogText(message);
        msg.loggerName = NkSanitizeLogText(m_Name); // Nom du logger ajouté ici

        if (sourceFile)
            msg.sourceFile = NkSanitizeLogText(sourceFile);
        if (sourceLine > 0)
            msg.sourceLine = sourceLine;
        if (functionName)
            msg.functionName = NkSanitizeLogText(functionName);

    // Obtention du nom du thread si disponible
    // pthread_getname_np disponible sur Linux et macOS, mais pas Android < API 26
    #if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
        char threadName[256] = {0};
        if (pthread_getname_np(pthread_self(), threadName, sizeof(threadName)) == 0) {
            msg.threadName = NkSanitizeLogText(threadName);
        }
    #endif

        // Envoi à tous les sinks
        loggersync::NkScopedLock lock(m_Mutex);
        for (auto &sink : m_Sinks) {
            if (sink) {
                NkFormatter* sinkFormatter = sink->GetFormatter();
                if (sinkFormatter != nullptr && m_Formatter) {
                    sinkFormatter->SetPattern(m_Formatter->GetPattern());
                }
                sink->Log(msg);
            }
        }
    }

    /**
     * @brief Formatage variadique
     */
    NkString NkLogger::FormatString(const char *format, va_list args) {
        // Détermination de la taille nécessaire
        va_list argsCopy;
        va_copy(argsCopy, args);
        int size = ::vsnprintf(nullptr, 0, format, argsCopy);
        va_end(argsCopy);

        if (size < 0)
            return NkString();

        // Allocation et formatage
        NkString result(static_cast<usize>(size + 1), '\0');
        ::vsnprintf(&result[0], static_cast<size_t>(result.Size()), format, args);
        result.Resize(static_cast<usize>(size)); // Retirer le null terminator

        return result;
    }

    /**
     * @brief Log avec format string (style printf)
     */
    void NkLogger::Logf(NkLogLevel level, const char *format, ...) {
        if (!ShouldLog(level))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(level, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log avec format string et informations de source
     */
    void NkLogger::Logf(NkLogLevel level, const char *file, int line, const char *func, const char *format, ...) {
        if (!ShouldLog(level))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(level, message, file, line, func);
    }

    /**
     * @brief Log avec message string et informations de source
     */
    void NkLogger::Log(NkLogLevel level, const char *file, int line, const char *func, const NkString &message) {
        if (!ShouldLog(level))
            return;
        LogInternal(level, message, file, line, func);
    }

    /**
     * @brief Log avec format string, informations de source et va_list
     */
    void NkLogger::Logf(NkLogLevel level, const char *file, int line, const char *func, const char *format, va_list args) {
        if (!ShouldLog(level))
            return;

        NkString message = FormatString(format, args);
        LogInternal(level, message, file, line, func);
    }

    /**
     * @brief Log trace avec format string
     */
    void NkLogger::Tracef(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_TRACE))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log debug avec format string
     */
    void NkLogger::Debugf(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_DEBUG))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log info avec format string
     */
    void NkLogger::Infof(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_INFO))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log warning avec format string
     */
    void NkLogger::Warnf(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_WARN))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log error avec format string
     */
    void NkLogger::Errorf(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_ERROR))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log critical avec format string
     */
    void NkLogger::Criticalf(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_CRITICAL))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log fatal avec format string
     */
    void NkLogger::Fatalf(const char *format, ...) {
        if (!ShouldLog(NkLogLevel::NK_FATAL))
            return;

        va_list args;
        va_start(args, format);
        NkString message = FormatString(format, args);
        va_end(args);

        LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log avec stream style
     */
    void NkLogger::Log(NkLogLevel level, const NkString &message) {
        if (!ShouldLog(level))
            return;
        LogInternal(level, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log trace avec stream style
     */
    void NkLogger::Trace(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_TRACE))
            return;
        LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log debug avec stream style
     */
    void NkLogger::Debug(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_DEBUG))
            return;
        LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log info avec stream style
     */
    void NkLogger::Info(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_INFO))
            return;
        LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log warning avec stream style
     */
    void NkLogger::Warn(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_WARN))
            return;
        LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log error avec stream style
     */
    void NkLogger::Error(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_ERROR))
            return;
        LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log critical avec stream style
     */
    void NkLogger::Critical(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_CRITICAL))
            return;
        LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Log fatal avec stream style
     */
    void NkLogger::Fatal(const NkString &message) {
        if (!ShouldLog(NkLogLevel::NK_FATAL))
            return;
        LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
    }

    /**
     * @brief Force le flush de tous les sinks
     */
    void NkLogger::Flush() {
        loggersync::NkScopedLock lock(m_Mutex);
        for (auto &sink : m_Sinks) {
            if (sink) {
                sink->Flush();
            }
        }
    }

    /**
     * @brief Obtient le nom du logger
     * @return Nom du logger
     */
    const NkString &NkLogger::GetName() const {
        return m_Name;
    }

    /**
     * @brief Vérifie si le logger est actif
     * @return true si actif, false sinon
     */
    bool NkLogger::IsEnabled() const {
        return m_Enabled;
    }

    /**
     * @brief Active ou désactive le logger
     * @param enabled État d'activation
     */
    void NkLogger::SetEnabled(bool enabled) {
        loggersync::NkScopedLock lock(m_Mutex);
        m_Enabled = enabled;
    }
} // namespace nkentseu

