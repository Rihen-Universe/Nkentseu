// =============================================================================
// NkGuiFont.cpp — chargement police + atlas (Phase 3).
// =============================================================================
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
    namespace nkgui {

        // Plage de glyphes ETENDUE (au-dela du Latin-1) pour l'Unicode courant :
        // Latin etendu, ponctuation generale, fleches, box-drawing, blocs, formes
        // geometriques. L'atlas saute les glyphes absents de la police.
        static const uint32* NkGlyphRanges() noexcept {
            static const uint32 kRanges[] = {
                0x0020, 0x00FF,   // Latin + Latin-1
                0x0100, 0x024F,   // Latin etendu-A/B
                0x0250, 0x02AF,   // API (phonetique)
                0x0300, 0x036F,   // diacritiques
                0x0370, 0x03FF,   // grec
                0x0400, 0x04FF,   // cyrillique
                0x2000, 0x206F,   // ponctuation generale (tirets, guillemets, puces...)
                0x2070, 0x209F,   // exposants / indices
                0x20A0, 0x20CF,   // symboles monetaires (€, £, ¥...)
                0x2100, 0x214F,   // symboles type-lettre (™, №, Ω...)
                0x2150, 0x218F,   // formes numeriques (fractions, chiffres romains)
                0x2190, 0x21FF,   // fleches
                0x2200, 0x22FF,   // operateurs mathematiques
                0x2300, 0x23FF,   // technique divers (symboles, fleches retour...)
                0x2460, 0x24FF,   // alphanumeriques encercles (①, Ⓐ...)
                0x2500, 0x257F,   // box-drawing
                0x2580, 0x259F,   // blocs
                0x25A0, 0x25FF,   // formes geometriques
                0x2600, 0x26FF,   // symboles divers (☀, ⚠, ★...)
                0x2700, 0x27BF,   // dingbats (✓, ✗, ✔, ✦...)
                0x2B00, 0x2BFF,   // fleches & symboles divers
                0xFB00, 0xFB06,   // ligatures latines (ﬀ, ﬁ, ﬂ...)
                0
            };
            return kRanges;
        }

        bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx) noexcept {
            atlas.Clear(); face = nullptr; pixels = nullptr;   // rechargeable
            NkFontConfig cfg; cfg.glyphRanges = NkGlyphRanges();
            face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
            if (!face) return false;
            if (!atlas.Build()) return false;
            int32 bpp = 0;
            atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
            dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
            return dirty;
        }

        bool NkGuiFont::LoadFromFile(const char* path, float32 sizePx) noexcept {
            if (!path || !*path) return false;
            atlas.Clear(); face = nullptr; pixels = nullptr;   // rechargeable
            NkFontConfig cfg; cfg.glyphRanges = NkGlyphRanges();
            face = atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &cfg);
            if (!face) return false;
            if (!atlas.Build()) return false;
            int32 bpp = 0;
            atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
            dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
            return dirty;
        }

    } // namespace nkgui
} // namespace nkentseu
