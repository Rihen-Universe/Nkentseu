// =============================================================================
// NkRenderWindowCapture.cpp
// Capture d'ecran NKCanvas : readback du backbuffer presente -> fichier image.
//
// Implemente NkRenderWindow::Capture() de facon SPECIFIQUE AU BACKEND. La
// sauvegarde (PNG/JPEG/BMP/...) est deleguee a NkImage (deduction par extension).
// Backends : DX11 (Windows) en premier ; les autres retournent false en attendant.
//
// Isole dans son propre .cpp pour confiner les en-tetes natifs (d3d11.h, etc.)
// hors du NkRenderWindow.cpp principal.
// =============================================================================
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"   // NkNativeContext + en-tetes natifs
#include "NKCanvas/Core/NkIGraphicsContext.h"      // NkIGraphicsContext::GetApi
#include "NKImage/Core/NkImage.h"                  // NkImage::Create / Save (multi-format)

namespace nkentseu {
    namespace renderer {

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        // Readback DX11 : copie le backbuffer vers une texture STAGING lisible CPU,
        // la mappe, convertit en RGBA32, puis enregistre via NkImage.
        static bool NkCaptureDX11(NkIGraphicsContext* ctx, const char* path) noexcept {
            ID3D11Device1*        dev = NkNativeContext::GetDX11Device(ctx);
            ID3D11DeviceContext1* dc  = NkNativeContext::GetDX11Context(ctx);
            IDXGISwapChain1*      sc  = NkNativeContext::GetDX11Swapchain(ctx);
            if (!dev || !dc || !sc) return false;

            ID3D11Texture2D* back = nullptr;
            if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                     reinterpret_cast<void**>(&back))) || !back) {
                return false;
            }

            D3D11_TEXTURE2D_DESC d{};
            back->GetDesc(&d);
            if (d.SampleDesc.Count > 1) { back->Release(); return false; } // MSAA : resolve a faire (TODO)

            D3D11_TEXTURE2D_DESC sd = d;
            sd.Usage          = D3D11_USAGE_STAGING;
            sd.BindFlags      = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags      = 0;

            ID3D11Texture2D* staging = nullptr;
            if (FAILED(dev->CreateTexture2D(&sd, nullptr, &staging)) || !staging) {
                back->Release(); return false;
            }

            dc->CopyResource(staging, back);

            D3D11_MAPPED_SUBRESOURCE m{};
            if (FAILED(dc->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
                staging->Release(); back->Release(); return false;
            }

            const uint32 w = d.Width, h = d.Height;
            // Les swapchains DX11 sont le plus souvent en B8G8R8A8 (BGRA) -> swap R/B.
            const bool bgra = (d.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                               d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

            NkImage img;
            bool ok = img.Create(w, h, math::NkColor(0, 0, 0, 255), 4);
            if (ok) {
                for (uint32 y = 0; y < h; ++y) {
                    const uint8* src = static_cast<const uint8*>(m.pData) + static_cast<usize>(y) * m.RowPitch;
                    uint8*       dst = img.RowPtr(static_cast<int32>(y));
                    for (uint32 x = 0; x < w; ++x) {
                        const uint8* s = src + static_cast<usize>(x) * 4u;
                        uint8*       o = dst + static_cast<usize>(x) * 4u;
                        if (bgra) { o[0] = s[2]; o[1] = s[1]; o[2] = s[0]; o[3] = s[3]; }
                        else      { o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = s[3]; }
                    }
                }
            }

            dc->Unmap(staging, 0);
            staging->Release();
            back->Release();
            return ok && img.Save(path);
        }
#endif // NKENTSEU_PLATFORM_WINDOWS

        bool NkRenderWindow::Capture(const char* path) const {
            if (!path || !*path || !mContext) return false;
            switch (mContext->GetApi()) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                case NkGraphicsApi::NK_GFX_API_DX11: return NkCaptureDX11(mContext, path);
#endif
                default: return false;   // autres backends (GL/DX12/Vulkan/Metal/Software) : a venir
            }
        }

    } // namespace renderer
} // namespace nkentseu
