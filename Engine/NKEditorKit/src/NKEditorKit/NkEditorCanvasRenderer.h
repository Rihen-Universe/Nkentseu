#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorCanvasRenderer.h
// @Brief   Impl NKCanvas de NkIEditorRenderer (backend de rendu PAR DEFAUT).
// @License Proprietary - Free to use and modify
//
// Enveloppe le chemin de rendu historique de la coquille : NkRenderWindow
// (contexte GPU + cible fenetre, NKCanvas) + NkGuiCanvasBackend (draw-lists NKGui
// -> NkIRenderer2D). C'est le backend de l'IDE (NKCode). Comportement IDENTIQUE a
// l'ancien NkEditorShell couple NKCanvas.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKWindow/NKWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkentseu {
    namespace editorkit {

        class NkEditorCanvasRenderer final : public NkIEditorRenderer {
            public:
                bool Init(NkWindow& window, NkEditorGfxApi api) override {
                    NkContextDesc desc;
                    switch (api) {
                        case NkEditorGfxApi::OpenGL:   desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;   break;
                        case NkEditorGfxApi::Vulkan:   desc.api = NkGraphicsApi::NK_GFX_API_VULKAN;   break;
                        case NkEditorGfxApi::DX11:     desc.api = NkGraphicsApi::NK_GFX_API_DX11;     break;
                        case NkEditorGfxApi::DX12:     desc.api = NkGraphicsApi::NK_GFX_API_DX12;     break;
                        case NkEditorGfxApi::Software: desc.api = NkGraphicsApi::NK_GFX_API_SOFTWARE; break;
                        default:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                            desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
                            desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
                            break;
                    }
                    mTarget = memory::NkMakeUnique<renderer::NkRenderWindow>(window, desc);
                    if (!mTarget || !mTarget->IsValid()) { mTarget.Reset(); return false; }
                    return mBackend.Init(mTarget->GetRenderer());
                }

                void Shutdown() override { mTarget.Reset(); }
                bool IsValid() const override { return mTarget && mTarget->IsValid(); }

                math::NkVec2u Size() const override {
                    return mTarget ? mTarget->GetSize() : math::NkVec2u{0, 0};
                }
                void OnResize(uint32 w, uint32 h) override { if (mTarget) mTarget->OnResize(w, h); }

                void BeginFrame() override { if (mTarget) mTarget->Clear(); }
                void SubmitDrawList(const nkgui::NkGuiDrawList& dl, uint32 fbW, uint32 fbH) override {
                    mBackend.Submit(dl, fbW, fbH);
                }
                void EndFrame() override { if (mTarget) mTarget->Display(); }

                bool UploadFontGray8(uint32 texId, const uint8* px, int32 w, int32 h) override {
                    return mBackend.UploadFontGray8(texId, px, w, h);
                }
                bool UploadImageRGBA(uint32 texId, const uint8* px, int32 w, int32 h) override {
                    return mBackend.UploadImageRGBA(texId, px, w, h);
                }

            private:
                memory::NkUniquePtr<renderer::NkRenderWindow> mTarget;
                renderer::NkGuiCanvasBackend                  mBackend;
        };

    } // namespace editorkit
} // namespace nkentseu
