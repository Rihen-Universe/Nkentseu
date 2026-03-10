#pragma once
// =============================================================================
// NkGraphicsContext.h
// Interface abstraite cross-API du contexte graphique.
//
// Relation avec les renderers :
//   NkGraphicsContext = pont vers le GPU (device, swapchain, command buffers)
//   NkRenderer*       = consommateurs du contexte (gèrent leurs ressources)
//
// Plusieurs renderers peuvent partager UN SEUL contexte :
//
//   NkWindow  window(...);
//   auto ctx = NkGraphicsContextFactory::Create(window, cfg);
//
//   Renderer2D   r2d(*ctx);   // sprites, UI, texte
//   Renderer3D   r3d(*ctx);   // scène 3D PBR
//   RendererDebug dbg(*ctx);  // wireframe, gizmos
//
//   while (window.IsOpen()) {
//       NkFrameContext frame;
//       ctx->BeginFrame(frame);
//           r3d.Render(scene, frame);    // ordre = décision du user
//           r2d.Render(ui,    frame);
//           dbg.Render(frame);
//       ctx->EndFrame(frame);
//       ctx->Present(frame);
//   }
//
// En OpenGL  : tous les renderers partagent le même GL context courant.
// En Vulkan  : tous partagent le même VkDevice et enregistrent dans le
//              même VkCommandBuffer (frame courante) ou des secondary buffers.
// En DirectX : même ID3D11DeviceContext / ID3D12CommandList.
// =============================================================================

#include "NkGraphicsContextConfig.h"
#include "NKWindow/Core/NkSurface.h"
#include "NKWindow/Core/NkTypes.h"
#include <memory>

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // NkContextError
    // ---------------------------------------------------------------------------
    struct NkContextError {
        NkU32          code    = 0;
        NkString message;

        bool IsOk()  const { return code == 0; }
        bool IsFail() const { return code != 0; }

        static NkContextError Ok()                            { return {0, {}}; }
        static NkContextError Fail(NkU32 c, const char* msg)  { return {c, msg}; }
    };

    // ---------------------------------------------------------------------------
    // NkFrameContext — état d'une frame acquise
    // Transmis à BeginFrame / EndFrame / Present ET aux renderers.
    // ---------------------------------------------------------------------------
    struct NkFrameContext {
        NkU32  imageIndex    = 0;      ///< Index dans la swapchain
        NkU32  frameIndex    = 0;      ///< Index en vol (0 .. maxFramesInFlight-1)
        bool   needsRecreate = false;  ///< true → appeler Recreate() avant le prochain frame
        void*  commandBuffer = nullptr;///< VkCommandBuffer* ou ID3D12GraphicsCommandList* courant
                                    ///< (nullptr pour OpenGL/Metal qui gèrent en interne)
    };

    // ---------------------------------------------------------------------------
    // NkSwapchainDesc
    // ---------------------------------------------------------------------------
    struct NkSwapchainDesc {
        NkU32  width         = 0;
        NkU32  height        = 0;
        NkU32  imageCount    = 0;
        NkU32  maxFrames     = 0;   ///< MAX_FRAMES_IN_FLIGHT
        bool   vsync         = true;
        float  dpi           = 1.f;
    };

    // ---------------------------------------------------------------------------
    // NkGraphicsContext — interface pure
    // ---------------------------------------------------------------------------
    class NkGraphicsContext {
        public:
            virtual ~NkGraphicsContext() = default;

            // ── Cycle de vie ────────────────────────────────────────────────────────

            /// Initialise le contexte à partir du NkSurfaceDesc et de la config.
            virtual NkContextError Init(const NkSurfaceDesc&           surface,
                                        const NkGraphicsContextConfig& config)  = 0;

            /// Libère toutes les ressources.
            virtual void           Shutdown()                                    = 0;

            /// Reconstruit la swapchain (resize, changement de mode plein-écran).
            /// À appeler quand BeginFrame() retourne false + needsRecreate.
            virtual NkContextError Recreate(NkU32 newWidth, NkU32 newHeight)    = 0;

            // ── Boucle de rendu ─────────────────────────────────────────────────────

            /// Acquiert le prochain backbuffer/image.
            /// Retourne false si la swapchain est périmée (frame.needsRecreate = true).
            /// Si false : appeler Recreate() puis réessayer au prochain tick.
            virtual bool BeginFrame(NkFrameContext& frame) = 0;

            /// Termine l'enregistrement du frame courant.
            virtual void EndFrame  (NkFrameContext& frame) = 0;

            /// Présente l'image à l'écran (SwapBuffers / QueuePresent / etc.).
            virtual void Present   (NkFrameContext& frame) = 0;

            // ── État ────────────────────────────────────────────────────────────────

            virtual bool            IsInitialized() const = 0;
            virtual NkGraphicsApi   GetApi()        const = 0;
            virtual NkSwapchainDesc GetSwapchain()  const = 0;
            virtual NkContextError  GetLastError()  const = 0;
            virtual void            SetVSync(bool enabled) = 0;
            virtual bool            GetVSync()      const  = 0;

            // ── Accès aux handles natifs ─────────────────────────────────────────────
            //
            // T doit définir : static constexpr NkU32 kTypeId = <valeur unique>;
            //
            // Types disponibles (définis dans NkNativeHandles.h) :
            //   NkOpenGLNativeHandles    (OpenGL)
            //   NkVulkanNativeHandles    (Vulkan)
            //   NkMetalNativeHandles     (Metal)
            //   NkDirectX11NativeHandles (DirectX 11)
            //   NkDirectX12NativeHandles (DirectX 12)
            //   NkSoftwareNativeHandles  (Software)
            //
            // Retourne nullptr si T ne correspond pas à l'API réelle du contexte.
            // Les renderers utilisent ceci pour accéder aux ressources GPU.
            //
            // Exemple dans un Renderer2D :
            //   Renderer2D(NkGraphicsContext& ctx) {
            //       auto* gl = ctx.GetNativeHandle<NkOpenGLNativeHandles>();
            //       assert(gl && "Renderer2D requiert OpenGL");
            //       // créer VAO, shaders, etc. avec gl->hdc/glxContext/eglContext…
            //   }
            template<typename T>
            T* GetNativeHandle() {
                return static_cast<T*>(GetNativeHandleImpl(T::kTypeId));
            }
            template<typename T>
            const T* GetNativeHandle() const {
                return static_cast<const T*>(GetNativeHandleImpl(T::kTypeId));
            }

        protected:
            virtual void*       GetNativeHandleImpl(NkU32 typeId)       = 0;
            virtual const void* GetNativeHandleImpl(NkU32 typeId) const = 0;
    };

    using NkGraphicsContextPtr = std::unique_ptr<NkGraphicsContext>;

} // namespace nkentseu