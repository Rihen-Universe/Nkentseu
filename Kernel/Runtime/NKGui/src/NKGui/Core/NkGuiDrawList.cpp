// =============================================================================
// NkGuiDrawList.cpp — primitives de dessin NKGui (Phase 2).
// =============================================================================
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKFont/NkFont.h"
#include <cmath>

namespace nkentseu {
    namespace nkgui {

        void NkGuiDrawList::Reset() noexcept {
            vtx.Clear();
            idx.Clear();
            cmds.Clear();
            clipDepth = 0;
        }

        void NkGuiDrawList::Append(const NkGuiDrawList& o) noexcept {
            if (o.idx.Size() == 0u) return;
            const uint32 vbase = static_cast<uint32>(vtx.Size());
            const uint32 ibase = static_cast<uint32>(idx.Size());
            for (uint32 i = 0; i < o.vtx.Size(); ++i) vtx.PushBack(o.vtx[i]);
            for (uint32 i = 0; i < o.idx.Size(); ++i) idx.PushBack(o.idx[i] + vbase);
            for (uint32 i = 0; i < o.cmds.Size(); ++i) {
                NkGuiDrawCmd c = o.cmds[i];
                c.idxOffset += ibase;
                cmds.PushBack(c);
            }
        }

        NkRect NkGuiDrawList::CurrentClip() const noexcept {
            return clipDepth > 0 ? clipStack[clipDepth - 1] : NkRect{ 0.f, 0.f, 1.0e9f, 1.0e9f };
        }

        void NkGuiDrawList::PushClipRect(const NkRect& r, bool intersect) noexcept {
            NkRect c = r;
            if (intersect && clipDepth > 0) {
                const NkRect& p = clipStack[clipDepth - 1];
                const float32 x1 = (c.x > p.x) ? c.x : p.x;
                const float32 y1 = (c.y > p.y) ? c.y : p.y;
                const float32 x2 = ((c.x + c.w) < (p.x + p.w)) ? (c.x + c.w) : (p.x + p.w);
                const float32 y2 = ((c.y + c.h) < (p.y + p.h)) ? (c.y + c.h) : (p.y + p.h);
                c = { x1, y1, x2 > x1 ? x2 - x1 : 0.f, y2 > y1 ? y2 - y1 : 0.f };
            }
            if (clipDepth < 32) clipStack[clipDepth++] = c;
        }

        void NkGuiDrawList::PopClipRect() noexcept {
            if (clipDepth > 0) --clipDepth;
        }

        NkGuiDrawCmd& NkGuiDrawList::CurCmd(uint32 texId) noexcept {
            const NkRect clip = CurrentClip();
            bool need = (cmds.Size() == 0);
            if (!need) {
                const NkGuiDrawCmd& b = cmds.Back();
                need = (b.texId != texId)
                    || b.clipRect.x != clip.x || b.clipRect.y != clip.y
                    || b.clipRect.w != clip.w || b.clipRect.h != clip.h;
            }
            if (need) {
                NkGuiDrawCmd c;
                c.texId     = texId;
                c.clipRect  = clip;
                c.idxOffset = static_cast<uint32>(idx.Size());
                c.idxCount  = 0;
                c.type      = texId ? NkGuiDrawCmdType::TexturedTriangles
                                    : NkGuiDrawCmdType::Triangles;
                cmds.PushBack(c);
            }
            return cmds.Back();
        }

        uint32 NkGuiDrawList::Vtx(const NkVec2& p, const NkVec2& uv, uint32 col) noexcept {
            NkGuiVertex v; v.pos = p; v.uv = uv; v.col = col;
            vtx.PushBack(v);
            return static_cast<uint32>(vtx.Size()) - 1u;
        }

        void NkGuiDrawList::Tri(uint32 a, uint32 b, uint32 c, uint32 texId) noexcept {
            NkGuiDrawCmd& cmd = CurCmd(texId);
            idx.PushBack(a);
            idx.PushBack(b);
            idx.PushBack(c);
            cmd.idxCount += 3u;
        }

        void NkGuiDrawList::AddRectFilled(const NkRect& r, const NkColor& col, float32 /*rounding*/) noexcept {
            // Phase 2 : coins droits. Le rounding (fans de coins) arrivera avec le
            // path-builder. La signature est en place pour ne pas casser l'API.
            if (r.w <= 0.f || r.h <= 0.f) return;
            const uint32 c = NkGuiPackColor(col);
            const NkVec2 uv{ 0.f, 0.f };
            const uint32 i0 = Vtx({ r.x,       r.y       }, uv, c);
            const uint32 i1 = Vtx({ r.x + r.w, r.y       }, uv, c);
            const uint32 i2 = Vtx({ r.x + r.w, r.y + r.h }, uv, c);
            const uint32 i3 = Vtx({ r.x,       r.y + r.h }, uv, c);
            Tri(i0, i1, i2, 0u);
            Tri(i0, i2, i3, 0u);
        }

