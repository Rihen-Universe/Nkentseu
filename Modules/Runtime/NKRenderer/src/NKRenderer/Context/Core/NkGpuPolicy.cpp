#include "NkGpuPolicy.h"

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/Context/Core/NkGraphicsApi.h"

#include <cstdlib>
#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace nkentseu {
namespace {

    static bool IsOpenGLLike(NkGraphicsApi api) {
        return api == NkGraphicsApi::NK_API_OPENGL ||
               api == NkGraphicsApi::NK_API_OPENGLES ||
               api == NkGraphicsApi::NK_API_WEBGL;
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
        case NkGpuPreference::NK_DEFAULT:         return "Default";
        case NkGpuPreference::NK_LOW_POWER:       return "LowPower";
        case NkGpuPreference::NK_HIGH_PERFORMANCE: return "HighPerformance";
        default: return "Unknown";
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
        default: return "Unknown";
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
        default:                     return false;
    }
}

void NkGpuPolicy::ApplyPreContext(const NkContextDesc& desc) {
    logger.Infof(
        "[NkGpuPolicy] api=%s pref=%s vendor=%s adapterIndex=%d",
        NkGraphicsApiName(desc.api),
        PreferenceName(desc.gpu.preference),
        VendorName(desc.gpu.vendorPreference),
        (int)desc.gpu.adapterIndex);

    if (!IsOpenGLLike(desc.api) || !desc.gpu.enableOpenGLPlatformHints) {
        return;
    }

    if (desc.gpu.adapterIndex >= 0) {
        logger.Warnf("[NkGpuPolicy] OpenGL explicit adapter index is not portable; using OS/driver selection hints.");
    }

#if defined(NKENTSEU_PLATFORM_LINUX)
    if (desc.gpu.preference == NkGpuPreference::NK_HIGH_PERFORMANCE) {
        // Mesa/Intel/AMD: PRIME render offload.
        SetEnvIfUnset("DRI_PRIME", "1");
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        // NVIDIA PRIME offload on many X11 setups.
        SetEnvIfUnset("__NV_PRIME_RENDER_OFFLOAD", "1");
        SetEnvIfUnset("__GLX_VENDOR_LIBRARY_NAME", "nvidia");
#endif
        logger.Infof("[NkGpuPolicy] OpenGL high-performance hints set (DRI_PRIME / PRIME offload).");
    } else if (desc.gpu.preference == NkGpuPreference::NK_LOW_POWER) {
        SetEnvIfUnset("DRI_PRIME", "0");
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        UnsetEnvVar("__NV_PRIME_RENDER_OFFLOAD");
        UnsetEnvVar("__GLX_VENDOR_LIBRARY_NAME");
#endif
        logger.Infof("[NkGpuPolicy] OpenGL low-power hints set (DRI_PRIME=0).");
    }
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
    if (desc.gpu.preference == NkGpuPreference::NK_HIGH_PERFORMANCE) {
        logger.Infof("[NkGpuPolicy] Windows OpenGL high-performance mode: prefer OS Graphics settings / NVIDIA / AMD app profile.");
    } else if (desc.gpu.preference == NkGpuPreference::NK_LOW_POWER) {
        logger.Infof("[NkGpuPolicy] Windows OpenGL low-power mode: prefer OS Graphics settings app profile.");
    }
#elif defined(NKENTSEU_PLATFORM_MACOS)
    logger.Infof("[NkGpuPolicy] macOS OpenGL uses OS policy (automatic graphics switching / power settings).");
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_IOS)
    logger.Infof("[NkGpuPolicy] Mobile OpenGL ES does not expose explicit GPU selection.");
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    logger.Infof("[NkGpuPolicy] WebGL GPU is selected by browser/OS.");
#endif
}

} // namespace nkentseu
