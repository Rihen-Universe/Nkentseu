#pragma once
// =============================================================================
// NkRenderTexture.h — NkRenderTarget concret : rendu offscreen vers une texture
//
// Equivalent de sf::RenderTexture. Cree un framebuffer offscreen + une color
// texture attachment via NkRenderTextureBackend (dispatch cross-API). Apres
// Display(), la texture est disponible via GetTexture() / GetGPUId() pour
// sampler en post-process / mini-map / blit UI / etc.
//
// CYCLE DE VIE
//   NkRenderTexture rt;
//   if (rt.Create(target.GetRenderer2D(), 512, 512)) {
//       rt.Clear({0, 0, 0, 255});
//       rt.Draw(scene);                          // dessin offscreen
//       rt.Display();                            // flush + unbind
//       uint32 texId = rt.GetColorTextureGPUId();// utilisable en NkTexture
//   }
//
// SUPPORT BACKEND (2026-05-30)
//   - OpenGL : FBO + color texture attachment GL_RGBA8 ✓ (livre)
//   - Software / Vulkan / DX11 / DX12 / Metal : stub (Create renvoie false).
//     Implementations differees — chaque backend doit poser sa logique de
//     framebuffer offscreen specifique (VkFramebuffer + VkRenderPass pour
//     Vulkan, ID3D11RenderTargetView pour DX11, etc.).
//
// API NkRenderTarget complete supportee : Clear/Display/SetView/Draw(...)
// (delegue au NkIRenderer2D du target principal, mais avec le framebuffer
// offscreen actif via Bind/Unbind dispatch).
// =============================================================================

#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"
#include "NKCanvas/Renderer/Targets/NkRenderTextureBackend.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"

namespace nkentseu {
    namespace renderer {

        class NkIRenderer2D;
        class NkTexture;

        class NkRenderTexture : public NkRenderTarget {
            public:
                NkRenderTexture() noexcept = default;
                ~NkRenderTexture() noexcept override { Destroy(); }

                NkRenderTexture(const NkRenderTexture&)            = delete;
                NkRenderTexture& operator=(const NkRenderTexture&) = delete;

                /// Cree le framebuffer offscreen via le dispatch backend actif.
                /// Le `renderer` du target principal doit avoir ete initialise
                /// (il a installe le NkRenderTextureBackend par son Initialize).
                bool Create(NkIRenderer2D& renderer, uint32 width, uint32 height);

                /// Libere le framebuffer (idempotent).
                void Destroy();

                bool   IsValid() const noexcept { return mHandle != 0; }
                uint32 GetHandle() const noexcept { return mHandle; }

                /// GPU id de la color texture (utilisable par NkTexture::SetGPUId
                /// ou par un shader user qui veut la sampler).
                uint32 GetColorTextureGPUId() const noexcept;

                // ── NkRenderTarget : implementations ───────────────────────────
                void Clear(const NkColor2D& color = NkColor2D::Black) override;
                void Display() override;

                void     SetView(const NkView2D& v) override;
                NkView2D GetView()       const override;
                NkView2D GetDefaultView() const override;

                void     SetViewport(NkRect2i v) override;
                NkRect2i GetViewport() const override;

                math::NkVec2u GetSize() const override { return {mWidth, mHeight}; }

                // Re-expose les surcharges Draw du parent (cf NkRenderWindow.h).
                using NkRenderTarget::Draw;

                void Draw(const NkVertex* vertices, uint32 count,
                          NkPrimitiveType primitive,
                          const NkRenderStates& states = NkRenderStates::Default()) override;

                NkVec2f MapPixelToCoords(NkVec2i pixel) const override;
                NkVec2i MapCoordsToPixel(NkVec2f point) const override;

                NkIRenderer2D*       GetRenderer()       noexcept override { return mRenderer; }
                const NkIRenderer2D* GetRenderer() const noexcept override { return mRenderer; }

                NkRenderer2D&        GetRenderer2D()       noexcept override { return mFacade; }
                const NkRenderer2D&  GetRenderer2D() const noexcept override { return mFacade; }

            private:
                uint32          mHandle{0};
                uint32          mWidth{0};
                uint32          mHeight{0};
                NkIRenderer2D*  mRenderer{nullptr}; ///< non-owning, ref vers le renderer principal
                NkRenderer2D    mFacade;
                NkView2D        mView{};
                NkRect2i        mViewport{};
                bool            mFrameOpen{false};
        };

    } // namespace renderer
} // namespace nkentseu