        void NkGuiDrawList::AddTriangleMultiColor(const NkVec2& a, const NkVec2& b, const NkVec2& c,
                                                  const NkColor& ca, const NkColor& cb, const NkColor& cc) noexcept {
            // Triangle à couleurs de sommet (dégradé barycentrique) — roue de teinte +
            // triangle SV du color picker circulaire.
            const NkVec2 uv{ 0.f, 0.f };
            const uint32 i0 = Vtx(a, uv, NkGuiPackColor(ca));
            const uint32 i1 = Vtx(b, uv, NkGuiPackColor(cb));
            const uint32 i2 = Vtx(c, uv, NkGuiPackColor(cc));
            Tri(i0, i1, i2, 0u);
        }

        void NkGuiDrawList::AddImage(uint32 texId, const NkRect& r, const NkVec2& uv0, const NkVec2& uv1,
                                     const NkColor& tint) noexcept {
            // Quad TEXTURÉ (texId backend) — image/icône. La couleur de sommet `tint`
            // multiplie l'échantillon (blanc = image telle quelle). Même chemin que le
            // texte (TexturedTriangles), donc le backend résout déjà `texId`.
            if (r.w <= 0.f || r.h <= 0.f || texId == 0u) return;
            const uint32 c  = NkGuiPackColor(tint);
            const uint32 i0 = Vtx({ r.x,       r.y       }, { uv0.x, uv0.y }, c);
            const uint32 i1 = Vtx({ r.x + r.w, r.y       }, { uv1.x, uv0.y }, c);
            const uint32 i2 = Vtx({ r.x + r.w, r.y + r.h }, { uv1.x, uv1.y }, c);
            const uint32 i3 = Vtx({ r.x,       r.y + r.h }, { uv0.x, uv1.y }, c);
            Tri(i0, i1, i2, texId);
            Tri(i0, i2, i3, texId);
        }

        void NkGuiDrawList::AddRectFilledMultiColor(const NkRect& r, const NkColor& tl, const NkColor& tr,
                                                    const NkColor& br, const NkColor& bl) noexcept {
            // Quad à couleurs de coin (dégradé bilinéaire) — base du sélecteur de
            // couleur (carré SV, barre de teinte, barre alpha). uv=(0,0) → couleur unie.
            if (r.w <= 0.f || r.h <= 0.f) return;
            const NkVec2 uv{ 0.f, 0.f };
            const uint32 i0 = Vtx({ r.x,       r.y       }, uv, NkGuiPackColor(tl));
            const uint32 i1 = Vtx({ r.x + r.w, r.y       }, uv, NkGuiPackColor(tr));
            const uint32 i2 = Vtx({ r.x + r.w, r.y + r.h }, uv, NkGuiPackColor(br));
            const uint32 i3 = Vtx({ r.x,       r.y + r.h }, uv, NkGuiPackColor(bl));
            Tri(i0, i1, i2, 0u);
            Tri(i0, i2, i3, 0u);
        }

        void NkGuiDrawList::AddLine(const NkVec2& a, const NkVec2& b, const NkColor& col, float32 thickness) noexcept {
            float32 dx = b.x - a.x, dy = b.y - a.y;
            const float32 len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0e-4f) return;
            const float32 nx = -dy / len * thickness * 0.5f;
            const float32 ny =  dx / len * thickness * 0.5f;
            const uint32 c = NkGuiPackColor(col);
            const NkVec2 uv{ 0.f, 0.f };
            const uint32 i0 = Vtx({ a.x + nx, a.y + ny }, uv, c);
            const uint32 i1 = Vtx({ b.x + nx, b.y + ny }, uv, c);
            const uint32 i2 = Vtx({ b.x - nx, b.y - ny }, uv, c);
            const uint32 i3 = Vtx({ a.x - nx, a.y - ny }, uv, c);
            Tri(i0, i1, i2, 0u);
            Tri(i0, i2, i3, 0u);
        }

        void NkGuiDrawList::AddRect(const NkRect& r, const NkColor& col, float32 thickness) noexcept {
            // Bordure = 4 rectangles pleins tracés STRICTEMENT à l'intérieur du rect.
            // (AddLine centrerait l'épaisseur sur l'arête → la moitié extérieure sort
            // du rect et est rognée par le clip — scissor exclusif à droite/bas → bord
            // droit invisible/aminci.) Ici chaque bord tient dans [r.x, r.x+r.w] etc.
            const float32 t = thickness;
            AddRectFilled({ r.x,           r.y,           r.w, t   }, col); // haut
            AddRectFilled({ r.x,           r.y + r.h - t, r.w, t   }, col); // bas
            AddRectFilled({ r.x,           r.y,           t,   r.h }, col); // gauche
            AddRectFilled({ r.x + r.w - t, r.y,           t,   r.h }, col); // droite
        }

