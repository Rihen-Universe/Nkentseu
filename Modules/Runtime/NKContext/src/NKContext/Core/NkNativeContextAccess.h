#pragma once
// =============================================================================
// NkNativeContextAccess.h
// Accès typé et sécurisé aux données natives de chaque contexte.
// Remplace les casts void* manuels partout dans le code.
//
// Usage :
//   auto* glData = NkNativeContext::OpenGL(ctx);
//   HGLRC hglrc  = NkNativeContext::GetWGLContext(ctx);
//   VkDevice dev = NkNativeContext::GetVkDevice(ctx);
// =============================================================================
#include "NkIGraphicsContext.h"
#include "NKContext/Graphics/OpenGL/NkOpenGLContextData.h"
#include "NKContext/Graphics/Vulkan/NkVulkanContextData.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "NKContext/Graphics/DirectX/NkDirectXContextData.h"
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   include "NKContext/Graphics/Metal/NkMetalContext.h"
#endif
#include "NKContext/Graphics/Software/NkSoftwareContext.h"

namespace nkentseu {

    // -------------------------------------------------------------------------
    // Helpers internes — retournent nullptr si l'API ne correspond pas
    // -------------------------------------------------------------------------
    template<typename T>
    static inline T* NkGetNativeAs(NkIGraphicsContext* ctx, NkGraphicsApi expected) {
        if (!ctx || ctx->GetApi() != expected) return nullptr;
        return static_cast<T*>(ctx->GetNativeContextData());
    }

    // =========================================================================
    struct NkNativeContext {

        // ── OpenGL ────────────────────────────────────────────────────────────
        static NkOpenGLContextData* OpenGL(NkIGraphicsContext* ctx) {
            if (!ctx) {
                return nullptr;
            }
            const NkGraphicsApi api = ctx->GetApi();
            if (api != NkGraphicsApi::NK_GFX_API_OPENGL &&
                api != NkGraphicsApi::NK_GFX_API_OPENGLES &&
                api != NkGraphicsApi::NK_GFX_API_WEBGL) {
                return nullptr;
            }
            return static_cast<NkOpenGLContextData*>(ctx->GetNativeContextData());
        }

        static NkOpenGLContextData::ProcAddressFn GetOpenGLProcAddressLoader(
            NkIGraphicsContext* ctx)
        {
            auto* d = OpenGL(ctx);
            if (d && d->getProcAddress) {
                return d->getProcAddress;
            }
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            if (ctx && ctx->GetApi() == NkGraphicsApi::NK_GFX_API_WEBGL) {
                static auto kWebGLFallbackLoader = +[](const char* name) -> void* {
                    return reinterpret_cast<void*>(emscripten_webgl_get_proc_address(name));
                };
                return kWebGLFallbackLoader;
            }
#endif
            return nullptr;
        }

        static void* GetOpenGLProcAddress(NkIGraphicsContext* ctx, const char* name) {
            auto fn = GetOpenGLProcAddressLoader(ctx);
            return (fn && name && *name) ? fn(name) : nullptr;
        }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        static HGLRC GetWGLContext(NkIGraphicsContext* ctx) {
            auto* d = NkGetNativeAs<NkOpenGLContextData>(ctx, NkGraphicsApi::NK_GFX_API_OPENGL);
            return d ? d->hglrc : nullptr;
        }
        static HDC GetWGLDC(NkIGraphicsContext* ctx) {
            auto* d = NkGetNativeAs<NkOpenGLContextData>(ctx, NkGraphicsApi::NK_GFX_API_OPENGL);
            return d ? d->hdc : nullptr;
        }
#endif

#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        static GLXContext GetGLXContext(NkIGraphicsContext* ctx) {
            auto* d = NkGetNativeAs<NkOpenGLContextData>(ctx, NkGraphicsApi::NK_GFX_API_OPENGL);
            return d ? d->context : nullptr;
        }
        static Display* GetGLXDisplay(NkIGraphicsContext* ctx) {
            auto* d = NkGetNativeAs<NkOpenGLContextData>(ctx, NkGraphicsApi::NK_GFX_API_OPENGL);
            return d ? static_cast<Display*>(d->display) : nullptr;
        }
#endif

#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        static EGLContext GetEGLContext(NkIGraphicsContext* ctx) {
            auto* d = NkGetNativeAs<NkOpenGLContextData>(ctx, NkGraphicsApi::NK_GFX_API_OPENGLES);
            return d ? d->eglContext : EGL_NO_CONTEXT;
        }
        static EGLDisplay GetEGLDisplay(NkIGraphicsContext* ctx) {
            auto* d = NkGetNativeAs<NkOpenGLContextData>(ctx, NkGraphicsApi::NK_GFX_API_OPENGLES);
            return d ? d->eglDisplay : EGL_NO_DISPLAY;
        }
