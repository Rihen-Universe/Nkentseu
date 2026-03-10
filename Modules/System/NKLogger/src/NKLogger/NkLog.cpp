// -----------------------------------------------------------------------------
// NkLog.cpp
// Default logger singleton implementation.
// -----------------------------------------------------------------------------

#if !defined(NK_LOG_HAS_ANDROID_LOG)
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define NK_LOG_HAS_ANDROID_LOG 1
#elif defined(__has_include)
#if __has_include(<android/log.h>)
#define NK_LOG_HAS_ANDROID_LOG 1
#else
#define NK_LOG_HAS_ANDROID_LOG 0
#endif
#else
#define NK_LOG_HAS_ANDROID_LOG 0
#endif
#endif

#if NK_LOG_HAS_ANDROID_LOG
#include "NKLogger/Sinks/NkAndroidSink.h"
#else
#include "NKLogger/Sinks/NkConsoleSink.h"
#endif

#include "NKLogger/NkLog.h"
#include "NKLogger/Sinks/NkFileSink.h"

namespace nkentseu {

bool NkLog::s_Initialized = false;

NkLog::NkLog(const NkString& name)
    : NkLogger(name) {
#if NK_LOG_HAS_ANDROID_LOG
    // Android: route runtime logs to logcat.
    memory::NkSharedPtr<NkISink> androidSink(
        new NkAndroidSink(name.Empty() ? NkString("Nkentseu") : name));
    AddSink(androidSink);
#else
    // Desktop: keep console output.
    NkConsoleSink* consoleSinkRaw = new NkConsoleSink();
    consoleSinkRaw->SetColorEnabled(true);
    memory::NkSharedPtr<NkISink> consoleSink(consoleSinkRaw);
    AddSink(consoleSink);
#endif

    // Keep file sink for persisted logs.
    memory::NkSharedPtr<NkISink> fileSink(new NkFileSink("logs/app.log"));
    AddSink(fileSink);

    SetLevel(NkLogLevel::NK_INFO);
    SetPattern(NkFormatter::NK_NKENTSEU_PATTERN);
}

NkLog::~NkLog() {
    Flush();
}

NkLog& NkLog::Instance() {
    static NkLog instance;
    s_Initialized = true;
    return instance;
}

void NkLog::Initialize(const NkString& name, const NkString& pattern, NkLogLevel level) {
    auto& instance = Instance();

    if (instance.GetName() != name && !name.Empty()) {
        instance.SetName(name);
    }

    instance.SetPattern(pattern);
    instance.SetLevel(level);
    s_Initialized = true;
}

void NkLog::Shutdown() {
    auto& instance = Instance();
    instance.Flush();
    instance.ClearSinks();
}

NkLog& NkLog::Named(const NkString& name) {
    SetName(name);

#if NK_LOG_HAS_ANDROID_LOG
    // Keep Android sink tag aligned with logger name.
    for (auto& sink : m_Sinks) {
        if (!sink) {
            continue;
        }
        if (auto* androidSink = dynamic_cast<NkAndroidSink*>(sink.Get())) {
            androidSink->SetTag(name);
        }
    }
#endif

    return *this;
}

NkLog& NkLog::Level(NkLogLevel level) {
    SetLevel(level);
    return *this;
}

NkLog& NkLog::Pattern(const NkString& pattern) {
    SetPattern(pattern);
    return *this;
}

NkLog& NkLog::Source(const char* sourceFile, uint32 sourceLine, const char* functionName) {
    NkLogger::Source(sourceFile, sourceLine, functionName);
    return *this;
}

} // namespace nkentseu
