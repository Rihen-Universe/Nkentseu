#pragma once
// =============================================================================
// NkTextDraw.h — Rendu de texte avec INTERCEPTION des codepoints "dessinables"
//   (box-drawing U+2500-257F, blocs U+2580-259F, formes geometriques U+25A0-25FF,
//   fleches U+2190-2193). Ces glyphes manquent aux polices embarquees -> on les
//   DESSINE en primitives (lignes/rects), independamment de la police. Le reste
//   du texte passe par AddTextRange (police). Partage par l'editeur et le terminal.
// =============================================================================
#include "NKGui/NKGui.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::nkgui;

    // Decode un codepoint UTF-8 ; avance p. 0xFFFD si invalide.
    inline uint32 NkDecodeU8(const char*& p, const char* end) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x80) { ++p; return c; }
        if ((c >> 5) == 0x6 && p + 1 < end) { uint32 cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(p[1]) & 0x3Fu); p += 2; return cp; }
        if ((c >> 4) == 0xE && p + 2 < end) { uint32 cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(p[1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(p[2]) & 0x3Fu); p += 3; return cp; }
        if ((c >> 3) == 0x1E && p + 3 < end) { uint32 cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(p[1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(p[2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(p[3]) & 0x3Fu); p += 4; return cp; }
        ++p; return 0xFFFDu;
    }

    inline bool NkIsDrawable(uint32 cp) {
        return (cp >= 0x2500u && cp <= 0x25FFu) || (cp >= 0x2190u && cp <= 0x2193u);
    }

    // Dessine le glyphe primitif pour `cp` dans la cellule (x, top, w, h).
    inline void NkDrawGlyphPrim(NkGuiDrawList& dl, uint32 cp, float32 x, float32 top, float32 w, float32 h, const NkColor& col) {
        const float32 mx = x + w * 0.5f, my = top + h * 0.5f, r = x + w, b = top + h, t = 1.3f;
        const NkColor a64{ col.r, col.g, col.b, 64 }, a110{ col.r, col.g, col.b, 110 }, a170{ col.r, col.g, col.b, 170 };
        auto H = [&](float32 x0, float32 x1) { dl.AddLine({ x0, my }, { x1, my }, col, t); };
        auto V = [&](float32 y0, float32 y1) { dl.AddLine({ mx, y0 }, { mx, y1 }, col, t); };
        switch (cp) {
            case 0x2500: H(x, r); break;                         // ─
            case 0x2502: V(top, b); break;                       // │
            case 0x250C: H(mx, r); V(my, b); break;              // ┌
            case 0x2510: H(x, mx); V(my, b); break;              // ┐
            case 0x2514: H(mx, r); V(top, my); break;            // └
            case 0x2518: H(x, mx); V(top, my); break;            // ┘
            case 0x251C: V(top, b); H(mx, r); break;             // ├
            case 0x2524: V(top, b); H(x, mx); break;             // ┤
            case 0x252C: H(x, r); V(my, b); break;               // ┬
            case 0x2534: H(x, r); V(top, my); break;             // ┴
            case 0x253C: H(x, r); V(top, b); break;              // ┼
            case 0x2550: dl.AddLine({ x, my - 2 }, { r, my - 2 }, col, t); dl.AddLine({ x, my + 2 }, { r, my + 2 }, col, t); break;          // ═
            case 0x2551: dl.AddLine({ mx - 2, top }, { mx - 2, b }, col, t); dl.AddLine({ mx + 2, top }, { mx + 2, b }, col, t); break;      // ║
            case 0x2588: dl.AddRectFilled({ x, top, w, h }, col); break;            // █
            case 0x2580: dl.AddRectFilled({ x, top, w, h * 0.5f }, col); break;     // ▀
            case 0x2584: dl.AddRectFilled({ x, my, w, h * 0.5f }, col); break;      // ▄
            case 0x258C: dl.AddRectFilled({ x, top, w * 0.5f, h }, col); break;     // ▌
            case 0x2590: dl.AddRectFilled({ mx, top, w * 0.5f, h }, col); break;    // ▐
            case 0x2591: dl.AddRectFilled({ x, top, w, h }, a64); break;            // ░
            case 0x2592: dl.AddRectFilled({ x, top, w, h }, a110); break;           // ▒
            case 0x2593: dl.AddRectFilled({ x, top, w, h }, a170); break;           // ▓
            case 0x25A0: dl.AddRectFilled({ x + w * 0.2f, top + h * 0.28f, w * 0.6f, h * 0.45f }, col); break;  // ■
            case 0x25A1: dl.AddRect({ x + w * 0.2f, top + h * 0.28f, w * 0.6f, h * 0.45f }, col, t); break;     // □
            case 0x25CF: dl.AddCircleFilled({ mx, my }, w * 0.3f, col); break;      // ●
            case 0x25B2: dl.AddTriangleFilled({ mx, top + h * 0.28f }, { x + w * 0.25f, b - h * 0.28f }, { r - w * 0.25f, b - h * 0.28f }, col); break;  // ▲
            case 0x25BC: dl.AddTriangleFilled({ x + w * 0.25f, top + h * 0.28f }, { r - w * 0.25f, top + h * 0.28f }, { mx, b - h * 0.28f }, col); break;  // ▼
            case 0x2190: H(x, r); dl.AddTriangleFilled({ x, my }, { x + w * 0.3f, my - h * 0.16f }, { x + w * 0.3f, my + h * 0.16f }, col); break;  // ←
            case 0x2192: H(x, r); dl.AddTriangleFilled({ r, my }, { r - w * 0.3f, my - h * 0.16f }, { r - w * 0.3f, my + h * 0.16f }, col); break;  // →
            case 0x2191: V(top, b); dl.AddTriangleFilled({ mx, top }, { mx - w * 0.16f, top + h * 0.3f }, { mx + w * 0.16f, top + h * 0.3f }, col); break;  // ↑
            case 0x2193: V(top, b); dl.AddTriangleFilled({ mx, b }, { mx - w * 0.16f, b - h * 0.3f }, { mx + w * 0.16f, b - h * 0.3f }, col); break;        // ↓
            default:     dl.AddRect({ x + w * 0.22f, top + h * 0.3f, w * 0.56f, h * 0.4f }, col, 1.f); break;   // autres : petit cadre
        }
    }

    // Rend [begin,end) en interceptant les codepoints dessinables (primitives) ;
    // le reste via la police. Retourne le x final. (cellTop, cellH) = la ligne.
    inline float32 NkDrawTextU(NkGuiContext& ctx, float32 x, float32 baseline, float32 cellTop, float32 cellH,
                               const char* begin, const char* end, const NkColor& col) {
        if (!ctx.font || !ctx.font->Valid()) return x;
        const NkFont* face = ctx.font->Face(); const uint32 tex = ctx.font->TexId();
        const char* run = begin; const char* p = begin;
        auto flush = [&](const char* e) { if (e > run) { ctx.DL().AddTextRange(face, tex, { x, baseline }, run, e, col); x += face->CalcTextSizeX(run, e); } };
        while (p < end) {
            const char* q = p;
            const uint32 cp = NkDecodeU8(q, end);
            // Primitive UNIQUEMENT si la police n'a pas le glyphe (DejaVu a le
            // box-drawing -> on le laisse rendre ; DroidSans non -> on dessine).
            if (NkIsDrawable(cp) && !face->FindGlyphNoFallback(cp)) {
                flush(p);
                const float32 w = face->CalcTextSizeX(p, q);     // avance (glyphe de repli)
                NkDrawGlyphPrim(ctx.DL(), cp, x, cellTop, w, cellH, col);
                x += w; run = q;
            }
            p = q;
        }
        flush(p);
        return x;
    }

} // namespace nkcode
