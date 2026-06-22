// =============================================================================
// NkImGuiCanvasBackend.cpp — Backend de rendu Dear ImGui -> NkIRenderer2D (NKCanvas)
// =============================================================================
#include "NkImGuiCanvasBackend.h"

#include "imgui.h"                                    // ImDrawData/ImDrawList/ImDrawCmd/ImDrawVert/ImFontAtlas
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"     // NkIRenderer2D : DrawVertices/SetClip/PopClip
#include "NKCanvas/Renderer/Resources/NkTexture.h"    // NkTexture : Create/Update/SetFilter
#include "NKMemory/NKMemory.h"                         // memory::NkGetDefaultAllocator

namespace nkentseu {
    namespace renderer {

        bool NkImGuiCanvasBackend::Init(NkIRenderer2D* renderer) {
            if (!renderer) return false;
            mRenderer = renderer;
            return true;
        }

        void NkImGuiCanvasBackend::Destroy() {
            auto& alloc = memory::NkGetDefaultAllocator();
            if (mFontTex) {
                // Detacher l'atlas de l'eventuel contexte ImGui encore vivant pour
                // ne pas laisser une ImTextureID pendante.
                if (ImGui::GetCurrentContext()) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (io.Fonts && io.Fonts->TexID == reinterpret_cast<ImTextureID>(mFontTex)) {
                        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(0));
                    }
                }
                alloc.Delete(mFontTex);
                mFontTex = nullptr;
            }
            mScratchVtx.Clear();
            mScratchIdx.Clear();
            mRenderer = nullptr;
        }

        bool NkImGuiCanvasBackend::RebuildFontAtlas() {
            if (!mRenderer) return false;
            if (!ImGui::GetCurrentContext()) return false;

            ImGuiIO& io = ImGui::GetIO();
            if (!io.Fonts) return false;

            unsigned char* pixels = nullptr;
            int width = 0, height = 0, bpp = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);  // RGBA8
            if (!pixels || width <= 0 || height <= 0) return false;

            auto& alloc = memory::NkGetDefaultAllocator();

            // (Re)creer la texture si necessaire (taille differente -> recreer).
            if (mFontTex &&
                (mFontTex->GetWidth() != static_cast<uint32>(width) ||
                 mFontTex->GetHeight() != static_cast<uint32>(height))) {
                alloc.Delete(mFontTex);
                mFontTex = nullptr;
            }
            if (!mFontTex) {
                mFontTex = alloc.New<NkTexture>();
                if (!mFontTex) return false;
                if (!mFontTex->Create(*mRenderer, static_cast<uint32>(width), static_cast<uint32>(height))) {
                    alloc.Delete(mFontTex);
                    mFontTex = nullptr;
                    return false;
                }
                mFontTex->SetFilter(NkTextureFilter::NK_LINEAR);
            }

            if (!mFontTex->Update(reinterpret_cast<const uint8*>(pixels),
                                  static_cast<uint32>(width), static_cast<uint32>(height), 0, 0)) {
                return false;
            }

            // ImGui retrouvera la texture via cette ID dans chaque ImDrawCmd.
            io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(mFontTex));
            return true;
        }

        NkTexture* NkImGuiCanvasBackend::ResolveTexture(ImTextureID texId) const noexcept {
            // Convention de ce backend : l'ImTextureID EST le pointeur NkTexture
            // (l'atlas police, ou toute texture utilisateur passee a ImGui::Image()).
            return reinterpret_cast<NkTexture*>(texId);
        }

        void NkImGuiCanvasBackend::RenderDrawData(const ImDrawData* drawData, uint32 fbW, uint32 fbH) {
            if (!mRenderer || !drawData || drawData->CmdListsCount <= 0) return;
            if (fbW == 0u || fbH == 0u) return;

            // ImGui peut rendre dans un espace decale (DisplayPos) et a une echelle
            // framebuffer differente (FramebufferScale, ex. Retina 2x). On ramene
            // positions et clip-rects en pixels framebuffer.
            const float32 clipOffX = drawData->DisplayPos.x;
            const float32 clipOffY = drawData->DisplayPos.y;
            const float32 clipScaleX = drawData->FramebufferScale.x != 0.f ? drawData->FramebufferScale.x : 1.f;
            const float32 clipScaleY = drawData->FramebufferScale.y != 0.f ? drawData->FramebufferScale.y : 1.f;

            const float32 fbWf = static_cast<float32>(fbW);
            const float32 fbHf = static_cast<float32>(fbH);

            for (int n = 0; n < drawData->CmdListsCount; ++n) {
                const ImDrawList* cmdList = drawData->CmdLists[n];
                if (!cmdList) continue;

                const int vtxCount = cmdList->VtxBuffer.Size;
                const int idxCount = cmdList->IdxBuffer.Size;
                const int cmdCount = cmdList->CmdBuffer.Size;
                if (vtxCount <= 0 || idxCount <= 0 || cmdCount <= 0) continue;

                const ImDrawVert* vtxSrc = cmdList->VtxBuffer.Data;
                const ImDrawIdx*  idxSrc = cmdList->IdxBuffer.Data;

                // Conversion ImDrawVert -> NkVertex2D (une fois par cmd-list).
                // col = IM_COL32 = R | G<<8 | B<<16 | A<<24  (meme layout que NkUI).
                mScratchVtx.Resize(static_cast<usize>(vtxCount));
                for (int i = 0; i < vtxCount; ++i) {
                    const ImDrawVert& s = vtxSrc[i];
                    NkVertex2D& d = mScratchVtx[static_cast<usize>(i)];
                    d.x = (s.pos.x - clipOffX) * clipScaleX;
                    d.y = (s.pos.y - clipOffY) * clipScaleY;
                    d.u = s.uv.x;
                    d.v = s.uv.y;
                    d.r = static_cast<uint8>( s.col        & 0xFFu);
                    d.g = static_cast<uint8>((s.col >>  8) & 0xFFu);
                    d.b = static_cast<uint8>((s.col >> 16) & 0xFFu);
                    d.a = static_cast<uint8>((s.col >> 24) & 0xFFu);
                }

                for (int ci = 0; ci < cmdCount; ++ci) {
                    const ImDrawCmd& dc = cmdList->CmdBuffer[ci];

                    // Callback utilisateur : on ne sait pas l'executer ici (pas de
                    // contexte de commande bas-niveau). On l'ignore (sans le cas
                    // special ResetRenderState d'ImGui qui n'a pas de sens en mode
                    // renderer 2D haut-niveau).
                    if (dc.UserCallback != nullptr) continue;
                    if (dc.ElemCount == 0u) continue;

                    // Clip : ClipRect (x1,y1,x2,y2) en coords ImGui -> pixels fb.
                    float32 x0 = (dc.ClipRect.x - clipOffX) * clipScaleX;
                    float32 y0 = (dc.ClipRect.y - clipOffY) * clipScaleY;
                    float32 x1 = (dc.ClipRect.z - clipOffX) * clipScaleX;
                    float32 y1 = (dc.ClipRect.w - clipOffY) * clipScaleY;
                    if (x0 < 0.f) x0 = 0.f;
                    if (y0 < 0.f) y0 = 0.f;
                    if (x1 > fbWf) x1 = fbWf;
                    if (y1 > fbHf) y1 = fbHf;
                    if (x1 <= x0 || y1 <= y0) continue;   // clip vide -> rien a dessiner

                    mRenderer->SetClip(NkRect2i{ static_cast<int32>(x0), static_cast<int32>(y0),
                                                 static_cast<int32>(x1 - x0), static_cast<int32>(y1 - y0) });

                    // Indices : NkIRenderer2D::DrawVertices attend des uint32 ; les
                    // ImDrawIdx sont des uint16. On reconstruit la plage [IdxOffset,
                    // IdxOffset+ElemCount) en ajoutant VtxOffset (ImGui >=1.71), de
                    // sorte que les indices referencent bien mScratchVtx (cmd-list).
                    mScratchIdx.Resize(static_cast<usize>(dc.ElemCount));
                    const uint32 vtxOffset = dc.VtxOffset;
                    for (uint32 i = 0; i < dc.ElemCount; ++i) {
                        mScratchIdx[static_cast<usize>(i)] =
                            static_cast<uint32>(idxSrc[dc.IdxOffset + i]) + vtxOffset;
                    }

                    NkTexture* tex = ResolveTexture(dc.GetTexID());

                    mRenderer->DrawVertices(mScratchVtx.Data(), static_cast<uint32>(vtxCount),
                                            mScratchIdx.Data(), dc.ElemCount, tex);

                    mRenderer->PopClip();
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu
