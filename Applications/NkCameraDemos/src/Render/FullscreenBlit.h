#pragma once
// =============================================================================
// FullscreenBlit.h
// -----------------------------------------------------------------------------
// Shader + VAO/VBO d'un quad unitaire repositionne en NDC via l'uniform uRect
// (x, y, w, h). Sert a "blitter" une CameraGLTexture en plein ecran ou en
// quadrant 2x2.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace cameradem
    {

        // ─────────────────────────────────────────────────────────────────────
        // FullscreenBlit — dessine une texture 2D dans une zone NDC (x, y, w, h).
        // Coordinate convention NDC : (-1, -1) bottom-left, (+1, +1) top-right.
        // Pour plein ecran : Draw(tex, -1, -1, 2, 2).
        // Le shader inverse les V pour que les frames camera (row 0 = haut)
        // apparaissent droites a l'ecran.
        // ─────────────────────────────────────────────────────────────────────
        class FullscreenBlit
        {
        public:
            FullscreenBlit()  = default;
            ~FullscreenBlit();

            // Compile shader + cree VAO/VBO. Retourne false sur echec.
            bool Initialize();
            // Libere program + buffers.
            void Shutdown();

            // Efface le framebuffer (couleur uniquement).
            void Clear(float r, float g, float b);

            // Dessine la texture texId dans le rect NDC (x, y, w, h).
            // No-op si texId == 0 ou Initialize n'a pas reussi.
            void Draw(uint32 texId, float x, float y, float w, float h);

            bool IsValid() const noexcept { return mProgram != 0 && mVAO != 0; }

        private:
            uint32 mProgram   = 0;
            uint32 mVAO       = 0;
            uint32 mVBO       = 0;
            int32  mLocRect   = -1;
            int32  mLocTex    = -1;
        };

    } // namespace cameradem
} // namespace nkentseu
