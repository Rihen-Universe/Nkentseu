#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerComponentPaint.h
// @Brief   L'ADAPTATEUR : NkModelerPainter vu comme un NkComponentPaint.
//          C'est la livraison convenue le 18/08 (NkComponentPaint.h, en-tete :
//          « le peintre partage est extrait par l'agent NK3DModeler depuis
//          NkModelerUI.h — je le RECOIS, je ne le prends pas »).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QUE C'EST : une sous-classe MINCE. Mesure avant ecriture (NK3D-118/119) :
//   1 correspondance exacte (ColorOf), 4 renommages, 5 conversions mecaniques,
//   3 ecarts obligatoires — le role sur HLine/VLine (surcharges ajoutees au
//   peintre) et l'ALIGNEMENT du texte, implemente ici parce que `NkTextAlign`
//   est le vocabulaire du kit, pas celui du peintre.
//
// ⚠️ LES TROIS PIEGES MESURES AVANT ECRITURE, ET COMMENT ILS SONT EVITES :
//   - HLine/VLine : les composants passent TROIS roles (7x border, 1x guide,
//     1x text). Un adaptateur qui appellerait les variantes sans role peindrait
//     le guide d'indentation et le curseur de renommage en BORDURE — rien ne
//     planterait, la couleur mentirait. D'ou les surcharges avec role.
//   - Text : `NkTextAlign::Center` est UTILISE (pied de carte du navigateur,
//     2 occurrences). Un adaptateur qui ignorerait `align` collerait les deux
//     libelles a gauche. L'alignement est donc implemente, pas differe.
//   - L'ECHELLE : l'interface exige un peintre qui ne connait PAS l'echelle
//     (elle voyage dans NkComponentInput::surfaceScale, et les composants
//     multiplient leurs metriques par elle AVANT d'appeler le peintre — mesure
//     dans NkTreeViewDraw.cpp:257 et NkContentBrowserDraw.cpp:108). Les
//     coordonnees arrivent donc en pixels PHYSIQUES : l'adaptateur les passe
//     TELLES QUELLES. Lui faire appliquer S() les doublerait.
//
// LA POIGNEE D'ICONE (exigence B de l'interface) : `0` = aucune, sinon
//   poignee = (uint16)NkIcon + 1. La MEME convention doit servir a remplir les
//   modeles (NkTreeNode::icon, NkTreeViewIcons) : c'est l'application qui
//   choisit le mappage, le kit ne connait pas l'enumeration. NkIconHandle()
//   ci-dessous est l'unique endroit qui l'encode — remplir un modele a la main
//   avec `(uint16)ic` (sans le +1) decalerait toutes les icones d'un cran.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NK3DModeler/Shell/NkModelerUI.h"

namespace nkentseu {
	namespace nk3d {

		/// L'UNIQUE encodage poignee <-> NkIcon. 0 = aucune icone.
		inline uint16 NkIconHandle(NkIcon ic) {
			return (uint16)((uint16)ic + 1u);
		}

		class NkModelerComponentPaint final : public editorkit::NkComponentPaint {
			public:
				explicit NkModelerComponentPaint(NkModelerPainter &p) noexcept : mP(p) {}

				// ── Theme et metrologie ─────────────────────────────────────────
				uint32 ColorOf(uint16 role) const override {
					return mP.PackedColor(role); // deja empaquete : la forme du theme
				}
				float32 LineHeight() const override {
					return mP.LineH();
				}
				float32 TextWidth(const char *s) const override {
					return mP.TextW(s);
				}

				// ── Primitives ──────────────────────────────────────────────────
				void Fill(const editorkit::NkPaintRect &r, uint16 role, float32 rounding = 0.f) override {
					mP.Fill(R(r), (NkRole)role, rounding);
				}
				void FillColor(const editorkit::NkPaintRect &r, uint32 rgba, float32 rounding = 0.f) override {
					// Depaquetage LOCAL (0xRRGGBBAA -> NkColor) : celui du peintre est
					// prive, et 4 decalages ne justifient pas d'elargir son API.
					mP.Fill(R(r),
							NkColor{(uint8)((rgba >> 24) & 0xFF), (uint8)((rgba >> 16) & 0xFF),
									(uint8)((rgba >> 8) & 0xFF), (uint8)(rgba & 0xFF)},
							rounding);
				}
				void Outline(const editorkit::NkPaintRect &r, uint16 border, uint16 inner,
							 float32 rounding = 0.f) override {
					mP.Outline(R(r), (NkRole)border, (NkRole)inner, rounding);
				}
				void OutlineSharp(const editorkit::NkPaintRect &r, uint16 role) override {
					mP.OutlineSharp(R(r), (NkRole)role);
				}
				// Les surcharges AVEC role du peintre (2026-08-29) — pas les variantes
				// historiques, qui codent Border en dur. Cf. le piege n.1 en tete.
				void HLine(float32 x, float32 y, float32 w, uint16 role) override {
					mP.HLine(x, y, w, role);
				}
				void VLine(float32 x, float32 y, float32 h, uint16 role) override {
					mP.VLine(x, y, h, role);
				}

				// ── Texte : ALIGNEMENT + ELLIPSE, les deux obligations ──────────
				// L'ellipse est une obligation du contrat (« une implementation qui
				// coupe net respecte la signature et trahit le contrat ») ; le
				// peintre l'a deja (TextClipped). L'alignement est l'ecart n.3 :
				// on ne decale que si le texte TIENT — un texte tronque occupe toute
				// la largeur, l'aligner n'aurait pas de sens.
				void Text(const editorkit::NkPaintRect &r, const char *s, uint16 role,
						  editorkit::NkTextAlign align = editorkit::NkTextAlign::Left) override {
					if (!s || !*s || r.w <= 0.f)
						return;
					const float32 tw = mP.TextW(s);
					const float32 yc = r.y + (r.h - mP.LineH()) * 0.5f; // centre vertical
					if (tw > r.w) {
						mP.TextClipped(r.x, yc, r.w, s, (NkRole)role);
						return;
					}
					float32 x = r.x;
					if (align == editorkit::NkTextAlign::Center)
						x += (r.w - tw) * 0.5f;
					else if (align == editorkit::NkTextAlign::Right)
						x += r.w - tw;
					mP.Text(x, yc, s, (NkRole)role);
				}

				void Icon(const editorkit::NkPaintRect &r, uint16 iconHandle, uint16 role) override {
					if (iconHandle == 0)
						return; // 0 = aucune, par contrat
					const NkIcon ic = (NkIcon)(iconHandle - 1u);
					// Centre dans le rect : IconV centre verticalement dans h ; le
					// centrage horizontal se calcule sur la taille reelle du glyphe.
					const float32 sz = mP.IconSize();
					mP.IconV(r.x + (r.w - sz) * 0.5f, r.y, r.h, ic, (NkRole)role);
				}

				// ── Decoupe ─────────────────────────────────────────────────────
				void PushClip(const editorkit::NkPaintRect &r) override {
					mP.Clip(R(r));
				}
				void PopClip() override {
					mP.Unclip();
				}

			private:
				// NkPaintRect (kit, plat) -> NkRect (NKGui). Champs identiques ; la
				// duplication de type est VOULUE par l'interface (« ce fichier ne
				// doit rien savoir de NKGui »).
				static NkRect R(const editorkit::NkPaintRect &r) {
					return NkRect{r.x, r.y, r.w, r.h};
				}
				NkModelerPainter &mP;
		};

	} // namespace nk3d
} // namespace nkentseu