#endif

        // ── Vulkan ────────────────────────────────────────────────────────────
        #if NKENTSEU_HAS_VULKAN_HEADERS
        static NkVulkanContextData* Vulkan(NkIGraphicsContext* ctx) {
            return NkGetNativeAs<NkVulkanContextData>(ctx, NkGraphicsApi::NK_GFX_API_VULKAN);
        }
        static VkDevice GetVkDevice(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->device : VK_NULL_HANDLE;
        }
        static VkCommandBuffer GetVkCurrentCommandBuffer(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->commandBuffers[d->currentFrame] : VK_NULL_HANDLE;
        }
        static VkRenderPass GetVkRenderPass(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->renderPass : VK_NULL_HANDLE;
        }
        static VkFramebuffer GetVkCurrentFramebuffer(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->framebuffers[d->currentImageIndex] : VK_NULL_HANDLE;
        }
        static VkQueue GetVkGraphicsQueue(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->graphicsQueue : VK_NULL_HANDLE;
        }
        static VkQueue GetVkComputeQueue(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->computeQueue : VK_NULL_HANDLE;
        }
        static VkInstance GetVkInstance(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->instance : VK_NULL_HANDLE;
        }
        static VkPhysicalDevice GetVkPhysicalDevice(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->physicalDevice : VK_NULL_HANDLE;
        }
        static VkCommandPool GetVkComputePool(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx); return d ? d->computePool : VK_NULL_HANDLE;
        }
        #endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        // ── DirectX 11 ────────────────────────────────────────────────────────
        static NkDX11ContextData* DX11(NkIGraphicsContext* ctx) {
            return NkGetNativeAs<NkDX11ContextData>(ctx, NkGraphicsApi::NK_GFX_API_D3D11);
        }
        static ID3D11Device1* GetDX11Device(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx); return d ? d->device.Get() : nullptr;
        }
        static ID3D11DeviceContext1* GetDX11Context(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx); return d ? d->context.Get() : nullptr;
        }
        static IDXGISwapChain1* GetDX11Swapchain(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx); return d ? d->swapchain.Get() : nullptr;
        }
        static ID3D11RenderTargetView* GetDX11RTV(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx); return d ? d->rtv.Get() : nullptr;
        }
        static ID3D11DepthStencilView* GetDX11DSV(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx); return d ? d->dsv.Get() : nullptr;
        }

        // ── DirectX 12 ────────────────────────────────────────────────────────
        static NkDX12ContextData* DX12(NkIGraphicsContext* ctx) {
            return NkGetNativeAs<NkDX12ContextData>(ctx, NkGraphicsApi::NK_GFX_API_D3D12);
        }
        static ID3D12Device5* GetDX12Device(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx); return d ? d->device.Get() : nullptr;
        }
        static ID3D12CommandQueue* GetDX12CommandQueue(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx); return d ? d->commandQueue.Get() : nullptr;
        }
        static ID3D12CommandQueue* GetDX12ComputeQueue(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx); return d ? d->computeCommandQueue.Get() : nullptr;
        }
        static ID3D12GraphicsCommandList4* GetDX12CommandList(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx); return d ? d->cmdList.Get() : nullptr;
        }
        static ID3D12GraphicsCommandList4* GetDX12ComputeCmdList(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx); return d ? d->computeCmdList.Get() : nullptr;
        }
        static ID3D12Resource* GetDX12CurrentBackBuffer(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            return d ? d->backBuffers[d->currentBackBuffer].Get() : nullptr;
        }
        static D3D12_CPU_DESCRIPTOR_HANDLE GetDX12CurrentRTV(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            D3D12_CPU_DESCRIPTOR_HANDLE h{0};
            return d ? d->rtvHandles[d->currentBackBuffer] : h;
        }
        static D3D12_CPU_DESCRIPTOR_HANDLE GetDX12DSV(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            D3D12_CPU_DESCRIPTOR_HANDLE h{0};
            return d ? d->dsvHandle : h;
        }
#endif // WINDOWS

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        // ── Metal ─────────────────────────────────────────────────────────────
        static NkMetalContextData* Metal(NkIGraphicsContext* ctx) {
            return NkGetNativeAs<NkMetalContextData>(ctx, NkGraphicsApi::NK_GFX_API_METAL);
        }
        // Retourne id<MTLDevice> — le caller doit caster : (__bridge id<MTLDevice>)ptr
        static void* GetMetalDevice(NkIGraphicsContext* ctx) {
            auto* d = Metal(ctx); return d ? d->device : nullptr;
        }
        // Retourne id<MTLRenderCommandEncoder>
        static void* GetMetalCommandEncoder(NkIGraphicsContext* ctx) {
            auto* d = Metal(ctx); return d ? d->currentEncoder : nullptr;
        }
        static void* GetMetalCommandBuffer(NkIGraphicsContext* ctx) {
            auto* d = Metal(ctx); return d ? d->commandBuffer : nullptr;
        }
        static void* GetMetalLayer(NkIGraphicsContext* ctx) {
            auto* d = Metal(ctx); return d ? d->layer : nullptr;
        }
#endif

        // ── Software ──────────────────────────────────────────────────────────
        static NkSoftwareContextData* Software(NkIGraphicsContext* ctx) {
            return NkGetNativeAs<NkSoftwareContextData>(ctx, NkGraphicsApi::NK_GFX_API_SOFTWARE);
        }
        static NkSoftwareFramebuffer* GetSoftwareBackBuffer(NkIGraphicsContext* ctx) {
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_SOFTWARE) return nullptr;
            auto* sw = dynamic_cast<NkSoftwareContext*>(ctx);
            return sw ? &sw->GetBackBuffer() : nullptr;
        }
        static NkSoftwareFramebuffer* GetSoftwareFrontBuffer(NkIGraphicsContext* ctx) {
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_SOFTWARE) return nullptr;
            auto* sw = dynamic_cast<NkSoftwareContext*>(ctx);
            return sw ? &sw->GetFrontBuffer() : nullptr;
        }
    };

} // namespace nkentseu
