#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"

#include <vulkan/vulkan.h>

using namespace nkentseu;

// =============================================================================
// Point d'entrée — nkmain (appelé par NkMetalEntryPoint.mm sur Apple,
//                           ou directement depuis main() sur Windows/Linux)
// =============================================================================
int nkmain(const NkEntryState& state) {

    // -------------------------------------------------------------------------
    // 2. FenAtre
    // -------------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "Vulkna-TP01";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[Sandbox] Window creation FAILED");
        NkContextShutdown();
        return -2;
    }

    NkSurfaceDesc& surf = window.GetSurfaceDesc();
    surf.

    // -------------- creation de l';instance
    VkInstance               instance        = VK_NULL_HANDLE;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "TP01";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Unkeny";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    

    NkVector<const char*> extensions;
    extensions.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    extensions.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_WINDOWING_XLIB)
    extensions.PushBack(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_WINDOWING_XCB)
    extensions.PushBack(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    extensions.PushBack(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    extensions.PushBack(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_PLATFORM_MACOS)
    extensions.PushBack(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    extensions.PushBack(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.PushBack(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
    if (d.debugMessenger)
        extensions.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    for (uint32 i = 0; i < d.extraInstanceExtCount; ++i)
        extensions.PushBack(d.extraInstanceExt[i]);

    NkVector<const char*> layers;
    if (d.validationLayers) {
        layers.PushBack("VK_LAYER_KHRONOS_validation");
        logger.Infof("[NkVulkan] Validation layers enabled");  // ← AJOUTEZ
    }

    VkInstanceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo        = &appInfo;
    info.enabledExtensionCount   = (uint32)extensions.Size();
    info.ppEnabledExtensionNames = extensions.Data();
    info.enabledLayerCount       = (uint32)layers.Size();
    info.ppEnabledLayerNames     = layers.Data();
#if defined(NKENTSEU_PLATFORM_MACOS)
    info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    NK_VK_CHECK(vkCreateInstance(&info, nullptr, &instance));

    // -------------------------------------------------------------------------
    // 5. Boucle principale
    // -------------------------------------------------------------------------
    auto& eventSystem = NkEvents();

    bool running = true;
    float timeSeconds = 0.f;
    NkChrono chrono;
    NkElapsedTime elapsed;

    while (running)
    {
        NkElapsedTime e = chrono.Reset();

        // --- Pattern A : Dispatcher typA (OnEvent pour chaque event)
        while (NkEvent* event = eventSystem.PollEvent())
        {
            if (auto wcl = event->As<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
        }

        if (!running) break;

        // --- Cap 60 fps ---
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }
    window.Close();
    return 0;
}