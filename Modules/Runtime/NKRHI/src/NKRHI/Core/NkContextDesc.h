#pragma once
// NkContextDesc.h - Descripteur unifie de creation/configuration de device RHI
#include "NKRHI/Core/NkGraphicsApi.h"

namespace nkentseu {

    enum class NkGpuPreference : uint8 {
        NK_DEFAULT          = 0,
        NK_LOW_POWER        = 1,
        NK_HIGH_PERFORMANCE = 2,
    };

    enum class NkGpuVendor : uint8 {
        NK_ANY       = 0,
        NK_NVIDIA    = 1,
        NK_AMD       = 2,
        NK_INTEL     = 3,
        NK_ARM       = 4,
        NK_QUALCOMM  = 5,
        NK_APPLE     = 6,
        NK_MICROSOFT = 7,
    };

    struct NkGpuSelectionDesc {
        NkGpuPreference preference = NkGpuPreference::NK_DEFAULT;
        int32 adapterIndex = -1; // -1 = auto
        NkGpuVendor vendorPreference = NkGpuVendor::NK_ANY;
        bool allowSoftwareAdapter = true;
        bool enableOpenGLPlatformHints = true;
    };

    struct NkVulkanDesc {
        const char*  appName               = "NkApp";
        const char*  engineName            = "Unkeny";
        uint32       appVersion            = 1;
        uint32       apiVersion            = 0; // 0 = auto
        bool         validationLayers      = true;
        bool         debugMessenger        = true;
        bool         vsync                 = true;
        uint32       swapchainImages       = 3;
        int32        msaaSamples           = 1;
        uint32       preferredAdapterIndex = UINT32_MAX;
        bool         enableComputeQueue    = true;
        const char** extraInstanceExt      = nullptr;
        uint32       extraInstanceExtCount = 0;
        const char** extraDeviceExt        = nullptr;
        uint32       extraDeviceExtCount   = 0;
    };

    struct NkDirectX11Desc {
        bool   debugDevice      = false;
        bool   vsync            = true;
        bool   allowTearing     = false;
        uint32 msaaSamples      = 1;
        uint32 msaaQuality      = 0;
        uint32 swapchainBuffers = 2;
        uint32 preferredAdapter = UINT32_MAX;
        uint32 minFeatureLevel  = 0; // 0 = D3D_FEATURE_LEVEL_11_0
    };

    struct NkDirectX12Desc {
        bool   debugDevice        = false;
        bool   gpuValidation      = false;
        bool   vsync              = true;
        bool   allowTearing       = true;
        uint32 swapchainBuffers   = 3;
        uint32 rtvHeapSize        = 256;
        uint32 dsvHeapSize        = 64;
        uint32 srvHeapSize        = 1024;
        uint32 samplerHeapSize    = 64;
        uint32 preferredAdapter   = UINT32_MAX;
        bool   enableComputeQueue = true;
    };

    struct NkMetalDesc {
        bool   validation  = false;
        bool   vsync       = true;
        uint32 sampleCount = 1;
        bool   srgb        = true;
    };

    struct NkSoftwareDesc {
        bool   threading   = true;
        uint32 threadCount = 0; // 0 = hardware_concurrency
        bool   useSSE      = true;
        uint32 pixelFormat = 0; // 0 = RGBA8
    };

    struct NkComputeActivationDesc {
        bool enable = false;
        bool opengl = true;
        bool vulkan = true;
        bool directx11 = true;
        bool directx12 = true;
        bool metal = true;
        bool software = true;
    };

#if defined(NKENTSEU_PLATFORM_WINDOWS)

    enum class NkPFDFlags : uint32 {
        NK_PFD_DRAW_TO_WINDOW  = 0x00000004,
        NK_PFD_DRAW_TO_BITMAP  = 0x00000008,
        NK_PFD_SUPPORT_OPENGL  = 0x00000020,
        NK_PFD_DOUBLE_BUFFER   = 0x00000001,
        NK_PFD_STEREO          = 0x00000002,
        NK_PFD_SWAP_EXCHANGE   = 0x00000200,
        NK_PFD_SWAP_COPY       = 0x00000400,
        NK_PFD_DEFAULT         = 0x00000004 | 0x00000020 | 0x00000001,
        NK_PFD_DEFAULT_SINGLE  = 0x00000004 | 0x00000020
    };

    inline NkPFDFlags operator|(NkPFDFlags a, NkPFDFlags b) {
        return static_cast<NkPFDFlags>(uint32(a) | uint32(b));
    }

    enum class NkPFDPixelType : uint8 {
        NK_PFD_PIXEL_RGBA = 0,
        NK_PFD_PIXEL_COLOR_INDEX = 1
    };

