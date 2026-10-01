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
#include "NKCanvas/Core/NkNativeContextAccess.h" // NkNativeContext + en-tetes natifs
#include "NKCanvas/Core/NkIGraphicsContext.h"	 // NkIGraphicsContext::GetApi
#include "NKImage/Core/NkImage.h"				 // NkImage::Create / Save (multi-format)
#include "NKContainers/Sequential/NkVector.h"
#include <cstring>

namespace nkentseu {
	namespace renderer {

#if defined(NKENTSEU_PLATFORM_WINDOWS)
		// Readback DX11 : copie le backbuffer vers une texture STAGING lisible CPU, la mappe et
		// remplit `out` (RGBA32). Base commune de Capture(fichier) et CaptureToImage(mémoire).
		static bool NkReadbackDX11(NkIGraphicsContext *ctx, NkImage &out) noexcept {
			ID3D11Device1 *dev = NkNativeContext::GetDX11Device(ctx);
			ID3D11DeviceContext1 *dc = NkNativeContext::GetDX11Context(ctx);
			IDXGISwapChain1 *sc = NkNativeContext::GetDX11Swapchain(ctx);
			if (!dev || !dc || !sc)
				return false;

			ID3D11Texture2D *back = nullptr;
			if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back))) || !back) {
				return false;
			}

			D3D11_TEXTURE2D_DESC d{};
			back->GetDesc(&d);
			if (d.SampleDesc.Count > 1) {
				back->Release();
				return false;
			} // MSAA : resolve a faire (TODO)

			D3D11_TEXTURE2D_DESC sd = d;
			sd.Usage = D3D11_USAGE_STAGING;
			sd.BindFlags = 0;
			sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			sd.MiscFlags = 0;

			ID3D11Texture2D *staging = nullptr;
			if (FAILED(dev->CreateTexture2D(&sd, nullptr, &staging)) || !staging) {
				back->Release();
				return false;
			}

			dc->CopyResource(staging, back);

			D3D11_MAPPED_SUBRESOURCE m{};
			if (FAILED(dc->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
				staging->Release();
				back->Release();
				return false;
			}

			const uint32 w = d.Width, h = d.Height;
			// Les swapchains DX11 sont le plus souvent en B8G8R8A8 (BGRA) -> swap R/B.
			const bool bgra = (d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

			bool ok = out.Create(w, h, math::NkColor(0, 0, 0, 255), 4);
			if (ok) {
				for (uint32 y = 0; y < h; ++y) {
					const uint8 *src = static_cast<const uint8 *>(m.pData) + static_cast<usize>(y) * m.RowPitch;
					uint8 *dst = out.RowPtr(static_cast<int32>(y));
					for (uint32 x = 0; x < w; ++x) {
						const uint8 *s = src + static_cast<usize>(x) * 4u;
						uint8 *o = dst + static_cast<usize>(x) * 4u;
						if (bgra) {
							o[0] = s[2];
							o[1] = s[1];
							o[2] = s[0];
							o[3] = s[3];
						} else {
							o[0] = s[0];
							o[1] = s[1];
							o[2] = s[2];
							o[3] = s[3];
						}
					}
				}
			}

			dc->Unmap(staging, 0);
			staging->Release();
			back->Release();
			return ok;
		}
#endif // NKENTSEU_PLATFORM_WINDOWS

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
		// Metal (2026-09-30) : le contexte garde une copie CPU de l'image presentee
		// quand PrepareCapture l'a demande (NkMetalContext::KeepLastFrame).
		static bool NkReadbackMetal(NkIGraphicsContext *ctx, NkImage &out) noexcept {
			auto *mtl = static_cast<NkMetalContext *>(ctx);
			NkVector<uint8> px;
			uint32 w = 0, h = 0;
			if (!mtl || !mtl->ReadLastFrame(px, w, h))
				return false;
			if (!out.Create(w, h, math::NkColor(0, 0, 0, 255), 4))
				return false;
			for (uint32 y = 0; y < h; ++y)
				memcpy(out.RowPtr(static_cast<int32>(y)), px.Data() + static_cast<usize>(y) * w * 4u,
					   static_cast<usize>(w) * 4u);
			return true;
		}
#endif

		// Rendu logiciel (2026-09-30) : l'image presentee EST en memoire (tampon
		// avant). C'est la reference a laquelle la CI compare le rendu Metal.
		static bool NkReadbackSoftware(NkIGraphicsContext *ctx, NkImage &out) noexcept {
			NkSoftwareFramebuffer *fb = NkNativeContext::GetSoftwareFrontBuffer(ctx);
			if (!fb || fb->width == 0 || fb->height == 0)
				return false;
			if (!out.Create(fb->width, fb->height, math::NkColor(0, 0, 0, 255), 4))
				return false;
			for (uint32 y = 0; y < fb->height; ++y) {
				uint8 *dst = out.RowPtr(static_cast<int32>(y));
				for (uint32 x = 0; x < fb->width; ++x) {
					const math::NkColor c = fb->GetPixel(x, y);
					dst[x * 4u + 0] = c.r;
					dst[x * 4u + 1] = c.g;
					dst[x * 4u + 2] = c.b;
					dst[x * 4u + 3] = c.a;
				}
			}
			return true;
		}

		void NkRenderWindow::PrepareCapture() {
			if (!mContext)
				return;
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
			if (mContext->GetApi() == NkGraphicsApi::NK_GFX_API_METAL)
				static_cast<NkMetalContext *>(mContext)->KeepLastFrame(true);
#endif
		}

		bool NkRenderWindow::Capture(const char *path) const {
			if (!path || !*path || !mContext)
				return false;
			NkImage img;
			return CaptureToImage(img) && img.Save(path);
		}

		bool NkRenderWindow::CaptureToImage(NkImage &out) const {
			if (!mContext)
				return false;
			switch (mContext->GetApi()) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
				case NkGraphicsApi::NK_GFX_API_DX11:
					return NkReadbackDX11(mContext, out);
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
				case NkGraphicsApi::NK_GFX_API_METAL:
					return NkReadbackMetal(mContext, out);
#endif
				case NkGraphicsApi::NK_GFX_API_SOFTWARE:
					return NkReadbackSoftware(mContext, out);
				default:
					return false; // GL / DX12 / Vulkan : a venir
			}
		}

	} // namespace renderer
} // namespace nkentseu
