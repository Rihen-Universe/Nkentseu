// =============================================================================
// Games/Common/NkoungFrame.h
// Contexte de rendu + entrée passé à chaque jeu Nkoung.
//
// Tout est RESPONSIVE : un jeu ne dessine jamais à des coordonnées fixes, mais
// calcule ses positions depuis (width,height) et la ZONE SÛRE (safe-area, pour
// les notches mobile / bords web). Le POINTEUR est unifié (souris OU 1er contact
// tactile) → les jeux marchent au doigt comme à la souris sur toutes les
// plateformes (Windows, Linux, Web, Android, HarmonyOS).
//
// Le rendu passe par le draw list NKUI (rects arrondis + texte + lignes/cercles),
// cohérent avec le menu. RAPPEL : le texte se dessine via NkUIFont::RenderText
// (pos.y = baseline), pas via NkUIDrawList::AddText (stub vide).
// =============================================================================
#pragma once

#ifndef NKOUNG_FRAME_H
#define NKOUNG_FRAME_H

#include "NKMath/NKMath.h"
#include "NKUI/NKUI.h"

namespace nkoung {

    struct NkoungFrame {
        nkentseu::nkui::NkUIDrawList* dl = nullptr;        // liste de dessin courante
        nkentseu::nkui::NkUIFont*     font = nullptr;      // police corps
        nkentseu::nkui::NkUIFont*     titleFont = nullptr; // police titres

        nkentseu::float32 width = 0.f, height = 0.f;       // taille de rendu complète
        // Zone sûre : rectangle où placer le contenu (insets appliqués).
        nkentseu::float32 safeX = 0.f, safeY = 0.f, safeW = 0.f, safeH = 0.f;

        // Pointeur unifié (souris ou 1er doigt).
        nkentseu::math::NkVec2f pointer { 0.f, 0.f };
        bool pointerDown = false;       // maintenu
        bool pointerPressed = false;    // appui (edge) cette frame
        bool pointerReleased = false;   // relâché (edge) cette frame

        // ── Rectangles / formes ───────────────────────────────────────────
        void Rect(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
                  const nkentseu::math::NkColor& c, nkentseu::float32 round = 0.f) const noexcept {
            if (dl) dl->AddRectFilled(nkentseu::math::NkFloatRect{ x, y, w, h }, c, round, round);
        }
        void Border(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
                    const nkentseu::math::NkColor& c, nkentseu::float32 th = 1.5f,
                    nkentseu::float32 round = 0.f) const noexcept {
            if (dl) dl->AddRect(nkentseu::math::NkFloatRect{ x, y, w, h }, c, th, round, round);
        }
        void Line(nkentseu::math::NkVec2f a, nkentseu::math::NkVec2f b,
                  const nkentseu::math::NkColor& c, nkentseu::float32 th = 2.f) const noexcept {
            if (dl) dl->AddLine(a, b, c, th);
        }
        void Circle(nkentseu::math::NkVec2f center, nkentseu::float32 r,
                    const nkentseu::math::NkColor& c, nkentseu::int32 segs = 0) const noexcept {
            if (dl) dl->AddCircleFilled(center, r, c, segs);
        }
        void CircleOutline(nkentseu::math::NkVec2f center, nkentseu::float32 r,
                           const nkentseu::math::NkColor& c, nkentseu::float32 th = 2.f,
                           nkentseu::int32 segs = 0) const noexcept {
            if (dl) dl->AddCircle(center, r, c, th, segs);
        }

        // ── Texte (RenderText : pos.y = baseline → on passe un Y de HAUT) ──
        void Text(nkentseu::nkui::NkUIFont* f, nkentseu::float32 x, nkentseu::float32 topY,
                  const char* s, const nkentseu::math::NkColor& c,
                  nkentseu::float32 maxW = -1.f) const noexcept {
            if (f && s && dl) f->RenderText(*dl, nkentseu::math::NkVec2f{ x, topY + f->metrics.ascender }, s, c, maxW);
        }
        void TextCentered(nkentseu::nkui::NkUIFont* f, nkentseu::float32 x, nkentseu::float32 w,
                          nkentseu::float32 topY, const char* s,
                          const nkentseu::math::NkColor& c) const noexcept {
            if (!f || !s) return;
            Text(f, x + (w - f->MeasureWidth(s)) * 0.5f, topY, s, c);
        }
        nkentseu::float32 TextW(nkentseu::nkui::NkUIFont* f, const char* s) const noexcept {
            return (f && s) ? f->MeasureWidth(s) : 0.f;
        }
        nkentseu::float32 LineH(nkentseu::nkui::NkUIFont* f) const noexcept {
            return f ? f->metrics.lineHeight : 14.f;
        }

        // ── Entrée ─────────────────────────────────────────────────────────
        bool PointIn(nkentseu::float32 x, nkentseu::float32 y,
                     nkentseu::float32 w, nkentseu::float32 h) const noexcept {
            return pointer.x >= x && pointer.x < x + w && pointer.y >= y && pointer.y < y + h;
        }

        // Bouton tactile/souris : dessine un rect arrondi + label centré.
        // Retourne true si activé (pointeur relâché dessus).
        bool Button(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
                    const char* label, const nkentseu::math::NkColor& bg,
                    const nkentseu::math::NkColor& bgHover, const nkentseu::math::NkColor& fg,
                    const nkentseu::math::NkColor* border = nullptr) const noexcept {
            const bool over = PointIn(x, y, w, h);
            Rect(x, y, w, h, over ? bgHover : bg, 8.f);
            if (border) Border(x, y, w, h, *border, 1.5f, 8.f);
            if (font && label)
                Text(font, x + (w - font->MeasureWidth(label)) * 0.5f,
                     y + (h - font->metrics.lineHeight) * 0.5f, label, fg);
            return over && pointerReleased;
        }
    };

}  // namespace nkoung

#endif // NKOUNG_FRAME_H
