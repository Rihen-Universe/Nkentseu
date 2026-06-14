// =============================================================================
// NkRenderTexture.cpp — Render texture offscreen cross-API via dispatch.
//
// Toutes les operations passent par NkRenderTextureBackend (dispatch table)
// publie par le renderer principal a son Initialize. Le rendu offscreen
// reutilise le NkIRenderer2D du target principal : on bind le framebuffer
// offscreen avant Begin/Draw, on unbind apres Display.
// =============================================================================

#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"

namespace nkentseu {
    namespace renderer {

        // Active dispatch table (publie par chaque backend a Initialize).
        static NkRenderTextureBackend gRTBackend{};

        void NkRenderTextureSetBackend(const NkRenderTextureBackend& backend) {
            gRTBackend = backend;
        }

        static uint32 sUnsupportedRTCreate(uint32 /*w*/, uint32 /*h*/) { return 0; }
        void NkRenderTextureInstallUnsupportedBackend(const char* backendName) {
            (void)backendName; // reserve pour log futur
            NkRenderTextureBackend stub{};
            stub.Create = &sUnsupportedRTCreate;
            NkRenderTextureSetBackend(stub);
        }

        // =====================================================================
        // NkRenderTexture
        // =====================================================================

        bool NkRenderTexture::Create(NkIRenderer2D& renderer, uint32 width, uint32 height) {
            Destroy();
            if (!gRTBackend.Create) return false;
            mHandle = gRTBackend.Create(width, height);
            if (mHandle == 0) return false;
            mWidth  = width;
            mHeight = height;
            mRenderer = &renderer;
            mFacade.Bind(mRenderer, this);
            // View par defaut couvrant tout le framebuffer (origine top-left,
            // axe Y vers le bas — coherent avec NkRenderWindow).
            mView.center = {width * 0.5f, height * 0.5f};
            mView.size   = {(float)width, (float)height};
            mView.rotation = 0.f;
            mViewport = NkRect2i(0, 0, (int32)width, (int32)height);
            return true;
        }

        void NkRenderTexture::Destroy() {
            if (mHandle != 0 && gRTBackend.Destroy) {
                gRTBackend.Destroy(mHandle);
            }
            mHandle = 0;
            mWidth  = 0;
            mHeight = 0;
            mRenderer = nullptr;
            mFacade.Bind(nullptr, nullptr);
            mFrameOpen = false;
        }

        uint32 NkRenderTexture::GetColorTextureGPUId() const noexcept {
            if (!mHandle || !gRTBackend.GetColorTextureGPUId) return 0;
            return gRTBackend.GetColorTextureGPUId(mHandle);
        }

        void NkRenderTexture::Clear(const NkColor2D& color) {
            if (!mHandle || !mRenderer) return;
            if (gRTBackend.Bind) gRTBackend.Bind(mHandle);
            if (!mFrameOpen) {
                mRenderer->Begin();
                mFrameOpen = true;
            }
            mRenderer->Clear(color);
        }

        void NkRenderTexture::Display() {
            if (!mHandle || !mRenderer) return;
            if (mFrameOpen) {
                mRenderer->End();
                mFrameOpen = false;
            }
            // Restaure le framebuffer principal pour que les draws suivants
            // (sur le target principal) reprennent normalement.
            if (gRTBackend.Unbind) gRTBackend.Unbind();
        }

        void NkRenderTexture::SetView(const NkView2D& v)        { mView = v; if (mRenderer) mRenderer->SetView(v); }
        NkView2D NkRenderTexture::GetView()        const        { return mView; }
        NkView2D NkRenderTexture::GetDefaultView() const        {
            NkView2D dv;
            dv.center = {mWidth * 0.5f, mHeight * 0.5f};
            dv.size   = {(float)mWidth, (float)mHeight};
            return dv;
        }

        void     NkRenderTexture::SetViewport(NkRect2i v)       { mViewport = v; if (mRenderer) mRenderer->SetViewport(v); }
        NkRect2i NkRenderTexture::GetViewport() const           { return mViewport; }

        void NkRenderTexture::Draw(const NkVertex* vertices, uint32 count,
                                   NkPrimitiveType primitive,
                                   const NkRenderStates& states) {
            if (!mHandle || !mRenderer || !vertices || count == 0) return;
            if (gRTBackend.Bind) gRTBackend.Bind(mHandle);  // s'assure qu'on est sur l'offscreen
            if (!mFrameOpen) { mRenderer->Begin(); mFrameOpen = true; }

            mRenderer->SetBlendMode(states.blendMode);

            // Submit minimal — fait l'identite indices pour TRIANGLES, et delegue
            // l'expansion strip/fan au backend Renderer2D si ce dernier la supporte.
            // Note : NkRenderWindow:Draw gere les conversions strip/fan vers
            // TRIANGLES. Ici on prend le chemin court (TRIANGLES uniquement)
            // pour rester simple ; l'extension viendra si necessaire.
            if (primitive == NkPrimitiveType::NK_TRIANGLES) {
                // Stack indices identity (max 256, sinon fallback heap a faire).
                uint32 idxStack[256];
                if (count <= 256) {
                    for (uint32 i = 0; i < count; ++i) idxStack[i] = i;
                    mRenderer->DrawVertices(vertices, count, idxStack, count, states.texture);
                }
            }
            // Les autres primitive types pourront etre supportes en delegant
            // au NkRenderWindow::Draw helper logic (a factoriser).
        }

        NkVec2f NkRenderTexture::MapPixelToCoords(NkVec2i pixel) const {
            return mRenderer ? mRenderer->MapPixelToCoords(pixel) : NkVec2f{0.f, 0.f};
        }
        NkVec2i NkRenderTexture::MapCoordsToPixel(NkVec2f point) const {
            return mRenderer ? mRenderer->MapCoordsToPixel(point) : NkVec2i{0, 0};
        }

    } // namespace renderer
} // namespace nkentseu
