// =============================================================================
// NkRenderer2DFactory.cpp
// =============================================================================
#include "NkRenderer2DFactory.h"
#include "NKContext/Renderer/Backends/Software/NkSoftwareRenderer2D.h"
#include "NKContext/Renderer/Backends/OpenGL/NkOpenGLRenderer2D.h"
#include "NKContext/Renderer/Backends/Vulkan/NkVulkanRenderer2D.h"
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "NKContext/Renderer/Backends/DirectX/NkDX11Renderer2D.h"
#   include "NKContext/Renderer/Backends/DirectX/NkDX12Renderer2D.h"
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
// Metal 2D renderer — forward declaration (implemented in NkMetalRenderer2D.mm)
// #include "NKContext/Renderer./Backends/Metal/NkMetalRenderer2D.h"
#endif

#define NK_R2D_FACTORY_LOG(...) logger.Infof("[NkRenderer2DFactory] " __VA_ARGS__)
#define NK_R2D_FACTORY_ERR(...) logger.Errorf("[NkRenderer2DFactory] " __VA_ARGS__)

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        NkIRenderer2D* NkRenderer2DFactory::Create(NkIGraphicsContext* ctx) {
            if (!ctx || !ctx->IsValid()) {
                NK_R2D_FACTORY_ERR("Invalid graphics context");
                return nullptr;
            }

            const NkGraphicsApi api = ctx->GetApi();
            NK_R2D_FACTORY_LOG("Creating 2D renderer for API: %s", NkGraphicsApiName(api));

            NkIRenderer2D* r2d = nullptr;

            switch (api) {
                // ── Software ─────────────────────────────────────────────────────────
                case NkGraphicsApi::NK_GFX_API_SOFTWARE:
                    r2d = new NkSoftwareRenderer2D();
                    break;

                // ── OpenGL / OpenGL ES / WebGL ────────────────────────────────────────
                case NkGraphicsApi::NK_GFX_API_OPENGL:
                case NkGraphicsApi::NK_GFX_API_OPENGLES:
                case NkGraphicsApi::NK_GFX_API_WEBGL:
                    r2d = new NkOpenGLRenderer2D();
                    break;

                // ── Vulkan ────────────────────────────────────────────────────────────
                case NkGraphicsApi::NK_GFX_API_VULKAN:
                    r2d = new NkVulkanRenderer2D();
                    break;

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // ── DirectX 11 ───────────────────────────────────────────────────────
                case NkGraphicsApi::NK_GFX_API_D3D11:
                    r2d = new NkDX11Renderer2D();
                    break;

                // ── DirectX 12 ───────────────────────────────────────────────────────
                case NkGraphicsApi::NK_GFX_API_D3D12:
                    r2d = new NkDX12Renderer2D();
                    break;
        #endif

        #if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                // ── Metal ─────────────────────────────────────────────────────────────
                case NkGraphicsApi::NK_GFX_API_METAL:
                    // r2d = new NkMetalRenderer2D();
                    NK_R2D_FACTORY_ERR("Metal 2D renderer not yet implemented");
                    return nullptr;
        #endif

                default:
                    NK_R2D_FACTORY_ERR("Unsupported API for 2D rendering: %s",
                                    NkGraphicsApiName(api));
                    return nullptr;
            }

            if (!r2d->Initialize(ctx)) {
                NK_R2D_FACTORY_ERR("Initialize failed for API: %s", NkGraphicsApiName(api));
                delete r2d;
                return nullptr;
            }

            NK_R2D_FACTORY_LOG("2D renderer created: %s", NkGraphicsApiName(api));
            return r2d;
        }

        // =============================================================================
        NkRenderer2DPtr NkRenderer2DFactory::CreateUnique(NkIGraphicsContext* ctx) {
            return NkRenderer2DPtr(Create(ctx));
        }

        // =============================================================================
        bool NkRenderer2DFactory::IsApiSupported(NkGraphicsApi api) {
            switch (api) {
                case NkGraphicsApi::NK_GFX_API_SOFTWARE:
                case NkGraphicsApi::NK_GFX_API_OPENGL:
                case NkGraphicsApi::NK_GFX_API_OPENGLES:
                case NkGraphicsApi::NK_GFX_API_WEBGL:
                case NkGraphicsApi::NK_GFX_API_VULKAN:
                    return true;
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                case NkGraphicsApi::NK_GFX_API_D3D11:
                case NkGraphicsApi::NK_GFX_API_D3D12:
                    return true;
        #endif
        #if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                case NkGraphicsApi::NK_GFX_API_METAL:
                    return false; // not yet implemented
        #endif
                default:
                    return false;
            }
        }

    } // namespace renderer
} // namespace nkentseu