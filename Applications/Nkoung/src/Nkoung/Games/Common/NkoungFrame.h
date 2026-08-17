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
// Le rendu passe par la draw-list NKGui (rects arrondis + texte + lignes/cercles),
// cohérente avec le menu. RAPPEL : NkGuiDrawList::AddText attend une BASELINE ;
// les helpers Text* prennent un Y de HAUT et ajoutent l'ascent de la police.
// (Portage NKUI -> NKGui, campagne de retrait NKUI 2026-08.)
// =============================================================================
#pragma once

#ifndef NKOUNG_FRAME_H
#define NKOUNG_FRAME_H

#include "NKMath/NKMath.h"
#include "NKGui/NKGui.h"

namespace nkoung {

	struct NkoungFrame {
			nkentseu::nkgui::NkGuiDrawList *dl = nullptr;   // liste de dessin courante
			nkentseu::nkgui::NkGuiFont *font = nullptr;	    // police corps
			nkentseu::nkgui::NkGuiFont *titleFont = nullptr; // police titres

			nkentseu::float32 width = 0.f, height = 0.f; // taille de rendu complète
			// Zone sûre : rectangle où placer le contenu (insets appliqués).
			nkentseu::float32 safeX = 0.f, safeY = 0.f, safeW = 0.f, safeH = 0.f;

			// Pointeur unifié (souris ou 1er doigt).
			nkentseu::math::NkVec2f pointer{0.f, 0.f};
			bool pointerDown = false;	  // maintenu
			bool pointerPressed = false;  // appui (edge) cette frame
			bool pointerReleased = false; // relâché (edge) cette frame

			// ── Rectangles / formes ───────────────────────────────────────────
			void Rect(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
					  const nkentseu::math::NkColor &c, nkentseu::float32 round = 0.f) const noexcept {
				if (dl)
					dl->AddRectFilled(nkentseu::math::NkFloatRect{x, y, w, h}, c, round);
			}

			// NkGuiDrawList::AddRect n'a pas d'arrondi : le contour est carré (le fond
			// Rect, lui, garde ses coins arrondis). Différence visuelle assumée au portage.
			void Border(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
						const nkentseu::math::NkColor &c, nkentseu::float32 th = 1.5f,
						nkentseu::float32 round = 0.f) const noexcept {
				(void)round;
				if (dl)
					dl->AddRect(nkentseu::math::NkFloatRect{x, y, w, h}, c, th);
			}

			void Line(nkentseu::math::NkVec2f a, nkentseu::math::NkVec2f b, const nkentseu::math::NkColor &c,
					  nkentseu::float32 th = 2.f) const noexcept {
				if (dl)
					dl->AddLine(a, b, c, th);
			}

			void Circle(nkentseu::math::NkVec2f center, nkentseu::float32 r, const nkentseu::math::NkColor &c,
						nkentseu::int32 segs = 0) const noexcept {
				if (dl)
					dl->AddCircleFilled(center, r, c, segs);
			}

			// Contour de cercle : NkGuiDrawList n'a pas de AddCircle (contour) — émulé
			// par un anneau de quads (rayon extérieur r + th/2, intérieur r - th/2),
			// même règle de segments automatique que AddCircleFilled.
			void CircleOutline(nkentseu::math::NkVec2f center, nkentseu::float32 r, const nkentseu::math::NkColor &c,
							   nkentseu::float32 th = 2.f, nkentseu::int32 segs = 0) const noexcept {
				if (!dl || r <= 0.f)
					return;
				if (segs <= 0) {
					segs = static_cast<nkentseu::int32>(8.f * r / 4.f) + 8;
					if (segs < 12)
						segs = 12;
					else if (segs > 128)
						segs = 128;
				}
				const nkentseu::float32 ro = r + th * 0.5f;
				nkentseu::float32 ri = r - th * 0.5f;
				if (ri < 0.f)
					ri = 0.f;
				const nkentseu::float32 kTau = 6.28318530718f;
				nkentseu::float32 px = 1.f, py = 0.f; // cos/sin de l'angle précédent
				for (nkentseu::int32 s = 1; s <= segs; ++s) {
					const nkentseu::float32 ang = kTau * static_cast<nkentseu::float32>(s) / static_cast<nkentseu::float32>(segs);
					const nkentseu::float32 cx = nkentseu::math::NkCos(ang), cy = nkentseu::math::NkSin(ang);
					const nkentseu::math::NkVec2f o0{center.x + px * ro, center.y + py * ro};
					const nkentseu::math::NkVec2f o1{center.x + cx * ro, center.y + cy * ro};
					const nkentseu::math::NkVec2f i1{center.x + cx * ri, center.y + cy * ri};
					const nkentseu::math::NkVec2f i0{center.x + px * ri, center.y + py * ri};
					dl->AddTriangleFilled(o0, o1, i1, c);
					dl->AddTriangleFilled(o0, i1, i0, c);
					px = cx;
					py = cy;
				}
			}

			// ── Texte (AddText : baseline → on passe un Y de HAUT et on ajoute l'ascent) ──
			void Text(nkentseu::nkgui::NkGuiFont *f, nkentseu::float32 x, nkentseu::float32 topY, const char *s,
					  const nkentseu::math::NkColor &c, nkentseu::float32 maxW = -1.f) const noexcept {
				if (f && f->Face() && s && dl)
					dl->AddText(f->Face(), f->TexId(), nkentseu::math::NkVec2f{x, topY + f->Ascent()}, s, c, maxW);
			}

			void TextCentered(nkentseu::nkgui::NkGuiFont *f, nkentseu::float32 x, nkentseu::float32 w,
							  nkentseu::float32 topY, const char *s, const nkentseu::math::NkColor &c) const noexcept {
				if (!f || !s)
					return;
				Text(f, x + (w - f->MeasureWidth(s)) * 0.5f, topY, s, c);
			}

			nkentseu::float32 TextW(nkentseu::nkgui::NkGuiFont *f, const char *s) const noexcept {
				return (f && s) ? f->MeasureWidth(s) : 0.f;
			}

			nkentseu::float32 LineH(nkentseu::nkgui::NkGuiFont *f) const noexcept {
				return (f && f->Face()) ? f->LineHeight() : 14.f;
			}

			// ── Entrée ─────────────────────────────────────────────────────────
			bool PointIn(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w,
						 nkentseu::float32 h) const noexcept {
				return pointer.x >= x && pointer.x < x + w && pointer.y >= y && pointer.y < y + h;
			}

			// Bouton tactile/souris : dessine un rect arrondi + label centré.
			// Retourne true si activé (pointeur relâché dessus).
			bool Button(nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
						const char *label, const nkentseu::math::NkColor &bg, const nkentseu::math::NkColor &bgHover,
						const nkentseu::math::NkColor &fg,
						const nkentseu::math::NkColor *border = nullptr) const noexcept {
				const bool over = PointIn(x, y, w, h);
				Rect(x, y, w, h, over ? bgHover : bg, 8.f);
				if (border)
					Border(x, y, w, h, *border, 1.5f, 8.f);
				if (font && label)
					Text(font, x + (w - font->MeasureWidth(label)) * 0.5f, y + (h - font->LineHeight()) * 0.5f,
						 label, fg);
				return over && pointerReleased;
			}
	};

} // namespace nkoung

#endif // NKOUNG_FRAME_H
