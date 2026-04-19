// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkLogMessage.cpp
// DESCRIPTION: Implémentation de la structure NkLogMessage.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkSync.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include <ctime>
#if !defined(_WIN32)
#   include <time.h>
#else
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

namespace {
    static inline uint64_t NkGetNowNs() {
#if defined(_WIN32)
        FILETIME fileTime{};
        ::GetSystemTimeAsFileTime(&fileTime);

        ULARGE_INTEGER ticks{};
        ticks.LowPart = fileTime.dwLowDateTime;
        ticks.HighPart = fileTime.dwHighDateTime;

        constexpr uint64_t kWinToUnixEpochOffsetNs = 11644473600ULL * 1000000000ULL;
        const uint64_t nowNs = ticks.QuadPart * 100ULL; // 100ns ticks
        return (nowNs >= kWinToUnixEpochOffsetNs) ? (nowNs - kWinToUnixEpochOffsetNs) : 0ULL;
#else
        timespec ts{};
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
            return (static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL) + static_cast<uint64_t>(ts.tv_nsec);
        return 0;
#endif
    }
} // namespace

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // IMPLÉMENTATION DE NkLogMessage
    // -------------------------------------------------------------------------

    /**
     * @brief Constructeur par défaut
     */
    NkLogMessage::NkLogMessage()
        : timestamp(0), threadId(0), sourceLine(0), level(NkLogLevel::NK_INFO) {
        timestamp = static_cast<uint64>(NkGetNowNs());

        // Obtention de l'ID du thread
        threadId = static_cast<uint32>(loggersync::GetCurrentThreadId());
    }

    /**
     * @brief Constructeur avec niveau et message
     */
    NkLogMessage::NkLogMessage(NkLogLevel lvl, const NkString &msg, const NkString &logger) : NkLogMessage() {
        level = lvl;
        message = msg;
        if (!logger.Empty()) {
            loggerName = logger;
        }
    }

    /**
     * @brief Constructeur avec informations complètes
     */
    NkLogMessage::NkLogMessage(NkLogLevel lvl, const NkString &msg, const NkString &file, uint32 line,
                        const NkString &func, const NkString &logger)
        : NkLogMessage(lvl, msg, logger) {
        if (!file.Empty())
            sourceFile = file;
        if (line > 0)
            sourceLine = line;
        if (!func.Empty())
            functionName = func;
    }

    /**
     * @brief Réinitialise le message
     */
    void NkLogMessage::Reset() {
        timestamp = 0;
        threadId = 0;
        threadName.Clear();
        level = NkLogLevel::NK_INFO;
        message.Clear();
        loggerName.Clear();
        sourceFile.Clear();
        sourceLine = 0;
        functionName.Clear();

        timestamp = static_cast<uint64>(NkGetNowNs());

        // Recalcul de l'ID du thread
        threadId = static_cast<uint32>(loggersync::GetCurrentThreadId());
    }

    /**
     * @brief Vérifie si le message est valide
     */
    bool NkLogMessage::IsValid() const {
        return !message.Empty() && timestamp > 0;
    }

    /**
     * @brief Obtient l'heure sous forme de structure tm
     */
    tm NkLogMessage::GetLocalTime() const {
        time_t time = static_cast<time_t>(timestamp / 1000000000ULL);
        tm localTime{};

    #ifdef _WIN32
        localtime_s(&localTime, &time);
    #else
        localtime_r(&time, &localTime);
    #endif

        return localTime;
    }

    /**
     * @brief Obtient l'heure sous forme de structure tm (UTC)
     */
    tm NkLogMessage::GetUTCTime() const {
        time_t time = static_cast<time_t>(timestamp / 1000000000ULL);
        tm utcTime{};

    #ifdef _WIN32
        gmtime_s(&utcTime, &time);
    #else
        gmtime_r(&time, &utcTime);
    #endif

        return utcTime;
    }

    /**
     * @brief Obtient le timestamp en millisecondes
     */
    uint64 NkLogMessage::GetMillis() const {
        return timestamp / 1000000ULL;
    }

    /**
     * @brief Obtient le timestamp en microsecondes
     */
    uint64 NkLogMessage::GetMicros() const {
        return timestamp / 1000ULL;
    }

    /**
     * @brief Obtient le timestamp en secondes
     */
    double NkLogMessage::GetSeconds() const {
        return static_cast<double>(timestamp) / 1000000000.0;
    }

} // namespace nkentseu
