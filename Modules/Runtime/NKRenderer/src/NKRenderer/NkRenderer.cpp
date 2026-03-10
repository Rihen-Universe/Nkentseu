// =============================================================================
// NkRenderer.cpp
// Implémentation de la façade NkRenderer.
// =============================================================================

#include "NkRenderer.h"
#include "NKWindow/Core/NkWindow.h"
#include "Backends/Software/NkSoftwareRenderer.h"
#include "NkRendererStubs.h"
#include "NKMemory/NkUtils.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <mutex>
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // Factory interne
    // ---------------------------------------------------------------------------

    namespace {

    using NkRendererFactoryMap = NkUnorderedMap<uint32, NkRenderer::NkRendererFactory>;

    NkRendererFactoryMap& ExternalRendererFactories() {
        static NkRendererFactoryMap sFactories;
        return sFactories;
    }

    std::mutex& ExternalRendererFactoryMutex() {
        static std::mutex sMutex;
        return sMutex;
    }

    std::unique_ptr<NkIRenderer> CreateBuiltinRendererImpl(NkRendererApi api) {
        switch (api) {
            case NkRendererApi::NK_NONE:      return nullptr;
            case NkRendererApi::NK_SOFTWARE:  return std::make_unique<NkSoftwareRenderer>();
            case NkRendererApi::NK_VULKAN:    return std::make_unique<NkVulkanRendererImpl>();
            case NkRendererApi::NK_OPENGL:    return std::make_unique<NkOpenGLRendererImpl>();
            case NkRendererApi::NK_DIRECTX11: return std::make_unique<NkDX11RendererImpl>();
            case NkRendererApi::NK_DIRECTX12: return std::make_unique<NkDX12RendererImpl>();
            case NkRendererApi::NK_METAL:     return std::make_unique<NkMetalRendererImpl>();
            default: return nullptr;
        }
    }

    std::unique_ptr<NkIRenderer> CreateRendererImpl(NkRendererApi api) {
        if (api == NkRendererApi::NK_NONE)
            return nullptr;
        {
            std::lock_guard<std::mutex> lock(ExternalRendererFactoryMutex());
            auto* factory = ExternalRendererFactories().Find(static_cast<uint32>(api));
            if (factory && *factory) {
                if (auto impl = (*factory)())
                    return impl;
            }
        }
        return CreateBuiltinRendererImpl(api);
    }

    } // namespace

    // ---------------------------------------------------------------------------
    // Construction
    // ---------------------------------------------------------------------------

    NkRenderer::NkRenderer() = default;

    NkRenderer::NkRenderer(NkWindow& window, const NkRendererConfig& config) {
        Create(window, config);
    }

    NkRenderer::NkRenderer(NkWindow& window, std::unique_ptr<NkIRenderer> externalImpl, const NkRendererConfig& config) {
        Create(window, std::move(externalImpl), config);
    }

    NkRenderer::~NkRenderer() {
        Shutdown();
    }

    bool NkRenderer::Create(NkWindow& window, const NkRendererConfig& config) {
        return Create(window, std::unique_ptr<NkIRenderer>{}, config);
    }

    bool NkRenderer::Create(NkWindow& window, std::unique_ptr<NkIRenderer> externalImpl, const NkRendererConfig& config) {
        Shutdown();
        mWindow        = &window;
        mConfig        = config;
        mExternalTarget = nullptr;
        mImpl          = std::move(externalImpl);
        if (!mImpl)
            mImpl = CreateRendererImpl(config.api);

        if (!mImpl)
            return config.api == NkRendererApi::NK_NONE;

        return mImpl->Init(config, window.GetSurfaceDesc());
    }

    void NkRenderer::Shutdown() {
        if (mImpl) {
            mImpl->Shutdown();
            mImpl.reset();
        }
    }

    bool NkRenderer::IsValid() const {
        return mImpl ? mImpl->IsValid() : mConfig.api == NkRendererApi::NK_NONE;
    }

    bool NkRenderer::IsEnabled() const {
        return mImpl != nullptr;
    }

    bool NkRenderer::RegisterExternalRendererFactory(NkRendererApi api, NkRendererFactory factory) {
        if (api == NkRendererApi::NK_NONE || !factory) return false;
        std::lock_guard<std::mutex> lock(ExternalRendererFactoryMutex());
        ExternalRendererFactories()[static_cast<uint32>(api)] = std::move(factory);
        return true;
    }

    bool NkRenderer::UnregisterExternalRendererFactory(NkRendererApi api) {
        if (api == NkRendererApi::NK_NONE) return false;
        std::lock_guard<std::mutex> lock(ExternalRendererFactoryMutex());
        return ExternalRendererFactories().Erase(static_cast<uint32>(api));
    }

    bool NkRenderer::HasExternalRendererFactory(NkRendererApi api) {
        if (api == NkRendererApi::NK_NONE) return false;
        std::lock_guard<std::mutex> lock(ExternalRendererFactoryMutex());
        return ExternalRendererFactories().Contains(static_cast<uint32>(api));
    }

    // ---------------------------------------------------------------------------
    // Informations
    // ---------------------------------------------------------------------------

    NkRendererApi NkRenderer::GetApi() const {
        return mConfig.api;
    }

    NkString NkRenderer::GetApiName() const {
        return mImpl ? mImpl->GetName() : NkRendererApiToString(mConfig.api);
    }

    bool NkRenderer::IsHardwareAccelerated() const {
        return mImpl && mImpl->IsHardwareAccelerated();
    }

    NkError NkRenderer::GetLastError() const {
        return mImpl ? mImpl->GetLastError() : NkError::Ok();
    }

    const NkFramebufferInfo& NkRenderer::GetFramebufferInfo() const {
        static NkFramebufferInfo sDummy;
        return mImpl ? mImpl->GetFramebufferInfo() : sDummy;
    }

    NkRendererContext NkRenderer::GetContext() const {
        if (mImpl)
            return mImpl->GetContext();
        const NkSurfaceDesc surface = (mWindow && mWindow->IsOpen()) ? mWindow->GetSurfaceDesc() : NkSurfaceDesc{};
        return NkMakeRendererContext(mConfig.api, surface, {});
    }

    // ---------------------------------------------------------------------------
    // Couleur de fond
    // ---------------------------------------------------------------------------

    void  NkRenderer::SetBackgroundColor(uint32 rgba) { if (mImpl) mImpl->SetBackgroundColor(rgba); }
    uint32 NkRenderer::GetBackgroundColor() const     { return mImpl ? mImpl->GetBackgroundColor() : 0x141414FF; }

    // ---------------------------------------------------------------------------
    // Trame
    // ---------------------------------------------------------------------------

    void NkRenderer::BeginFrame(uint32 clearColor) {
        if (!mImpl) return;
        if (mConfig.autoResizeFramebuffer && mWindow && mWindow->IsOpen()) {
            NkVec2u wsize = mWindow->GetSize();
            const NkFramebufferInfo& fb = mImpl->GetFramebufferInfo();
            if (wsize.x > 0 && wsize.y > 0 && (wsize.x != fb.width || wsize.y != fb.height))
                mImpl->Resize(wsize.x, wsize.y);
        }
        uint32 color = (clearColor == 0xFFFFFFFF) ? mImpl->GetBackgroundColor() : clearColor;
        mImpl->BeginFrame(color);
    }

    void NkRenderer::EndFrame() { if (mImpl) mImpl->EndFrame(); }

    void NkRenderer::Present() {
        if (!mImpl) return;
        if (mExternalTarget) ResolveToExternalRenderTarget();
        if (!mWindowPresentEnabled) return;
        if (!mWindow || !mWindow->IsOpen()) return;
        mImpl->Present(mWindow->GetSurfaceDesc());
    }

    void NkRenderer::Resize(uint32 w, uint32 h) { if (mImpl) mImpl->Resize(w, h); }

    // ---------------------------------------------------------------------------
    // Sortie
    // ---------------------------------------------------------------------------

    void             NkRenderer::SetWindowPresentEnabled(bool e)     { mWindowPresentEnabled = e; }
    bool             NkRenderer::IsWindowPresentEnabled()  const     { return mWindowPresentEnabled; }
    void             NkRenderer::SetExternalRenderTarget(NkRenderTexture* t) { mExternalTarget = t; }
    NkRenderTexture* NkRenderer::GetExternalRenderTarget() const     { return mExternalTarget; }

    bool NkRenderer::ResolveToExternalRenderTarget() {
        if (!mExternalTarget || !mImpl) return false;
        const NkFramebufferInfo& fb = mImpl->GetFramebufferInfo();
        if (!fb.pixels || !fb.width || !fb.height || !fb.pitch) return false;
        const uint32 dstPitch = fb.width * 4;
        if (fb.pitch < dstPitch) return false;
        mExternalTarget->width  = fb.width;
        mExternalTarget->height = fb.height;
        mExternalTarget->pitch  = dstPitch;
        const std::size_t rowCount = static_cast<std::size_t>(fb.height);
        const std::size_t dstBytes = static_cast<std::size_t>(dstPitch) * rowCount;
        mExternalTarget->pixels.Resize(dstBytes);
        const uint8* src = fb.pixels;
        uint8*       dst = mExternalTarget->pixels.Data();
        if (fb.pitch == dstPitch) { memory::NkMemCopy(dst, src, dstBytes); return true; }
        for (std::size_t row = 0; row < rowCount; ++row)
            memory::NkMemCopy(dst + row * dstPitch, src + row * fb.pitch, dstPitch);
        return true;
    }

    // ---------------------------------------------------------------------------
    // Couleur utilitaires
    // ---------------------------------------------------------------------------

    uint32 NkRenderer::PackColor(uint8 r, uint8 g, uint8 b, uint8 a) {
        return (static_cast<uint32>(r) << 24) | (static_cast<uint32>(g) << 16) |
            (static_cast<uint32>(b) << 8)  | a;
    }

    void NkRenderer::UnpackColor(uint32 rgba, uint8& r, uint8& g, uint8& b, uint8& a) {
        r = (rgba >> 24) & 0xFF;
        g = (rgba >> 16) & 0xFF;
        b = (rgba >> 8)  & 0xFF;
        a =  rgba        & 0xFF;
    }

    // ---------------------------------------------------------------------------
    // Primitives
    // ---------------------------------------------------------------------------

    void NkRenderer::SetPixel(int32 x, int32 y, uint32 rgba) { if (mImpl) mImpl->SetPixel(x, y, rgba); }
    void NkRenderer::DrawPixel(int32 x, int32 y, uint32 rgba) { SetPixel(x, y, rgba); }

} // namespace nkentseu
