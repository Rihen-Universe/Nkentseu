// =============================================================================
// NkGuiFont.cpp — chargement police + atlas (Phase 3).
// =============================================================================
#include "NKGui/Core/NkGuiFont.h"
#include <cstdio>
#include <cstring>

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

        // Plages couvertes par la police de REPLI « broad » (large couverture).
        static const uint32* NkBroadRanges() noexcept {
            static const uint32 r[] = {
                0x0020, 0x024F, 0x0250, 0x02FF, 0x0300, 0x036F,   // Latin, API, diacritiques
                0x0370, 0x052F, 0x0590, 0x05FF, 0x0600, 0x06FF,   // grec, cyrillique, hebreu, arabe
                0x1E00, 0x1EFF, 0x1F00, 0x1FFF,                   // latin etendu add., grec etendu
                0x2000, 0x2BFF, 0xFB00, 0xFB4F, 0
            };
            return r;
        }
        static const uint32* NkCjkRanges() noexcept {
            static const uint32 r[] = {
                0x2E80, 0x2EFF, 0x3000, 0x303F, 0x3040, 0x30FF,   // radicaux, ponctuation, kana
                0x3100, 0x312F, 0x3130, 0x318F, 0x31A0, 0x31FF,   // bopomofo, jamo, ext bopomofo
                0x3400, 0x4DBF, 0x4E00, 0x9FFF,                   // ext-A + unified (ideogrammes)
                0xAC00, 0xD7A3, 0xF900, 0xFAFF, 0xFF00, 0xFFEF, 0 // hangul, compat, pleine chasse
            };
            return r;
        }
        static const uint32* NkEmojiRanges() noexcept {
            static const uint32 r[] = {
                0x2600, 0x27BF, 0x2B00, 0x2BFF,                   // symboles divers, dingbats
                0x1F000, 0x1FAFF, 0                               // emoji (mahjong, pictogrammes, emoticones...)
            };
            return r;
        }

        // Chemins des polices de repli EXTERNES (poses par l'app via NkSetFallbackFontPaths).
        static char gFbBroad[600] = {}, gFbCjk[600] = {}, gFbEmoji[600] = {};
        static void NkCopyPath(char* dst, const char* src) { dst[0] = '\0'; if (src) { usize i = 0; for (; src[i] && i + 1 < 600; ++i) dst[i] = src[i]; dst[i] = '\0'; } }
        void NkSetFallbackFontPaths(const char* broad, const char* cjk, const char* emoji) noexcept {
            NkCopyPath(gFbBroad, broad); NkCopyPath(gFbCjk, cjk); NkCopyPath(gFbEmoji, emoji);
        }
        static bool NkFileExists(const char* p) noexcept { if (!p || !*p) return false; std::FILE* f = std::fopen(p, "rb"); if (f) { std::fclose(f); return true; } return false; }

        // Fusionne les polices de repli externes presentes (merge -> comble les manquants).
        static void NkMergeFallback(NkFontAtlas& atlas, float32 sizePx) noexcept {
            auto add = [&](const char* path, const uint32* ranges) {
                if (!NkFileExists(path)) return;
                NkFontConfig fb; fb.glyphRanges = ranges; fb.mergeMode = true;
                atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &fb);
            };
            add(gFbBroad, NkBroadRanges());
            add(gFbCjk,   NkCjkRanges());
            add(gFbEmoji, NkEmojiRanges());
        }

        bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx) noexcept {
            atlas.Clear(); face = nullptr; pixels = nullptr;   // rechargeable
            NkFontConfig cfg; cfg.glyphRanges = NkGlyphRanges();
            face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
            if (!face) return false;
            NkMergeFallback(atlas, sizePx);            // repli pour les glyphes manquants
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
            NkMergeFallback(atlas, sizePx > 0.f ? sizePx : 16.f);
            if (!atlas.Build()) return false;
            int32 bpp = 0;
            atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
            dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
            return dirty;
        }

    } // namespace nkgui
} // namespace nkentseu
