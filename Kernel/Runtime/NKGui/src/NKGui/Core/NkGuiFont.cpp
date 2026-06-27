// =============================================================================
// NkGuiFont.cpp — chargement police + atlas (Phase 3).
// =============================================================================
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
    namespace nkgui {

        bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx) noexcept {
            // Plage de glyphes ETENDUE (au-dela du Latin-1) pour l'Unicode courant :
            // Latin etendu, ponctuation generale (guillemets/tirets/puces/points de
            // suspension), fleches, box-drawing, blocs, formes geometriques. L'atlas
            // saute les glyphes absents de la police (DroidSans). box-drawing/CJK/emoji
            // complets necessitent une police plus fournie (a embarquer plus tard).
            static const uint32 kRanges[] = {
                0x0020, 0x00FF,   // Latin + Latin-1
                0x0100, 0x024F,   // Latin etendu-A
                0x2010, 0x205F,   // ponctuation generale
                0x2190, 0x21FF,   // fleches
                0x2500, 0x257F,   // box-drawing
                0x2580, 0x259F,   // blocs
                0x25A0, 0x25FF,   // formes geometriques
                0
            };
            NkFontConfig cfg; cfg.glyphRanges = kRanges;
            face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
            if (!face) return false;
            if (!atlas.Build()) return false;
            int32 bpp = 0;
            atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
            dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
            return dirty;
        }

    } // namespace nkgui
} // namespace nkentseu
