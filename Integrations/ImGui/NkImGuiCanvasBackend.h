#pragma once
// =============================================================================
// NkImGuiCanvasBackend.h — Backend de rendu Dear ImGui -> NkIRenderer2D (NKCanvas)
// -----------------------------------------------------------------------------
// Rend les ImDrawData* produits par Dear ImGui via le renderer 2D haut-niveau
// de NKCanvas (NkIRenderer2D). C'est l'equivalent ImGui de NkUICanvasBackend :
// NkRenderer2D absorbe la plomberie GPU (pipeline/buffers/clip/textures), on se
// contente de convertir la geometrie ImGui et d'emettre des DrawVertices.
//
// DECOUPLAGE : ce header n'inclut PAS imgui.h (forward-declare ImDrawData +
// le typedef ImTextureID). ImGui n'est donc PAS lie dans le coeur de NKCanvas :
// c'est l'application consommatrice qui compile cette integration. Les
// utilisateurs qui ne veulent pas ImGui ne paient rien.
//
// Usage (immediate-mode, par frame) :
//   NkImGuiCanvasBackend imgui;
//   imgui.Init(&renderer);                 // une fois
//   imgui.RebuildFontAtlas();              // upload io.Fonts -> texture, SetTexID
//   ...
//   ImGui::NewFrame(); ...; ImGui::Render();
//   target->Begin();
//   imgui.RenderDrawData(ImGui::GetDrawData(), fbW, fbH);  // entre Begin()/End()
//   target->End();
// =============================================================================

#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"   // NkVertex2D
#include "NKContainers/Sequential/NkVector.h"

// --- Forward declarations ImGui (pas d'include d'imgui.h dans le .h) ---------
struct ImDrawData;
#ifndef ImTextureID
typedef void* ImTextureID;   // doit correspondre au typedef d'imgui.h (void*)
#endif

namespace nkentseu {
    namespace renderer {

        class NkIRenderer2D;
        class NkTexture;

        // Bridge immediate-mode Dear ImGui -> NKCanvas. Possede la texture de
        // l'atlas de police (creee a RebuildFontAtlas). La ImTextureID stockee
        // par ImGui pointe directement sur la NkTexture correspondante.
        class NkImGuiCanvasBackend {
            public:
                NkImGuiCanvasBackend() = default;
                ~NkImGuiCanvasBackend() { Destroy(); }

                NkImGuiCanvasBackend(const NkImGuiCanvasBackend&)            = delete;
                NkImGuiCanvasBackend& operator=(const NkImGuiCanvasBackend&) = delete;

                // renderer : NkIRenderer2D actif (cf. NkRenderWindow / NkRenderer2D).
                bool Init(NkIRenderer2D* renderer);
                void Destroy();

                // (Re)construit la texture de l'atlas de police depuis le contexte
                // ImGui courant : io.Fonts->GetTexDataAsRGBA32 -> NkTexture ->
                // io.Fonts->SetTexID(texture). A appeler une fois apres Init() (et
                // a nouveau si les polices changent). Necessite un contexte ImGui actif.
                bool RebuildFontAtlas();

                // Rend toutes les cmd-lists de drawData. A appeler entre Begin()/End()
                // de la cible. fbW/fbH = taille framebuffer en pixels (clip + projection).
                // drawData peut etre nul (no-op).
                void RenderDrawData(const ImDrawData* drawData, uint32 fbW, uint32 fbH);

            private:
                NkIRenderer2D*       mRenderer  = nullptr;
                NkTexture*           mFontTex   = nullptr;   // atlas police (owned)
                NkVector<NkVertex2D> mScratchVtx;            // conversion ImDrawVert -> NkVertex2D
                NkVector<uint32>     mScratchIdx;            // conversion ImDrawIdx(u16) -> u32

                // Retrouve la NkTexture associee a une ImTextureID ImGui (== ptr NkTexture).
                NkTexture* ResolveTexture(ImTextureID texId) const noexcept;
        };

    } // namespace renderer
} // namespace nkentseu
