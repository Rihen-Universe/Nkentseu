#pragma once
// =============================================================================
// NkUICanvasBackend.h — Backend NKUI -> NkRenderer2D (NKCanvas)
// -----------------------------------------------------------------------------
// Rend les draw-lists immediate-mode de NKUI (NkUIContext) via le renderer 2D
// de NKCanvas (NkIRenderer2D). Equivalent de NkUINKRHIBackend mais sans la
// plomberie GPU bas-niveau : NkRenderer2D absorbe pipeline/buffers/clip/textures.
//
// Optionnel : compile uniquement si NK_CANVAS_WITH_NKUI=1 (cf. NKCanvas.jenga,
// flag env NK_CANVAS_NKUI). Les utilisateurs de NKCanvas restent libres de ne
// pas lier NKUI.
//
// Usage (immediate-mode, par frame) :
//   NkUICanvasBackend ui;
//   ui.Init(&renderer);                 // une fois
//   ui.UploadTextureGray8(fontTexId, atlas, w, h);   // atlas police NKUI
//   ...
//   target->Begin();
//   ui.Submit(uiContext, fbW, fbH);     // entre Begin()/End()
//   target->End();
// Le header n'inclut PAS NKUI (forward-declare NkUIContext) : toujours utilisable
// par les consommateurs. Seul le .cpp depend de NKUI (compile si NK_CANVAS_WITH_NKUI=1).
// =============================================================================

#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"   // NkVertex2D
#include "NKCanvas/Renderer/Resources/NkTexture.h"      // NkTexture
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace nkui { struct NkUIContext; }               // fwd

    namespace renderer {

        class NkIRenderer2D;

        // Bridge immediate-mode NKUI -> NKCanvas. Possede les textures uploadees
        // (atlas police / images), indexees par le texId NKUI.
        class NkUICanvasBackend {
            public:
                NkUICanvasBackend() = default;
                ~NkUICanvasBackend() { Destroy(); }

                NkUICanvasBackend(const NkUICanvasBackend&)            = delete;
                NkUICanvasBackend& operator=(const NkUICanvasBackend&) = delete;

                // renderer : NkIRenderer2D actif (cf. NkRenderWindow / NkRenderer2D).
                bool Init(NkIRenderer2D* renderer);
                void Destroy();

                // Rend toutes les couches du contexte NKUI. A appeler entre
                // Begin()/End() de la cible. fbW/fbH = taille framebuffer (clip).
                void Submit(const nkui::NkUIContext& ctx, uint32 fbW, uint32 fbH);

                // Upload / mise a jour d'une texture NKUI (texId != 0).
                // RGBA8 : data = w*h*4. Gray8 : data = w*h (etendu en RGBA blanc + alpha).
                bool UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height);
                bool UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height);
                bool HasTexture(uint32 texId) const noexcept;

            private:
                NkIRenderer2D*                    mRenderer = nullptr;
                NkHashMap<uint32, NkTexture*>     mTextures;       // texId -> texture (owned)
                NkVector<NkVertex2D>              mScratch;        // conversion par couche
                NkVector<uint8>                   mExpand;         // gray -> rgba scratch

                NkTexture* AcquireTexture(uint32 texId, int32 width, int32 height);
        };

    } // namespace renderer
} // namespace nkentseu
