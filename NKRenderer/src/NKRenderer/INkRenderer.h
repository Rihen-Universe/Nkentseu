#pragma once
// =============================================================================
// INkRenderer.h
// Interface de base pour tous les backends de rendu NKRenderer.
//
// POUR LES ETUDIANTS :
// Pour creer votre propre renderer, implementez cette interface :
//
//   class MyRenderer : public nkentseu::INkRenderer {
//   public:
//       bool Init(const NkRendererConfig& cfg, const NkSurfaceDesc& surf) override;
//       void Shutdown() override;
//       bool IsValid() const override { return mReady; }
//       const char* GetName() const override { return "MyRenderer"; }
//       bool IsHardwareAccelerated() const override { return false; }
//       const NkFramebufferInfo& GetFramebufferInfo() const override;
//       void SetBackgroundColor(NkU32 rgba) override { mBgColor = rgba; }
//       NkU32 GetBackgroundColor() const override { return mBgColor; }
//       void BeginFrame(NkU32 clearColor) override;
//       void EndFrame() override {}
//       void Present(const NkSurfaceDesc& surface) override;
//       void Resize(NkU32 w, NkU32 h) override;
//       void SetPixel(NkI32 x, NkI32 y, NkU32 rgba) override;
//   private:
//       bool mReady = false;
//       NkU32 mBgColor = 0x141414FF;
//   };
//
// Puis passez une instance a NkRenderer :
//   NkRenderer renderer;
//   renderer.Create(window, std::make_unique<MyRenderer>());
// =============================================================================

#include "NkRendererTypes.h"
#include <string>

namespace nkentseu {

class INkRenderer {
public:
    virtual ~INkRenderer() = default;

    virtual bool Init(const NkRendererConfig& config, const NkSurfaceDesc& surface) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsValid() const = 0;

    virtual const char* GetName() const = 0;
    virtual bool IsHardwareAccelerated() const = 0;
    virtual NkError GetLastError() const { return NkError::Ok(); }

    virtual const NkFramebufferInfo& GetFramebufferInfo() const = 0;
    virtual NkRendererContext GetContext() const { return NkRendererContext{}; }

    virtual void SetBackgroundColor(NkU32 rgba) = 0;
    virtual NkU32 GetBackgroundColor() const = 0;

    virtual void BeginFrame(NkU32 clearColor = 0xFFFFFFFF) = 0;
    virtual void EndFrame() = 0;
    virtual void Present(const NkSurfaceDesc& surface) = 0;
    virtual void Resize(NkU32 width, NkU32 height) = 0;
    virtual void SetPixel(NkI32 x, NkI32 y, NkU32 rgba) = 0;
};

using INkRendererImpl = INkRenderer;

} // namespace nkentseu
