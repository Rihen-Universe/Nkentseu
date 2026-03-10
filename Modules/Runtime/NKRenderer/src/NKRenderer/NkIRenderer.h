#pragma once
// =============================================================================
// NkIRenderer.h
// Interface de base pour tous les backends de rendu NKRenderer.
//
// POUR LES ETUDIANTS :
// Pour creer votre propre renderer, implementez cette interface :
//
//   class MyRenderer : public nkentseu::NkIRenderer {
//   public:
//       bool Init(const NkRendererConfig& cfg, const NkSurfaceDesc& surf) override;
//       void Shutdown() override;
//       bool IsValid() const override { return mReady; }
//       const char* GetName() const override { return "MyRenderer"; }
//       bool IsHardwareAccelerated() const override { return false; }
//       const NkFramebufferInfo& GetFramebufferInfo() const override;
//       void SetBackgroundColor(uint32 rgba) override { mBgColor = rgba; }
//       uint32 GetBackgroundColor() const override { return mBgColor; }
//       void BeginFrame(uint32 clearColor) override;
//       void EndFrame() override {}
//       void Present(const NkSurfaceDesc& surface) override;
//       void Resize(uint32 w, uint32 h) override;
//       void SetPixel(int32 x, int32 y, uint32 rgba) override;
//   private:
//       bool mReady = false;
//       uint32 mBgColor = 0x141414FF;
//   };
//
// Puis passez une instance a NkRenderer :
//   NkRenderer renderer;
//   renderer.Create(window, std::make_unique<MyRenderer>());
// =============================================================================

#include "NkRendererTypes.h"
#include "NkRendererConfig.h"
#include <string>

namespace nkentseu {

    class NkIRenderer {
        public:
            virtual ~NkIRenderer() = default;

            virtual bool Init(const NkRendererConfig& config, const NkSurfaceDesc& surface) = 0;
            virtual void Shutdown() = 0;
            virtual bool IsValid() const = 0;

            virtual const char* GetName() const = 0;
            virtual bool IsHardwareAccelerated() const = 0;
            virtual NkError GetLastError() const { return NkError::Ok(); }

            virtual const NkFramebufferInfo& GetFramebufferInfo() const = 0;
            virtual NkRendererContext GetContext() const { return NkRendererContext{}; }

            virtual void SetBackgroundColor(uint32 rgba) = 0;
            virtual uint32 GetBackgroundColor() const = 0;

            virtual void BeginFrame(uint32 clearColor = 0xFFFFFFFF) = 0;
            virtual void EndFrame() = 0;
            virtual void Present(const NkSurfaceDesc& surface) = 0;
            virtual void Resize(uint32 width, uint32 height) = 0;
            virtual void SetPixel(int32 x, int32 y, uint32 rgba) = 0;
    };

    using NkIRendererImpl = NkIRenderer;

} // namespace nkentseu
