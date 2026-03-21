#pragma once
// NkContextDesc.h — Descripteur unifié création de contexte
#include "NkGraphicsApi.h"
#include "NkOpenGLDesc.h"

namespace nkentseu {

    struct NkVulkanDesc {
        const char*  appName               = "NkApp";
        uint32       appVersion            = 1;
        uint32       apiVersion            = 0;         // 0=auto VK_API_VERSION_1_3
        bool         validationLayers      = false;
        bool         debugMessenger        = false;
        bool         vsync                 = true;
        uint32       swapchainImages       = 3;
        int32        msaaSamples           = 1;
        uint32       preferredAdapterIndex = 0;
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
        uint32 minFeatureLevel  = 0; // 0=D3D_FEATURE_LEVEL_11_0
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
        uint32 preferredAdapter   = 0;
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
        uint32 threadCount = 0;    // 0=hardware_concurrency
        bool   useSSE      = true;
        uint32 pixelFormat = 0;    // 0=RGBA8
    };

    struct NkContextDesc {
        NkGraphicsApi   api      = NkGraphicsApi::NK_API_NONE;
        NkOpenGLDesc    opengl;
        NkVulkanDesc    vulkan;
        NkDirectX11Desc dx11;
        NkDirectX12Desc dx12;
        NkMetalDesc     metal;
        NkSoftwareDesc  software;

        static NkContextDesc MakeOpenGL(int maj=4,int min=6,bool dbg=false){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_OPENGL;
            d.opengl=NkOpenGLDesc::Desktop46(dbg);
            d.opengl.majorVersion=maj; d.opengl.minorVersion=min; return d;
        }
        static NkContextDesc MakeOpenGLES(int maj=3,int min=2){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_OPENGLES;
            d.opengl=NkOpenGLDesc::ES32();
            d.opengl.majorVersion=maj; d.opengl.minorVersion=min; return d;
        }
        static NkContextDesc MakeVulkan(bool val=false){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_VULKAN;
            d.vulkan.validationLayers=val; d.vulkan.debugMessenger=val; return d;
        }
        static NkContextDesc MakeDirectX11(bool dbg=false){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_DIRECTX11;
            d.dx11.debugDevice=dbg; return d;
        }
        static NkContextDesc MakeDirectX12(bool dbg=false){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_DIRECTX12;
            d.dx12.debugDevice=dbg; return d;
        }
        static NkContextDesc MakeMetal(){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_METAL; return d;
        }
        static NkContextDesc MakeSoftware(bool threaded=true){
            NkContextDesc d; d.api=NkGraphicsApi::NK_API_SOFTWARE;
            d.software.threading=threaded; return d;
        }
    };

} // namespace nkentseu