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

				/// (j2) QUI SUIS-JE — le message de repli inerte nomme le peintre,
				/// pas seulement la primitive : « X n'implemente pas Y » dit ou
				/// aller, « Y n'est pas implementee » laisse chercher.
				/// 0xRRGGBBAA -> NkColor. UN SEUL site : deux copies du meme
				/// decalage finissent par diverger, et un canal inverse d'un cote
				/// seulement ne se voit qu'a l'ecran.
				static NkColor Depaqueter(uint32 rgba) noexcept {
					return NkColor{(uint8)((rgba >> 24) & 0xFF), (uint8)((rgba >> 16) & 0xFF),
								   (uint8)((rgba >> 8) & 0xFF), (uint8)(rgba & 0xFF)};
				}

				const char *NomDuPeintre() const noexcept override {
					return "NkModelerComponentPaint";
				}

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
					mP.Fill(R(r), Depaqueter(rgba), rounding);
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

				// ── LES DEUX PRIMITIVES QUI TRACENT (2026-09-14) ────────────────
				// ⚠️ ELLES MANQUAIENT, ET LE MANQUE NE SE VOYAIT PAS DANS UN BANC.
				//    `Line` et `Ellipse` sont additives a DEFAUT INERTE : la classe
				//    de base rend `false`, l'appelant peint alors un repli VISIBLE.
				//    Resultat mesure le 14/09 sur une capture de NK3DModeler : le
				//    « + » de la bande d'onglets sortait en CARRE BLANC, pendant que
				//    le banc rendait 104/104 et la geometrie au centieme. Aucun
				//    critere ne pouvait le voir -- le repli est un comportement
				//    LEGITIME, pas une erreur.
				//
				// ⚠️ ET LE DEFAUT N'EST PAS CELUI DE LA BANDE D'ONGLETS : il frappe
				//    TOUT composant partage qui trace (croix, chevrons, pastilles,
				//    courbes). La bande ne l'a que revele, parce qu'elle est le
				//    premier composant du kit a TRACER chez cet hote.
				//
				// Les deux primitives existaient deja chez `NkModelerPainter`
				// (`Line` par couleur et `Disc`) : rien a inventer, seulement a
				// router. On passe par la COULEUR plutot que par `NkRole` -- le kit
				// donne un `uint16` opaque, et `mP.C(role)` est la traduction que
				// l'adaptateur fait deja partout ailleurs.
				bool Line(float32 x1, float32 y1, float32 x2, float32 y2, uint16 role,
						  float32 thickness) override {
					mP.Line(x1, y1, x2, y2, mP.C(role), thickness);
					return true;
				}
				/// ⚠️ UNE ELLIPSE INSCRITE, APPROCHEE PAR UN DISQUE. `NkModelerPainter`
				///    ne sait tracer qu'un CERCLE (`Disc`) : on prend le plus petit
				///    des deux demi-axes. C'est exact pour une boite carree -- le cas
				///    de tous les usages actuels (pastilles, points d'etat) -- et
				///    approche pour une boite allongee. Dit plutot que tu : une
				///    ellipse franchement ovale sortira ronde ici, et le jour ou un
				///    composant en demandera une, c'est `NkModelerPainter` qu'il
				///    faudra doter, pas cet adaptateur qu'il faudra ruser.
				// ── LES QUATRE ROUTAGES DU 2026-09-14 (canal onglets, o1) ───────
				// ⚠️ AUCUNE DE CES QUATRE N'EST DU CODE NEUF : la liste de dessin du
				//    peintre les portait deja. Elles etaient absentes de CET
				//    adaptateur, donc inertes chez cet hote, donc silencieuses --
				//    exactement comme `Line` et `Ellipse` l'ont ete des semaines.
				//
				// ⚠️ ET UNE PRIMITIVE ROUTEE VERS LA MAUVAISE FONCTION COMPILE ET SE
				//    DECLARE PRESENTE. Le recensement les comptera « oui » quoi qu'il
				//    arrive. La preuve est donc une IMAGE ou chacune se voit :
				//    `NK3D_SONDE_PEINTRE=1` les dessine toutes les quatre.

				/// Polygone plein en 0xRRGGBBAA. ⚠️ CONVEXE seulement -- c'est le
				/// contrat de la liste de dessin, et on ne le maquille pas : les cinq
				/// appelants du kit passent `count = 3`, donc des triangles, toujours
				/// convexes. Un appelant concave aurait un resultat faux ET
				/// silencieux ; le jour ou il existe, c'est l'ear-clipping de
				/// `NkGuiComponentPaint` (`NkEarcut.h`) qu'il faudra reprendre ici.
				bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
					if (!xy || count < 3 || count > 128)
						return false;
					NkVec2 pts[128];
					for (int32 i = 0; i < count; ++i)
						pts[i] = NkVec2{xy[i * 2], xy[i * 2 + 1]};
					mP.PolyFilled(pts, count, Depaqueter(rgba));
					return true;
				}

				/// Polygone texture. `image == 0` = aucune texture : on rend FAUX et
				/// l'appelant peint son damier -- rien n'est simule (meme contrat que
				/// `NkGuiComponentPaint`).
				bool ImagePolygone(const float32 *xy, const float32 *uv, int32 count, uint32 image,
								   float32 opacite) override {
					if (!xy || !uv || count < 3 || count > 128 || image == 0u)
						return false;
					NkVec2 pts[128], uvs[128];
					for (int32 i = 0; i < count; ++i) {
						pts[i] = NkVec2{xy[i * 2], xy[i * 2 + 1]};
						uvs[i] = NkVec2{uv[i * 2], uv[i * 2 + 1]};
					}
					// ⚠️ LE BLANC EST LE MULTIPLICATEUR : toute autre teinte
					//    assombrirait l'image. Seul l'alpha porte l'opacite.
					const float32 k = opacite < 0.f ? 0.f : (opacite > 100.f ? 1.f : opacite * 0.01f);
					const uint8 a = (uint8)(255.f * k + 0.5f);
					mP.ImagePolygon(image, pts, uvs, count, NkColor{255, 255, 255, a});
					return true;
				}

				/// ⚠️ LES DEUX ENUMERATIONS SONT ALIGNEES, ET C'EST VERIFIE PLUTOT QUE
				///    CRU : `NkPaintBlend` et `NkGuiBlend` declarent Alpha, Multiply,
				///    Screen, Darken, Lighten, PlusLighter dans CET ordre toutes les
				///    deux. Les `static_assert` ci-dessous tombent si l'une bouge --
				///    sans eux, un jour, « Multiply » deviendrait « Screen » a
				///    l'ecran sans qu'aucun compilateur ne bronche.
				void PushBlend(editorkit::NkComponentPaint::NkPaintBlend b) override {
					static_assert((uint8)editorkit::NkComponentPaint::NkPaintBlend::Alpha ==
									  (uint8)nkgui::NkGuiBlend::Alpha,
								  "NkPaintBlend et NkGuiBlend ont diverge (Alpha)");
					static_assert((uint8)editorkit::NkComponentPaint::NkPaintBlend::PlusLighter ==
									  (uint8)nkgui::NkGuiBlend::PlusLighter,
								  "NkPaintBlend et NkGuiBlend ont diverge (PlusLighter)");
					mP.PushBlend((nkgui::NkGuiBlend)(uint8)b);
				}
				void PopBlend() override {
					mP.PopBlend();
				}

				bool Ellipse(const editorkit::NkPaintRect &r, uint16 role) override {
					const float32 rayon = (r.w < r.h ? r.w : r.h) * 0.5f;
					mP.Disc(r.x + r.w * 0.5f, r.y + r.h * 0.5f, rayon, mP.C(role));
					return true;
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