        void NkGuiDrawList::AddTriangleFilled(const NkVec2& a, const NkVec2& b, const NkVec2& c, const NkColor& col) noexcept {
            const uint32 cc = NkGuiPackColor(col);
            const NkVec2 uv{ 0.f, 0.f };
            const uint32 i0 = Vtx(a, uv, cc);
            const uint32 i1 = Vtx(b, uv, cc);
            const uint32 i2 = Vtx(c, uv, cc);
            Tri(i0, i1, i2, 0u);
        }

        void NkGuiDrawList::AddText(const NkFont* face, uint32 texId, const NkVec2& baseline,
                                    const char* text, const NkColor& col, float32 maxWidth) noexcept {
            if (!face || !text || !*text || texId == 0u) return;
            const uint32  c    = NkGuiPackColor(col);
            const char*   p    = text;
            const char*   end  = text;
            while (*end) ++end;                       // fin de chaîne (UTF-8)
            const float32 xEnd = (maxWidth >= 0.f) ? baseline.x + maxWidth : 1.0e30f;
            float32       x    = baseline.x;
            const float32 y    = baseline.y;

            while (p < end) {
                const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
                if (cp == 0u) break;
                const NkFontGlyph* g = face->FindGlyph(cp);
                if (!g) continue;
                if (g->visible) {
                    const float32 x0 = x + g->x0, y0 = y + g->y0;
                    const float32 x1 = x + g->x1, y1 = y + g->y1;
                    if (x1 > xEnd) break;             // troncature simple
                    const uint32 i0 = Vtx({ x0, y0 }, { g->u0, g->v0 }, c);
                    const uint32 i1 = Vtx({ x1, y0 }, { g->u1, g->v0 }, c);
                    const uint32 i2 = Vtx({ x1, y1 }, { g->u1, g->v1 }, c);
                    const uint32 i3 = Vtx({ x0, y1 }, { g->u0, g->v1 }, c);
                    Tri(i0, i1, i2, texId);
                    Tri(i0, i2, i3, texId);
                }
                x += g->advanceX;
            }
        }

        void NkGuiDrawList::AddTextRange(const NkFont* face, uint32 texId, const NkVec2& baseline,
                                         const char* begin, const char* end, const NkColor& col) noexcept {
            if (!face || !begin || begin >= end || texId == 0u) return;
            const uint32 c = NkGuiPackColor(col);
            const char*  p = begin;
            float32      x = baseline.x;
            const float32 y = baseline.y;
            while (p < end) {
                const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
                if (cp == 0u) break;
                const NkFontGlyph* g = face->FindGlyph(cp);
                if (!g) continue;
                if (g->visible) {
                    const float32 x0 = x + g->x0, y0 = y + g->y0;
                    const float32 x1 = x + g->x1, y1 = y + g->y1;
                    const uint32 i0 = Vtx({ x0, y0 }, { g->u0, g->v0 }, c);
                    const uint32 i1 = Vtx({ x1, y0 }, { g->u1, g->v0 }, c);
                    const uint32 i2 = Vtx({ x1, y1 }, { g->u1, g->v1 }, c);
                    const uint32 i3 = Vtx({ x0, y1 }, { g->u0, g->v1 }, c);
                    Tri(i0, i1, i2, texId);
                    Tri(i0, i2, i3, texId);
                }
                x += g->advanceX;
            }
        }

        void NkGuiDrawList::AddCircleFilled(const NkVec2& center, float32 r, const NkColor& col, int32 segs) noexcept {
            if (r <= 0.f) return;
            if (segs <= 0) {
                segs = static_cast<int32>(8.f * r / 4.f) + 8;
                if (segs < 12) segs = 12; else if (segs > 128) segs = 128;
            }
            const uint32 cc = NkGuiPackColor(col);
            const NkVec2 uv{ 0.f, 0.f };
            const uint32 ic = Vtx(center, uv, cc);
            const float32 kTau = 6.28318530718f;
            uint32 prev = Vtx({ center.x + r, center.y }, uv, cc);
            for (int32 s = 1; s <= segs; ++s) {
                const float32 ang = kTau * static_cast<float32>(s) / static_cast<float32>(segs);
                const uint32 cur = Vtx({ center.x + std::cos(ang) * r, center.y + std::sin(ang) * r }, uv, cc);
                Tri(ic, prev, cur, 0u);
                prev = cur;
            }
        }

    } // namespace nkgui
} // namespace nkentseu
