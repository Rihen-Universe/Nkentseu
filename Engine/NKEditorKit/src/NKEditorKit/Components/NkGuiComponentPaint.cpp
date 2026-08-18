// -----------------------------------------------------------------------------
// @File    NkGuiComponentPaint.cpp
// @Brief   Les deux seules methodes du peintre-adaptateur qui ne sont pas un
//          renvoi d'une ligne : le texte (ellipse + alignement) et l'icone.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkGuiComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		namespace {
			uint32 Len(const char *s) {
				uint32 n = 0;
				if (s)
					while (s[n])
						++n;
				return n;
			}
		} // namespace

		void NkGuiComponentPaint::Text(const NkPaintRect &r, const char *s, uint16 role,
									   NkTextAlign align) {
			if (!s || !*s || !mCtx.font || r.w <= 0.f)
				return;

			const NkFont *face = mCtx.font->Face();
			if (!face)
				return;

			// ── L'ELLIPSE ───────────────────────────────────────────────────────
			// `AddText(maxWidth)` COUPE AU GLYPHE, sans points de suite : un nom
			// tronque devient indiscernable d'un nom court, et deux fichiers
			// distincts s'affichent identiques. C'est l'ecart n.11/16 des planches.
			//
			// ⚠️ RECHERCHE LINEAIRE, PAS DICHOTOMIQUE, et c'est un choix : la
			//    largeur d'un texte n'est PAS monotone au sens ou une dichotomie
			//    l'exigerait (crenage, ligatures). Le cout est un libelle par
			//    carte, sur des chaines de l'ordre de 30 caracteres.
			const char *tail = "...";
			const float32 full = mCtx.font->MeasureWidth(s);
			NkString clipped;
			const char *draw = s;
			if (full > r.w) {
				const float32 tailW = mCtx.font->MeasureWidth(tail);
				const uint32 n = Len(s);
				uint32 keep = 0;
				for (uint32 i = 1; i <= n; ++i) {
					NkString probe;
					for (uint32 k = 0; k < i; ++k)
						probe.Append(s[k]);
					if (mCtx.font->MeasureWidth(probe.Data()) + tailW > r.w)
						break;
					keep = i;
				}
				for (uint32 k = 0; k < keep; ++k)
					clipped.Append(s[k]);
				clipped.Append(tail);
				draw = clipped.Data();
				// Cas limite : le rectangle est plus etroit que « ... ». On ne
				// dessine alors RIEN plutot que des points seuls, qui feraient
				// croire a un contenu vide.
				if (keep == 0 && tailW > r.w)
					return;
			}

			const float32 w = mCtx.font->MeasureWidth(draw);
			float32 x = r.x;
			if (align == NkTextAlign::Center)
				x = r.x + (r.w - w) * 0.5f;
			else if (align == NkTextAlign::Right)
				x = r.x + r.w - w;
			if (x < r.x)
				x = r.x;

			// Ligne de base centree verticalement dans le rectangle.
			const float32 lh = mCtx.font->LineHeight();
			const float32 baseY = r.y + (r.h - lh) * 0.5f + mCtx.font->Ascent();

			mCtx.DL().AddText(face, mCtx.font->TexId(), {Px(x), Px(baseY)}, draw, C(role));
		}

		void NkGuiComponentPaint::Icon(const NkPaintRect &r, uint16 iconHandle, uint16 role) {
			// ⚠️ AUCUNE ICONE N'EST DESSINEE ICI, ET CE N'EST PAS UN OUBLI.
			//    NKGui n'a AUCUNE notion d'icone ; les 193 glyphes du depot sont
			//    definis deux fois (102 SVG chez NK3DModeler, 91 PNG chez NKCode),
			//    a raison d'une texture GPU par glyphe. L'atlas appartient a
			//    l'agent NKGui et figure dans la liste qui lui a ete passee.
			//
			//    On peint donc un carre plein du role demande : la PLACE est prise
			//    (la mise en page est deja juste et ne bougera pas quand l'atlas
			//    arrivera), la COULEUR est juste, le GLYPHE manque.
			//
			//    Le choix inverse — ne rien peindre — aurait ete pire : la mise en
			//    page paraitrait correcte a l'ecran alors qu'elle reserve une place
			//    qu'on ne verrait pas, et le defaut n'apparaitrait qu'a l'arrivee
			//    de l'atlas, loin de sa cause.
			if (iconHandle == 0 || r.w <= 0.f || r.h <= 0.f)
				return;
			// Un carre centre, jamais un rectangle etire : une icone est carree, et
			// la mise en page doit s'en apercevoir des maintenant.
			const float32 side = r.w < r.h ? r.w : r.h;
			const NkPaintRect q{r.x + (r.w - side) * 0.5f, r.y + (r.h - side) * 0.5f, side, side};
			mCtx.DL().AddRectFilled(R(q), C(role), side * 0.15f);
		}

	} // namespace editorkit
} // namespace nkentseu
