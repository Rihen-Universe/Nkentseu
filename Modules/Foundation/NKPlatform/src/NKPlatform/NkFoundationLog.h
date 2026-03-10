// -----------------------------------------------------------------------------
// FICHIER: NkFoundationLog.h
// DESCRIPTION: Logging ultra-leger pour la couche Foundation (sans NKLogger).
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_PLATFORM_NKFOUNDATIONLOG_H_INCLUDED
#define NKENTSEU_PLATFORM_NKFOUNDATIONLOG_H_INCLUDED

#include "NKPlatform/NkPlatformDetect.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if !defined(NK_FOUNDATION_HAS_ANDROID_LOG)
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define NK_FOUNDATION_HAS_ANDROID_LOG 1
#elif defined(__has_include)
#if __has_include(<android/log.h>)
#define NK_FOUNDATION_HAS_ANDROID_LOG 1
#else
#define NK_FOUNDATION_HAS_ANDROID_LOG 0
#endif
#else
#define NK_FOUNDATION_HAS_ANDROID_LOG 0
#endif
#endif

#if NK_FOUNDATION_HAS_ANDROID_LOG
#if defined(logger)
#pragma push_macro("logger")
#undef logger
#define NK_FOUNDATION_RESTORE_LOGGER_MACRO
#endif
#include <android/log.h>
#if defined(NK_FOUNDATION_RESTORE_LOGGER_MACRO)
#pragma pop_macro("logger")
#undef NK_FOUNDATION_RESTORE_LOGGER_MACRO
#endif
#endif

// Niveaux de log Foundation (compile-time).
#define NK_FOUNDATION_LOG_LEVEL_NONE  0
#define NK_FOUNDATION_LOG_LEVEL_ERROR 1
#define NK_FOUNDATION_LOG_LEVEL_WARN  2
#define NK_FOUNDATION_LOG_LEVEL_INFO  3
#define NK_FOUNDATION_LOG_LEVEL_DEBUG 4
#define NK_FOUNDATION_LOG_LEVEL_TRACE 5

#ifndef NK_FOUNDATION_LOG_LEVEL
#if defined(_DEBUG) || defined(DEBUG) || defined(NKENTSEU_DEBUG)
#define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_DEBUG
#else
#define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_WARN
#endif
#endif

