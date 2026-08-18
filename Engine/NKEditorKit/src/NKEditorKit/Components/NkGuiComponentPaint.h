#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiComponentPaint.h
// @Brief   Implementation MINCE de `NkComponentPaint` sur la liste d'affichage
//          de NKGui — le strict necessaire pour qu'un composant s'affiche.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE FICHIER EST PROVISOIRE, ET IL FAUT QU'IL LE RESTE.
// =============================================================================
//  Le peintre partage arrive de NK3DModeler (`NkModelerPainter`, 571 l.,
//  `NkModelerUI.h`), extrait par son agent — je le RECOIS, je ne le prends pas.
//  Ce fichier n'est PAS ce peintre et ne cherche pas a le devenir.
//
//  IL EXISTE POUR UNE SEULE RAISON : une interface sans implementation ne se
//  compile contre aucun appelant reel, et une declaration sans consommateur
//  serait exactement le quatrieme systeme dormant que cette tranche existe pour
//  ne pas creer. Il faut un peintre — n'importe lequel — pour que la chaine
//  « declaration -> dessin -> ecran » soit fermee aujourd'hui.
//
//  🚧 REGLE POUR CELUI QUI PASSERA APRES MOI : **il ne doit pas grossir.**
//     Chaque methode se contente de traduire un appel vers `NkGuiDrawList`.
//     Aucune geometrie nouvelle, aucune decision de rendu, aucun cas
//     particulier. Le jour ou l'on est tente d'ajouter un effet ici, c'est le
//     signe que le peintre de NK3DModeler doit arriver — pas que ce fichier
//     doit s'etoffer.
//
// CE QU'IL FAIT QUAND MEME, ET POURQUOI IL LE FAUT :
//   - **l'ellipse** (« mon_tres_long_fichier... ») : le contrat de
//     `NkComponentPaint::Text` l'exige, et `NkGuiDrawList::AddText(maxWidth)`
//     coupe au glyphe sans points de suite. Sans elle, la signature mentirait —
//     et c'est exactement le defaut « un parametre qui n'est pas honore ». Ce
//     n'est donc pas une extension : c'est le respect du contrat.
//   - **le centrage** : meme raison. L'aide existe dans NKGui mais elle est
//     INTERNE a un `.cpp`, non exposee (mesure du 18/08).
//
// ⚠️ CE QU'IL NE SAIT PAS FAIRE, ET C'EST DIT PLUTOT QUE DECOUVERT :
//   - **`Icon` NE DESSINE AUCUNE ICONE.** Il n'existe aucune notion d'icone
//     dans NKGui, et les 193 glyphes du depot sont definis DEUX FOIS (102 en
//     SVG chez NK3DModeler, 91 en PNG chez NKCode), a raison d'une texture GPU
//     par glyphe. L'atlas appartient a l'agent NKGui et figure dans sa liste.
//     En attendant, `Icon` peint un **carre plein du role demande** : la place
//     est prise, la couleur est juste, le glyphe manque. Un composant qui
//     l'appelle est donc deja correct le jour ou l'atlas arrive.
//     **Ce n'est pas un oubli — c'est une dependance nommee.**
//   - **`Outline` et le contour arrondi** restent le contournement a deux
//     rectangles, parce que `AddRect` ne sait pas arrondir. Meme proprietaire,
//     meme liste.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKEditorKit/NkTheme.h"
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		class NkGuiComponentPaint : public NkComponentPaint {
			public:
				NkGuiComponentPaint(nkgui::NkGuiContext &ctx, const NkTheme &theme) noexcept
					: mCtx(ctx), mTheme(theme) {}

				// ── Theme et metrologie ─────────────────────────────────────────
				uint32 ColorOf(uint16 role) const override {
					return mTheme.Get(role);
				}
				// ⚠️ AUCUN `Scale()` — arbitrage du 18/08 : l'echelle appartient a
				//    la SURFACE, pas au peintre, parce que la disposition et les
				//    tables de metrique la lisent aussi et qu'elles ne peignent
				//    pas. Elle voyage dans `NkComponentInput::surfaceScale`.
				//    Ce peintre ne l'a jamais vue et n'en a pas besoin.
				float32 LineHeight() const override {
					return mCtx.font ? mCtx.font->LineHeight() : 16.f;
				}
				float32 TextWidth(const char *s) const override {
					return (mCtx.font && s) ? mCtx.font->MeasureWidth(s) : 0.f;
				}

				// ── Primitives ──────────────────────────────────────────────────
				void Fill(const NkPaintRect &r, uint16 role, float32 rounding) override {
					mCtx.DL().AddRectFilled(R(r), C(role), rounding);
				}
				void FillColor(const NkPaintRect &r, uint32 rgba, float32 rounding) override {
					mCtx.DL().AddRectFilled(R(r), Unpack(rgba), rounding);
				}
				void Outline(const NkPaintRect &r, uint16 border, uint16 inner,
							 float32 rounding) override {
					// Contournement assume : `AddRect` ne sait pas arrondir. Plein
					// puis creusement d'un pixel. Meme geste que `NkModelerPainter`,
					// et il disparait le jour ou NKGui sait arrondir un contour.
					const nkgui::NkRect q = R(r);
					mCtx.DL().AddRectFilled(q, C(border), rounding);
					mCtx.DL().AddRectFilled({q.x + 1.f, q.y + 1.f, q.w - 2.f, q.h - 2.f}, C(inner),
											rounding > 1.f ? rounding - 1.f : 0.f);
				}
				void OutlineSharp(const NkPaintRect &r, uint16 role) override {
					mCtx.DL().AddRect(R(r), C(role), 1.f);
				}
				void HLine(float32 x, float32 y, float32 w, uint16 role) override {
					mCtx.DL().AddRectFilled({Px(x), Px(y), Px(x + w) - Px(x), 1.f}, C(role));
				}
				void VLine(float32 x, float32 y, float32 h, uint16 role) override {
					mCtx.DL().AddRectFilled({Px(x), Px(y), 1.f, Px(y + h) - Px(y)}, C(role));
				}

				void Text(const NkPaintRect &r, const char *s, uint16 role, NkTextAlign align) override;
				void Icon(const NkPaintRect &r, uint16 iconHandle, uint16 role) override;

				void PushClip(const NkPaintRect &r) override {
					mCtx.DL().PushClipRect(R(r), true);
				}
				void PopClip() override {
					mCtx.DL().PopClipRect();
				}

			private:
				/// ALIGNEMENT AU PIXEL. C'est une lecon deja payee ailleurs (le flou
				/// des glyphes poses a mi-pixel) : `Px` est repris tel quel de
				/// `NkModelerUI.h:59`, et c'est l'une des choses que la reception du
				/// peintre doit GARDER, pas « nettoyer ».
				static float32 Px(float32 v) noexcept {
					return (float32)(int32)(v + 0.5f);
				}
				static nkgui::NkRect R(const NkPaintRect &r) noexcept {
					const float32 x = Px(r.x), y = Px(r.y);
					return {x, y, Px(r.x + r.w) - x, Px(r.y + r.h) - y};
				}
				static nkgui::NkColor Unpack(uint32 c) noexcept {
					return {(uint8)((c >> 24) & 0xFFu), (uint8)((c >> 16) & 0xFFu),
							(uint8)((c >> 8) & 0xFFu), (uint8)(c & 0xFFu)};
				}
				nkgui::NkColor C(uint16 role) const noexcept {
					return Unpack(mTheme.Get(role));
				}

				nkgui::NkGuiContext &mCtx;
				const NkTheme &mTheme;
		};

	} // namespace editorkit
} // namespace nkentseu
