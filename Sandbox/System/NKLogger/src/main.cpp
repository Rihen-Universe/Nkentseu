// =============================================================================
// FICHIER  : Sandbox/System/NKLogger/src/main.cpp
// PROJET   : SandboxNKLogger
// OBJET    : Validation compile + execution du module NKLogger
// =============================================================================

#include "NKLogger/NkRegistry.h"
#include "NKLogger/NkLog.h"
#include "NKLogger/NkLogLevel.h"

using namespace nkentseu;

int main() {
	NkLog::Initialize(
		"sandbox-logger",
		NkLoggerFormatter::NK_DETAILED_PATTERN,
		NkLogLevel::NK_DEBUG
	);

	logger.Info("SandboxNKLogger start");
	logger.Warn("value={0}", 42);

	memory::NkSharedPtr<NkLogger> core = CreateLogger("core-sandbox");

	if (core) {
		core->Info("Core logger ready");
	}

	const char* lvlText = NkLogLevelToString(NkLogLevel::NK_INFO);
	logger.Info("[SandboxNKLogger] level={0}", lvlText);

	NkLog::Shutdown();
	return 0;
}
