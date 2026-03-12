#pragma once

// =============================================================================
// NkRenderer.h
// Façade pratique de haut niveau pour un NkIRenderer.
// Stocke le backend, gère le cycle de vie et l'auto-resize framebuffer.
// =============================================================================

#include "NkRendererTypes.h"
#include "NkIRenderer.h"

#include <functional>
#include <memory>
#include <string>
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // Forward declaration — NkWindow est défini dans NKWindow
    class NkWindow;

    // ---------------------------------------------------------------------------
    // NkRenderTexture - cible de rendu offscreen CPU RGBA8
    // ---------------------------------------------------------------------------

    struct NkRenderTexture {
        uint32 width  = 0;
        uint32 height = 0;
        uint32 pitch  = 0;
        NkVector<uint8> pixels;
    };

    // ---------------------------------------------------------------------------
    // NkRenderer
    // ---------------------------------------------------------------------------

    class NkRenderer {
        public:
            using NkRendererFactory = std::function<std::unique_ptr<NkIRenderer>()>;

            NkRenderer();
            NkRenderer(NkWindow& window, const NkRendererConfig& config = {});
            NkRenderer(NkWindow& window, std::unique_ptr<NkIRenderer> externalImpl, const NkRendererConfig& config = {});
            ~NkRenderer();

            NkRenderer(const NkRenderer&) = delete;
            NkRenderer& operator=(const NkRenderer&) = delete;

            // --- Lifecycle ---

            bool Create(NkWindow& window, const NkRendererConfig& config = {});
            bool Create(NkWindow& window, std::unique_ptr<NkIRenderer> externalImpl, const NkRendererConfig& config = {});
            void Shutdown();
            bool IsValid()   const;
            bool IsEnabled() const;

            static bool RegisterExternalRendererFactory(NkRendererApi api, NkRendererFactory factory);
            static bool UnregisterExternalRendererFactory(NkRendererApi api);
            static bool HasExternalRendererFactory(NkRendererApi api);

            // --- Info ---

            NkRendererApi GetApi()              const;
            NkString   GetApiName()          const;
            bool          IsHardwareAccelerated() const;
            NkError       GetLastError()        const;

            const NkFramebufferInfo& GetFramebufferInfo() const;
            NkRendererContext         GetContext()         const;

            // --- Background color ---

            void  SetBackgroundColor(uint32 rgba);
            uint32 GetBackgroundColor() const;

            // --- Frame ---

            void BeginFrame(uint32 clearColor = 0xFFFFFFFF);
            void EndFrame();
            void Present();
            void Resize(uint32 width, uint32 height);

            // --- Output ---

            void SetWindowPresentEnabled(bool enabled);
            bool IsWindowPresentEnabled() const;

            void              SetExternalRenderTarget(NkRenderTexture* target);
            NkRenderTexture*  GetExternalRenderTarget() const;
            bool              ResolveToExternalRenderTarget();

            // --- Color helpers ---

            static uint32 PackColor(uint8 r, uint8 g, uint8 b, uint8 a = 255);
            static void  UnpackColor(uint32 rgba, uint8& r, uint8& g, uint8& b, uint8& a);

            // --- 2D primitives ---

            void SetPixel(int32 x, int32 y, uint32 rgba);
            void DrawPixel(int32 x, int32 y, uint32 rgba);

            // --- Impl access ---

            NkIRenderer*       GetImpl()       { return mImpl.get(); }
            const NkIRenderer* GetImpl() const { return mImpl.get(); }

        private:
            std::unique_ptr<NkIRenderer> mImpl;
            NkWindow*         mWindow              = nullptr;
            NkRenderTexture*  mExternalTarget      = nullptr;
            bool              mWindowPresentEnabled = true;
            NkRendererConfig  mConfig;
    };

} // namespace nkentseu
