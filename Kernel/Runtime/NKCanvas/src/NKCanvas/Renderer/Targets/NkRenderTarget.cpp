// =============================================================================
// NkRenderTarget.cpp — Implementations non-virtuelles (dispatch helpers).
//
// Pour les methodes Draw() qui ne sont pas pures virtuelles : elles delegent
// soit au submit raw vertices (Draw(NkVertexArray)) soit au renderer interne
// (Draw(NkIDrawable2D)). Les vraies virtuelles sont implementees dans les
// classes derivees (NkRenderWindow, NkRenderTexture).
// =============================================================================

#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"
#include "NKCanvas/Renderer/Core/NkVertexArray.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"

namespace nkentseu {
    namespace renderer {

        void NkRenderTarget::Draw(const NkVertexArray& va,
                                  const NkRenderStates& states) {
            const uint32 n = va.GetVertexCount();
            if (n == 0) return;
            // Delegue au submit raw vertices virtuel pur (impl par la cible concrete).
            Draw(va.Data(), n, va.GetPrimitiveType(), states);
        }

        void NkRenderTarget::Draw(const NkIDrawable2D& drawable) {
            // Compat : l'ancien drawable veut un NkIRenderer2D&. On lui passe le
            // notre. Le drawable est cense gerer ses Begin/Draw/End correctement.
            NkIRenderer2D* r = GetRenderer();
            if (r) drawable.Draw(*r);
        }

    } // namespace renderer
} // namespace nkentseu
