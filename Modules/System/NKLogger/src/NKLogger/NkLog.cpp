// -----------------------------------------------------------------------------
// NkLog.cpp
// Default logger singleton implementation.
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkConsoleSink.h"

#include "NKLogger/NkLog.h"
#include "NKLogger/Sinks/NkFileSink.h"

namespace nkentseu {

bool NkLog::s_Initialized = false;

NkLog::NkLog(const NkString& name)
    : NkLogger(name) {
    // Console sink on all platforms.
    // On Android, NkConsoleSink routes to logcat with platform guards.
    NkConsoleSink* consoleSinkRaw = new NkConsoleSink();
    consoleSinkRaw->SetColorEnabled(true);
    memory::NkSharedPtr<NkISink> consoleSink(consoleSinkRaw);
    AddSink(consoleSink);

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
