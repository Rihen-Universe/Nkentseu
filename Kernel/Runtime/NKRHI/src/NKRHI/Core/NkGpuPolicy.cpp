#include "NKRHI/Core/NkGpuPolicy.h"

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#include <cstdlib>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace nkentseu {
namespace {

static bool IsOpenGLLike(NkGraphicsApi api) {
    return api == NkGraphicsApi::NK_GFX_API_OPENGL ||
           api == NkGraphicsApi::NK_GFX_API_OPENGLES ||
           api == NkGraphicsApi::NK_GFX_API_WEBGL;
}

static bool HasEnvVar(const char* key) {
    const char* v = std::getenv(key);
    return v && *v;
}

static void SetEnvIfUnset(const char* key, const char* value) {
    if (!key || !*key || !value) return;
    if (HasEnvVar(key)) return;
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    _putenv_s(key, value);
#else
    setenv(key, value, 0);
#endif
}

static void UnsetEnvVar(const char* key) {
    if (!key || !*key) return;
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
}

} // namespace

const char* NkGpuPolicy::PreferenceName(NkGpuPreference preference) {
    switch (preference) {
        case NkGpuPreference::NK_DEFAULT:          return "Default";
        case NkGpuPreference::NK_LOW_POWER:        return "LowPower";
        case NkGpuPreference::NK_HIGH_PERFORMANCE: return "HighPerformance";
        default:                                   return "Unknown";
    }
}

const char* NkGpuPolicy::VendorName(NkGpuVendor vendor) {
    switch (vendor) {
        case NkGpuVendor::NK_ANY:       return "Any";
        case NkGpuVendor::NK_NVIDIA:    return "Nvidia";
        case NkGpuVendor::NK_AMD:       return "AMD";
        case NkGpuVendor::NK_INTEL:     return "Intel";
        case NkGpuVendor::NK_APPLE:     return "Apple";
        case NkGpuVendor::NK_QUALCOMM:  return "Qualcomm";
        case NkGpuVendor::NK_ARM:       return "ARM";
        case NkGpuVendor::NK_MICROSOFT: return "Microsoft";
        default:                        return "Unknown";
    }
}

bool NkGpuPolicy::MatchesVendorPciId(uint32 pciVendorId, NkGpuVendor vendor) {
    switch (vendor) {
        case NkGpuVendor::NK_ANY:       return true;
        case NkGpuVendor::NK_NVIDIA:    return pciVendorId == 0x10DEu;
        case NkGpuVendor::NK_AMD:       return pciVendorId == 0x1002u || pciVendorId == 0x1022u;
        case NkGpuVendor::NK_INTEL:     return pciVendorId == 0x8086u;
        case NkGpuVendor::NK_APPLE:     return pciVendorId == 0x106Bu;
        case NkGpuVendor::NK_QUALCOMM:  return pciVendorId == 0x5143u;
        case NkGpuVendor::NK_ARM:       return pciVendorId == 0x13B5u;
        case NkGpuVendor::NK_MICROSOFT: return pciVendorId == 0x1414u;
        default:                        return false;
    }
}

void NkGpuPolicy::ApplyPreContext(const NkContextDesc& desc) {
    logger_src.Infof(
        "[NkRHI_GpuPolicy] api=%s pref=%s vendor=%s adapterIndex=%d\n",
        NkGraphicsApiName(desc.api),
        PreferenceName(desc.gpu.preference),
        VendorName(desc.gpu.vendorPreference),
        (int)desc.gpu.adapterIndex);

    if (!IsOpenGLLike(desc.api) || !desc.gpu.enableOpenGLPlatformHints) {
        return;
    }

#if defined(NKENTSEU_PLATFORM_LINUX)
    if (desc.gpu.preference == NkGpuPreference::NK_HIGH_PERFORMANCE) {
        SetEnvIfUnset("DRI_PRIME", "1");
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        SetEnvIfUnset("__NV_PRIME_RENDER_OFFLOAD", "1");
        SetEnvIfUnset("__GLX_VENDOR_LIBRARY_NAME", "nvidia");
#endif
        logger_src.Infof("[NkRHI_GpuPolicy] OpenGL high-performance hints enabled\n");
    } else if (desc.gpu.preference == NkGpuPreference::NK_LOW_POWER) {
        SetEnvIfUnset("DRI_PRIME", "0");
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        UnsetEnvVar("__NV_PRIME_RENDER_OFFLOAD");
        UnsetEnvVar("__GLX_VENDOR_LIBRARY_NAME");
#endif
        logger_src.Infof("[NkRHI_GpuPolicy] OpenGL low-power hints enabled\n");
    }
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
    if (desc.gpu.preference == NkGpuPreference::NK_HIGH_PERFORMANCE) {
        logger_src.Infof("[NkRHI_GpuPolicy] OpenGL high-performance mode requested\n");
    } else if (desc.gpu.preference == NkGpuPreference::NK_LOW_POWER) {
        logger_src.Infof("[NkRHI_GpuPolicy] OpenGL low-power mode requested\n");
    }
#elif defined(NKENTSEU_PLATFORM_MACOS)
    logger_src.Infof("[NkRHI_GpuPolicy] macOS selects GPU through OS policy\n");
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_IOS)
    logger_src.Infof("[NkRHI_GpuPolicy] Mobile OpenGL ES does not expose explicit GPU selection\n");
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    logger_src.Infof("[NkRHI_GpuPolicy] WebGL GPU selection is browser/OS controlled\n");
#endif
}

} // namespace nkentseu

