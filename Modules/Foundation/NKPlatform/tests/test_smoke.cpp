#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformConfig.h"
#include "NKPlatform/NkCPUFeatures.h"
#include "NKPlatform/NkEndianness.h"

#if defined(__has_include)
    #if __has_include(<bit>)
        #include <bit>
        #define NK_PLATFORM_TEST_HAS_STD_ENDIAN 1
    #else
        #define NK_PLATFORM_TEST_HAS_STD_ENDIAN 0
    #endif
#else
    #define NK_PLATFORM_TEST_HAS_STD_ENDIAN 0
#endif

using namespace nkentseu::platform;

TEST_CASE(NKPlatformSmoke, PlatformConfigAndCapabilities) {
    const PlatformConfig& cfg = GetPlatformConfig();
    const PlatformCapabilities& caps = GetPlatformCapabilities();

    ASSERT_NOT_NULL(cfg.platformName);
    ASSERT_NOT_NULL(cfg.archName);
    ASSERT_NOT_NULL(cfg.compilerName);
    ASSERT_TRUE(cfg.maxPathLength > 0);
    ASSERT_TRUE(caps.pageSize > 0);
    ASSERT_TRUE(caps.processorCount > 0);
    ASSERT_TRUE(caps.logicalProcessorCount > 0);
}

TEST_CASE(NKPlatformSmoke, BuildAndFeatureFlags) {
    const PlatformConfig& cfg = GetPlatformConfig();
    ASSERT_TRUE(cfg.isDebugBuild || cfg.isReleaseBuild);
    ASSERT_TRUE(cfg.maxPathLength > 0);
    ASSERT_TRUE(cfg.cacheLineSize > 0);
}

TEST_CASE(NKPlatformSmoke, CpuFeatureSnapshot) {
    const NkCPUFeatures& cpu = NkGetCPUFeatures();

    ASSERT_TRUE(cpu.vendor[0] != '\0');
    ASSERT_TRUE(cpu.brand[0] != '\0');
    ASSERT_TRUE(cpu.cache.lineSize > 0);
    ASSERT_TRUE(cpu.topology.numLogicalCores > 0);
    ASSERT_TRUE(cpu.topology.numPhysicalCores > 0);

    ASSERT_EQUAL(cpu.cache.lineSize, NkGetCacheLineSize());
    ASSERT_EQUAL(cpu.topology.numPhysicalCores, NkGetPhysicalCoreCount());
    ASSERT_EQUAL(cpu.topology.numLogicalCores, NkGetLogicalCoreCount());
}

TEST_CASE(NKPlatformSmoke, EndiannessAndByteSwap) {
    const NkEndianness compileEndian = NkGetCompileTimeEndianness();
    const NkEndianness runtimeEndian = NkGetRuntimeEndianness();

    ASSERT_TRUE(compileEndian == NkEndianness::NK_LITTLE ||
                compileEndian == NkEndianness::NK_BIG ||
                compileEndian == NkEndianness::NK_UNKNOWN);

    if (compileEndian != NkEndianness::NK_UNKNOWN) {
        ASSERT_EQUAL(static_cast<int>(compileEndian), static_cast<int>(runtimeEndian));
    }

    ASSERT_EQUAL(NkIsLittleEndian(), compileEndian == NkEndianness::NK_LITTLE);
    ASSERT_EQUAL(NkIsBigEndian(), compileEndian == NkEndianness::NK_BIG);

    ASSERT_EQUAL(static_cast<uint16_t>(0x3412u), NkByteSwap16(static_cast<uint16_t>(0x1234u)));
    ASSERT_EQUAL(static_cast<uint32_t>(0x78563412u), NkByteSwap32(static_cast<uint32_t>(0x12345678u)));
    ASSERT_EQUAL(static_cast<uint64_t>(0x8877665544332211ULL),
                 NkByteSwap64(static_cast<uint64_t>(0x1122334455667788ULL)));

    const uint32_t host32 = static_cast<uint32_t>(0x12345678u);
    ASSERT_EQUAL(host32, NkNetworkToHost32(NkHostToNetwork32(host32)));

#if NK_PLATFORM_TEST_HAS_STD_ENDIAN && defined(__cpp_lib_endian)
    if (std::endian::native == std::endian::little) {
        ASSERT_EQUAL(static_cast<int>(NkEndianness::NK_LITTLE), static_cast<int>(compileEndian));
    } else if (std::endian::native == std::endian::big) {
        ASSERT_EQUAL(static_cast<int>(NkEndianness::NK_BIG), static_cast<int>(compileEndian));
    }
#endif
}