namespace nkentseu {
namespace platform {

// Sink utilisateur optionnel (override du backend de sortie).
using NkFoundationLogSink = void (*)(const char* level, const char* file, int line, const char* message);
inline NkFoundationLogSink gNkFoundationLogSink = nullptr;

inline void NkFoundationSetLogSink(NkFoundationLogSink sink) {
    gNkFoundationLogSink = sink;
}

inline NkFoundationLogSink NkFoundationGetLogSink() {
    return gNkFoundationLogSink;
}

namespace detail {

#if NK_FOUNDATION_HAS_ANDROID_LOG
inline int NkFoundationAndroidPriority(const char* level) {
    if (!level) {
        return ANDROID_LOG_INFO;
    }
    if (strcmp(level, "TRC") == 0) return ANDROID_LOG_VERBOSE;
    if (strcmp(level, "DBG") == 0) return ANDROID_LOG_DEBUG;
    if (strcmp(level, "WRN") == 0) return ANDROID_LOG_WARN;
    if (strcmp(level, "ERR") == 0) return ANDROID_LOG_ERROR;
    return ANDROID_LOG_INFO;
}
#endif

inline void NkFoundationLogWrite(const char* level, const char* file, int line, const char* message) {
    if (!message) {
        return;
    }

    char payload[1024];
    const int payloadLen = snprintf(payload, sizeof(payload), "%s", message);

    if (payloadLen <= 0) {
        return;
    }

    if (const NkFoundationLogSink sink = ::nkentseu::platform::NkFoundationGetLogSink()) {
        sink(level, file, line, payload);
        return;
    }

#if NK_FOUNDATION_HAS_ANDROID_LOG
    __android_log_print(
        NkFoundationAndroidPriority(level),
        "NkFoundation",
        "[%s] %s:%d %s",
        level ? level : "INF",
        file ? file : "<unknown>",
        line,
        payload);
#else
    fprintf(stderr, "[%s] %s:%d %s\n", level, file ? file : "<unknown>", line, payload);
#endif
}

template <typename... Args>
inline void NkFoundationLogWrite(const char* level, const char* file, int line, const char* fmt, Args... args) {
    if (!fmt) {
        return;
    }

    char payload[1024];
    const int payloadLen = snprintf(payload, sizeof(payload), fmt, args...);

    if (payloadLen <= 0) {
        return;
    }

    if (const NkFoundationLogSink sink = ::nkentseu::platform::NkFoundationGetLogSink()) {
        sink(level, file, line, payload);
        return;
    }

#if NK_FOUNDATION_HAS_ANDROID_LOG
    __android_log_print(
        NkFoundationAndroidPriority(level),
        "NkFoundation",
        "[%s] %s:%d %s",
        level ? level : "INF",
        file ? file : "<unknown>",
        line,
        payload);
#else
    fprintf(stderr, "[%s] %s:%d %s\n", level, file ? file : "<unknown>", line, payload);
#endif
}

// ---- Formatage valeur (extensible par l'utilisateur) -----------------------
//
// Pour un type custom T, definir:
//   int NKFoundationToString(const T& value, char* out, size_t outSize);
//
// Le log typed utilisera automatiquement cette surcharge (ADL).

inline int NkFoundationFormatValueDefault(const char* value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%s", value ? value : "");
}

inline int NkFoundationFormatValueDefault(char* value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%s", value ? value : "");
}

inline int NkFoundationFormatValueDefault(bool value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%s", value ? "true" : "false");
}

inline int NkFoundationFormatValueDefault(signed char value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%d", static_cast<int>(value));
}

inline int NkFoundationFormatValueDefault(unsigned char value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%u", static_cast<unsigned int>(value));
}

inline int NkFoundationFormatValueDefault(short value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%d", static_cast<int>(value));
}

inline int NkFoundationFormatValueDefault(unsigned short value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%u", static_cast<unsigned int>(value));
}

inline int NkFoundationFormatValueDefault(int value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%d", value);
}

inline int NkFoundationFormatValueDefault(unsigned int value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%u", value);
}

inline int NkFoundationFormatValueDefault(long value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%ld", value);
}

inline int NkFoundationFormatValueDefault(unsigned long value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%lu", value);
}

inline int NkFoundationFormatValueDefault(long long value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%lld", value);
}

inline int NkFoundationFormatValueDefault(unsigned long long value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%llu", value);
}

inline int NkFoundationFormatValueDefault(float value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%g", static_cast<double>(value));
}

inline int NkFoundationFormatValueDefault(double value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%g", value);
}

inline int NkFoundationFormatValueDefault(long double value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%Lg", value);
}

template <typename T>
inline int NkFoundationFormatValueDefault(T* value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%p", static_cast<void*>(value));
}

template <typename T>
inline int NkFoundationFormatValueDefault(const T* value, char* out, size_t outSize) {
    return snprintf(out, outSize, "%p", static_cast<const void*>(value));
}

template <typename T>
inline int NkFoundationFormatValueDefault(const T&, char* out, size_t outSize) {
    return snprintf(out, outSize, "<unformattable>");
}

template <typename T>
inline auto NkFoundationFormatValueImpl(const T& value, char* out, size_t outSize, int)
    -> decltype(NKFoundationToString(value, out, outSize), int()) {
    return NKFoundationToString(value, out, outSize);
}

template <typename T>
inline int NkFoundationFormatValueImpl(const T& value, char* out, size_t outSize, long) {
    return NkFoundationFormatValueDefault(value, out, outSize);
}

template <typename T>
inline const char* NkFoundationFormatValueCStr(const T& value, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return "";
    }
    const int written = NkFoundationFormatValueImpl(value, out, outSize, 0);
    if (written < 0) {
        out[0] = '\0';
    } else if (static_cast<size_t>(written) >= outSize) {
        out[outSize - 1] = '\0';
    }
    return out;
}

template <typename T>
inline void NkFoundationLogValue(const char* level, const char* file, int line, const char* label, const T& value) {
    char valueBuffer[256];
    const char* text = NkFoundationFormatValueCStr(value, valueBuffer, sizeof(valueBuffer));
    if (label && label[0] != '\0') {
        NkFoundationLogWrite(level, file, line, "%s=%s", label, text);
    } else {
        NkFoundationLogWrite(level, file, line, "%s", text);
    }
}

} // namespace detail
} // namespace platform
} // namespace nkentseu

