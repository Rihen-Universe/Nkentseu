// =============================================================================
// FICHIER  : Sandbox/System/NKLogger/src/main.cpp
// PROJET   : SandboxNKLogger
// OBJET    : Validation compile + execution du module NKLogger
// =============================================================================

#include "NKLogger/NkRegistry.h"
#include "NKLogger/NkLog.h"
#include "NKLogger/NkLogLevel.h"

#include <cstdio>

using namespace nkentseu;

int main() {
    NkLog::Initialize(
        "sandbox-logger",
        NkFormatter::NK_DETAILED_PATTERN,
        NkLogLevel::NK_DEBUG
    );

    logger.Info("SandboxNKLogger start");
    logger.Warn("value=%d", 42);

    memory::NkSharedPtr<NkLogger> core = CreateLogger("core-sandbox");
    if (core) {
        core->Info("Core logger ready");
    }

    const char* lvlText = NkLogLevelToString(NkLogLevel::NK_INFO);
    std::printf("[SandboxNKLogger] level=%s\n", lvlText);

    NkLog::Shutdown();
    return 0;
}
