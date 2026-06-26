// =============================================================================
// NkGuiFont.cpp — chargement police + atlas (Phase 3).
// =============================================================================
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
    namespace nkgui {

        bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx) noexcept {
            face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx);
            if (!face) return false;
            if (!atlas.Build()) return false;
            int32 bpp = 0;
            atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
            dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
            return dirty;
        }

    } // namespace nkgui
} // namespace nkentseu
