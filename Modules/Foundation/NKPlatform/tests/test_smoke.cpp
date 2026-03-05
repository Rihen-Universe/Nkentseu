#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformConfig.h"

using namespace nkentseu::platform;

TEST_CASE(NKPlatformSmoke, PlatformConfigAndCapabilities) {
    const PlatformConfig& cfg = GetPlatformConfig();
    const PlatformCapabilities& caps = GetPlatformCapabilities();

    ASSERT_NOT_NULL(cfg.platformName);
    ASSERT_NOT_NULL(cfg.archName);
    ASSERT_NOT_NULL(cfg.compilerName);
    ASSERT_TRUE(cfg.maxPathLength > 0);
    ASSERT_TRUE(caps.processorCount >= 0);
    ASSERT_TRUE(caps.logicalProcessorCount >= 0);
}

TEST_CASE(NKPlatformSmoke, BuildAndFeatureFlags) {
    const PlatformConfig& cfg = GetPlatformConfig();
    ASSERT_TRUE(cfg.isDebugBuild || cfg.isReleaseBuild);
    ASSERT_TRUE(cfg.maxPathLength > 0);
    ASSERT_TRUE(cfg.cacheLineSize > 0);
}