#define NK_FOUNDATION_PRINT(fmt, ...)                                                                                     \
    do {                                                                                                                  \
        ::nkentseu::platform::detail::NkFoundationLogWrite("INF", __FILE__, __LINE__, (fmt), ##__VA_ARGS__);           \
    } while (0)
#define NK_FOUNDATION_SPRINT(buffer, bufferSize, fmt, ...) snprintf((buffer), (bufferSize), (fmt), ##__VA_ARGS__)

#define NK_FOUNDATION_LOG_ERROR(fmt, ...)                                                                                 \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_ERROR) {                                                  \
            ::nkentseu::platform::detail::NkFoundationLogWrite("ERR", __FILE__, __LINE__, (fmt), ##__VA_ARGS__);       \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_WARN(fmt, ...)                                                                                  \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_WARN) {                                                   \
            ::nkentseu::platform::detail::NkFoundationLogWrite("WRN", __FILE__, __LINE__, (fmt), ##__VA_ARGS__);       \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_INFO(fmt, ...)                                                                                  \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_INFO) {                                                   \
            ::nkentseu::platform::detail::NkFoundationLogWrite("INF", __FILE__, __LINE__, (fmt), ##__VA_ARGS__);       \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_DEBUG(fmt, ...)                                                                                 \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_DEBUG) {                                                  \
            ::nkentseu::platform::detail::NkFoundationLogWrite("DBG", __FILE__, __LINE__, (fmt), ##__VA_ARGS__);       \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_TRACE(fmt, ...)                                                                                 \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE) {                                                  \
            ::nkentseu::platform::detail::NkFoundationLogWrite("TRC", __FILE__, __LINE__, (fmt), ##__VA_ARGS__);       \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_ERROR_VALUE(label, value)                                                                       \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_ERROR) {                                                  \
            ::nkentseu::platform::detail::NkFoundationLogValue("ERR", __FILE__, __LINE__, (label), (value));           \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_WARN_VALUE(label, value)                                                                        \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_WARN) {                                                   \
            ::nkentseu::platform::detail::NkFoundationLogValue("WRN", __FILE__, __LINE__, (label), (value));           \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_INFO_VALUE(label, value)                                                                        \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_INFO) {                                                   \
            ::nkentseu::platform::detail::NkFoundationLogValue("INF", __FILE__, __LINE__, (label), (value));           \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_DEBUG_VALUE(label, value)                                                                       \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_DEBUG) {                                                  \
            ::nkentseu::platform::detail::NkFoundationLogValue("DBG", __FILE__, __LINE__, (label), (value));           \
        }                                                                                                                 \
    } while (0)

#define NK_FOUNDATION_LOG_TRACE_VALUE(label, value)                                                                       \
    do {                                                                                                                  \
        if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE) {                                                  \
            ::nkentseu::platform::detail::NkFoundationLogValue("TRC", __FILE__, __LINE__, (label), (value));           \
        }                                                                                                                 \
    } while (0)

#endif // NKENTSEU_PLATFORM_NKFOUNDATIONLOG_H_INCLUDED
