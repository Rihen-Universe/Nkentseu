#pragma once

// =============================================================================
// NkRenderer.h
// Façade pratique de haut niveau pour un INkRenderer.
// Stocke le backend, gère le cycle de vie et l'auto-resize framebuffer.
// =============================================================================

#include "NkRendererTypes.h"
#include "INkRenderer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nkentseu {

// Forward declaration — NkWindow est défini dans NKWindow
class NkWindow;

// ---------------------------------------------------------------------------
// NkRenderTexture - cible de rendu offscreen CPU RGBA8
// ---------------------------------------------------------------------------

struct NkRenderTexture {
    NkU32 width  = 0;
    NkU32 height = 0;
    NkU32 pitch  = 0;
    std::vector<NkU8> pixels;
};

// ---------------------------------------------------------------------------
// NkRenderer
// ---------------------------------------------------------------------------

class NkRenderer {
public:
    using NkRendererFactory = std::function<std::unique_ptr<INkRenderer>()>;

    NkRenderer();
    NkRenderer(NkWindow& window, const NkRendererConfig& config = {});
    NkRenderer(NkWindow& window, std::unique_ptr<INkRenderer> externalImpl, const NkRendererConfig& config = {});
    ~NkRenderer();

    NkRenderer(const NkRenderer&) = delete;
    NkRenderer& operator=(const NkRenderer&) = delete;

    // --- Lifecycle ---

    bool Create(NkWindow& window, const NkRendererConfig& config = {});
    bool Create(NkWindow& window, std::unique_ptr<INkRenderer> externalImpl, const NkRendererConfig& config = {});
    void Shutdown();
    bool IsValid()   const;
    bool IsEnabled() const;

    static bool RegisterExternalRendererFactory(NkRendererApi api, NkRendererFactory factory);
    static bool UnregisterExternalRendererFactory(NkRendererApi api);
    static bool HasExternalRendererFactory(NkRendererApi api);

    // --- Info ---

    NkRendererApi GetApi()              const;
    std::string   GetApiName()          const;
    bool          IsHardwareAccelerated() const;
    NkError       GetLastError()        const;

    const NkFramebufferInfo& GetFramebufferInfo() const;
    NkRendererContext         GetContext()         const;

    // --- Background color ---

    void  SetBackgroundColor(NkU32 rgba);
    NkU32 GetBackgroundColor() const;

    // --- Frame ---

    void BeginFrame(NkU32 clearColor = 0xFFFFFFFF);
    void EndFrame();
    void Present();
    void Resize(NkU32 width, NkU32 height);

    // --- Output ---

    void SetWindowPresentEnabled(bool enabled);
    bool IsWindowPresentEnabled() const;

    void              SetExternalRenderTarget(NkRenderTexture* target);
    NkRenderTexture*  GetExternalRenderTarget() const;
    bool              ResolveToExternalRenderTarget();

    // --- Color helpers ---

    static NkU32 PackColor(NkU8 r, NkU8 g, NkU8 b, NkU8 a = 255);
    static void  UnpackColor(NkU32 rgba, NkU8& r, NkU8& g, NkU8& b, NkU8& a);

    // --- 2D primitives ---

    void SetPixel(NkI32 x, NkI32 y, NkU32 rgba);
    void DrawPixel(NkI32 x, NkI32 y, NkU32 rgba);

    // --- Impl access ---

    INkRenderer*       GetImpl()       { return mImpl.get(); }
    const INkRenderer* GetImpl() const { return mImpl.get(); }

private:
    std::unique_ptr<INkRenderer> mImpl;
    NkWindow*         mWindow              = nullptr;
    NkRenderTexture*  mExternalTarget      = nullptr;
    bool              mWindowPresentEnabled = true;
    NkRendererConfig  mConfig;
};

} // namespace nkentseu
