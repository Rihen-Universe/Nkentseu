#pragma once
// =============================================================================
// NkModelerUI.h — PEINTURE de l'interface, calquee sur la maquette Banani.
//
// POURQUOI ON NE PASSE PAS PAR NkEditorShell
//   Le shell de NKEditorKit apporte sa PROPRE chrome : barre de menus, barre
//   d'etat, systeme de docking, palette de commandes. C'est ce qu'il faut pour un
//   IDE, et c'est ce qui empeche de coller a une maquette au pixel pres -- on
//   passerait son temps a lutter contre une disposition qu'on ne controle pas.
//   NK3DModeler doit ressembler EXACTEMENT a l'ecran A : on peint donc nous-memes,
//   directement dans la draw list.
//
// TOUTES LES COULEURS VIENNENT DU THEME. Pas un seul 0xRRGGBB dans ce fichier :
//   c'est la condition pour que le theme clair, les themes utilisateur et les
//   roles propres au produit servent a quelque chose. Les seules constantes ici
//   sont des GEOMETRIES (hauteurs de ligne, largeurs de colonne), reprises des
//   proportions de la maquette.
//
// ETAT D'AVANCEMENT : la STRUCTURE est peinte, les interactions viendront par
//   iterations successives, comme convenu avec Rihen.
// =============================================================================

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKEditorKit/NkTheme.h"
#include "NK3DModeler/Shell/NkModelerTheme.h"

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::editorkit;
		using nkgui::NkColor;
		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		// ── PROPORTIONS DE LA MAQUETTE ──────────────────────────────────────────
		// Reprises telles quelles de l'ecran A. Les pourcentages sont bornes par un
		// minimum en pixels : sous 1024 de large, un panneau a 16 % deviendrait
		// illisible et l'utilisateur ne pourrait plus rien y lire.
		struct NkLayout {
				float32 menuH = 0.f, tabsH = 0.f, toolH = 0.f, browserH = 0.f, statusH = 0.f;
				float32 leftW = 0.f, rightW = 0.f;
				NkRect menu{}, tabs{}, tool{}, left{}, view{}, right{}, propsR{}, detailsR{}, browser{},
					status{};

				void Compute(float32 W, float32 H) {
					menuH = H * 0.04f;
					if (menuH < 44.f)
						menuH = 44.f;
					tabsH = H * 0.03f;
					if (tabsH < 32.f)
						tabsH = 32.f;
					// AMINCIE par rapport a la maquette (5 %) : Banani y avait mis des
					// boutons qui font doublon avec les menus de la vue. Seul son
					// commutateur Objet/Edition est garde -- il rend visible un etat qui,
					// chez Blender, n'existe que dans un menu deroulant.
					toolH = 34.f;
					browserH = H * 0.22f;
					if (browserH < 200.f)
						browserH = 200.f;
					statusH = H * 0.03f;
					if (statusH < 30.f)
						statusH = 30.f;
					leftW = W * 0.16f;
					if (leftW < 200.f)
						leftW = 200.f;
					rightW = W * 0.29f;
					if (rightW < 300.f)
						rightW = 300.f;

					float32 y = 0.f;
					menu = {0.f, y, W, menuH};
					y += menuH;
					tabs = {0.f, y, W, tabsH};
					y += tabsH;
					tool = {0.f, y, W, toolH};
					y += toolH;
					const float32 midH = H - y - browserH - statusH;
					left = {0.f, y, leftW, midH};
					view = {leftW, y, W - leftW - rightW, midH};
					right = {W - rightW, y, rightW, midH};
					// Proprietes AU-DESSUS des details, 45 / 55 comme la maquette.
					propsR = {right.x, right.y, right.w, midH * 0.45f};
					detailsR = {right.x, right.y + propsR.h, right.w, midH - propsR.h};
					y += midH;
					browser = {0.f, y, W, browserH};
					y += browserH;
					status = {0.f, y, W, statusH};
				}
		};

		// ── PEINTRE ─────────────────────────────────────────────────────────────
		class NkModelerPainter {
			public:
				NkModelerPainter(NkGuiDrawList &dl, NkGuiFont &font, const NkTheme &theme,
								 const NkModelerRoles &roles) noexcept
					: mDl(dl), mFont(font), mTh(theme), mRoles(roles) {}

				// Conversion theme -> couleur du moteur. Le theme stocke 0xRRGGBBAA ;
				// NkColor attend quatre octets. Un seul endroit fait la traduction.
				NkColor C(NkRole r) const {
					return Unpack(mTh.Get(r));
				}
				NkColor C(uint16 id) const {
					return Unpack(mTh.Get(id));
				}

				float32 LineH() const {
					return mFont.LineHeight();
				}
				float32 TextW(const char *s) const {
					return mFont.MeasureWidth(s);
				}

				void Fill(const NkRect &r, NkRole role, float32 rounding = 0.f) {
					mDl.AddRectFilled(r, C(role), rounding);
				}
				void Fill(const NkRect &r, const NkColor &c, float32 rounding = 0.f) {
					mDl.AddRectFilled(r, c, rounding);
				}

				// Trait de separation. Toujours au meme role : une bordure dessinee
				// avec une autre couleur « qui va bien » se voit immediatement quand on
				// change de theme.
				void HLine(float32 x, float32 y, float32 w) {
					mDl.AddRectFilled({x, y, w, 1.f}, C(NkRole::Border));
				}
				void VLine(float32 x, float32 y, float32 h) {
					mDl.AddRectFilled({x, y, 1.f, h}, C(NkRole::Border));
				}

				// Texte cale sur la LIGNE DE BASE, pas sur le haut du glyphe : sans ca
				// les libelles « sautent » d'un widget a l'autre selon leurs jambages.
				void Text(float32 x, float32 y, const char *s, NkRole role = NkRole::Text) {
					mDl.AddText(mFont.Face(), mFont.TexId(), {x, y + mFont.Ascent()}, s, C(role));
				}
				void Text(float32 x, float32 y, const char *s, const NkColor &c) {
					mDl.AddText(mFont.Face(), mFont.TexId(), {x, y + mFont.Ascent()}, s, c);
				}
				// Centre verticalement dans une hauteur donnee.
				void TextV(float32 x, float32 y, float32 h, const char *s, NkRole role = NkRole::Text) {
					Text(x, y + (h - mFont.LineHeight()) * 0.5f, s, role);
				}
				void TextV(float32 x, float32 y, float32 h, const char *s, const NkColor &c) {
					Text(x, y + (h - mFont.LineHeight()) * 0.5f, s, c);
				}

				const NkTheme &Theme() const {
					return mTh;
				}
				const NkModelerRoles &Roles() const {
					return mRoles;
				}

			private:
				static NkColor Unpack(uint32 c) {
					return NkColor{(uint8)((c >> 24) & 0xFF), (uint8)((c >> 16) & 0xFF),
								   (uint8)((c >> 8) & 0xFF), (uint8)(c & 0xFF)};
				}
				NkGuiDrawList &mDl;
				NkGuiFont &mFont;
				const NkTheme &mTh;
				const NkModelerRoles &mRoles;
		};

	} // namespace nk3d
} // namespace nkentseu
