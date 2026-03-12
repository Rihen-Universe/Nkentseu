#pragma once
// =============================================================================
// nativecontextAccess.h
// Accès typé et sécurisé aux données internes de chaque contexte.
//
// Problème résolu :
//   GetNativeContextData() retourne un void* — l'appelant doit caster.
//   Ces helpers évitent les casts manuels et vérifient l'API au runtime.
//
// Usage :
//   auto* vkData = nativecontext::Vulkan(ctx);     // NkVulkanContextData*
//   auto* dx12   = nativecontext::DX12(ctx);       // NkDX12ContextData*
//   auto* glData = nativecontext::OpenGL(ctx);     // NkOpenGLContextData*
//   // Retourne nullptr si l'API du contexte ne correspond pas.
//
// Accès aux objets natifs directement :
//   VkDevice device = nativecontext::GetVkDevice(ctx);
//   ID3D12Device* dev = nativecontext::GetDX12Device(ctx);
// =============================================================================

#include "NkIGraphicsContext.h"
#include "NKRenderer/Backends/OpenGL/NkOpenGLContextData.h"
#include "NKRenderer/Backends/Vulkan/NkVulkanContextData.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "NKRenderer/Backends/DirectX/NkDirectXContextData.h"
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   include "NKRenderer/Backends/Metal/NkMetalContext.h"
#endif

#include "NKRenderer/Backends/Software/NkSoftwareContext.h"
#include <cassert>

namespace nkentseu {

    // -------------------------------------------------------------------------
    // Helper générique de cast sécurisé
    // -------------------------------------------------------------------------
    template<typename TData, NkGraphicsApi ExpectedApi>
    inline TData* NkContextCast(NkIGraphicsContext* ctx) {
        if (!ctx || ctx->GetApi() != ExpectedApi) return nullptr;
        return static_cast<TData*>(ctx->GetNativeContextData());
    }
    template<typename TData, NkGraphicsApi ExpectedApi>
    inline const TData* NkContextCast(const NkIGraphicsContext* ctx) {
        if (!ctx || ctx->GetApi() != ExpectedApi) return nullptr;
        return static_cast<const TData*>(
            const_cast<NkIGraphicsContext*>(ctx)->GetNativeContextData());
    }

    // =========================================================================
    // nativecontext — namespace de fonctions d'accès
    // =========================================================================
    namespace nativecontext {

        // ── OpenGL ────────────────────────────────────────────────────────────
        inline NkOpenGLContextData* OpenGL(NkIGraphicsContext* ctx) {
            if (!ctx) return nullptr;
            auto api = ctx->GetApi();
            if (api != NkGraphicsApi::NK_API_OPENGL &&
                api != NkGraphicsApi::NK_API_OPENGLES &&
                api != NkGraphicsApi::NK_API_WEBGL) return nullptr;
            return static_cast<NkOpenGLContextData*>(ctx->GetNativeContextData());
        }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        // HGLRC WGL
        inline HGLRC GetWGLContext(NkIGraphicsContext* ctx) {
            auto* d = OpenGL(ctx);
            return d ? d->hglrc : nullptr;
        }
        inline HDC GetWGLDC(NkIGraphicsContext* ctx) {
            auto* d = OpenGL(ctx);
            return d ? d->hdc : nullptr;
        }
#endif

#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        inline GLXContext GetGLXContext(NkIGraphicsContext* ctx) {
            auto* d = OpenGL(ctx);
            return d ? d->context : nullptr;
        }
        inline Display* GetGLXDisplay(NkIGraphicsContext* ctx) {
            auto* d = OpenGL(ctx);
            return d ? d->display : nullptr;
        }
#endif

#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        inline EGLContext GetEGLContext(NkIGraphicsContext* ctx) {
            auto* d = OpenGL(ctx);
            return d ? d->eglContext : EGL_NO_CONTEXT;
        }
        inline EGLDisplay GetEGLDisplay(NkIGraphicsContext* ctx) {
            auto* d = OpenGL(ctx);
            return d ? d->eglDisplay : EGL_NO_DISPLAY;
        }
#endif

        // ── Vulkan ────────────────────────────────────────────────────────────
