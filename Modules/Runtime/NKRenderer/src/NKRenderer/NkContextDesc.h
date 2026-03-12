#pragma once
// =============================================================================
// NkContextDesc.h
// Descripteur de création d'un contexte graphique.
//
// Passé à NkContextFactory::Create() pour instancier le bon backend.
// Chaque champ de la sous-structure est ignoré si l'API ne le concerne pas.
//
// Workflow :
//   1. Remplir NkContextDesc (api + sous-struct correspondant)
//   2. Pour OpenGL/GLX uniquement : appeler
//      NkContextFactory::PrepareWindowConfig(desc, windowConfig)
//      AVANT NkWindow::Create() — injecte les hints GLX dans la config.
//   3. Créer NkWindow normalement.
//   4. NkContextFactory::Create(desc, window) → NkIGraphicsContext*
// =============================================================================

#include "NkGraphicsApi.h"
#include "NKRenderer/Backends/OpenGL/NkOpenGLDesc.h"

namespace nkentseu {

    // -------------------------------------------------------------------------
    // Paramètres Vulkan
    // -------------------------------------------------------------------------
    struct NkVulkanDesc {
        const char*  appName         = "NkApp";
        uint32       appVersion      = 1;
        uint32       apiVersion      = 0; // 0 = auto-détect la version max disponible
        bool         validationLayers = false; // true en développement uniquement
        bool         debugMessenger   = false;
        int          msaaSamples      = 1;  // VkSampleCountFlagBits implicite
        bool         vsync            = true; // FIFO vs MAILBOX vs IMMEDIATE
        // Swapchain
        uint32       swapchainImages  = 2;  // 2 = double buffering, 3 = triple
        // Extensions supplémentaires demandées par l'application
        const char** extraInstanceExtensions = nullptr;
        uint32       extraInstanceExtCount   = 0;
        const char** extraDeviceExtensions   = nullptr;
        uint32       extraDeviceExtCount     = 0;
    };

    // -------------------------------------------------------------------------
    // Paramètres DirectX 11
    // -------------------------------------------------------------------------
    struct NkDirectX11Desc {
        bool         debugDevice    = false;  // D3D11_CREATE_DEVICE_DEBUG
        bool         vsync          = true;
        uint32       msaaSamples    = 1;
        uint32       msaaQuality    = 0;
        uint32       swapchainBuffers = 2;
        bool         allowTearing   = false;  // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
        // Feature level minimum accepté
        // D3D_FEATURE_LEVEL_11_0 par défaut
        uint32       minFeatureLevel = 0; // 0 = D3D_FEATURE_LEVEL_11_0
    };

    // -------------------------------------------------------------------------
    // Paramètres DirectX 12
    // -------------------------------------------------------------------------
    struct NkDirectX12Desc {
        bool         debugDevice      = false;
        bool         gpuValidation    = false;
        bool         vsync            = true;
        uint32       msaaSamples      = 1;
        uint32       swapchainBuffers = 3;    // Triple buffering recommandé pour DX12
        bool         allowTearing     = true;
        uint32       rtvDescriptorHeapSize = 256;
        uint32       dsvDescriptorHeapSize = 64;
        uint32       srvDescriptorHeapSize = 1024;
        uint32       samplerHeapSize       = 64;
        // Adapter préféré : 0 = premier GPU discret disponible
        uint32       preferredAdapterIndex = 0;
    };

    // -------------------------------------------------------------------------
    // Paramètres Metal
    // -------------------------------------------------------------------------
    struct NkMetalDesc {
        bool         validation     = false;  // MTLDevice capture
        bool         vsync          = true;
        uint32       sampleCount    = 1;       // MSAA
        // MTLPixelFormat implicite : BGRA8Unorm_sRGB
        bool         srgb           = true;
    };

    // -------------------------------------------------------------------------
    // Paramètres Software Renderer
    // -------------------------------------------------------------------------
    struct NkSoftwareDesc {
        bool   threading    = true;   // Rasterisation multi-thread (tiles)
        uint32 threadCount  = 0;      // 0 = std::thread::hardware_concurrency()
        bool   useSSE       = true;   // Optimisations SIMD si disponible
        // Format pixel du framebuffer
        // 0 = RGBA8, 1 = BGRA8 (Windows GDI compatible)
        uint32 pixelFormat  = 0;
    };

    // -------------------------------------------------------------------------
    // NkContextDesc — descripteur principal
    // -------------------------------------------------------------------------
    struct NkContextDesc {
        NkGraphicsApi  api = NkGraphicsApi::NK_API_NONE;

        // Une seule de ces structs est lue selon api.
        // Les autres sont ignorées.
        NkOpenGLDesc   opengl;
        NkVulkanDesc   vulkan;
        NkDirectX11Desc dx11;
        NkDirectX12Desc dx12;
        NkMetalDesc    metal;
        NkSoftwareDesc software;

        // --- Helpers de construction rapide ---
        static NkContextDesc MakeOpenGL(int major = 4, int minor = 6, bool debug = false) {
            NkContextDesc d;
            d.api                    = NkGraphicsApi::NK_API_OPENGL;
            d.opengl.majorVersion    = major;
            d.opengl.minorVersion    = minor;
            d.opengl.profile         = NkGLProfile::Core;
            d.opengl.contextFlags    = NkGLContextFlags::ForwardCompat |
                (debug ? NkGLContextFlags::Debug : NkGLContextFlags::NoneFlag);
            d.opengl.glad2.installDebugCallback = debug;
            return d;
        }

        static NkContextDesc MakeOpenGLES(int major = 3, int minor = 2) {
            NkContextDesc d;
            d.api                    = NkGraphicsApi::NK_API_OPENGLES;
            d.opengl.majorVersion    = major;
            d.opengl.minorVersion    = minor;
            d.opengl.profile         = NkGLProfile::ES;
            d.opengl.contextFlags    = NkGLContextFlags::NoneFlag;
            return d;
        }

        static NkContextDesc MakeVulkan(bool validation = false) {
            NkContextDesc d;
            d.api                       = NkGraphicsApi::NK_API_VULKAN;
            d.vulkan.validationLayers   = validation;
            d.vulkan.debugMessenger     = validation;
            return d;
        }

        static NkContextDesc MakeDirectX11(bool debug = false) {
            NkContextDesc d;
            d.api              = NkGraphicsApi::NK_API_DIRECTX11;
            d.dx11.debugDevice = debug;
            return d;
        }

        static NkContextDesc MakeDirectX12(bool debug = false) {
            NkContextDesc d;
            d.api              = NkGraphicsApi::NK_API_DIRECTX12;
            d.dx12.debugDevice = debug;
            return d;
        }

        static NkContextDesc MakeMetal() {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_API_METAL;
            return d;
        }

        static NkContextDesc MakeSoftware(bool threaded = true) {
            NkContextDesc d;
            d.api                    = NkGraphicsApi::NK_API_SOFTWARE;
            d.software.threading     = threaded;
            return d;
        }
    };

} // namespace nkentseu
