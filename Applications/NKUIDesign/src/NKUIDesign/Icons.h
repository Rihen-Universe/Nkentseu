#pragma once
// -----------------------------------------------------------------------------
// @File    Icons.h
// @Brief   LES POIGNEES D'ICONES DE NKUIDESIGN, et le peintre qui sait les lire.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ON A MESURE AVANT D'ECRIRE UNE LIGNE ICI
// =============================================================================
//  On m'a transmis — et je l'ai relaye moi-meme — que « pas de chevron = un
//  arbre qui ne se plie pas ». C'etait une deduction faite en regardant une
//  capture, la **troisieme de la journee**, apres deux qui se sont revelees
//  fausses. Avant de construire quoi que ce soit, la famille 36 de la sonde a
//  pose la question qui vient avant « comment fait-on ca » : **est-ce qu'on a le
//  probleme ?**
//
//  **Reponse mesuree : NON.** Le clic dans la zone du chevron plie ET deplie,
//  alors qu'aucune icone n'est dessinee (`36b`, `36c`). Dans
//  `NkTreeViewDraw.cpp`, `hitChevron` est **geometrique** — il ne consulte
//  aucune poignee. Le composant emet bien **29 commandes `Icon`**, toutes a
//  poignee **nulle** (`36d`) : c'est l'HOTE qui n'en fournissait aucune.
//
//  ⚠️ **Le manque est donc le SIGNE, pas le geste** : on ne voit pas ou cliquer,
//     mais cliquer marche. Ca ne le rend pas benin — une commande invisible est
//     une commande qui n'existe pas pour l'utilisateur — mais ca change ce qu'il
//     faut ecrire : un DESSIN, pas un mecanisme. Ce fichier fait exactement ca,
//     et rien de plus.
//
// =============================================================================
//  POURQUOI UN PEINTRE A MOI, ET PAS UNE CORRECTION DANS LE KIT
// =============================================================================
//  `NkGuiComponentPaint::Icon` peint un **carre plein** pour toute poignee non
//  nulle, et le dit : NKGui n'a aucune notion d'icone, l'atlas appartient a
//  l'agent NKGui et arrive (poignees opaques, atlas + vectoriel). Ce choix est
//  bon — la PLACE est prise, la mise en page ne bougera pas quand l'atlas
//  arrivera.
//
//  Mais un carre ne dit pas « ouvert » ou « ferme ». Or **la poignee est opaque
//  et c'est l'HOTE qui la choisit** (`NkTreeViewIcons`, rempli par
//  l'application) : le composant ne connait l'enumeration d'icones de personne.
//  Le peintre qui sait traduire MES poignees est donc, par construction, **le
//  mien** — pas celui du kit.
//
//  ⚠️ CE QUE CA EVITE, ET C'EST LE MOTIF QUE CE DEPOT A DEJA PAYE : inventer ici
//     un vocabulaire de poignees partage ferait **un quatrieme jeu d'icones**
//     (102 SVG chez NK3DModeler, 91 PNG chez NKCode, l'atlas NKGui en cours).
//     Rien de ce fichier n'est partageable, et c'est voulu : il ne declare que
//     ce dont NkUIDesign a besoin, il ne touche a aucun fichier commun, et **il
//     disparait le jour ou l'atlas NKGui arrive** — la table ci-dessous devient
//     alors des `Find("chevron_open")`.
//
//  ⚠️ CE QU'IL NE FAIT PAS : l'oeil, le cadenas et l'icone de nature restent
//     **des carres**. Les dessiner en vectoriel demanderait des formes que
//     `NkComponentPaint` n'expose pas, et surtout ce sont des INFORMATIONS, pas
//     des commandes — on peut lire l'arbre sans elles, on ne peut pas le plier
//     sans savoir ou cliquer. Le chevron d'abord, le reste avec l'atlas.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"
#include "Costume.h" // les tracés exacts des icônes Banani (remandat 31/08)

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint16;
	using nkentseu::uint32;
	using nkentseu::uint8;
	using nkentseu::editorkit::NkGuiComponentPaint;
	using nkentseu::editorkit::NkPaintRect;
	using nkentseu::editorkit::NkTheme;
	using nkentseu::editorkit::NkTreeViewIcons;

	// ── LES POIGNEES DE CETTE APPLICATION ───────────────────────────────────
	// ⚠️ ELLES NE VEULENT RIEN DIRE POUR LE COMPOSANT, et c'est exactement le
	//    contrat : il les transporte sans les lire. Elles ne signifient quelque
	//    chose que pour `NkDesignPaint`, plus bas. Zero reste « aucune icone ».
	enum NkDesignIcon : uint16 {
		NK_ICON_AUCUNE = 0,
		NK_ICON_CHEVRON_FERME,
		NK_ICON_CHEVRON_OUVERT,
		NK_ICON_OEIL_OUVERT,
		NK_ICON_OEIL_FERME,
		NK_ICON_CADENAS_OUVERT,
		NK_ICON_CADENAS_FERME,
		// ── Icônes de NATURE des rangées (costume Banani, 31/08) — chacune
		//    recopie les primitives du JSX (Costume.h), 11×11 dans sa case. ──
		NK_ICON_NATURE_PAGE,	///< un artboard/page (rect + 2 lignes)
		NK_ICON_NATURE_PANNEAU, ///< un conteneur (rect + ligne médiane)
		NK_ICON_NATURE_BOUTON,	///< un élément à rôle bouton (pilule + trait)
		NK_ICON_NATURE_TEXTE,	///< un texte (le « T »)
	};

	/// Le jeu que l'hote donne a l'arbre. Un seul endroit, pour que la question
	/// « quelle poignee vaut quoi » ait une seule reponse dans le programme.
	inline NkTreeViewIcons NkDesignTreeIcons() {
		NkTreeViewIcons ic;
		ic.chevronClosed = NK_ICON_CHEVRON_FERME;
		ic.chevronOpen = NK_ICON_CHEVRON_OUVERT;
		ic.eyeOpen = NK_ICON_OEIL_OUVERT;
		ic.eyeClosed = NK_ICON_OEIL_FERME;
		ic.lockOpen = NK_ICON_CADENAS_OUVERT;
		ic.lockClosed = NK_ICON_CADENAS_FERME;
		return ic;
	}

	// ── LE PEINTRE ──────────────────────────────────────────────────────────
	// Il ne surcharge QUE `Icon`. Tout le reste — texte, remplissages, contours,
	// clips, metrologie — reste celui du kit : une copie de ces primitives serait
	// une divergence garantie a la premiere correction faite en amont.
	class NkDesignPaint : public NkGuiComponentPaint {
		public:
			NkDesignPaint(nkentseu::nkgui::NkGuiContext &ctx, const NkTheme &theme) noexcept
				: NkGuiComponentPaint(ctx, theme), mCtx(ctx), mTheme(theme) {}

			void Icon(const NkPaintRect &r, uint16 iconHandle, uint16 role) override {
				if (iconHandle == NK_ICON_AUCUNE || r.w <= 0.f || r.h <= 0.f)
					return;
				if (iconHandle == NK_ICON_CHEVRON_FERME) {
					Chevron(r, role, false);
					return;
				}
				if (iconHandle == NK_ICON_CHEVRON_OUVERT) {
					Chevron(r, role, true);
					return;
				}
				// Les icônes de NATURE (costume Banani) : 11×11 centrées dans la
				// case, tracés du JSX recopiés dans Costume.h.
				if (iconHandle >= NK_ICON_NATURE_PAGE && iconHandle <= NK_ICON_NATURE_TEXTE) {
					const float32 ix = r.x + (r.w - 11.f) * 0.5f;
					const float32 iy = r.y + (r.h - 11.f) * 0.5f;
					const nkentseu::nkgui::NkColor col = Couleur(role);
					auto &dl = mCtx.DL();
					if (iconHandle == NK_ICON_NATURE_PAGE)
						costume::IcPage(dl, ix, iy, col);
					else if (iconHandle == NK_ICON_NATURE_PANNEAU)
						costume::IcPanneau(dl, ix, iy, col);
					else if (iconHandle == NK_ICON_NATURE_BOUTON)
						costume::IcBouton(dl, ix, iy, col);
					else
						costume::IcTexte(dl, ix, iy, col);
					return;
				}
				// ⚠️ TOUT LE RESTE RETOMBE SUR LE KIT, deliberement : un carre plein
				//    du bon role. La place est prise, la couleur est juste, le
				//    glyphe manque — et il manque VISIBLEMENT, ce qui vaut mieux
				//    qu'un vide qui ferait croire la mise en page correcte.
				NkGuiComponentPaint::Icon(r, iconHandle, role);
			}

			/// Le texte a COULEUR ET CORPS POSES (vocabulaire d'apparence §8ter).
			/// Le corps demande choisit le plus proche des sept atlas du costume
			/// (9..16 px) ; la graisse >= 500 s'approche par double trait — meme
			/// approximation que partout (les graisses d'Inter ne sont pas encore
			/// embarquees, dit dans Costume.h). Sans atlas valide : le repli du
			/// kit (texte au role, corps du peintre).
			void TextHex(const NkPaintRect &r, const char *s, uint32 rgba, uint16 roleRepli,
						 nkentseu::editorkit::NkTextAlign align, float32 px,
						 float32 graisse) override {
				auto &F = costume::Fontes();
				const nkentseu::nkgui::NkGuiFont *f = nullptr;
				if (px > 0.f) {
					struct C {
							const nkentseu::nkgui::NkGuiFont *f;
							float32 px;
					};
					const C c[7] = {{&F.px9, 9.f},	 {&F.px10, 10.f}, {&F.px11, 11.f},
									{&F.px12, 12.f}, {&F.px13, 13.f}, {&F.px15, 15.f},
									{&F.px16, 16.f}};
					float32 best = 1.0e9f;
					for (int32 i = 0; i < 7; ++i) {
						const float32 d = c[i].px > px ? c[i].px - px : px - c[i].px;
						if (c[i].f->Valid() && d < best) {
							best = d;
							f = c[i].f;
						}
					}
				}
				if (!f || !f->Valid()) {
					NkGuiComponentPaint::TextHex(r, s, rgba, roleRepli, align, px, graisse);
					return;
				}
				const nkentseu::nkgui::NkColor col = {(uint8)((rgba >> 24) & 0xFFu),
													  (uint8)((rgba >> 16) & 0xFFu),
													  (uint8)((rgba >> 8) & 0xFFu),
													  (uint8)(rgba & 0xFFu)};
				float32 tx = r.x;
				if (align == nkentseu::editorkit::NkTextAlign::Center)
					tx = r.x + (r.w - f->MeasureWidth(s)) * 0.5f;
				else if (align == nkentseu::editorkit::NkTextAlign::Right)
					tx = r.x + r.w - f->MeasureWidth(s);
				const float32 ty = costume::CentrerY(*f, r.y, r.h);
				costume::Texte(mCtx.DL(), *f, tx, ty, s, col);
				if (graisse >= 500.f)
					costume::Texte(mCtx.DL(), *f,
								   tx + (graisse >= 700.f ? 0.8f
										 : graisse >= 600.f ? 0.5f
															: 0.3f),
								   ty, s, col);
			}

		private:
			// Un triangle, pas un carre. `AddTriangleFilled` existe dans la liste
			// de dessin de NKGui ; c'est la seule forme dont ce fichier a besoin.
			//
			// ⚠️ LES PROPORTIONS SONT CELLES D'UN CHEVRON DE HIERARCHIE, pas d'une
			//    fleche : plus haut que large quand il pointe a droite, l'inverse
			//    quand il pointe en bas. Un triangle equilateral se lit comme un
			//    bouton « lecture », pas comme un pli.
			void Chevron(const NkPaintRect &r, uint16 role, bool ouvert) {
				// COSTUME BANANI (31/08) : le chevron de la maquette est un TRAIT
				// (polyligne 2 segments, bouts ronds, 9×9), plus le triangle plein
				// d'avant — même vocabulaire que toutes les icônes de l'export.
				const float32 ix = r.x + (r.w - 9.f) * 0.5f;
				const float32 iy = r.y + (r.h - 9.f) * 0.5f;
				const nkentseu::nkgui::NkColor col = Couleur(role);
				if (ouvert)
					costume::ChevronBas9(mCtx.DL(), ix, iy, col);
				else
					costume::ChevronDroit9(mCtx.DL(), ix, iy, col);
			}

			nkentseu::nkgui::NkColor Couleur(uint16 role) const {
				const uint32 c = mTheme.Get(role);
				return {(uint8)((c >> 24) & 0xFFu), (uint8)((c >> 16) & 0xFFu),
						(uint8)((c >> 8) & 0xFFu), (uint8)(c & 0xFFu)};
			}

			// ⚠️ DEUX REFERENCES DE PLUS, ET NON UN ACCES A CELLES DU PARENT : les
			//    membres de `NkGuiComponentPaint` sont **prives**. Les redeclarer
			//    ici est le prix d'une surcharge propre ; les rendre `protected`
			//    serait une modification d'un fichier partage pour un besoin local.
			nkentseu::nkgui::NkGuiContext &mCtx;
			const NkTheme &mTheme;
	};

} // namespace nkuidesign