#if NKENTSEU_HAS_VULKAN_HEADERS
        inline NkVulkanContextData* Vulkan(NkIGraphicsContext* ctx) {
            return NkContextCast<NkVulkanContextData,
                                 NkGraphicsApi::NK_API_VULKAN>(ctx);
        }
        inline VkDevice GetVkDevice(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->device : VK_NULL_HANDLE;
        }
        inline VkPhysicalDevice GetVkPhysicalDevice(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->physicalDevice : VK_NULL_HANDLE;
        }
        inline VkRenderPass GetVkRenderPass(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->renderPass : VK_NULL_HANDLE;
        }
        inline VkCommandBuffer GetVkCurrentCommandBuffer(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->commandBuffers[d->currentFrame] : VK_NULL_HANDLE;
        }
        inline VkFramebuffer GetVkCurrentFramebuffer(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->framebuffers[d->currentImageIndex] : VK_NULL_HANDLE;
        }
        inline VkQueue GetVkGraphicsQueue(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->graphicsQueue : VK_NULL_HANDLE;
        }
        inline VkInstance GetVkInstance(NkIGraphicsContext* ctx) {
            auto* d = Vulkan(ctx);
            return d ? d->instance : VK_NULL_HANDLE;
        }
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        // ── DirectX 11 ────────────────────────────────────────────────────────
        inline NkDX11ContextData* DX11(NkIGraphicsContext* ctx) {
            return NkContextCast<NkDX11ContextData,
                                 NkGraphicsApi::NK_API_DIRECTX11>(ctx);
        }
        inline ID3D11Device1* GetDX11Device(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx);
            return d ? d->device.Get() : nullptr;
        }
        inline ID3D11DeviceContext1* GetDX11Context(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx);
            return d ? d->context.Get() : nullptr;
        }
        inline IDXGISwapChain1* GetDX11Swapchain(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx);
            return d ? d->swapchain.Get() : nullptr;
        }
        inline ID3D11RenderTargetView* GetDX11RTV(NkIGraphicsContext* ctx) {
            auto* d = DX11(ctx);
            return d ? d->rtv.Get() : nullptr;
        }

        // ── DirectX 12 ────────────────────────────────────────────────────────
        inline NkDX12ContextData* DX12(NkIGraphicsContext* ctx) {
            return NkContextCast<NkDX12ContextData,
                                 NkGraphicsApi::NK_API_DIRECTX12>(ctx);
        }
        inline ID3D12Device5* GetDX12Device(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            return d ? d->device.Get() : nullptr;
        }
        inline ID3D12CommandQueue* GetDX12CommandQueue(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            return d ? d->commandQueue.Get() : nullptr;
        }
        inline ID3D12GraphicsCommandList4* GetDX12CommandList(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            return d ? d->cmdList.Get() : nullptr;
        }
        inline ID3D12Resource* GetDX12CurrentBackBuffer(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            return d ? d->backBuffers[d->currentBackBuffer].Get() : nullptr;
        }
        inline D3D12_CPU_DESCRIPTOR_HANDLE GetDX12CurrentRTV(NkIGraphicsContext* ctx) {
            auto* d = DX12(ctx);
            return d ? d->rtvHandles[d->currentBackBuffer] : D3D12_CPU_DESCRIPTOR_HANDLE{0};
        }
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        // ── Metal ─────────────────────────────────────────────────────────────
        inline NkMetalContextData* Metal(NkIGraphicsContext* ctx) {
            return NkContextCast<NkMetalContextData,
                                 NkGraphicsApi::NK_API_METAL>(ctx);
        }
        inline void* GetMetalDevice(NkIGraphicsContext* ctx) {
            auto* d = Metal(ctx);
            return d ? d->device : nullptr; // cast en id<MTLDevice> côté .mm
        }
        inline void* GetMetalCommandEncoder(NkIGraphicsContext* ctx) {
            auto* d = Metal(ctx);
            return d ? d->currentEncoder : nullptr;
        }
#endif

        // ── Software ──────────────────────────────────────────────────────────
        inline NkSoftwareContextData* Software(NkIGraphicsContext* ctx) {
            return NkContextCast<NkSoftwareContextData,
                                 NkGraphicsApi::NK_API_SOFTWARE>(ctx);
        }
        inline NkSoftwareFramebuffer* GetSoftwareBackBuffer(NkIGraphicsContext* ctx) {
            // Accès via NkSoftwareContext* pour GetBackBuffer()
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_API_SOFTWARE) return nullptr;
            return &static_cast<NkSoftwareContext*>(ctx)->GetBackBuffer();
        }

    } // namespace nativecontext

} // namespace nkentseu