    struct NkWGLFallbackPixelFormat {
        uint8          colorBits   = 32;
        uint8          alphaBits   = 8;
        uint8          depthBits   = 24;
        uint8          stencilBits = 8;
        uint8          accumBits   = 0;
        uint8          auxBuffers  = 0;
        uint16         version     = 1;
        NkPFDPixelType pixelType   = NkPFDPixelType::NK_PFD_PIXEL_RGBA;
        NkPFDFlags     flags       = NkPFDFlags::NK_PFD_DEFAULT;

        static NkWGLFallbackPixelFormat Minimal() {
            NkWGLFallbackPixelFormat f;
            f.colorBits = 24;
            f.alphaBits = 0;
            f.depthBits = 16;
            f.stencilBits = 0;
            return f;
        }
        static NkWGLFallbackPixelFormat Standard() { return {}; }
        static NkWGLFallbackPixelFormat HighPrecision() {
            NkWGLFallbackPixelFormat f;
            f.colorBits = 32;
            f.depthBits = 32;
            return f;
        }
        static NkWGLFallbackPixelFormat SingleBuffer() {
            NkWGLFallbackPixelFormat f;
            f.flags = NkPFDFlags::NK_PFD_DEFAULT_SINGLE;
            return f;
        }
    };

#else

    struct NkWGLFallbackPixelFormat {
        static NkWGLFallbackPixelFormat Minimal()       { return {}; }
        static NkWGLFallbackPixelFormat Standard()      { return {}; }
        static NkWGLFallbackPixelFormat HighPrecision() { return {}; }
        static NkWGLFallbackPixelFormat SingleBuffer()  { return {}; }
    };

#endif

    enum class NkGLProfile : uint32 {
        NK_GL_PROFILE_CORE = 0,
        NK_GL_PROFILE_COMPATIBILITY = 1,
        NK_GL_PROFILE_ES = 2
    };

    enum class NkGLContextFlags : uint32 {
        NK_GL_CONTEXT_FLAG_NONE = 0,
        NK_GL_CONTEXT_FLAG_DEBUG = 1 << 0,
        NK_GL_CONTEXT_FLAG_FORWARD_COMPAT = 1 << 1,
        NK_GL_CONTEXT_FLAG_ROBUST_ACCESS = 1 << 2,
        NK_GL_CONTEXT_FLAG_NO_ERROR = 1 << 3
    };

    inline NkGLContextFlags operator|(NkGLContextFlags a, NkGLContextFlags b) {
        return static_cast<NkGLContextFlags>(uint32(a) | uint32(b));
    }

    inline bool HasFlag(NkGLContextFlags s, NkGLContextFlags f) {
        return (uint32(s) & uint32(f)) != 0;
    }

    enum class NkGLSwapInterval : int32 {
        NK_GL_SWAP_IMMEDIATE = 0,
        NK_GL_SWAP_VSYNC = 1,
        NK_GL_SWAP_ADAPTIVE_VSYNC = -1
    };

    struct NkGLXHints {
        bool  stereoRendering   = false;
        bool  floatingPointFB   = false;
        bool  transparentWindow = false;
        bool  allowPixmap       = false;
        bool  allowPbuffer      = false;
        int32 redBits = 8;
        int32 greenBits = 8;
        int32 blueBits = 8;
        int32 alphaBits = 8;
    };

    struct NkEGLHints {
        int32 redBits = 8;
        int32 greenBits = 8;
        int32 blueBits = 8;
        int32 alphaBits = 8;
        bool  bindToTextureRGBA = false;
        bool  pbufferSurface    = false;
        bool  openGLConformant  = false;
    };

    struct NkOpenGLRuntimeOptions {
        bool   autoLoadEntryPoints = false;
        bool   validateVersion     = true;
        bool   installDebugCallback = true;
        uint32 debugSeverityLevel  = 2;
    };

    struct NkOpenGLDesc {
        int32            majorVersion = 4;
        int32            minorVersion = 6;
        NkGLProfile      profile      = NkGLProfile::NK_GL_PROFILE_CORE;
        NkGLContextFlags contextFlags = NkGLContextFlags::NK_GL_CONTEXT_FLAG_FORWARD_COMPAT;

        int32  colorBits      = 32;
        int32  redBits        = 8;
        int32  greenBits      = 8;
        int32  blueBits       = 8;
        int32  alphaBits      = 8;
        int32  depthBits      = 24;
        int32  stencilBits    = 8;
        int32  msaaSamples    = 0;
        bool   srgbFramebuffer = true;
        bool   doubleBuffer   = true;
        bool   accumBuffer    = false;

        NkGLSwapInterval swapInterval = NkGLSwapInterval::NK_GL_SWAP_VSYNC;
        NkOpenGLRuntimeOptions runtime;

        NkWGLFallbackPixelFormat wglFallback = NkWGLFallbackPixelFormat::Standard();
        NkGLXHints               glxHints;
        NkEGLHints               eglHints;

        static NkOpenGLDesc Desktop46(bool dbg = false) {
            NkOpenGLDesc d;
            d.majorVersion = 4;
            d.minorVersion = 6;
            d.profile = NkGLProfile::NK_GL_PROFILE_CORE;
            d.contextFlags = NkGLContextFlags::NK_GL_CONTEXT_FLAG_FORWARD_COMPAT |
                (dbg ? NkGLContextFlags::NK_GL_CONTEXT_FLAG_DEBUG : NkGLContextFlags::NK_GL_CONTEXT_FLAG_NONE);
            d.runtime.installDebugCallback = dbg;
            return d;
        }

        static NkOpenGLDesc Desktop33(bool dbg = false) {
            NkOpenGLDesc d;
            d.majorVersion = 3;
            d.minorVersion = 3;
            d.profile = NkGLProfile::NK_GL_PROFILE_CORE;
            d.contextFlags = NkGLContextFlags::NK_GL_CONTEXT_FLAG_FORWARD_COMPAT |
                (dbg ? NkGLContextFlags::NK_GL_CONTEXT_FLAG_DEBUG : NkGLContextFlags::NK_GL_CONTEXT_FLAG_NONE);
            return d;
        }

        static NkOpenGLDesc ES32() {
            NkOpenGLDesc d;
            d.majorVersion = 3;
            d.minorVersion = 2;
            d.profile = NkGLProfile::NK_GL_PROFILE_ES;
            d.contextFlags = NkGLContextFlags::NK_GL_CONTEXT_FLAG_NONE;
            d.runtime.installDebugCallback = false;
            return d;
        }
    };

    struct NkContextDesc {
        NkGraphicsApi   api = NkGraphicsApi::NK_GFX_API_NONE;
        NkOpenGLDesc    opengl;
        NkVulkanDesc    vulkan;
        NkDirectX11Desc dx11;
        NkDirectX12Desc dx12;
        NkMetalDesc     metal;
        NkSoftwareDesc  software;
        NkComputeActivationDesc compute;
        NkGpuSelectionDesc gpu;

        bool IsComputeEnabledForApi(NkGraphicsApi backendApi) const {
            if (!compute.enable) return false;
            switch (backendApi) {
                case NkGraphicsApi::NK_GFX_API_OPENGL:
                case NkGraphicsApi::NK_GFX_API_OPENGLES:
                    return compute.opengl;
                case NkGraphicsApi::NK_GFX_API_VULKAN:
                    return compute.vulkan;
                case NkGraphicsApi::NK_GFX_API_D3D11:
                    return compute.directx11;
                case NkGraphicsApi::NK_GFX_API_D3D12:
                    return compute.directx12;
                case NkGraphicsApi::NK_GFX_API_METAL:
                    return compute.metal;
                case NkGraphicsApi::NK_GFX_API_SOFTWARE:
                    return compute.software;
                default:
                    return false;
            }
        }

        static NkContextDesc MakeOpenGL(int maj = 4, int min = 6, bool dbg = false) {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_OPENGL;
            d.opengl = NkOpenGLDesc::Desktop46(dbg);
            d.opengl.majorVersion = maj;
            d.opengl.minorVersion = min;
            return d;
        }

        static NkContextDesc MakeOpenGLES(int maj = 3, int min = 2) {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_OPENGLES;
            d.opengl = NkOpenGLDesc::ES32();
            d.opengl.majorVersion = maj;
            d.opengl.minorVersion = min;
            return d;
        }

        static NkContextDesc MakeVulkan(bool val = false) {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_VULKAN;
            d.vulkan.validationLayers = val;
            d.vulkan.debugMessenger = val;
            return d;
        }

        static NkContextDesc MakeDirectX11(bool dbg = false) {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_D3D11;
            d.dx11.debugDevice = dbg;
            return d;
        }

        static NkContextDesc MakeDirectX12(bool dbg = false) {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_D3D12;
            d.dx12.debugDevice = dbg;
            return d;
        }

        static NkContextDesc MakeMetal() {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_METAL;
            return d;
        }

        static NkContextDesc MakeSoftware(bool threaded = true) {
            NkContextDesc d;
            d.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
            d.software.threading = threaded;
            return d;
        }
    };

} // namespace nkentseu
